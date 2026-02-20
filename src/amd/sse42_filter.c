/*
 * PlumbrC - SSE 4.2 Trigger Pre-filter
 *
 * Uses SSE 4.2 PCMPESTRI instruction for hardware-accelerated
 * trigger character detection. Scans 16 bytes per cycle to find
 * if any AC automaton trigger chars exist in a line.
 *
 * Lines without trigger chars can skip AC scanning entirely (~85% of lines).
 */

#include "sse42_filter.h"
#include "aho_corasick.h"
#include "config.h"

#include <string.h>

#ifdef __SSE4_2__
#include <nmmintrin.h>

bool sse42_available(void) { return true; }

bool sse42_has_triggers(const char *triggers, size_t trigger_count,
                        const char *line, size_t len) {
  if (trigger_count == 0 || len == 0) {
    return false;
  }

  /* Cap at 16 chars — PCMPESTRI limit */
  if (trigger_count > 16) {
    trigger_count = 16;
  }

  /* Load trigger characters into SSE register */
  __m128i trig_vec = _mm_loadu_si128((const __m128i *)triggers);

  size_t i = 0;

  /* Process 16 bytes at a time */
  for (; i + 16 <= len; i += 16) {
    __m128i line_vec = _mm_loadu_si128((const __m128i *)(line + i));

    /*
     * PCMPESTRI: compare each byte of line_vec against the set of
     * trigger chars in trig_vec.
     *
     * _SIDD_UBYTE_OPS:     unsigned byte comparison
     * _SIDD_CMP_EQUAL_ANY: check if any byte in line matches any trigger
     * _SIDD_LEAST_SIGNIFICANT: return index of first match
     *
     * Returns index of first match (0-15), or 16 if no match.
     * CF flag (carry) is set if any match found.
     */
    int idx = _mm_cmpestri(trig_vec, (int)trigger_count, line_vec, 16,
                           _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY);

    if (idx < 16) {
      return true; /* Found a trigger character */
    }
  }

  /* Handle remaining bytes (< 16) */
  if (i < len) {
    /* Safe: pad remaining bytes into a 16-byte buffer */
    char tail[16] = {0};
    size_t remaining = len - i;
    memcpy(tail, line + i, remaining);

    __m128i line_vec = _mm_loadu_si128((const __m128i *)tail);
    int idx =
        _mm_cmpestri(trig_vec, (int)trigger_count, line_vec, (int)remaining,
                     _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY);

    if (idx < (int)remaining) {
      return true;
    }
  }

  return false;
}

#else /* No SSE 4.2 */

bool sse42_available(void) { return false; }

bool sse42_has_triggers(const char *triggers, size_t trigger_count,
                        const char *line, size_t len) {
  /* Scalar fallback — check each character */
  for (size_t i = 0; i < len; i++) {
    for (size_t t = 0; t < trigger_count; t++) {
      if (line[i] == triggers[t]) {
        return true;
      }
    }
  }
  return false;
}

#endif /* __SSE4_2__ */

size_t sse42_build_triggers(const void *automaton, char *triggers,
                            size_t max_triggers) {
  const ACAutomaton *ac = (const ACAutomaton *)automaton;
  const int16_t *root_row = ac_get_root_transitions(ac);
  if (!root_row) {
    return 0;
  }

  size_t count = 0;

  /* Collect up to max_triggers (16) unique first characters from the AC
   * root state. When there are more unique first chars than can fit in the
   * SSE4.2 register, this is a PARTIAL filter — some valid first chars
   * will be missing and their lines will pass through unchecked.
   *
   * This is safe when PLUMBR_TWO_TIER_AC is enabled: the sentinel AC
   * provides a second accurate filter after SSE4.2. Lines that slip past
   * the partial SSE4.2 filter are caught by the sentinel. */
  for (int c = 0; c < AC_ALPHABET_SIZE && count < max_triggers; c++) {
    if (root_row[c] != 0) {
      triggers[count++] = (char)c;
    }
  }

  /* Pad to 16 bytes for SSE alignment */
  for (size_t i = count; i < 16 && i < max_triggers; i++) {
    triggers[i] = '\0';
  }

  return count;
}
