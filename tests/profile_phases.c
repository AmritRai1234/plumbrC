/*
 * PlumbrC — Per-Phase Profiling Harness
 *
 * Measures time spent in each redaction pipeline phase:
 *   Phase 0:   SSE4.2 pre-filter
 *   Phase 0.5: Tier-1 sentinel AC (if enabled)
 *   Phase 1:   Full AC scan
 *   Phase 2:   PCRE2 verify
 *
 * Build: gcc -O3 -march=native -Iinclude -Isrc/amd tests/profile_phases.c \
 *        src/*.c src/amd/*.c -o build/bin/profile -lpcre2-8 -lpthread
 *
 * Usage: ./build/bin/profile < test_data/bench_1M.txt
 */

#include "aho_corasick.h"
#include "arena.h"
#include "config.h"
#include "patterns.h"
#include "redactor.h"
#include "sse42_filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline double elapsed_ns(struct timespec *start, struct timespec *end) {
  return (double)(end->tv_sec - start->tv_sec) * 1e9 +
         (double)(end->tv_nsec - start->tv_nsec);
}

int main(void) {
  /* Init arena and patterns */
  Arena arena_storage;
  if (!arena_init(&arena_storage, PLUMBR_ARENA_SIZE)) {
    fprintf(stderr, "Arena init failed\n");
    return 1;
  }
  Arena *arena = &arena_storage;

  PatternSet *ps = patterns_create(arena, PLUMBR_MAX_PATTERNS);
  if (!ps) {
    fprintf(stderr, "Pattern set create failed\n");
    return 1;
  }

  patterns_load_file(ps, "patterns/all.txt");
  patterns_build(ps);

  fprintf(stderr, "Loaded %zu patterns\n", patterns_count(ps));
  fprintf(stderr, "DFA memory: %zu bytes (%.1f KB)\n",
          ac_dfa_memory(ps->automaton),
          (double)ac_dfa_memory(ps->automaton) / 1024.0);

#if PLUMBR_TWO_TIER_AC
  if (ps->sentinel) {
    fprintf(stderr, "Sentinel DFA memory: %zu bytes (%.1f KB)\n",
            ac_dfa_memory(ps->sentinel),
            (double)ac_dfa_memory(ps->sentinel) / 1024.0);
  }
#endif

  /* Build SSE4.2 triggers */
  char triggers[16];
  size_t trigger_count = 0;
  bool use_sse42 = sse42_available();
  if (use_sse42) {
    trigger_count =
        sse42_build_triggers(ps->automaton, triggers, sizeof(triggers));
    fprintf(stderr, "SSE4.2 triggers: %zu chars\n", trigger_count);
  }

  /* Read all lines into memory */
  char **lines = NULL;
  size_t *lengths = NULL;
  size_t num_lines = 0;
  size_t line_cap = 0;

  char buf[65536];
  while (fgets(buf, sizeof(buf), stdin)) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
      buf[--len] = '\0';
    if (len == 0)
      continue;

    if (num_lines >= line_cap) {
      line_cap = line_cap ? line_cap * 2 : 1024;
      lines = realloc(lines, line_cap * sizeof(char *));
      lengths = realloc(lengths, line_cap * sizeof(size_t));
    }
    lines[num_lines] = strdup(buf);
    lengths[num_lines] = len;
    num_lines++;
  }

  fprintf(stderr, "Read %zu lines\n\n", num_lines);

  /* Phase timing accumulators */
  double ns_sse42 = 0, ns_sentinel = 0, ns_fullac = 0, ns_pcre2 = 0;
  size_t count_sse42_skip = 0, count_sentinel_skip = 0;
  size_t count_fullac = 0, count_pcre2 = 0;
  size_t total_matches = 0;

  struct timespec t0, t1;

  /* Redactor for PCRE2 phase */
  Redactor *r = redactor_create(arena, ps, PLUMBR_MAX_LINE_SIZE);
  if (!r) {
    fprintf(stderr, "Redactor create failed\n");
    return 1;
  }

  /* Profile each line through the phases */
  for (size_t i = 0; i < num_lines; i++) {
    const char *line = lines[i];
    size_t len = lengths[i];

    /* Phase 0: SSE4.2 */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    bool has_trigger = true;
    if (use_sse42 && trigger_count > 0) {
      has_trigger = sse42_has_triggers(triggers, trigger_count, line, len);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ns_sse42 += elapsed_ns(&t0, &t1);

    if (!has_trigger) {
      count_sse42_skip++;
      continue;
    }

    /* Phase 0.5: Sentinel AC */
#if PLUMBR_TWO_TIER_AC
    clock_gettime(CLOCK_MONOTONIC, &t0);
    bool sentinel_match = true;
    if (ps->sentinel) {
      sentinel_match = ac_search_has_match(ps->sentinel, line, len);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ns_sentinel += elapsed_ns(&t0, &t1);

    if (!sentinel_match) {
      count_sentinel_skip++;
      continue;
    }
#endif

    /* Phase 1: Full AC */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ACMatch ac_matches[64];
    size_t num_ac = ac_search_all(ps->automaton, line, len, ac_matches, 64);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ns_fullac += elapsed_ns(&t0, &t1);
    count_fullac++;

    if (num_ac == 0)
      continue;

    /* Phase 2: PCRE2 verify (just count, use full redactor for real verify) */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    size_t out_len;
    redactor_process(r, line, len, &out_len);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ns_pcre2 += elapsed_ns(&t0, &t1);
    count_pcre2++;
    total_matches += num_ac;
  }

  /* Report */
  double total = ns_sse42 + ns_sentinel + ns_fullac + ns_pcre2;

  fprintf(stderr, "═══════════════════════════════════════════════════\n");
  fprintf(stderr, "  PlumbrC Per-Phase Profile — %zu lines\n", num_lines);
  fprintf(stderr, "═══════════════════════════════════════════════════\n\n");

  fprintf(stderr, "  Phase 0   SSE4.2 pre-filter:  %8.1f ms  (%4.1f%%)\n",
          ns_sse42 / 1e6, ns_sse42 / total * 100);
  fprintf(stderr, "    → skipped %zu / %zu lines (%.1f%%)\n", count_sse42_skip,
          num_lines, (double)count_sse42_skip / (double)num_lines * 100);

#if PLUMBR_TWO_TIER_AC
  fprintf(stderr, "\n  Phase 0.5 Sentinel AC (L1):    %8.1f ms  (%4.1f%%)\n",
          ns_sentinel / 1e6, ns_sentinel / total * 100);
  fprintf(stderr, "    → skipped %zu / %zu remaining (%.1f%%)\n",
          count_sentinel_skip, num_lines - count_sse42_skip,
          (double)count_sentinel_skip / (double)(num_lines - count_sse42_skip) *
              100);
#endif

  fprintf(stderr, "\n  Phase 1   Full AC (702p):      %8.1f ms  (%4.1f%%)\n",
          ns_fullac / 1e6, ns_fullac / total * 100);
  fprintf(stderr, "    → scanned %zu lines, %zu AC matches\n", count_fullac,
          total_matches);

  fprintf(stderr, "\n  Phase 2   PCRE2 verify:        %8.1f ms  (%4.1f%%)\n",
          ns_pcre2 / 1e6, ns_pcre2 / total * 100);
  fprintf(stderr, "    → verified %zu lines\n", count_pcre2);

  fprintf(stderr, "\n  TOTAL:                         %8.1f ms\n", total / 1e6);
  fprintf(stderr, "  Throughput:                    %.1f MB/sec\n",
          (double)num_lines * 81.0 / (total / 1e9));
  fprintf(stderr, "═══════════════════════════════════════════════════\n");

  /* Cleanup */
  for (size_t i = 0; i < num_lines; i++)
    free(lines[i]);
  free(lines);
  free(lengths);
  patterns_destroy(ps);
  arena_destroy(arena);

  return 0;
}
