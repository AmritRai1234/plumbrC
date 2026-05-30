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

#if defined(__SSE4_2__)
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

  /* Process 16 bytes at a time using raw pcmpestri assembly */
  for (; i + 16 <= len; i += 16) {
    uint8_t match_found;
    __asm__ (
        "movdqu %[line_ptr], %%xmm1\n\t"
        "pcmpestri $0, %%xmm1, %[trig_vec]\n\t"
        "setc %[match]\n\t"
        : [match] "=r"(match_found)
        : [line_ptr] "m"(*(line + i)), [trig_vec] "x"(trig_vec), "a"(trigger_count), "d"(16)
        : "rcx", "xmm1", "cc"
    );
    if (match_found) {
      return true;
    }
  }

  /* Handle remaining bytes (< 16) */
  if (i < len) {
    char tail[16] = {0};
    size_t remaining = len - i;
    memcpy(tail, line + i, remaining);

    uint8_t match_found;
    __asm__ (
        "movdqu %[tail_ptr], %%xmm1\n\t"
        "pcmpestri $0, %%xmm1, %[trig_vec]\n\t"
        "setc %[match]\n\t"
        : [match] "=r"(match_found)
        : [tail_ptr] "m"(*tail), [trig_vec] "x"(trig_vec), "a"(trigger_count), "d"(remaining)
        : "rcx", "xmm1", "cc"
    );
    if (match_found) {
      return true;
    }
  }

  return false;
}

#elif defined(__aarch64__)

bool sse42_available(void) { return true; }

bool sse42_has_triggers(const char *triggers, size_t trigger_count,
                        const char *line, size_t len) {
  if (trigger_count == 0 || len == 0) {
    return false;
  }
  if (trigger_count > 16) {
    trigger_count = 16;
  }

  uint32_t found = 0;

  __asm__ (
      // Load triggers into v0.16b
      "ldr q0, [%[trig]]\n\t"
      
      // Loop over 16-byte chunks of line
      "mov x0, %[line]\n\t"        // x0 = line pointer
      "mov x1, %[len]\n\t"         // x1 = total len
      "xor x2, x2\n\t"             // x2 = byte index i
      
      "1:\n\t"
      // If i + 16 > len, jump to tail
      "sub x3, x1, x2\n\t"         // x3 = len - i
      "cmp x3, #16\n\t"
      "b.lt 3f\n\t"
      
      // Load 16 bytes of line into q1
      "ldr q1, [x0, x2]\n\t"
      
      // Clear accumulator register v2
      "movi v2.16b, #0\n\t"
      
      // Loop over trigger characters
      "mov x4, #0\n\t"             // x4 = trigger index t
      "2:\n\t"
      "cmp x4, %[trig_cnt]\n\t"
      "b.ge 4f\n\t"
      
      // Load triggers[t] in general-purpose register and dup
      "ldrb w5, [%[trig], x4]\n\t"
      "dup v3.16b, w5\n\t"
      
      // Compare and accumulate
      "cmeq v4.16b, v1.16b, v3.16b\n\t"
      "orr v2.16b, v2.16b, v4.16b\n\t"
      
      "add x4, x4, #1\n\t"
      "b 2b\n\t"
      
      "4:\n\t"
      // Check if any match in v2
      "umaxv b0, v2.16b\n\t"
      "fmov w5, s0\n\t"
      "cmp w5, #0\n\t"
      "b.ne 5f\n\t"                // Found a match!
      
      "add x2, x2, #16\n\t"
      "b 1b\n\t"
      
      "3:\n\t"
      // Tail handling: if remaining == 0, exit (not found)
      "cmp x3, #0\n\t"
      "b.eq 6f\n\t"
      
      // Scalar comparison loop for the tail bytes
      "7:\n\t"
      "cmp x2, x1\n\t"
      "b.ge 6f\n\t"
      "ldrb w5, [x0, x2]\n\t"
      
      // Check if w5 matches any trigger
      "mov x4, #0\n\t"
      "8:\n\t"
      "cmp x4, %[trig_cnt]\n\t"
      "b.ge 9f\n\t"
      "ldrb w6, [%[trig], x4]\n\t"
      "cmp w5, w6\n\t"
      "b.eq 5f\n\t"                // Match found!
      "add x4, x4, #1\n\t"
      "b 8b\n\t"
      
      "9:\n\t"
      "add x2, x2, #1\n\t"
      "b 7b\n\t"
      
      "5:\n\t"
      "mov %[found], #1\n\t"
      
      "6:\n\t"
      : [found] "=r"(found)
      : [trig] "r"(triggers), [trig_cnt] "r"(trigger_count), [line] "r"(line), [len] "r"(len)
      : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "v0", "v1", "v2", "v3", "v4", "cc", "memory"
  );

  return found != 0;
}

#else /* Fallback */

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
