/*
 * PlumbrC - AVX2 SIMD String Scanner
 * AMD Ryzen optimized implementation
 *
 * Uses 256-bit AVX2 registers to process 32 bytes per cycle
 */

#include "avx2_scan.h"

#include <stdint.h>
#include <string.h>

#if defined(__AVX2__)
#include <immintrin.h>

const char *avx2_memchr(const char *buf, size_t len, char c) {
  const char *end = buf + len;

  /* Handle small buffers with scalar code */
  if (len < 32) {
    return memchr(buf, c, len);
  }

  /* Broadcast search byte to all 32 lanes */
  __m256i needle = _mm256_set1_epi8(c);

  /* Process 32 bytes at a time */
  while (buf + 32 <= end) {
    int mask;
    __asm__ (
        "vmovdqu %[buf_val], %%ymm1\n\t"
        "vpcmpeqb %[needle], %%ymm1, %%ymm2\n\t"
        "vpmovmskb %%ymm2, %[mask]\n\t"
        : [mask] "=r"(mask)
        : [buf_val] "m"(*(const __m256i *)buf), [needle] "x"(needle)
        : "ymm1", "ymm2"
    );

    if (mask != 0) {
      /* Found! Return position of first match */
      uint32_t index;
      __asm__ (
          "tzcnt %[mask], %[idx]\n\t"
          : [idx] "=r"(index)
          : [mask] "r"(mask)
          : "cc"
      );
      return buf + index;
    }

    buf += 32;
  }

  /* Handle tail with scalar */
  if (buf < end) {
    return memchr(buf, c, end - buf);
  }

  return NULL;
}

#elif defined(__aarch64__)

#include <stdint.h>

const char *avx2_memchr(const char *buf, size_t len, char c) {
  const char *end = buf + len;

  if (len < 16) {
    return memchr(buf, c, len);
  }

  const char *ptr = buf;
  uint64_t dup_c = (uint8_t)c;

  while (ptr + 16 <= end) {
    uint64_t low = 0, high = 0;

    __asm__ (
        "ldr q1, [%[ptr]]\n\t"
        "dup v0.16b, %w[c]\n\t"
        "cmeq v2.16b, v1.16b, v0.16b\n\t"
        "umov %[low], v2.d[0]\n\t"
        "umov %[high], v2.d[1]\n\t"
        : [low] "=r"(low), [high] "=r"(high)
        : [ptr] "r"(ptr), [c] "r"(dup_c)
        : "v0", "v1", "v2", "cc"
    );

    if (low != 0) {
      uint64_t rbit_val, clz_val;
      __asm__ (
          "rbit %[r], %[val]\n\t"
          "clz %[c], %[r]\n\t"
          : [r] "=&r"(rbit_val), [c] "=&r"(clz_val)
          : [val] "r"(low)
      );
      return ptr + (clz_val >> 3);
    }

    if (high != 0) {
      uint64_t rbit_val, clz_val;
      __asm__ (
          "rbit %[r], %[val]\n\t"
          "clz %[c], %[r]\n\t"
          : [r] "=&r"(rbit_val), [c] "=&r"(clz_val)
          : [val] "r"(high)
      );
      return ptr + 8 + (clz_val >> 3);
    }

    ptr += 16;
  }

  if (ptr < end) {
    return memchr(ptr, c, end - ptr);
  }

  return NULL;
}

#else /* Fallback */

/* Fallback implementation */
const char *avx2_memchr(const char *buf, size_t len, char c) {
  return memchr(buf, c, len);
}

#endif /* __AVX2__ */
