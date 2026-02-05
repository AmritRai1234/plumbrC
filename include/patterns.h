/*
 * PlumbrC - Pattern Management
 * Loading, compilation, and pattern set management
 */

#ifndef PLUMBR_PATTERNS_H
#define PLUMBR_PATTERNS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "aho_corasick.h"
#include "arena.h"
#include "config.h"

/* Single pattern definition */
typedef struct {
  char name[PLUMBR_MAX_PATTERN_NAME];
  char literal[PLUMBR_MAX_LITERAL_LEN]; /* For AC automaton */
  size_t literal_len;
  pcre2_code *regex;            /* Compiled PCRE2 pattern */
  pcre2_match_data *match_data; /* Reusable match data */
  char replacement[PLUMBR_MAX_REPLACEMENT_LEN];
  size_t replacement_len;
  uint32_t id;
  bool has_literal; /* True if has usable literal */
} Pattern;

/* Pattern set with AC automaton */
typedef struct PatternSet {
  Pattern *patterns;
  size_t count;
  size_t capacity;
  ACAutomaton *automaton;
  Arena *arena;
  bool built;
} PatternSet;

/* Create empty pattern set */
PatternSet *patterns_create(Arena *arena, size_t initial_capacity);

/* Add pattern from components */
bool patterns_add(PatternSet *ps, const char *name,
                  const char *literal, /* Can be NULL if no literal */
                  const char *regex, const char *replacement);

/* Load patterns from file (one per line, format:
 * name|literal|regex|replacement) */
bool patterns_load_file(PatternSet *ps, const char *filename);

/* Build AC automaton (call after all patterns added) */
bool patterns_build(PatternSet *ps);

/* Get pattern by ID */
const Pattern *patterns_get(const PatternSet *ps, uint32_t id);

/* Get count */
size_t patterns_count(const PatternSet *ps);

/* Free PCRE2 resources (arena handles the rest) */
void patterns_destroy(PatternSet *ps);

/*
 * Built-in pattern helpers
 */

/* Add common secret detection patterns */
bool patterns_add_defaults(PatternSet *ps);

/* Extract literal from regex (best effort) */
bool patterns_extract_literal(const char *regex, char *out_literal,
                              size_t max_len);

#endif /* PLUMBR_PATTERNS_H */
