/*
 * PlumbrC - Redactor
 * In-place redaction with two-phase matching
 */

#ifndef PLUMBR_REDACTOR_H
#define PLUMBR_REDACTOR_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "patterns.h"

/* Redaction context */
typedef struct {
  PatternSet *patterns;
  Arena *scratch; /* Scratch arena for temporary allocations */

  /* Output buffer for redacted line */
  char *output_buf;
  size_t output_capacity;

  /* Thread-local PCRE2 match data (one per pattern) */
  pcre2_match_data **match_data;
  size_t num_patterns;

  /* SECURITY: Match context with limits for ReDoS protection */
  pcre2_match_context *match_ctx;

  /* SSE 4.2 trigger pre-filter cache */
  char triggers[16];    /* First-byte triggers from AC root state */
  size_t trigger_count; /* Number of trigger characters */
  bool use_sse42;       /* SSE 4.2 available */

  /* Stats (atomic for thread safety) */
  _Atomic size_t lines_scanned;
  _Atomic size_t lines_modified;
  _Atomic size_t patterns_matched;
  _Atomic size_t lines_prefiltered; /* Lines skipped by SSE 4.2 filter */
#if PLUMBR_TWO_TIER_AC
  _Atomic size_t lines_sentinel_filtered; /* Lines skipped by tier-1 sentinel */
#endif
} Redactor;

/* Initialize redactor */
Redactor *redactor_create(Arena *arena, PatternSet *patterns,
                          size_t output_capacity);

/* Free PCRE2 resources (match data + match context) */
void redactor_destroy(Redactor *r);

/* Process line (returns pointer to output, which may be input if unchanged) */
const char *redactor_process(Redactor *r, const char *line, size_t len,
                             size_t *out_len);

/* Reset stats */
void redactor_reset_stats(Redactor *r);

/* Get stats */
size_t redactor_lines_scanned(const Redactor *r);
size_t redactor_lines_modified(const Redactor *r);
size_t redactor_patterns_matched(const Redactor *r);

#endif /* PLUMBR_REDACTOR_H */
