/*
 * PlumbrC - AVX2 SIMD String Scanner
 * Fast parallel byte scanning using AMD Ryzen AVX2
 *
 * Processes 32 bytes at a time for ~4x speedup on byte search
 */

#ifndef PLUMBR_AVX2_SCAN_H
#define PLUMBR_AVX2_SCAN_H

#include <stddef.h>

/*
 * Scan for a single byte in a buffer using AVX2
 * Returns pointer to first occurrence, or NULL if not found
 */
const char *avx2_memchr(const char *buf, size_t len, char c);

#endif /* PLUMBR_AVX2_SCAN_H */
