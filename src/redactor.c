/*
 * PlumbrC - Redactor Implementation
 *
 * Two-phase matching:
 * 1. Aho-Corasick scan finds candidate positions
 * 2. PCRE2 verifies and extracts match bounds
 * 3. In-place redaction with replacement strings
 */

#include "redactor.h"
#include "config.h"

#include <string.h>

/* Max matches per line */
#define MAX_MATCHES_PER_LINE 256

/* Match location for sorting/merging */
typedef struct {
  size_t start;
  size_t end;
  uint32_t pattern_id;
} MatchLocation;

Redactor *redactor_create(Arena *arena, PatternSet *patterns,
                          size_t output_capacity) {
  Redactor *r = arena_alloc(arena, sizeof(Redactor));
  if (!r)
    return NULL;

  r->patterns = patterns;
  r->scratch = arena; /* Use same arena for scratch */

  r->output_buf = arena_alloc(arena, output_capacity);
  if (!r->output_buf)
    return NULL;
  r->output_capacity = output_capacity;

  r->lines_scanned = 0;
  r->lines_modified = 0;
  r->patterns_matched = 0;

  return r;
}

/* Compare function for sorting matches by start position */
static int match_compare(const void *a, const void *b) {
  const MatchLocation *ma = a;
  const MatchLocation *mb = b;
  if (ma->start < mb->start)
    return -1;
  if (ma->start > mb->start)
    return 1;
  return 0;
}

const char *redactor_process(Redactor *r, const char *line, size_t len,
                             size_t *out_len) {
  r->lines_scanned++;

  if (PLUMBR_UNLIKELY(len == 0)) {
    *out_len = 0;
    return line;
  }

  /* Phase 1: AC scan for candidate positions */
  ACMatch ac_matches[MAX_MATCHES_PER_LINE];
  size_t num_ac_matches = ac_search_all(r->patterns->automaton, line, len,
                                        ac_matches, MAX_MATCHES_PER_LINE);

  /* Fast path: no candidates found */
  if (num_ac_matches == 0) {
    *out_len = len;
    return line;
  }

  /* Phase 2: Verify each candidate with PCRE2 */
  MatchLocation verified[MAX_MATCHES_PER_LINE];
  size_t num_verified = 0;

  for (size_t i = 0; i < num_ac_matches && num_verified < MAX_MATCHES_PER_LINE;
       i++) {
    const ACMatch *ac = &ac_matches[i];
    const Pattern *pat = patterns_get(r->patterns, ac->pattern_id);

    if (!pat || !pat->regex) {
      continue;
    }

    /* Try to match near the AC hit position */
    size_t search_start =
        (ac->position >= ac->length) ? (ac->position - ac->length) : 0;

    /* Allow some slack for regex context */
    if (search_start > 10)
      search_start -= 10;

    int rc = pcre2_match(pat->regex, (PCRE2_SPTR)line, len, search_start, 0,
                         pat->match_data, NULL);

    if (rc > 0) {
      PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(pat->match_data);

      /* Check if match is valid (not past line end) */
      if (ovector[1] <= len) {
        verified[num_verified].start = ovector[0];
        verified[num_verified].end = ovector[1];
        verified[num_verified].pattern_id = ac->pattern_id;
        num_verified++;
        r->patterns_matched++;
      }
    }
  }

  /* Fast path: no verified matches */
  if (num_verified == 0) {
    *out_len = len;
    return line;
  }

  /* Sort matches by position */
  qsort(verified, num_verified, sizeof(MatchLocation), match_compare);

  /* Merge overlapping matches (keep first/longest) */
  size_t merged_count = 1;
  for (size_t i = 1; i < num_verified; i++) {
    MatchLocation *prev = &verified[merged_count - 1];
    MatchLocation *curr = &verified[i];

    if (curr->start < prev->end) {
      /* Overlapping - extend previous if current ends later */
      if (curr->end > prev->end) {
        prev->end = curr->end;
      }
    } else {
      /* Non-overlapping - keep it */
      verified[merged_count++] = *curr;
    }
  }
  num_verified = merged_count;

  /* Build output with redactions */
  size_t out_pos = 0;
  size_t in_pos = 0;

  for (size_t i = 0; i < num_verified; i++) {
    MatchLocation *m = &verified[i];
    const Pattern *pat = patterns_get(r->patterns, m->pattern_id);

    /* Copy text before match */
    size_t before_len = m->start - in_pos;
    if (before_len > 0) {
      if (out_pos + before_len >= r->output_capacity) {
        /* Output buffer overflow - truncate */
        break;
      }
      memcpy(r->output_buf + out_pos, line + in_pos, before_len);
      out_pos += before_len;
    }

    /* Insert replacement */
    if (pat && pat->replacement_len > 0) {
      if (out_pos + pat->replacement_len >= r->output_capacity) {
        break;
      }
      memcpy(r->output_buf + out_pos, pat->replacement, pat->replacement_len);
      out_pos += pat->replacement_len;
    }

    in_pos = m->end;
  }

  /* Copy remaining text */
  size_t remaining = len - in_pos;
  if (remaining > 0 && out_pos + remaining < r->output_capacity) {
    memcpy(r->output_buf + out_pos, line + in_pos, remaining);
    out_pos += remaining;
  }

  r->output_buf[out_pos] = '\0';
  *out_len = out_pos;
  r->lines_modified++;

  return r->output_buf;
}

void redactor_reset_stats(Redactor *r) {
  r->lines_scanned = 0;
  r->lines_modified = 0;
  r->patterns_matched = 0;
}

size_t redactor_lines_scanned(const Redactor *r) { return r->lines_scanned; }

size_t redactor_lines_modified(const Redactor *r) { return r->lines_modified; }

size_t redactor_patterns_matched(const Redactor *r) {
  return r->patterns_matched;
}
