/*
 * PlumbrC - Pipeline Implementation
 * Main processing loop
 */

#include "aho_corasick.h"
#include "arena.h"
#include "hwdetect.h"
#include "io.h"
#include "parallel.h"
#include "patterns.h"
#include "plumbr.h"
#include "redactor.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

struct PlumbrContext {
  Arena arena;
  PatternSet *patterns;
  Redactor *redactor;
  IOContext io;
  PlumbrConfig config;
  ParallelCtx *pctx; /* Cached parallel context */

  /* Timing */
  struct timespec start_time;
  struct timespec end_time;
};

void plumbr_config_init(PlumbrConfig *config) {
  memset(config, 0, sizeof(PlumbrConfig));
  config->use_defaults = true;
  config->quiet = false;
  config->stats_to_stderr = true;
}

PlumbrContext *plumbr_create(const PlumbrConfig *config) {
  PlumbrContext *ctx = calloc(1, sizeof(PlumbrContext));
  if (!ctx)
    return NULL;

  /* Copy config */
  ctx->config = *config;

  /* Initialize arena */
  if (!arena_init(&ctx->arena, PLUMBR_ARENA_SIZE)) {
    free(ctx);
    return NULL;
  }

  /* Create pattern set */
  ctx->patterns = patterns_create(&ctx->arena, PLUMBR_MAX_PATTERNS);
  if (!ctx->patterns) {
    arena_destroy(&ctx->arena);
    free(ctx);
    return NULL;
  }

  /* Load patterns */
  if (config->pattern_file) {
    if (!patterns_load_file(ctx->patterns, config->pattern_file)) {
      if (config->use_defaults) {
        patterns_add_defaults(ctx->patterns);
      } else {
        arena_destroy(&ctx->arena);
        free(ctx);
        return NULL;
      }
    }
  } else if (config->use_defaults) {
    patterns_add_defaults(ctx->patterns);
  }

  /* Load compliance patterns if requested */
  if (config->compliance) {
    const char *comp = config->compliance;
    bool load_hipaa = false, load_pci = false;
    bool load_gdpr = false, load_soc2 = false;

    if (strcmp(comp, "all") == 0) {
      load_hipaa = load_pci = load_gdpr = load_soc2 = true;
    } else {
      /* Parse comma-separated list: hipaa,pci,gdpr,soc2 */
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
        else
          fprintf(stderr, "Warning: unknown compliance profile '%s'\n", tok);
        tok = strtok(NULL, ",");
      }
    }

    char resolved[4096];
    if (load_hipaa && resolve_data_path("patterns/compliance/hipaa.txt", resolved, sizeof(resolved)))
      patterns_load_file(ctx->patterns, resolved);
    if (load_pci && resolve_data_path("patterns/compliance/pci_dss.txt", resolved, sizeof(resolved)))
      patterns_load_file(ctx->patterns, resolved);
    if (load_gdpr && resolve_data_path("patterns/compliance/gdpr.txt", resolved, sizeof(resolved)))
      patterns_load_file(ctx->patterns, resolved);
    if (load_soc2 && resolve_data_path("patterns/compliance/soc2.txt", resolved, sizeof(resolved)))
      patterns_load_file(ctx->patterns, resolved);
  }

  /* Build automaton */
  if (!patterns_build(ctx->patterns)) {
    patterns_destroy(ctx->patterns);
    arena_destroy(&ctx->arena);
    free(ctx);
    return NULL;
  }

  /* Create redactor */
  ctx->redactor =
      redactor_create(&ctx->arena, ctx->patterns, PLUMBR_MAX_LINE_SIZE);
  if (!ctx->redactor) {
    patterns_destroy(ctx->patterns);
    arena_destroy(&ctx->arena);
    free(ctx);
    return NULL;
  }

  return ctx;
}

/* Single-threaded processing (original) */
static int process_single_threaded(PlumbrContext *ctx) {
  size_t line_len;
  const char *line;

  while ((line = io_read_line(&ctx->io, &line_len)) != NULL) {
    size_t out_len;
    const char *output =
        redactor_process(ctx->redactor, line, line_len, &out_len);

    if (!io_write_line(&ctx->io, output, out_len)) {
      return 1;
    }
  }

  return 0;
}

/* Batch size for parallel processing - fits in L3 cache */
#define BATCH_SIZE PLUMBR_BATCH_SIZE

/*
 * New parallel processing using pthread barriers
 * More reliable than the old thread pool implementation
 */
static int process_parallel_new(PlumbrContext *ctx, int num_threads) {
  ParallelCtx *pctx = ctx->pctx;
  if (!pctx) {
    ctx->pctx = parallel_create(num_threads, ctx->patterns, PLUMBR_MAX_LINE_SIZE);
    pctx = ctx->pctx;
    if (!pctx) {
      fprintf(stderr, "Warning: Failed to create parallel context, "
                      "using single-threaded\n");
      return process_single_threaded(ctx);
    }
  }

  parallel_reset_stats(pctx);

  /* Retrieve persistent batch storage from the parallel context */
  const char **lines = NULL;
  size_t *lengths = NULL;
  char **outputs = NULL;
  size_t *out_lengths = NULL;
  char **line_copies = NULL;
  parallel_get_batch_buffers(pctx, &lines, &lengths, &outputs, &out_lengths, &line_copies);

  size_t batch_count = 0;
  size_t line_len;
  const char *line;
  int result = 0;

  while ((line = io_read_line(&ctx->io, &line_len)) != NULL) {
    /* Zero-copy fast path: pass I/O buffer pointer directly to workers.
     * io_read_line returns a pointer into the read buffer for complete lines
     * (fast path) or into the carry buffer for split lines. Either way the
     * pointer is stable until the next io_read_line call. Since we batch
     * lines and process the whole batch before reading more, all pointers
     * in the batch remain valid during parallel_process. */
    if (line_len < PLUMBR_MAX_LINE_SIZE) {
      /* Copy line data — I/O buffer will be reused on next read */
      memcpy(line_copies[batch_count], line, line_len);
      line_copies[batch_count][line_len] = '\0';
      lines[batch_count] = line_copies[batch_count];
    } else {
      memcpy(line_copies[batch_count], line, PLUMBR_MAX_LINE_SIZE - 1);
      line_copies[batch_count][PLUMBR_MAX_LINE_SIZE - 1] = '\0';
      line_len = PLUMBR_MAX_LINE_SIZE - 1;
      lines[batch_count] = line_copies[batch_count];
    }
    lengths[batch_count] = line_len;
    batch_count++;

    if (batch_count >= BATCH_SIZE) {
      /* Process batch */
      parallel_process(pctx, lines, lengths, outputs, out_lengths, batch_count);

      /* Write results in order */
      for (size_t i = 0; i < batch_count; i++) {
        if (!io_write_line(&ctx->io, outputs[i], out_lengths[i])) {
          result = 1;
          goto cleanup;
        }
      }
      batch_count = 0;
    }
  }


  /* Process remaining */
  if (batch_count > 0) {
    parallel_process(pctx, lines, lengths, outputs, out_lengths, batch_count);
    for (size_t i = 0; i < batch_count; i++) {
      if (!io_write_line(&ctx->io, outputs[i], out_lengths[i])) {
        result = 1;
        goto cleanup;
      }
    }
  }

cleanup:
  /* Aggregate parallel stats into the main redactor for reporting */
  ctx->redactor->lines_modified += parallel_lines_modified(pctx);
  ctx->redactor->patterns_matched += parallel_patterns_matched(pctx);

  return result;
}

int plumbr_process_fd(PlumbrContext *ctx, int in_fd, int out_fd) {
  /* Record start time */
  clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);

  /* Initialize I/O */
  io_init(&ctx->io, in_fd, out_fd);

  int result;
  int num_threads = ctx->config.num_threads;

  /* Auto-detect optimal thread count if 0 */
  if (num_threads == 0) {
    /* SECURITY: Thread-safe hardware detection with double-checked locking */
    static HardwareInfo hw_info = {0};
    static _Atomic int hw_initialized = 0;
    static pthread_mutex_t hw_mutex = PTHREAD_MUTEX_INITIALIZER;

    if (!atomic_load_explicit(&hw_initialized, memory_order_acquire)) {
      pthread_mutex_lock(&hw_mutex);
      if (!atomic_load_explicit(&hw_initialized, memory_order_relaxed)) {
        hwdetect_init(&hw_info);
        hwdetect_autotune_threads(&hw_info);
        ac_set_prefetch(ctx->patterns->automaton, hw_info.prefetch_distance,
                        hw_info.prefetch_hint);
        atomic_store_explicit(&hw_initialized, 1, memory_order_release);
      }
      pthread_mutex_unlock(&hw_mutex);
    }

    num_threads = hwdetect_get_optimal_threads(&hw_info);
    if (num_threads <= 0) {
      num_threads = sysconf(_SC_NPROCESSORS_ONLN);
      if (num_threads <= 0)
        num_threads = 1;
    }
  }

  /* Choose processing mode */
  if (num_threads > 1) {
    /* Use new parallel implementation with pthread barriers */
    result = process_parallel_new(ctx, num_threads);
  } else {
    result = process_single_threaded(ctx);
  }

  /* Flush output */
  if (!io_flush(&ctx->io)) {
    clock_gettime(CLOCK_MONOTONIC, &ctx->end_time);
    return 1;
  }

  /* Record end time */
  clock_gettime(CLOCK_MONOTONIC, &ctx->end_time);

  return result;
}

int plumbr_process(PlumbrContext *ctx, FILE *in, FILE *out) {
  return plumbr_process_fd(ctx, fileno(in), fileno(out));
}

PlumbrStats plumbr_get_stats(const PlumbrContext *ctx) {
  PlumbrStats stats = {0};

  stats.bytes_read = io_bytes_read(&ctx->io);
  stats.bytes_written = io_bytes_written(&ctx->io);
  stats.lines_processed = io_lines_processed(&ctx->io);
  stats.lines_modified = redactor_lines_modified(ctx->redactor);
  stats.patterns_matched = redactor_patterns_matched(ctx->redactor);
  stats.patterns_loaded = patterns_count(ctx->patterns);

  /* Calculate elapsed time */
  double start = ctx->start_time.tv_sec + ctx->start_time.tv_nsec / 1e9;
  double end = ctx->end_time.tv_sec + ctx->end_time.tv_nsec / 1e9;
  stats.elapsed_seconds = end - start;

  if (stats.elapsed_seconds > 0) {
    stats.lines_per_second = stats.lines_processed / stats.elapsed_seconds;
    stats.mb_per_second =
        (stats.bytes_read / (1024.0 * 1024.0)) / stats.elapsed_seconds;
  }

  return stats;
}

void plumbr_print_stats(const PlumbrContext *ctx, FILE *out) {
  PlumbrStats stats = plumbr_get_stats(ctx);

  fprintf(out, "\n");
  fprintf(out, "=== PlumbrC Statistics ===\n");
  fprintf(out, "Patterns loaded:    %zu\n", stats.patterns_loaded);
  fprintf(out, "Bytes read:         %zu (%.2f MB)\n", stats.bytes_read,
          stats.bytes_read / (1024.0 * 1024.0));
  fprintf(out, "Bytes written:      %zu (%.2f MB)\n", stats.bytes_written,
          stats.bytes_written / (1024.0 * 1024.0));
  fprintf(out, "Lines processed:    %zu\n", stats.lines_processed);
  fprintf(out, "Lines modified:     %zu (%.1f%%)\n", stats.lines_modified,
          stats.lines_processed > 0
              ? (100.0 * stats.lines_modified / stats.lines_processed)
              : 0.0);
  fprintf(out, "Patterns matched:   %zu\n", stats.patterns_matched);
  fprintf(out, "Elapsed time:       %.3f seconds\n", stats.elapsed_seconds);
  fprintf(out, "Throughput:         %.0f lines/sec\n", stats.lines_per_second);
  fprintf(out, "Throughput:         %.2f MB/sec\n", stats.mb_per_second);
  fprintf(out, "===========================\n");
}

void plumbr_destroy(PlumbrContext *ctx) {
  if (!ctx)
    return;

  if (ctx->pctx) {
    parallel_destroy(ctx->pctx);
  }
  redactor_destroy(ctx->redactor);
  patterns_destroy(ctx->patterns);
  arena_destroy(&ctx->arena);
  free(ctx);
}

/* SECURITY: Thread-safe version string using pthread_once */
static char g_plumbr_version[32];
static pthread_once_t g_version_once = PTHREAD_ONCE_INIT;
static void init_version(void) {
  snprintf(g_plumbr_version, sizeof(g_plumbr_version), "%d.%d.%d",
           PLUMBR_VERSION_MAJOR, PLUMBR_VERSION_MINOR, PLUMBR_VERSION_PATCH);
}
const char *plumbr_version(void) {
  pthread_once(&g_version_once, init_version);
  return g_plumbr_version;
}
