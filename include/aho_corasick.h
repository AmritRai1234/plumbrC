/*
 * PlumbrC - Aho-Corasick Automaton
 * Multi-pattern matching in O(n) time
 */

#ifndef PLUMBR_AHO_CORASICK_H
#define PLUMBR_AHO_CORASICK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"

/* Forward declarations */
typedef struct ACAutomaton ACAutomaton;

/* Match result */
typedef struct {
  size_t position;     /* Position in text where match ends */
  uint32_t pattern_id; /* Index of matched pattern */
  uint16_t length;     /* Length of matched pattern */
} ACMatch;

/* Match callback: return false to stop matching */
typedef bool (*ACMatchCallback)(const ACMatch *match, void *user_data);

/* Create automaton (allocates from arena) */
ACAutomaton *ac_create(Arena *arena);

/* Add pattern to automaton (must be called before ac_build) */
bool ac_add_pattern(ACAutomaton *ac, const char *pattern, size_t len,
                    uint32_t pattern_id);

/* Build failure links (call after all patterns added) */
bool ac_build(ACAutomaton *ac);

/* Set runtime prefetch tuning from hwdetect */
void ac_set_prefetch(ACAutomaton *ac, int distance, int hint);

/* Force flat (uncompressed) DFA for this automaton — faster for small DFA */
void ac_set_force_flat(ACAutomaton *ac);

/* Search text for all patterns, calls callback for each match */
void ac_search(const ACAutomaton *ac, const char *text, size_t len,
               ACMatchCallback callback, void *user_data);

/* Search and return first match only (faster for single-match case) */
bool ac_search_first(const ACAutomaton *ac, const char *text, size_t len,
                     ACMatch *out_match);

/* Search and collect all matches into array (must have space for max_matches)
 */
size_t ac_search_all(const ACAutomaton *ac, const char *text, size_t len,
                     ACMatch *matches, size_t max_matches);

/* Get DFA memory usage in bytes (for stats/compression validation) */
size_t ac_dfa_memory(const ACAutomaton *ac);

/* Fast boolean check: does text contain ANY match? (no callback overhead) */
bool ac_search_has_match(const ACAutomaton *ac, const char *text, size_t len);

/* Get root state DFA transitions (for building trigger sets)
 * Returns pointer to AC_ALPHABET_SIZE int16_t values.
 * Non-zero values indicate valid first-byte transitions */
const int16_t *ac_get_root_transitions(const ACAutomaton *ac);

#endif /* PLUMBR_AHO_CORASICK_H */
