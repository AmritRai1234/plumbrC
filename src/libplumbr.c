/*
 * PlumbrC Library Implementation
 * Shared library wrapper for embedding
 */

#include "libplumbr.h"
#include "arena.h"
#include "config.h"
#include "hwdetect.h"
#include "patterns.h"
#include "redactor.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Internal structure */
struct libplumbr {
  Arena arena;
  PatternSet *patterns;
  Redactor *redactor;
  libplumbr_stats_t stats;
  size_t max_line_size;
};

libplumbr_t *libplumbr_new(const libplumbr_config_t *config) {
  libplumbr_t *p = calloc(1, sizeof(libplumbr_t));
  if (!p)
    return NULL;

  p->max_line_size = PLUMBR_MAX_LINE_SIZE;

  /* Initialize arena */
  if (!arena_init(&p->arena, PLUMBR_ARENA_SIZE)) {
    free(p);
    return NULL;
  }

  /* Create pattern set */
  p->patterns = patterns_create(&p->arena, PLUMBR_MAX_PATTERNS);
  if (!p->patterns) {
    arena_destroy(&p->arena);
    free(p);
    return NULL;
  }

  /* Load patterns */
  int loaded = 0;
  if (config && config->pattern_file) {
    loaded = patterns_load_file(p->patterns, config->pattern_file);
  }
  if (config && config->pattern_dir) {
    loaded += patterns_load_directory(p->patterns, config->pattern_dir);
  }
  if (!loaded) {
    /* Use defaults */
    patterns_add_defaults(p->patterns);
  }

  /* Build automaton */
  if (!patterns_build(p->patterns)) {
    patterns_destroy(p->patterns);
    arena_destroy(&p->arena);
    free(p);
    return NULL;
  }

  /* Auto-detect hardware and apply CPU-specific tuning */
  HardwareInfo hw;
  hwdetect_init(&hw);
  ac_set_prefetch(p->patterns->automaton, hw.prefetch_distance,
                  hw.prefetch_hint);

  /* Create redactor */
  p->redactor = redactor_create(&p->arena, p->patterns, p->max_line_size);
  if (!p->redactor) {
    patterns_destroy(p->patterns);
    arena_destroy(&p->arena);
    free(p);
    return NULL;
  }

  return p;
}

char *libplumbr_redact(libplumbr_t *p, const char *input, size_t input_len,
                       size_t *output_len) {
  if (!p || !input)
    return NULL;

  /* SECURITY: Validate input length */
  if (input_len > PLUMBR_MAX_LINE_SIZE) {
    return NULL; /* Input too large */
  }

  size_t out_len;
  const char *result =
      redactor_process(p->redactor, input, input_len, &out_len);

  if (!result)
    return NULL;

  /* Allocate and copy result */
  char *output = malloc(out_len + 1);
  if (!output)
    return NULL;

  memcpy(output, result, out_len);
  output[out_len] = '\0';

  if (output_len)
    *output_len = out_len;

  /* Update stats */
  p->stats.lines_processed++;
  p->stats.bytes_processed += input_len;
  if (out_len != input_len || memcmp(input, output, input_len) != 0) {
    p->stats.lines_modified++;
  }

  return output;
}

int libplumbr_redact_inplace(libplumbr_t *p, char *buffer, size_t len,
                             size_t capacity) {
  if (!p || !buffer)
    return -1;

  size_t out_len;
  const char *result = redactor_process(p->redactor, buffer, len, &out_len);

  if (!result)
    return -1;
  if (out_len >= capacity)
    return -1;

  memcpy(buffer, result, out_len);
  buffer[out_len] = '\0';

  /* Update stats */
  p->stats.lines_processed++;
  p->stats.bytes_processed += len;
  if (out_len != len) {
    p->stats.lines_modified++;
  }

  return (int)out_len;
}

int libplumbr_redact_batch(libplumbr_t *p, const char **inputs,
                           const size_t *input_lens, char **outputs,
                           size_t *output_lens, size_t count) {
  if (!p || !inputs || !outputs)
    return -1;

  for (size_t i = 0; i < count; i++) {
    outputs[i] = libplumbr_redact(p, inputs[i], input_lens[i],
                                  output_lens ? &output_lens[i] : NULL);
    if (!outputs[i]) {
      /* Cleanup on error */
      for (size_t j = 0; j < i; j++) {
        free(outputs[j]);
        outputs[j] = NULL;
      }
      return -1;
    }
  }

  return (int)count;
}

libplumbr_stats_t libplumbr_get_stats(const libplumbr_t *p) {
  if (!p) {
    libplumbr_stats_t empty = {0};
    return empty;
  }

  libplumbr_stats_t stats = p->stats;
  stats.patterns_matched = redactor_patterns_matched(p->redactor);
  return stats;
}

void libplumbr_reset_stats(libplumbr_t *p) {
  if (p) {
    memset(&p->stats, 0, sizeof(p->stats));
  }
}

size_t libplumbr_pattern_count(const libplumbr_t *p) {
  if (!p || !p->patterns)
    return 0;
  return patterns_count(p->patterns);
}

const char *libplumbr_version(void) {
  static char version[32];
  snprintf(version, sizeof(version), "%d.%d.%d", PLUMBR_VERSION_MAJOR,
           PLUMBR_VERSION_MINOR, PLUMBR_VERSION_PATCH);
  return version;
}

void libplumbr_free(libplumbr_t *p) {
  if (!p)
    return;

  if (p->patterns) {
    patterns_destroy(p->patterns);
  }
  arena_destroy(&p->arena);
  free(p);
}

int libplumbr_is_threadsafe(void) {
  /* Each libplumbr_t instance is thread-safe to use from one thread.
   * Multiple instances can be used from multiple threads. */
  return 1;
}
