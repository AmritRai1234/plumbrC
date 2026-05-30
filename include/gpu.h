/*
 * PlumbrC - GPU Acceleration via OpenCL
 * Optional GPU-accelerated Aho-Corasick DFA scanning
 *
 * Compile with -DPLUMBR_GPU -lOpenCL to enable.
 * When disabled, all functions are no-ops or return NULL.
 */

#ifndef PLUMBR_GPU_H
#define PLUMBR_GPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct GpuContext GpuContext;

/* Match result from GPU scan (mirrors ACMatch) */
typedef struct {
  uint32_t line_id;    /* Which line in the batch */
  uint32_t pattern_id; /* Matched pattern ID */
  uint32_t position;   /* Byte position in line */
  uint16_t length;     /* Match length */
} GpuMatch;

/* Per-line GPU scan result */
typedef struct {
  uint32_t num_matches;        /* Number of matches found in this line */
  uint32_t match_indices[16];  /* Pattern IDs of matches (max 16 per line) */
} GpuLineResult;

#ifdef PLUMBR_GPU

/*
 * Initialize GPU context: detect OpenCL device, create context,
 * compile AC DFA kernel. Returns NULL if no GPU available.
 *
 * Thread safety: call once, share the returned context across threads
 * (OpenCL command queues are thread-safe).
 */
GpuContext *gpu_init(void);

/*
 * Upload the flat AC DFA table and metadata to GPU memory.
 * Must be called after ac_build() and before gpu_scan_batch().
 *
 * dfa:        flat DFA table — dfa[state * 256 + byte] = next_state
 * meta_final: per-state boolean — meta_final[state] = is accepting state
 * meta_pat:   per-state pattern ID — meta_pat[state] = pattern_id (if final)
 * meta_depth: per-state depth — meta_depth[state] = pattern length
 * num_states: total number of DFA states
 */
bool gpu_upload_dfa(GpuContext *ctx, const int16_t *dfa,
                    const bool *meta_final, const uint32_t *meta_pat,
                    const uint16_t *meta_depth, size_t num_states);

/*
 * Scan a batch of lines on the GPU using the uploaded DFA.
 *
 * lines:      array of line pointers
 * lengths:    array of line lengths
 * num_lines:  number of lines in batch
 * results:    output array (caller-allocated, num_lines elements)
 *
 * Returns true if GPU scan succeeded. On failure, caller should
 * fall back to CPU path.
 */
bool gpu_scan_batch(GpuContext *ctx, const char **lines,
                    const size_t *lengths, size_t num_lines,
                    GpuLineResult *results);

/*
 * Get GPU device name (for logging/stats).
 */
const char *gpu_device_name(const GpuContext *ctx);

/*
 * Get GPU memory usage in bytes.
 */
size_t gpu_memory_used(const GpuContext *ctx);

/*
 * Destroy GPU context and free all resources.
 */
void gpu_destroy(GpuContext *ctx);

#else /* !PLUMBR_GPU */

/* Stubs when GPU support is not compiled in */
static inline GpuContext *gpu_init(void) { return NULL; }
static inline bool gpu_upload_dfa(GpuContext *ctx, const int16_t *dfa,
                                  const bool *meta_final,
                                  const uint32_t *meta_pat,
                                  const uint16_t *meta_depth,
                                  size_t num_states) {
  (void)ctx; (void)dfa; (void)meta_final; (void)meta_pat;
  (void)meta_depth; (void)num_states;
  return false;
}
static inline bool gpu_scan_batch(GpuContext *ctx, const char **lines,
                                  const size_t *lengths, size_t num_lines,
                                  GpuLineResult *results) {
  (void)ctx; (void)lines; (void)lengths; (void)num_lines; (void)results;
  return false;
}
static inline const char *gpu_device_name(const GpuContext *ctx) {
  (void)ctx; return "none";
}
static inline size_t gpu_memory_used(const GpuContext *ctx) {
  (void)ctx; return 0;
}
static inline void gpu_destroy(GpuContext *ctx) { (void)ctx; }

#endif /* PLUMBR_GPU */

#endif /* PLUMBR_GPU_H */
