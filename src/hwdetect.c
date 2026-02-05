/*
 * PlumbrC - Hardware Detection
 * Auto-detect CPU/GPU capabilities using CPUID and system info
 */

#include "hwdetect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __x86_64__
#include <cpuid.h>
#endif

/* Get CPUID info */
static void get_cpuid(uint32_t func, uint32_t subfunc, uint32_t *eax,
                      uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
#ifdef __x86_64__
  __cpuid_count(func, subfunc, *eax, *ebx, *ecx, *edx);
#else
  *eax = *ebx = *ecx = *edx = 0;
#endif
}

/* Detect CPU vendor */
static CpuVendor detect_vendor(void) {
#ifdef __x86_64__
  uint32_t eax, ebx, ecx, edx;
  get_cpuid(0, 0, &eax, &ebx, &ecx, &edx);

  /* Check vendor string */
  char vendor[13] = {0};
  memcpy(vendor + 0, &ebx, 4);
  memcpy(vendor + 4, &edx, 4);
  memcpy(vendor + 8, &ecx, 4);

  if (strcmp(vendor, "AuthenticAMD") == 0) {
    return CPU_VENDOR_AMD;
  } else if (strcmp(vendor, "GenuineIntel") == 0) {
    return CPU_VENDOR_INTEL;
  }
#endif

#ifdef __aarch64__
  return CPU_VENDOR_ARM;
#endif

  return CPU_VENDOR_UNKNOWN;
}

/* Get CPU brand string */
static void get_brand_string(char *brand, size_t len) {
#ifdef __x86_64__
  uint32_t eax, ebx, ecx, edx;
  char tmp[49] = {0};

  /* Extended function 0x80000002-0x80000004 */
  get_cpuid(0x80000002, 0, &eax, &ebx, &ecx, &edx);
  memcpy(tmp + 0, &eax, 4);
  memcpy(tmp + 4, &ebx, 4);
  memcpy(tmp + 8, &ecx, 4);
  memcpy(tmp + 12, &edx, 4);

  get_cpuid(0x80000003, 0, &eax, &ebx, &ecx, &edx);
  memcpy(tmp + 16, &eax, 4);
  memcpy(tmp + 20, &ebx, 4);
  memcpy(tmp + 24, &ecx, 4);
  memcpy(tmp + 28, &edx, 4);

  get_cpuid(0x80000004, 0, &eax, &ebx, &ecx, &edx);
  memcpy(tmp + 32, &eax, 4);
  memcpy(tmp + 36, &ebx, 4);
  memcpy(tmp + 40, &ecx, 4);
  memcpy(tmp + 44, &edx, 4);

  /* Trim leading spaces */
  char *p = tmp;
  while (*p == ' ')
    p++;
  snprintf(brand, len, "%s", p);
#else
  snprintf(brand, len, "Unknown CPU");
#endif
}

/* Detect CPU features */
static void detect_features(CpuInfo *cpu) {
#ifdef __x86_64__
  uint32_t eax, ebx, ecx, edx;

  /* Check max supported function */
  get_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
  uint32_t max_func = eax;

  if (max_func >= 1) {
    get_cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    /* EDX flags */
    cpu->has_sse2 = (edx >> 26) & 1;

    /* ECX flags */
    cpu->has_sse42 = (ecx >> 20) & 1;
    cpu->has_popcnt = (ecx >> 23) & 1;
    cpu->has_avx = (ecx >> 28) & 1;
  }

  if (max_func >= 7) {
    get_cpuid(7, 0, &eax, &ebx, &ecx, &edx);

    /* EBX flags */
    cpu->has_bmi1 = (ebx >> 3) & 1;
    cpu->has_avx2 = (ebx >> 5) & 1;
    cpu->has_bmi2 = (ebx >> 8) & 1;
    cpu->has_avx512 = (ebx >> 16) & 1; /* AVX-512F */
  }

  /* AMD Zen detection using family/model */
  if (cpu->vendor == CPU_VENDOR_AMD) {
    get_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    uint32_t family = ((eax >> 8) & 0xF) + ((eax >> 20) & 0xFF);
    uint32_t model = ((eax >> 4) & 0xF) | ((eax >> 12) & 0xF0);

    /* Zen 3: Family 0x19, Models 0x00-0x0F, 0x20-0x2F, 0x40-0x4F */
    if (family == 0x19) {
      if (model <= 0x0F || (model >= 0x20 && model <= 0x2F) ||
          (model >= 0x40 && model <= 0x4F)) {
        cpu->is_zen3 = true;
      }
    }

    /* Zen 4: Family 0x19, Models 0x60-0x7F */
    if (family == 0x19 && model >= 0x60 && model <= 0x7F) {
      cpu->is_zen4 = true;
    }
  }
#endif
}

/* Detect cache info */
static void detect_cache(CpuInfo *cpu) {
  /* Default cache line size */
  cpu->cache_line_size = 64;

#ifdef __x86_64__
  uint32_t eax, ebx, ecx, edx;

  /* Use CPUID to get cache info */
  get_cpuid(0x80000006, 0, &eax, &ebx, &ecx, &edx);

  /* L2 cache in ECX */
  cpu->l2_cache_kb = (ecx >> 16) & 0xFFFF;
  cpu->cache_line_size = ecx & 0xFF;

  /* L3 cache in EDX */
  cpu->l3_cache_kb = ((edx >> 18) & 0x3FFF) * 512;

  /* L1 data cache */
  get_cpuid(0x80000005, 0, &eax, &ebx, &ecx, &edx);
  cpu->l1_cache_kb = (ecx >> 24) & 0xFF;
#endif

  /* Fallback to sysconf */
  if (cpu->l1_cache_kb == 0) {
    long l1 = sysconf(_SC_LEVEL1_DCACHE_SIZE);
    if (l1 > 0)
      cpu->l1_cache_kb = l1 / 1024;
  }
  if (cpu->l2_cache_kb == 0) {
    long l2 = sysconf(_SC_LEVEL2_CACHE_SIZE);
    if (l2 > 0)
      cpu->l2_cache_kb = l2 / 1024;
  }
  if (cpu->l3_cache_kb == 0) {
    long l3 = sysconf(_SC_LEVEL3_CACHE_SIZE);
    if (l3 > 0)
      cpu->l3_cache_kb = l3 / 1024;
  }
}

/* Detect core count */
static void detect_cores(CpuInfo *cpu) {
  cpu->logical_cores = sysconf(_SC_NPROCESSORS_ONLN);

  /* Try to get physical cores from /sys */
  FILE *f =
      fopen("/sys/devices/system/cpu/cpu0/topology/core_siblings_list", "r");
  if (f) {
    /* Approximate: logical / 2 for SMT */
    cpu->physical_cores = cpu->logical_cores / 2;
    if (cpu->physical_cores == 0)
      cpu->physical_cores = 1;
    fclose(f);
  } else {
    cpu->physical_cores = cpu->logical_cores;
  }
}

/* Detect GPU */
static void detect_gpu(GpuInfo *gpu) {
  memset(gpu, 0, sizeof(GpuInfo));

  /* Check for AMD GPU via /sys */
  FILE *f = fopen("/sys/class/drm/card0/device/vendor", "r");
  if (f) {
    char vendor[32];
    if (fgets(vendor, sizeof(vendor), f)) {
      /* AMD vendor ID: 0x1002 */
      if (strstr(vendor, "0x1002") || strstr(vendor, "1002")) {
        gpu->available = true;
        snprintf(gpu->name, sizeof(gpu->name), "AMD Radeon GPU");

        /* Check for OpenCL */
        if (access("/usr/lib/libOpenCL.so", F_OK) == 0 ||
            access("/usr/lib/x86_64-linux-gnu/libOpenCL.so", F_OK) == 0) {
          gpu->has_opencl = true;
        }
      }
    }
    fclose(f);
  }

  /* Try to get GPU name from /sys */
  f = fopen("/sys/class/drm/card0/device/product_name", "r");
  if (f) {
    if (fgets(gpu->name, sizeof(gpu->name), f)) {
      /* Remove newline */
      char *nl = strchr(gpu->name, '\n');
      if (nl)
        *nl = '\0';
    }
    fclose(f);
  }
}

/* Detect memory info */
static void detect_memory(MemoryInfo *mem) {
  memset(mem, 0, sizeof(MemoryInfo));

  /* Get total memory from /proc/meminfo */
  FILE *f = fopen("/proc/meminfo", "r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      if (strncmp(line, "MemTotal:", 9) == 0) {
        uint64_t kb;
        if (sscanf(line + 9, "%lu", &kb) == 1) {
          mem->total_mb = kb / 1024;
        }
        break;
      }
    }
    fclose(f);
  }

  /* Try to detect memory type and speed from DMI (requires root) */
  /* Fallback: estimate from CPU generation */
  mem->type = MEM_TYPE_DDR4; /* Default assumption */
  mem->speed_mhz = 3200;     /* Common DDR4 speed */
  mem->channels = 2;         /* Dual channel typical */

  /* Try dmidecode if available (requires root, so this is best-effort) */
  f = popen(
      "cat /sys/devices/system/edac/mc/mc0/dimm0/dimm_mem_type 2>/dev/null",
      "r");
  if (f) {
    char buf[64] = {0};
    if (fgets(buf, sizeof(buf), f)) {
      if (strstr(buf, "DDR5")) {
        mem->type = MEM_TYPE_DDR5;
        mem->speed_mhz = 4800; /* Base DDR5 */
      } else if (strstr(buf, "DDR4")) {
        mem->type = MEM_TYPE_DDR4;
      } else if (strstr(buf, "DDR3")) {
        mem->type = MEM_TYPE_DDR3;
        mem->speed_mhz = 1600;
      }
    }
    pclose(f);
  }

  /* Calculate theoretical bandwidth:
   * DDR4-3200 dual channel = 3200 MHz * 8 bytes * 2 channels = 51.2 GB/s */
  mem->bandwidth_mb_sec = (uint64_t)mem->speed_mhz * 8 * mem->channels;
}

/* Quick memory bandwidth test */
static uint64_t measure_memory_bandwidth(void) {
  /* Allocate 64MB test buffer */
  size_t test_size = 64 * 1024 * 1024;
  char *buf = malloc(test_size);
  if (!buf)
    return 0;

  /* Touch all pages */
  memset(buf, 0, test_size);

  /* Time sequential reads */
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  volatile uint64_t sum = 0;
  for (size_t i = 0; i < test_size; i += 64) {
    sum += buf[i];
  }
  (void)sum; /* Prevent optimization */

  clock_gettime(CLOCK_MONOTONIC, &end);

  free(buf);

  /* Calculate bandwidth */
  double elapsed =
      (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  if (elapsed < 0.001)
    elapsed = 0.001;

  return (uint64_t)(test_size / elapsed / (1024 * 1024));
}

/* Main detection function */
void hwdetect_init(HardwareInfo *info) {
  memset(info, 0, sizeof(HardwareInfo));

  /* Detect CPU */
  info->cpu.vendor = detect_vendor();
  get_brand_string(info->cpu.brand, sizeof(info->cpu.brand));
  detect_features(&info->cpu);
  detect_cache(&info->cpu);
  detect_cores(&info->cpu);

  /* Detect GPU */
  detect_gpu(&info->gpu);

  /* Detect Memory */
  detect_memory(&info->memory);
  info->memory.measured_bandwidth_mb_sec = measure_memory_bandwidth();

  /* Set basic recommendations */
  info->recommended_threads = info->cpu.physical_cores;
  info->use_avx2 = info->cpu.has_avx2;
  info->use_gpu = info->gpu.available && info->gpu.has_opencl;

  /* Calculate optimal batch size */
  info->optimal_batch_size = hwdetect_optimal_batch_size(info);

  /* Set initial thread estimates based on memory bandwidth */
  /* Rule: each thread needs ~100MB/s bandwidth for this workload */
  uint64_t bw = info->memory.measured_bandwidth_mb_sec;
  if (bw > 0) {
    info->max_useful_threads = (int)(bw / 100);
    if (info->max_useful_threads > (int)info->cpu.logical_cores) {
      info->max_useful_threads = info->cpu.logical_cores;
    }
    if (info->max_useful_threads < 1) {
      info->max_useful_threads = 1;
    }
  } else {
    info->max_useful_threads = info->cpu.physical_cores;
  }

  /* Default optimal = physical cores (safe choice) */
  info->optimal_threads = info->cpu.physical_cores;
}

/* Print hardware info */
void hwdetect_print(const HardwareInfo *info) {
  fprintf(stderr, "\n=== Hardware Detection ===\n");

  /* CPU Info */
  fprintf(stderr, "CPU: %s\n", info->cpu.brand);
  fprintf(stderr, "Vendor: %s\n",
          info->cpu.vendor == CPU_VENDOR_AMD     ? "AMD"
          : info->cpu.vendor == CPU_VENDOR_INTEL ? "Intel"
          : info->cpu.vendor == CPU_VENDOR_ARM   ? "ARM"
                                                 : "Unknown");
  fprintf(stderr, "Cores: %u physical, %u logical\n", info->cpu.physical_cores,
          info->cpu.logical_cores);
  fprintf(stderr, "Cache: L1=%uKB L2=%uKB L3=%uKB (line=%u bytes)\n",
          info->cpu.l1_cache_kb, info->cpu.l2_cache_kb, info->cpu.l3_cache_kb,
          info->cpu.cache_line_size);

  /* Features */
  fprintf(stderr, "SIMD: ");
  if (info->cpu.has_avx512)
    fprintf(stderr, "AVX-512 ");
  if (info->cpu.has_avx2)
    fprintf(stderr, "AVX2 ");
  if (info->cpu.has_avx)
    fprintf(stderr, "AVX ");
  if (info->cpu.has_sse42)
    fprintf(stderr, "SSE4.2 ");
  if (info->cpu.has_sse2)
    fprintf(stderr, "SSE2 ");
  fprintf(stderr, "\n");

  if (info->cpu.is_zen3)
    fprintf(stderr, "Architecture: AMD Zen 3 (Ryzen 5000)\n");
  if (info->cpu.is_zen4)
    fprintf(stderr, "Architecture: AMD Zen 4 (Ryzen 7000)\n");

  /* GPU Info */
  if (info->gpu.available) {
    fprintf(stderr, "GPU: %s\n", info->gpu.name);
    fprintf(stderr, "OpenCL: %s\n", info->gpu.has_opencl ? "Yes" : "No");
  } else {
    fprintf(stderr, "GPU: Not detected\n");
  }

  /* Memory Info */
  const char *mem_type_str = "Unknown";
  switch (info->memory.type) {
  case MEM_TYPE_DDR3:
    mem_type_str = "DDR3";
    break;
  case MEM_TYPE_DDR4:
    mem_type_str = "DDR4";
    break;
  case MEM_TYPE_DDR5:
    mem_type_str = "DDR5";
    break;
  case MEM_TYPE_LPDDR4:
    mem_type_str = "LPDDR4";
    break;
  case MEM_TYPE_LPDDR5:
    mem_type_str = "LPDDR5";
    break;
  default:
    break;
  }
  fprintf(stderr, "\nMemory: %lu MB %s-%u (%u-channel)\n",
          (unsigned long)info->memory.total_mb, mem_type_str,
          info->memory.speed_mhz, info->memory.channels);
  fprintf(stderr, "Bandwidth: %lu MB/s theoretical, %lu MB/s measured\n",
          (unsigned long)info->memory.bandwidth_mb_sec,
          (unsigned long)info->memory.measured_bandwidth_mb_sec);

  /* Recommendations */
  fprintf(stderr, "\n--- Auto-Tuned Settings ---\n");
  fprintf(stderr, "Optimal Threads: %d (max useful: %d)\n",
          info->optimal_threads, info->max_useful_threads);
  fprintf(stderr, "Batch Size: %zu lines\n", info->optimal_batch_size);
  fprintf(stderr, "Use AVX2: %s\n", info->use_avx2 ? "Yes" : "No");
  fprintf(stderr, "Use GPU: %s\n", info->use_gpu ? "Yes" : "No");
  fprintf(stderr, "==========================\n\n");
}

/* Optimal batch size based on L3 cache */
int hwdetect_optimal_batch_size(const HardwareInfo *info) {
  /* Target: fit 2 batches in L3 cache */
  /* Each line ~100 bytes, so batch_size * 100 * 2 < L3 */
  int l3_bytes = (int)(info->cpu.l3_cache_kb * 1024);
  int optimal = l3_bytes / (100 * 2);

  /* Clamp to reasonable range */
  if (optimal < 256)
    optimal = 256;
  if (optimal > 16384)
    optimal = 16384;

  /* Round to power of 2 */
  int power = 256;
  while (power < optimal)
    power *= 2;

  return power;
}

/* Auto-tune: run quick benchmark to find optimal thread count */
int hwdetect_autotune_threads(HardwareInfo *info) {
  fprintf(stderr, "\n=== Auto-Tuning Thread Count ===\n");

  /* Simple heuristic based on memory bandwidth and workload characteristics:
   * - This workload is ~60% memory-bound, ~40% CPU-bound
   * - Each thread needs about 80-120 MB/s of memory bandwidth
   * - Beyond memory saturation, adding threads hurts performance
   */

  uint64_t bw = info->memory.measured_bandwidth_mb_sec;
  int physical = info->cpu.physical_cores;
  int logical = info->cpu.logical_cores;

  /* Calculate memory-limited thread count */
  int mem_limited = (int)(bw / 100); /* ~100 MB/s per thread */
  if (mem_limited < 1)
    mem_limited = 1;
  if (mem_limited > logical)
    mem_limited = logical;

  /* AMD Zen3/Zen4 benefit from SMT for this workload */
  int optimal;
  if (info->cpu.is_zen3 || info->cpu.is_zen4) {
    /* Zen3/Zen4: SMT helps, but cap at ~1.5x physical cores */
    optimal = physical + physical / 2;
    if (optimal > mem_limited)
      optimal = mem_limited;
    if (optimal > logical)
      optimal = logical;
  } else if (info->cpu.vendor == CPU_VENDOR_INTEL) {
    /* Intel: SMT helps less for memory-bound work */
    optimal = physical;
    if (bw > 10000) { /* >10GB/s bandwidth - can use more threads */
      optimal = (physical * 3) / 2;
    }
    if (optimal > mem_limited)
      optimal = mem_limited;
  } else {
    /* Default: physical cores */
    optimal = physical;
  }

  /* Minimum 1, maximum logical cores */
  if (optimal < 1)
    optimal = 1;
  if (optimal > logical)
    optimal = logical;

  info->optimal_threads = optimal;
  info->max_useful_threads = mem_limited;

  fprintf(stderr, "Memory bandwidth: %lu MB/s\n",
          (unsigned long)info->memory.measured_bandwidth_mb_sec);
  fprintf(stderr, "Physical cores: %d, Logical: %d\n", physical, logical);
  fprintf(stderr, "Memory-limited threads: %d\n", mem_limited);
  fprintf(stderr, "Optimal threads: %d\n", optimal);
  fprintf(stderr, "================================\n\n");

  return optimal;
}

/* Get optimal thread count */
int hwdetect_get_optimal_threads(const HardwareInfo *info) {
  if (info->optimal_threads > 0) {
    return info->optimal_threads;
  }
  return info->recommended_threads;
}
