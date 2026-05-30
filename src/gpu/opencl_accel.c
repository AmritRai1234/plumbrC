/*
 * PlumbrC - OpenCL GPU Acceleration
 * Host-side code for GPU-accelerated AC DFA scanning
 *
 * Only compiled when PLUMBR_GPU is defined.
 */

#ifdef PLUMBR_GPU

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

#include "gpu.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Embedded kernel source (from ac_scan.cl) */
static const char *KERNEL_SOURCE =
"typedef struct { uint offset; uint length; } LineDesc;\n"
"typedef struct { uint num_matches; uint match_indices[16]; } LineResult;\n"
"\n"
"__kernel void ac_dfa_scan(\n"
"    __global const short *dfa,\n"
"    __global const uchar *meta_final,\n"
"    __global const uint  *meta_pat,\n"
"    __global const uchar *line_data,\n"
"    __global const LineDesc *line_descs,\n"
"    __global LineResult *results,\n"
"    const uint num_states\n"
") {\n"
"  uint line_id = get_global_id(0);\n"
"  uint offset = line_descs[line_id].offset;\n"
"  uint length = line_descs[line_id].length;\n"
"  results[line_id].num_matches = 0;\n"
"  short state = 0;\n"
"  uint num_matches = 0;\n"
"  for (uint i = 0; i < length; i++) {\n"
"    uchar byte = line_data[offset + i];\n"
"    short next = dfa[(uint)state * 256u + (uint)byte];\n"
"    if (next < 0 || (uint)next >= num_states) { state = 0; continue; }\n"
"    state = next;\n"
"    if (meta_final[(uint)state]) {\n"
"      if (num_matches < 16u) {\n"
"        results[line_id].match_indices[num_matches] = meta_pat[(uint)state];\n"
"        num_matches++;\n"
"      }\n"
"    }\n"
"  }\n"
"  results[line_id].num_matches = num_matches;\n"
"}\n";

/* GPU line descriptor (must match kernel struct) */
typedef struct {
  uint32_t offset;
  uint32_t length;
} GpuLineDesc;

/* Max batch parameters */
#define GPU_MAX_BATCH_LINES  PLUMBR_BATCH_SIZE
#define GPU_MAX_LINE_DATA    (GPU_MAX_BATCH_LINES * 256) /* ~2MB for line data */

struct GpuContext {
  /* OpenCL objects */
  cl_platform_id platform;
  cl_device_id device;
  cl_context context;
  cl_command_queue queue;
  cl_program program;
  cl_kernel kernel;

  /* DFA buffers (persistent — uploaded once) */
  cl_mem dfa_buf;        /* Flat DFA table */
  cl_mem meta_final_buf; /* Per-state is_final */
  cl_mem meta_pat_buf;   /* Per-state pattern_id */
  uint32_t num_states;

  /* Batch buffers (reused per batch) */
  cl_mem line_data_buf;  /* Packed line bytes */
  cl_mem line_desc_buf;  /* Per-line {offset, length} */
  cl_mem results_buf;    /* Per-line match results */

  /* Host staging buffers */
  unsigned char *line_data_staging;
  GpuLineDesc *line_desc_staging;

  /* Device info */
  char device_name[256];
  size_t memory_used;

  /* Workgroup size */
  size_t max_workgroup_size;
};

/* ─── Initialization ──────────────────────────────────────────── */

GpuContext *gpu_init(void) {
  /* Check for forced disable */
  if (getenv("PLUMBR_NO_GPU")) {
    return NULL;
  }

  cl_int err;

  /* Find OpenCL platform */
  cl_uint num_platforms = 0;
  err = clGetPlatformIDs(0, NULL, &num_platforms);
  if (err != CL_SUCCESS || num_platforms == 0) {
    return NULL;
  }

  cl_platform_id *platforms = malloc(num_platforms * sizeof(cl_platform_id));
  if (!platforms) return NULL;
  clGetPlatformIDs(num_platforms, platforms, NULL);

  /* Find a GPU device (prefer discrete, accept integrated) */
  cl_device_id device = NULL;
  cl_platform_id platform = NULL;

  for (cl_uint p = 0; p < num_platforms && !device; p++) {
    cl_uint num_devices = 0;
    err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
    if (err != CL_SUCCESS || num_devices == 0) continue;

    cl_device_id *devices = malloc(num_devices * sizeof(cl_device_id));
    if (!devices) continue;
    clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, num_devices, devices, NULL);

    /* Take the first GPU */
    device = devices[0];
    platform = platforms[p];
    free(devices);
  }
  free(platforms);

  if (!device) {
    return NULL;
  }

  /* Allocate context struct */
  GpuContext *ctx = calloc(1, sizeof(GpuContext));
  if (!ctx) return NULL;

  ctx->platform = platform;
  ctx->device = device;

  /* Get device name */
  clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(ctx->device_name),
                  ctx->device_name, NULL);

  /* Get max workgroup size */
  clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE,
                  sizeof(ctx->max_workgroup_size), &ctx->max_workgroup_size, NULL);

  /* Create OpenCL context */
  ctx->context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "PlumbrC GPU: failed to create OpenCL context (err=%d)\n", err);
    free(ctx);
    return NULL;
  }

  /* Create command queue */
  ctx->queue = clCreateCommandQueue(ctx->context, device, 0, &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "PlumbrC GPU: failed to create command queue (err=%d)\n", err);
    clReleaseContext(ctx->context);
    free(ctx);
    return NULL;
  }

  /* Build kernel from embedded source */
  size_t src_len = strlen(KERNEL_SOURCE);
  ctx->program = clCreateProgramWithSource(ctx->context, 1, &KERNEL_SOURCE,
                                           &src_len, &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "PlumbrC GPU: failed to create program (err=%d)\n", err);
    goto fail;
  }

  err = clBuildProgram(ctx->program, 1, &device, "-cl-fast-relaxed-math", NULL, NULL);
  if (err != CL_SUCCESS) {
    /* Print build log */
    size_t log_size;
    clGetProgramBuildInfo(ctx->program, device, CL_PROGRAM_BUILD_LOG,
                          0, NULL, &log_size);
    char *log = malloc(log_size + 1);
    if (log) {
      clGetProgramBuildInfo(ctx->program, device, CL_PROGRAM_BUILD_LOG,
                            log_size, log, NULL);
      log[log_size] = '\0';
      fprintf(stderr, "PlumbrC GPU: kernel build failed:\n%s\n", log);
      free(log);
    }
    goto fail;
  }

  ctx->kernel = clCreateKernel(ctx->program, "ac_dfa_scan", &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "PlumbrC GPU: failed to create kernel (err=%d)\n", err);
    goto fail;
  }

  /* Allocate batch buffers */
  size_t line_data_size = GPU_MAX_LINE_DATA;
  size_t line_desc_size = GPU_MAX_BATCH_LINES * sizeof(GpuLineDesc);
  size_t results_size = GPU_MAX_BATCH_LINES * sizeof(GpuLineResult);

  ctx->line_data_buf = clCreateBuffer(ctx->context,
      CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR,
      line_data_size, NULL, &err);
  if (err != CL_SUCCESS) goto fail;

  ctx->line_desc_buf = clCreateBuffer(ctx->context,
      CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR,
      line_desc_size, NULL, &err);
  if (err != CL_SUCCESS) goto fail;

  ctx->results_buf = clCreateBuffer(ctx->context,
      CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR,
      results_size, NULL, &err);
  if (err != CL_SUCCESS) goto fail;

  /* Allocate host staging buffers */
  ctx->line_data_staging = malloc(line_data_size);
  ctx->line_desc_staging = malloc(line_desc_size);
  if (!ctx->line_data_staging || !ctx->line_desc_staging) goto fail;

  ctx->memory_used = line_data_size + line_desc_size + results_size;

  fprintf(stderr, "PlumbrC GPU: initialized %s (OpenCL)\n", ctx->device_name);

  return ctx;

fail:
  gpu_destroy(ctx);
  return NULL;
}

/* ─── DFA Upload ──────────────────────────────────────────────── */

bool gpu_upload_dfa(GpuContext *ctx, const int16_t *dfa,
                    const bool *meta_final, const uint32_t *meta_pat,
                    const uint16_t *meta_depth, size_t num_states) {
  if (!ctx || !dfa || !meta_final || !meta_pat) return false;

  cl_int err;
  ctx->num_states = (uint32_t)num_states;

  /* DFA table: states * 256 * sizeof(int16_t) */
  size_t dfa_size = num_states * 256 * sizeof(int16_t);
  ctx->dfa_buf = clCreateBuffer(ctx->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                dfa_size, (void *)dfa, &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "PlumbrC GPU: failed to upload DFA table (err=%d)\n", err);
    return false;
  }

  /* Convert bool array to uchar for GPU (bool may be >1 byte on host) */
  unsigned char *final_uchar = malloc(num_states);
  if (!final_uchar) return false;
  for (size_t i = 0; i < num_states; i++) {
    final_uchar[i] = meta_final[i] ? 1 : 0;
  }

  ctx->meta_final_buf = clCreateBuffer(ctx->context,
      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
      num_states, final_uchar, &err);
  free(final_uchar);
  if (err != CL_SUCCESS) return false;

  ctx->meta_pat_buf = clCreateBuffer(ctx->context,
      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
      num_states * sizeof(uint32_t), (void *)meta_pat, &err);
  if (err != CL_SUCCESS) return false;

  ctx->memory_used += dfa_size + num_states + num_states * sizeof(uint32_t);

  /* Set persistent kernel arguments (DFA doesn't change between batches) */
  clSetKernelArg(ctx->kernel, 0, sizeof(cl_mem), &ctx->dfa_buf);
  clSetKernelArg(ctx->kernel, 1, sizeof(cl_mem), &ctx->meta_final_buf);
  clSetKernelArg(ctx->kernel, 2, sizeof(cl_mem), &ctx->meta_pat_buf);
  clSetKernelArg(ctx->kernel, 6, sizeof(uint32_t), &ctx->num_states);

  (void)meta_depth; /* Not needed for GPU scan — CPU uses it for verification */

  fprintf(stderr, "PlumbrC GPU: uploaded DFA (%zu states, %.1f KB)\n",
          num_states, (double)dfa_size / 1024.0);

  return true;
}

/* ─── Batch Scan ──────────────────────────────────────────────── */

bool gpu_scan_batch(GpuContext *ctx, const char **lines,
                    const size_t *lengths, size_t num_lines,
                    GpuLineResult *results) {
  if (!ctx || !lines || !lengths || !results || num_lines == 0) return false;
  if (!ctx->dfa_buf) return false; /* DFA not uploaded */

  cl_int err;

  /* Pack lines into contiguous buffer + build descriptors */
  uint32_t data_offset = 0;
  for (size_t i = 0; i < num_lines; i++) {
    size_t len = lengths[i];
    if (data_offset + len > GPU_MAX_LINE_DATA) {
      /* Batch too large for GPU buffer — fall back to CPU */
      return false;
    }
    memcpy(ctx->line_data_staging + data_offset, lines[i], len);
    ctx->line_desc_staging[i].offset = data_offset;
    ctx->line_desc_staging[i].length = (uint32_t)len;
    data_offset += (uint32_t)len;
  }

  /* Upload line data to GPU */
  err = clEnqueueWriteBuffer(ctx->queue, ctx->line_data_buf, CL_FALSE,
                             0, data_offset, ctx->line_data_staging,
                             0, NULL, NULL);
  if (err != CL_SUCCESS) return false;

  /* Upload line descriptors */
  size_t desc_size = num_lines * sizeof(GpuLineDesc);
  err = clEnqueueWriteBuffer(ctx->queue, ctx->line_desc_buf, CL_FALSE,
                             0, desc_size, ctx->line_desc_staging,
                             0, NULL, NULL);
  if (err != CL_SUCCESS) return false;

  /* Set per-batch kernel arguments */
  clSetKernelArg(ctx->kernel, 3, sizeof(cl_mem), &ctx->line_data_buf);
  clSetKernelArg(ctx->kernel, 4, sizeof(cl_mem), &ctx->line_desc_buf);
  clSetKernelArg(ctx->kernel, 5, sizeof(cl_mem), &ctx->results_buf);

  /* Launch kernel: one work-item per line */
  size_t global_size = num_lines;

  /* Round up to workgroup multiple for efficiency */
  size_t local_size = 64;
  if (local_size > ctx->max_workgroup_size) {
    local_size = ctx->max_workgroup_size;
  }
  /* Ensure global_size is a multiple of local_size */
  size_t padded_global = ((global_size + local_size - 1) / local_size) * local_size;

  /* For padded work-items, we need line_descs to have valid (but zero-length) entries.
   * Set extra descriptors to zero length to make the kernel a no-op for them. */
  for (size_t i = num_lines; i < padded_global && i < GPU_MAX_BATCH_LINES; i++) {
    ctx->line_desc_staging[i].offset = 0;
    ctx->line_desc_staging[i].length = 0;
  }
  if (padded_global > num_lines) {
    /* Re-upload expanded descriptors */
    size_t expanded_desc_size = padded_global * sizeof(GpuLineDesc);
    clEnqueueWriteBuffer(ctx->queue, ctx->line_desc_buf, CL_FALSE,
                         0, expanded_desc_size, ctx->line_desc_staging,
                         0, NULL, NULL);
  }

  err = clEnqueueNDRangeKernel(ctx->queue, ctx->kernel, 1, NULL,
                               &padded_global, &local_size, 0, NULL, NULL);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "PlumbrC GPU: kernel launch failed (err=%d)\n", err);
    return false;
  }

  /* Read back results (blocking) */
  size_t results_size = num_lines * sizeof(GpuLineResult);
  err = clEnqueueReadBuffer(ctx->queue, ctx->results_buf, CL_TRUE,
                            0, results_size, results, 0, NULL, NULL);
  if (err != CL_SUCCESS) return false;

  return true;
}

/* ─── Getters ─────────────────────────────────────────────────── */

const char *gpu_device_name(const GpuContext *ctx) {
  return ctx ? ctx->device_name : "none";
}

size_t gpu_memory_used(const GpuContext *ctx) {
  return ctx ? ctx->memory_used : 0;
}

/* ─── Cleanup ─────────────────────────────────────────────────── */

void gpu_destroy(GpuContext *ctx) {
  if (!ctx) return;

  if (ctx->kernel) clReleaseKernel(ctx->kernel);
  if (ctx->program) clReleaseProgram(ctx->program);
  if (ctx->dfa_buf) clReleaseMemObject(ctx->dfa_buf);
  if (ctx->meta_final_buf) clReleaseMemObject(ctx->meta_final_buf);
  if (ctx->meta_pat_buf) clReleaseMemObject(ctx->meta_pat_buf);
  if (ctx->line_data_buf) clReleaseMemObject(ctx->line_data_buf);
  if (ctx->line_desc_buf) clReleaseMemObject(ctx->line_desc_buf);
  if (ctx->results_buf) clReleaseMemObject(ctx->results_buf);
  if (ctx->queue) clReleaseCommandQueue(ctx->queue);
  if (ctx->context) clReleaseContext(ctx->context);

  free(ctx->line_data_staging);
  free(ctx->line_desc_staging);
  free(ctx);
}

#endif /* PLUMBR_GPU */
