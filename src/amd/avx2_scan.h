/*
 * PlumbrC - AVX2 SIMD String Scanner
 * Fast parallel literal scanning using AMD Ryzen AVX2
 *
 * Processes 32 bytes at a time for ~4-8x speedup on literal matching
 */

#ifndef PLUMBR_AVX2_SCAN_H
#define PLUMBR_AVX2_SCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Check if AVX2 is available at runtime */
bool avx2_available(void);

/*
 * Scan for a single byte in a buffer using AVX2
 * Returns pointer to first occurrence, or NULL if not found
 */
const char *avx2_memchr(const char *buf, size_t len, char c);

/*
 * Scan for a 2-byte literal using AVX2
 * Returns pointer to first occurrence, or NULL if not found
 */
const char *avx2_find2(const char *buf, size_t len, const char *needle);

/*
 * Scan for a 4-byte literal using AVX2
 * Returns pointer to first occurrence, or NULL if not found
 */
const char *avx2_find4(const char *buf, size_t len, const char *needle);

/*
 * Fast check if buffer contains any of the trigger characters
 * Used for quick-reject of lines without potential matches
 * Returns true if any trigger char found
 */
bool avx2_contains_any(const char *buf, size_t len, const char *triggers,
                       size_t trigger_count);

/*
 * Count occurrences of a byte in buffer
 */
size_t avx2_count_byte(const char *buf, size_t len, char c);

#endif /* PLUMBR_AVX2_SCAN_H */
