/*
 * PlumbrC - Hardware Detection
 * Auto-detect CPU/GPU capabilities for optimal settings
 */

#ifndef PLUMBR_HWDETECT_H
#define PLUMBR_HWDETECT_H

#include <stdbool.h>
#include <stdint.h>

/* CPU Vendor */
typedef enum {
  CPU_VENDOR_UNKNOWN = 0,
  CPU_VENDOR_AMD,
  CPU_VENDOR_INTEL,
  CPU_VENDOR_ARM
} CpuVendor;

/* CPU Features */
typedef struct {
  CpuVendor vendor;
  char brand[64];

  /* SIMD capabilities */
  bool has_sse2;
  bool has_sse42;
  bool has_avx;
  bool has_avx2;
  bool has_avx512;

  /* String operations */
  bool has_popcnt;
  bool has_bmi1;
  bool has_bmi2;

  /* Cache info */
  uint32_t l1_cache_kb;
  uint32_t l2_cache_kb;
  uint32_t l3_cache_kb;
  uint32_t cache_line_size;

  /* Cores */
  uint32_t physical_cores;
  uint32_t logical_cores;

  /* AMD specific */
  bool is_zen3; /* Ryzen 5000 */
  bool is_zen4; /* Ryzen 7000 */
} CpuInfo;

/* GPU Info */
typedef struct {
  bool available;
  char name[128];

  /* OpenCL support */
  bool has_opencl;
  uint32_t opencl_version; /* e.g., 200 for OpenCL 2.0 */

  /* Memory */
  uint64_t vram_mb;

  /* AMD specific */
  bool is_rdna;
  bool is_rdna2;
  bool is_rdna3;
} GpuInfo;

/* Combined hardware info */
typedef struct {
  CpuInfo cpu;
  GpuInfo gpu;

  /* Recommended settings based on detection */
  int recommended_threads;
  bool use_avx2;
  bool use_gpu;
} HardwareInfo;

/* Detect hardware - call once at startup */
void hwdetect_init(HardwareInfo *info);

/* Print hardware info */
void hwdetect_print(const HardwareInfo *info);

/* Get recommended batch size based on cache */
int hwdetect_optimal_batch_size(const HardwareInfo *info);

#endif /* PLUMBR_HWDETECT_H */
