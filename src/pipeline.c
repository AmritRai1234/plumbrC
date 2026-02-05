/*
 * PlumbrC - Pipeline Implementation
 * Main processing loop
 */

#include "arena.h"
#include "hwdetect.h"
#include "io.h"
#include "parallel.h"
#include "patterns.h"
#include "plumbr.h"
#include "redactor.h"
#include "thread_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct PlumbrContext {
  Arena arena;
  PatternSet *patterns;
  Redactor *redactor;
  IOContext io;
  PlumbrConfig config;

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
#define BATCH_SIZE 4096

/*
 * New parallel processing using pthread barriers
 * More reliable than the old thread pool implementation
 */
static int process_parallel_new(PlumbrContext *ctx, int num_threads) {
  ParallelCtx *pctx =
      parallel_create(num_threads, ctx->patterns, PLUMBR_MAX_LINE_SIZE);
  if (!pctx) {
    fprintf(stderr, "Warning: Failed to create parallel context, "
                    "using single-threaded\n");
    return process_single_threaded(ctx);
  }

  /* Allocate batch storage */
  const char **lines = malloc(BATCH_SIZE * sizeof(char *));
  size_t *lengths = malloc(BATCH_SIZE * sizeof(size_t));
  char **outputs = malloc(BATCH_SIZE * sizeof(char *));
  size_t *out_lengths = malloc(BATCH_SIZE * sizeof(size_t));
  char **line_copies = malloc(BATCH_SIZE * sizeof(char *));

  if (!lines || !lengths || !outputs || !out_lengths || !line_copies) {
    free(lines);
    free(lengths);
    free(outputs);
    free(out_lengths);
    free(line_copies);
    parallel_destroy(pctx);
    return process_single_threaded(ctx);
  }

  /* Allocate output buffers and line copies */
  for (size_t i = 0; i < BATCH_SIZE; i++) {
    outputs[i] = malloc(PLUMBR_MAX_LINE_SIZE);
    line_copies[i] = malloc(PLUMBR_MAX_LINE_SIZE);
    if (!outputs[i] || !line_copies[i]) {
      for (size_t j = 0; j <= i; j++) {
        free(outputs[j]);
        free(line_copies[j]);
      }
      free(lines);
      free(lengths);
      free(outputs);
      free(out_lengths);
      free(line_copies);
      parallel_destroy(pctx);
      return process_single_threaded(ctx);
    }
  }

  size_t batch_count = 0;
  size_t line_len;
  const char *line;
  int result = 0;

  while ((line = io_read_line(&ctx->io, &line_len)) != NULL) {
    /* Copy line (I/O buffer gets reused) */
    if (line_len < PLUMBR_MAX_LINE_SIZE) {
      memcpy(line_copies[batch_count], line, line_len + 1);
    } else {
      memcpy(line_copies[batch_count], line, PLUMBR_MAX_LINE_SIZE - 1);
      line_copies[batch_count][PLUMBR_MAX_LINE_SIZE - 1] = '\0';
      line_len = PLUMBR_MAX_LINE_SIZE - 1;
    }

    lines[batch_count] = line_copies[batch_count];
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
  for (size_t i = 0; i < BATCH_SIZE; i++) {
    free(outputs[i]);
    free(line_copies[i]);
  }
  free(lines);
  free(lengths);
  free(outputs);
  free(out_lengths);
  free(line_copies);
  parallel_destroy(pctx);

  return result;
}

/*
 * Multi-threaded batch processing
 * NOTE: Currently disabled due to synchronization issues causing data loss.
 * TODO: Fix thread pool before re-enabling.
 */
#if 0  /* DISABLED - sync issues */
static int process_multi_threaded(PlumbrContext *ctx, int num_threads) {
  /* Create thread pool */
  ThreadPool *pool =
      threadpool_create(num_threads, ctx->patterns, PLUMBR_MAX_LINE_SIZE);
  if (!pool) {
    fprintf(stderr, "Warning: Failed to create thread pool, falling back to "
                    "single-threaded\n");
    return process_single_threaded(ctx);
  }

  /* Allocate batch buffers */
  WorkItem *batch = malloc(BATCH_SIZE * sizeof(WorkItem));
  char **output_bufs = malloc(BATCH_SIZE * sizeof(char *));
  char **input_copies = malloc(BATCH_SIZE * sizeof(char *));

  if (!batch || !output_bufs || !input_copies) {
    free(batch);
    free(output_bufs);
    free(input_copies);
    threadpool_destroy(pool);
    return process_single_threaded(ctx);
  }

  /* Initialize output buffers */
  for (size_t i = 0; i < BATCH_SIZE; i++) {
    output_bufs[i] = malloc(PLUMBR_MAX_LINE_SIZE);
    input_copies[i] = malloc(PLUMBR_MAX_LINE_SIZE);
    if (!output_bufs[i] || !input_copies[i]) {
      for (size_t j = 0; j <= i; j++) {
        free(output_bufs[j]);
        free(input_copies[j]);
      }
      free(batch);
      free(output_bufs);
      free(input_copies);
      threadpool_destroy(pool);
      return process_single_threaded(ctx);
    }
  }

  size_t batch_count = 0;
  size_t line_len;
  const char *line;
  int result = 0;

  while ((line = io_read_line(&ctx->io, &line_len)) != NULL) {
    /* Copy input (needed because io buffer may be overwritten) */
    if (line_len < PLUMBR_MAX_LINE_SIZE) {
      memcpy(input_copies[batch_count], line, line_len + 1);
    } else {
      memcpy(input_copies[batch_count], line, PLUMBR_MAX_LINE_SIZE - 1);
      input_copies[batch_count][PLUMBR_MAX_LINE_SIZE - 1] = '\0';
      line_len = PLUMBR_MAX_LINE_SIZE - 1;
    }

    /* Setup work item */
    batch[batch_count].input = input_copies[batch_count];
    batch[batch_count].input_len = line_len;
    batch[batch_count].output = output_bufs[batch_count];
    batch[batch_count].output_cap = PLUMBR_MAX_LINE_SIZE;
    batch[batch_count].output_len = 0;
    batch[batch_count].modified = false;

    batch_count++;

    /* Process batch when full */
    if (batch_count >= BATCH_SIZE) {
      threadpool_process_batch(pool, batch, batch_count);

      /* Write outputs in order */
      for (size_t i = 0; i < batch_count; i++) {
        if (!io_write_line(&ctx->io, batch[i].output, batch[i].output_len)) {
          result = 1;
          goto cleanup;
        }
      }
      batch_count = 0;
    }
  }

  /* Process remaining items */
  if (batch_count > 0) {
    threadpool_process_batch(pool, batch, batch_count);
    for (size_t i = 0; i < batch_count; i++) {
      if (!io_write_line(&ctx->io, batch[i].output, batch[i].output_len)) {
        result = 1;
        goto cleanup;
      }
    }
  }

cleanup:
  /* Free buffers */
  for (size_t i = 0; i < BATCH_SIZE; i++) {
    free(output_bufs[i]);
    free(input_copies[i]);
  }
  free(batch);
  free(output_bufs);
  free(input_copies);
  threadpool_destroy(pool);

  return result;
}
#endif /* DISABLED */

int plumbr_process_fd(PlumbrContext *ctx, int in_fd, int out_fd) {
  /* Record start time */
  clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);

  /* Initialize I/O */
  io_init(&ctx->io, in_fd, out_fd);

  int result;
  int num_threads = ctx->config.num_threads;

  /* Auto-detect optimal thread count if 0 */
  if (num_threads == 0) {
    /* Use hardware detection for optimal tuning */
    static HardwareInfo hw_info = {0};
    static int hw_initialized = 0;

    if (!hw_initialized) {
      hwdetect_init(&hw_info);
      hwdetect_autotune_threads(&hw_info);
      hw_initialized = 1;
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

  patterns_destroy(ctx->patterns);
  arena_destroy(&ctx->arena);
  free(ctx);
}

const char *plumbr_version(void) {
  static char version[32];
  snprintf(version, sizeof(version), "%d.%d.%d", PLUMBR_VERSION_MAJOR,
           PLUMBR_VERSION_MINOR, PLUMBR_VERSION_PATCH);
  return version;
}
