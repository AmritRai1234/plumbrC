/*
 * PlumbrC - AVX2 SIMD String Scanner
 * AMD Ryzen optimized implementation
 *
 * Uses 256-bit AVX2 registers to process 32 bytes per cycle
 */

#include "avx2_scan.h"

#include <string.h>

#ifdef __AVX2__
#include <immintrin.h>

/* Check AVX2 availability */
bool avx2_available(void) {
  return true; /* Compiled with AVX2, so it's available */
}

/*
 * AVX2 memchr - find single byte
 * ~4x faster than libc memchr for buffers > 32 bytes
 */
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
    __m256i chunk = _mm256_loadu_si256((const __m256i *)buf);
    __m256i cmp = _mm256_cmpeq_epi8(chunk, needle);
    int mask = _mm256_movemask_epi8(cmp);

    if (mask != 0) {
      /* Found! Return position of first match */
      return buf + __builtin_ctz(mask);
    }

    buf += 32;
  }

  /* Handle tail with scalar */
  if (buf < end) {
    return memchr(buf, c, end - buf);
  }

  return NULL;
}

/*
 * AVX2 find 2-byte literal
 * Uses overlapping comparison technique
 */
const char *avx2_find2(const char *buf, size_t len, const char *needle) {
  if (len < 2)
    return NULL;

  const char *end = buf + len - 1;

  /* For short strings, use scalar */
  if (len < 34) {
    return memmem(buf, len, needle, 2);
  }

  /* Broadcast both bytes */
  __m256i n0 = _mm256_set1_epi8(needle[0]);
  __m256i n1 = _mm256_set1_epi8(needle[1]);

  while (buf + 33 <= end) {
    __m256i chunk0 = _mm256_loadu_si256((const __m256i *)buf);
    __m256i chunk1 = _mm256_loadu_si256((const __m256i *)(buf + 1));

    __m256i cmp0 = _mm256_cmpeq_epi8(chunk0, n0);
    __m256i cmp1 = _mm256_cmpeq_epi8(chunk1, n1);
    __m256i match = _mm256_and_si256(cmp0, cmp1);

    int mask = _mm256_movemask_epi8(match);

    if (mask != 0) {
      return buf + __builtin_ctz(mask);
    }

    buf += 32;
  }

  /* Handle tail */
  if (buf < end) {
    return memmem(buf, end - buf + 1, needle, 2);
  }

  return NULL;
}

/*
 * AVX2 find 4-byte literal
 * Uses SIMD for first byte, then scalar verify
 */
const char *avx2_find4(const char *buf, size_t len, const char *needle) {
  if (len < 4)
    return NULL;

  /* For short strings, use scalar */
  if (len < 36) {
    return memmem(buf, len, needle, 4);
  }

  const char *end = buf + len - 3;
  __m256i n0 = _mm256_set1_epi8(needle[0]);

  while (buf + 32 <= end) {
    __m256i chunk = _mm256_loadu_si256((const __m256i *)buf);
    __m256i cmp = _mm256_cmpeq_epi8(chunk, n0);
    int mask = _mm256_movemask_epi8(cmp);

    while (mask != 0) {
      int pos = __builtin_ctz(mask);
      const char *candidate = buf + pos;

      /* Verify full match */
      if (candidate + 4 <= end + 3 && memcmp(candidate, needle, 4) == 0) {
        return candidate;
      }

      /* Clear this bit and continue */
      mask &= mask - 1;
    }

    buf += 32;
  }

  /* Handle tail */
  if (buf < end) {
    return memmem(buf, end - buf + 3, needle, 4);
  }

  return NULL;
}

/*
 * Check if buffer contains any of the trigger characters
 * Fast rejection for lines without potential secrets
 */
bool avx2_contains_any(const char *buf, size_t len, const char *triggers,
                       size_t trigger_count) {
  if (len < 32 || trigger_count == 0) {
    /* Fall back to scalar for small buffers */
    for (size_t i = 0; i < len; i++) {
      for (size_t t = 0; t < trigger_count; t++) {
        if (buf[i] == triggers[t])
          return true;
      }
    }
    return false;
  }

  const char *end = buf + len;

  /* Build comparison vectors for each trigger */
  __m256i trigger_vecs[16]; /* Max 16 triggers */
  size_t num_triggers = trigger_count > 16 ? 16 : trigger_count;

  for (size_t t = 0; t < num_triggers; t++) {
    trigger_vecs[t] = _mm256_set1_epi8(triggers[t]);
  }

  while (buf + 32 <= end) {
    __m256i chunk = _mm256_loadu_si256((const __m256i *)buf);

    for (size_t t = 0; t < num_triggers; t++) {
      __m256i cmp = _mm256_cmpeq_epi8(chunk, trigger_vecs[t]);
      if (_mm256_movemask_epi8(cmp) != 0) {
        return true;
      }
    }

    buf += 32;
  }

  /* Check tail */
  while (buf < end) {
    for (size_t t = 0; t < trigger_count; t++) {
      if (*buf == triggers[t])
        return true;
    }
    buf++;
  }

  return false;
}

/*
 * Count occurrences of a byte using population count
 */
size_t avx2_count_byte(const char *buf, size_t len, char c) {
  size_t count = 0;
  const char *end = buf + len;

  if (len < 32) {
    for (size_t i = 0; i < len; i++) {
      if (buf[i] == c)
        count++;
    }
    return count;
  }

  __m256i needle = _mm256_set1_epi8(c);

  while (buf + 32 <= end) {
    __m256i chunk = _mm256_loadu_si256((const __m256i *)buf);
    __m256i cmp = _mm256_cmpeq_epi8(chunk, needle);
    int mask = _mm256_movemask_epi8(cmp);
    count += __builtin_popcount(mask);
    buf += 32;
  }

  /* Count tail */
  while (buf < end) {
    if (*buf == c)
      count++;
    buf++;
  }

  return count;
}

#else /* No AVX2 */

/* Fallback implementations */
bool avx2_available(void) { return false; }

const char *avx2_memchr(const char *buf, size_t len, char c) {
  return memchr(buf, c, len);
}

const char *avx2_find2(const char *buf, size_t len, const char *needle) {
  return memmem(buf, len, needle, 2);
}

const char *avx2_find4(const char *buf, size_t len, const char *needle) {
  return memmem(buf, len, needle, 4);
}

bool avx2_contains_any(const char *buf, size_t len, const char *triggers,
                       size_t trigger_count) {
  for (size_t i = 0; i < len; i++) {
    for (size_t t = 0; t < trigger_count; t++) {
      if (buf[i] == triggers[t])
        return true;
    }
  }
  return false;
}

size_t avx2_count_byte(const char *buf, size_t len, char c) {
  size_t count = 0;
  for (size_t i = 0; i < len; i++) {
    if (buf[i] == c)
      count++;
  }
  return count;
}

#endif /* __AVX2__ */
