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
#include "sse42_filter.h"

#include <stdio.h>
#include <string.h>

/* Max matches per line - use config constant for stack safety */
#define MAX_MATCHES_PER_LINE PLUMBR_MAX_MATCHES_PER_LINE

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

  /* Allocate per-redactor match data for thread safety */
  r->num_patterns = patterns_count(patterns);
  r->match_data =
      arena_alloc(arena, r->num_patterns * sizeof(pcre2_match_data *));
  if (!r->match_data)
    return NULL;

  /* Create match data for each pattern */
  for (size_t i = 0; i < r->num_patterns; i++) {
    const Pattern *pat = patterns_get(patterns, (uint32_t)i);
    if (pat && pat->regex) {
      r->match_data[i] = pcre2_match_data_create_from_pattern(pat->regex, NULL);
    } else {
      r->match_data[i] = NULL;
    }
  }

  /* SECURITY: Create match context with limits to prevent ReDoS attacks */
  r->match_ctx = pcre2_match_context_create(NULL);
  if (r->match_ctx) {
    /* Limit match attempts to prevent catastrophic backtracking */
    pcre2_set_match_limit(r->match_ctx, 100000);
    pcre2_set_depth_limit(r->match_ctx, 1000);
  }

  r->lines_scanned = 0;
  r->lines_modified = 0;
  r->patterns_matched = 0;
  r->lines_prefiltered = 0;
#if PLUMBR_TWO_TIER_AC
  r->lines_sentinel_filtered = 0;
#endif

  /* Build SSE 4.2 trigger cache from AC root state */
  r->use_sse42 = sse42_available();
  if (r->use_sse42 && patterns->automaton) {
    r->trigger_count = sse42_build_triggers(patterns->automaton, r->triggers,
                                            sizeof(r->triggers));
  } else {
    r->trigger_count = 0;
  }

  return r;
}

void redactor_destroy(Redactor *r) {
  if (!r)
    return;
  /* Free PCRE2 match data (heap-allocated, not arena) */
  if (r->match_data) {
    for (size_t i = 0; i < r->num_patterns; i++) {
      if (r->match_data[i])
        pcre2_match_data_free(r->match_data[i]);
    }
  }
  /* Free PCRE2 match context (heap-allocated) */
  if (r->match_ctx)
    pcre2_match_context_free(r->match_ctx);
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

/* Forward declaration */
static const char *redactor_apply(Redactor *r, const char *line, size_t len,
                                  MatchLocation *verified, size_t num_verified,
                                  size_t *out_len);

/* Verify AC match candidates with PCRE2 and collect verified matches.
 * Returns the number of verified matches appended to 'verified'. */
static size_t verify_ac_matches(Redactor *r, const char *line, size_t len,
                                const ACMatch *ac_matches, size_t num_ac,
                                MatchLocation *verified, size_t max_verified) {
  size_t num_verified = 0;

  for (size_t i = 0; i < num_ac && num_verified < max_verified; i++) {
    const ACMatch *ac = &ac_matches[i];
    const Pattern *pat = patterns_get(r->patterns, ac->pattern_id);

    if (!pat || !pat->regex) {
      continue;
    }

    /* Get per-redactor match data for thread safety */
    if (ac->pattern_id >= r->num_patterns || !r->match_data[ac->pattern_id]) {
      continue;
    }
    pcre2_match_data *md = r->match_data[ac->pattern_id];

    /* Try to match near the AC hit position */
    size_t search_start = 0;
    if (ac->position >= ac->length) {
      search_start = ac->position - ac->length;
    }
    if (search_start >= 10) {
      search_start -= 10;
    } else {
      search_start = 0;
    }

    /* PCRE2 JIT with ReDoS limits */
    int rc = pcre2_jit_match(pat->regex, (PCRE2_SPTR)line, len, search_start, 0,
                             md, r->match_ctx);
    if (rc == PCRE2_ERROR_JIT_BADOPTION) {
      rc = pcre2_match(pat->regex, (PCRE2_SPTR)line, len, search_start, 0, md,
                       r->match_ctx);
    }

    if (rc > 0) {
      PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(md);
      if (ovector[1] <= len) {
        verified[num_verified].start = ovector[0];
        verified[num_verified].end = ovector[1];
        verified[num_verified].pattern_id = ac->pattern_id;
        num_verified++;
        r->patterns_matched++;
      }
    }
  }
  return num_verified;
}

const char *redactor_process(Redactor *r, const char *line, size_t len,
                             size_t *out_len) {
  r->lines_scanned++;

  if (PLUMBR_UNLIKELY(len == 0)) {
    *out_len = 0;
    return line;
  }

  /* Phase 0: SSE 4.2 pre-filter — partial (16 trigger chars max).
   * Sentinel provides safety net for patterns not covered. */
  if (r->use_sse42 && r->trigger_count > 0) {
    if (!sse42_has_triggers(r->triggers, r->trigger_count, line, len)) {
#if PLUMBR_TWO_TIER_AC
      if (r->patterns->sentinel &&
          ac_search_has_match(r->patterns->sentinel, line, len)) {
        goto cold_ac_scan;
      }
#endif
      r->lines_prefiltered++;
      *out_len = len;
      return line;
    }
  }

#if PLUMBR_TWO_TIER_AC
  /* Phase 1: Sentinel gate — cheap boolean reject for 70% of lines */
  if (r->patterns->sentinel) {
    if (!ac_search_has_match(r->patterns->sentinel, line, len)) {
      r->lines_sentinel_filtered++;
      *out_len = len;
      return line;
    }
  }
#endif

  /* Phase 2: Hot AC scan — L1-resident flat DFA (top 20 patterns).
   * Lines here passed sentinel, so they likely contain secrets.
   * Hot DFA handles ~90% of real matches without touching the cold DFA. */
  if (r->patterns->hot_ac) {
    ACMatch hot_matches[MAX_MATCHES_PER_LINE];
    size_t num_hot = ac_search_all(r->patterns->hot_ac, line, len, hot_matches,
                                   MAX_MATCHES_PER_LINE);
    if (num_hot > 0) {
      MatchLocation verified[MAX_MATCHES_PER_LINE];
      size_t num_verified = verify_ac_matches(
          r, line, len, hot_matches, num_hot, verified, MAX_MATCHES_PER_LINE);
      if (num_verified > 0) {
        return redactor_apply(r, line, len, verified, num_verified, out_len);
      }
    }
  }

cold_ac_scan:;
  /* Phase 3: Cold AC — full compressed DFA (all patterns), L3 path */
  {
    ACMatch ac_matches[MAX_MATCHES_PER_LINE];
    size_t num_ac = ac_search_all(r->patterns->automaton, line, len, ac_matches,
                                  MAX_MATCHES_PER_LINE);
    if (num_ac == 0) {
      *out_len = len;
      return line;
    }

    MatchLocation verified[MAX_MATCHES_PER_LINE];
    size_t num_verified = verify_ac_matches(r, line, len, ac_matches, num_ac,
                                            verified, MAX_MATCHES_PER_LINE);
    if (num_verified > 0) {
      return redactor_apply(r, line, len, verified, num_verified, out_len);
    }
  }

  *out_len = len;
  return line;
}

/* Apply redactions: sort, merge overlaps, build output */
static const char *redactor_apply(Redactor *r, const char *line, size_t len,
                                  MatchLocation *verified, size_t num_verified,
                                  size_t *out_len) {
  /* Sort matches by position */
  qsort(verified, num_verified, sizeof(MatchLocation), match_compare);

  /* Merge overlapping matches (keep first/longest) */
  size_t merged_count = 1;
  for (size_t i = 1; i < num_verified; i++) {
    MatchLocation *prev = &verified[merged_count - 1];
    MatchLocation *curr = &verified[i];

    if (curr->start < prev->end) {
      if (curr->end > prev->end) {
        prev->end = curr->end;
      }
    } else {
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

    size_t before_len = m->start - in_pos;
    if (before_len > 0) {
      if (out_pos + before_len >= r->output_capacity) {
        fprintf(stderr,
                "Warning: redaction output truncated (line too long, "
                "%zu bytes > %zu capacity). Sensitive data may survive.\n",
                out_pos + before_len, r->output_capacity);
        break;
      }
      memcpy(r->output_buf + out_pos, line + in_pos, before_len);
      out_pos += before_len;
    }

    if (pat && pat->replacement_len > 0) {
      if (out_pos + pat->replacement_len >= r->output_capacity) {
        break;
      }
      memcpy(r->output_buf + out_pos, pat->replacement, pat->replacement_len);
      out_pos += pat->replacement_len;
    }

    in_pos = m->end;
  }

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
#if PLUMBR_TWO_TIER_AC
  r->lines_sentinel_filtered = 0;
#endif
}

size_t redactor_lines_scanned(const Redactor *r) { return r->lines_scanned; }

size_t redactor_lines_modified(const Redactor *r) { return r->lines_modified; }

size_t redactor_patterns_matched(const Redactor *r) {
  return r->patterns_matched;
}
