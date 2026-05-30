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
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ─── Thread-local error tracking ─── */
static _Thread_local libplumbr_error_t tl_last_error = PLUMBR_OK;

static inline void set_error(libplumbr_error_t err) { tl_last_error = err; }

libplumbr_error_t libplumbr_last_error(void) { return tl_last_error; }

const char *libplumbr_error_string(libplumbr_error_t err) {
  switch (err) {
  case PLUMBR_OK:                  return "success";
  case PLUMBR_ERR_ALLOC:           return "memory allocation failed";
  case PLUMBR_ERR_PATTERN:         return "invalid pattern or pattern file";
  case PLUMBR_ERR_INPUT_TOO_LARGE: return "input exceeds max line size";
  case PLUMBR_ERR_BUFFER_TOO_SMALL:return "output buffer too small";
  case PLUMBR_ERR_NULL_INPUT:      return "NULL pointer argument";
  default:                         return "unknown error";
  }
}

/* Resolve a data file path relative to the binary location.
 * Tries: 1) PLUMBR_DATA_DIR env var  2) /proc/self/exe directory  3) relative path (fallback) */
static bool resolve_data_path(const char *relative, char *out, size_t out_size) {
  /* 1. Check env var override */
  const char *data_dir = getenv("PLUMBR_DATA_DIR");
  if (data_dir) {
    int n = snprintf(out, out_size, "%s/%s", data_dir, relative);
    if (n > 0 && (size_t)n < out_size && access(out, R_OK) == 0)
      return true;
  }

  /* 2. Resolve relative to binary location via /proc/self/exe */
  char exe_path[4096];
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len > 0) {
    exe_path[len] = '\0';
    /* Find last slash to get directory */
    char *slash = strrchr(exe_path, '/');
    if (slash) {
      *slash = '\0';
      /* Try: <exe_dir>/../<relative>  (typical install layout) */
      int n = snprintf(out, out_size, "%s/../%s", exe_path, relative);
      if (n > 0 && (size_t)n < out_size && access(out, R_OK) == 0)
        return true;
      /* Try: <exe_dir>/<relative> */
      n = snprintf(out, out_size, "%s/%s", exe_path, relative);
      if (n > 0 && (size_t)n < out_size && access(out, R_OK) == 0)
        return true;
    }
  }

  /* 3. Try /usr/local/share/plumbr/<relative> */
  {
    int n = snprintf(out, out_size, "/usr/local/share/plumbr/%s", relative);
    if (n > 0 && (size_t)n < out_size && access(out, R_OK) == 0)
      return true;
  }

  /* 4. Fallback: use relative path as-is (works if CWD is project root) */
  snprintf(out, out_size, "%s", relative);
  return access(out, R_OK) == 0;
}

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

  /* Load compliance patterns if requested */
  if (config && config->compliance) {
    const char *comp = config->compliance;
    bool load_hipaa = false, load_pci = false;
    bool load_gdpr = false, load_soc2 = false;

    if (strcmp(comp, "all") == 0) {
      load_hipaa = load_pci = load_gdpr = load_soc2 = true;
    } else {
      char buf[256];
      snprintf(buf, sizeof(buf), "%s", comp);
      char *tok = strtok(buf, ",");
      while (tok) {
        if (strcmp(tok, "hipaa") == 0)
          load_hipaa = true;
        else if (strcmp(tok, "pci") == 0)
          load_pci = true;
        else if (strcmp(tok, "gdpr") == 0)
          load_gdpr = true;
        else if (strcmp(tok, "soc2") == 0)
          load_soc2 = true;
        tok = strtok(NULL, ",");
      }
    }

    char resolved[4096];
    if (load_hipaa && resolve_data_path("patterns/compliance/hipaa.txt", resolved, sizeof(resolved)))
      loaded += patterns_load_file(p->patterns, resolved);
    if (load_pci && resolve_data_path("patterns/compliance/pci_dss.txt", resolved, sizeof(resolved)))
      loaded += patterns_load_file(p->patterns, resolved);
    if (load_gdpr && resolve_data_path("patterns/compliance/gdpr.txt", resolved, sizeof(resolved)))
      loaded += patterns_load_file(p->patterns, resolved);
    if (load_soc2 && resolve_data_path("patterns/compliance/soc2.txt", resolved, sizeof(resolved)))
      loaded += patterns_load_file(p->patterns, resolved);
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
  if (!p || !input) {
    set_error(PLUMBR_ERR_NULL_INPUT);
    return NULL;
  }

  /* SECURITY: Validate input length */
  if (input_len > PLUMBR_MAX_LINE_SIZE) {
    set_error(PLUMBR_ERR_INPUT_TOO_LARGE);
    return NULL; /* Input too large */
  }

  set_error(PLUMBR_OK);

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

ssize_t libplumbr_redact_into(libplumbr_t *p, const char *input, size_t in_len,
                              char *output, size_t out_cap) {
  if (!p || !input || !output) {
    set_error(PLUMBR_ERR_NULL_INPUT);
    return PLUMBR_ERR_NULL_INPUT;
  }
  if (in_len > PLUMBR_MAX_LINE_SIZE) {
    set_error(PLUMBR_ERR_INPUT_TOO_LARGE);
    return PLUMBR_ERR_INPUT_TOO_LARGE;
  }

  size_t out_len;
  const char *result = redactor_process(p->redactor, input, in_len, &out_len);

  if (!result) {
    set_error(PLUMBR_ERR_ALLOC);
    return PLUMBR_ERR_ALLOC;
  }
  if (out_len >= out_cap) {
    set_error(PLUMBR_ERR_BUFFER_TOO_SMALL);
    return PLUMBR_ERR_BUFFER_TOO_SMALL;
  }

  memcpy(output, result, out_len);
  output[out_len] = '\0';

  /* Update stats */
  p->stats.lines_processed++;
  p->stats.bytes_processed += in_len;
  if (out_len != in_len || memcmp(input, output, in_len) != 0) {
    p->stats.lines_modified++;
  }

  set_error(PLUMBR_OK);
  return (ssize_t)out_len;
}

ssize_t libplumbr_redact_inplace(libplumbr_t *p, char *buffer, size_t len,
                                 size_t capacity) {
  if (!p || !buffer) {
    set_error(PLUMBR_ERR_NULL_INPUT);
    return PLUMBR_ERR_NULL_INPUT;
  }

  size_t out_len;
  const char *result = redactor_process(p->redactor, buffer, len, &out_len);

  if (!result) {
    set_error(PLUMBR_ERR_ALLOC);
    return PLUMBR_ERR_ALLOC;
  }
  if (out_len >= capacity) {
    set_error(PLUMBR_ERR_BUFFER_TOO_SMALL);
    return PLUMBR_ERR_BUFFER_TOO_SMALL;
  }

  set_error(PLUMBR_OK);

  /* SECURITY: Use memmove — buffer may overlap with result */
  memmove(buffer, result, out_len);
  buffer[out_len] = '\0';

  /* Update stats */
  p->stats.lines_processed++;
  p->stats.bytes_processed += len;
  if (out_len != len) {
    p->stats.lines_modified++;
  }

  return (ssize_t)out_len;
}

int libplumbr_redact_batch(libplumbr_t *p, const char **inputs,
                           const size_t *input_lens, char **outputs,
                           size_t *output_lens, size_t count) {
  if (!p || !inputs || !outputs)
    return -1;

  /* SECURITY FIX #13: Guard against int truncation on return.
   * count is size_t but return type is int. Values > INT_MAX truncate. */
  if (count > (size_t)INT_MAX)
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

/* SECURITY: Thread-safe version string using pthread_once */
static char g_lib_version[32];
static pthread_once_t g_lib_version_once = PTHREAD_ONCE_INIT;
static void init_lib_version(void) {
  snprintf(g_lib_version, sizeof(g_lib_version), "%d.%d.%d",
           PLUMBR_VERSION_MAJOR, PLUMBR_VERSION_MINOR, PLUMBR_VERSION_PATCH);
}
const char *libplumbr_version(void) {
  pthread_once(&g_lib_version_once, init_lib_version);
  return g_lib_version;
}

char *libplumbr_redact_buffer(libplumbr_t *p, const char *input,
                              size_t input_len, size_t *output_len) {
  if (!p || !input || input_len == 0)
    return NULL;

  /* Worst case: each line could expand (redaction tags are fixed-size).
   * Allocate 2x input as a safe upper bound. */
  /* SECURITY FIX #11: Guard against overflow. input_len * 2 + 1 overflows
   * when input_len > SIZE_MAX / 2. */
  if (input_len > SIZE_MAX / 2)
    return NULL;
  size_t cap = input_len * 2 + 1;
  char *output = malloc(cap);
  if (!output)
    return NULL;

  size_t out_pos = 0;
  const char *ptr = input;
  const char *end = input + input_len;

  while (ptr < end) {
    /* Find end of current line */
    const char *nl = memchr(ptr, '\n', (size_t)(end - ptr));
    size_t line_len = nl ? (size_t)(nl - ptr) : (size_t)(end - ptr);

    /* Redact this line */
    size_t redacted_len;
    const char *redacted =
        redactor_process(p->redactor, ptr, line_len, &redacted_len);

    /* Ensure output buffer has space */
    if (out_pos + redacted_len + 1 >= cap) {
      /* SECURITY FIX #12: Guard against overflow in growth calculation.
       * (out_pos + redacted_len + 1) * 2 can overflow on large buffers. */
      size_t needed = out_pos + redacted_len + 1;
      if (needed > SIZE_MAX / 2) {
        free(output);
        return NULL;
      }
      cap = needed * 2;
      char *tmp = realloc(output, cap);
      if (!tmp) {
        free(output);
        return NULL;
      }
      output = tmp;
    }

    /* Copy redacted line to output */
    memcpy(output + out_pos, redacted, redacted_len);
    out_pos += redacted_len;

    /* Add newline if original had one */
    if (nl) {
      output[out_pos++] = '\n';
      ptr = nl + 1;
    } else {
      ptr = end;
    }

    /* Update stats */
    p->stats.lines_processed++;
    p->stats.bytes_processed += line_len;
  }

  output[out_pos] = '\0';
  if (output_len)
    *output_len = out_pos;

  return output;
}

void libplumbr_free_string(char *str) { free(str); }

void libplumbr_free(libplumbr_t *p) {
  if (!p)
    return;

  redactor_destroy(p->redactor);
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
