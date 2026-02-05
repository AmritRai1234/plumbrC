/*
 * PlumbrC - Parallel Pipeline (Fixed)
 * Uses simple fork-join model with pthread barriers
 * Designed for AMD Ryzen 5000 series
 */

#include "parallel.h"
#include "arena.h"
#include "config.h"
#include "patterns.h"
#include "redactor.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Per-worker context */
typedef struct {
  int id;
  pthread_t thread;

  /* Thread-local resources */
  Arena arena;
  Redactor *redactor;

  /* Work assignment (set before each batch) */
  const char **lines;
  const size_t *lengths;
  char **outputs;
  size_t *out_lengths;
  size_t start_idx;
  size_t end_idx;

  /* Synchronization */
  pthread_barrier_t *start_barrier;
  pthread_barrier_t *done_barrier;
  volatile int *shutdown;

  /* Stats */
  size_t patterns_matched;
  size_t lines_modified;
} Worker;

struct ParallelCtx {
  int num_threads;
  Worker *workers;

  /* Shared resources */
  PatternSet *patterns;
  size_t max_line_size;

  /* Barriers for sync */
  pthread_barrier_t start_barrier;
  pthread_barrier_t done_barrier;

  volatile int shutdown;

  /* Aggregate stats */
  size_t total_patterns_matched;
  size_t total_lines_modified;
};

/* Worker thread function */
static void *worker_func(void *arg) {
  Worker *w = (Worker *)arg;

  while (1) {
    /* Wait at start barrier */
    pthread_barrier_wait(w->start_barrier);

    /* Check shutdown */
    if (*w->shutdown) {
      break;
    }

    /* Process assigned lines */
    for (size_t i = w->start_idx; i < w->end_idx; i++) {
      const char *line = w->lines[i];
      size_t len = w->lengths[i];

      /* Sanity check */
      if (len > PLUMBR_MAX_LINE_SIZE || line == NULL) {
        continue;
      }

      /* Process line */
      size_t out_len;
      const char *output = redactor_process(w->redactor, line, len, &out_len);

      /* Copy to output buffer */
      if (out_len < PLUMBR_MAX_LINE_SIZE) {
        memcpy(w->outputs[i], output, out_len);
        w->outputs[i][out_len] = '\0';
        w->out_lengths[i] = out_len;

        /* Track if modified */
        if (out_len != len || memcmp(output, line, len) != 0) {
          w->lines_modified++;
        }
      } else {
        /* Line too long, copy as-is */
        size_t copy_len =
            len < PLUMBR_MAX_LINE_SIZE - 1 ? len : PLUMBR_MAX_LINE_SIZE - 1;
        memcpy(w->outputs[i], line, copy_len);
        w->outputs[i][copy_len] = '\0';
        w->out_lengths[i] = copy_len;
      }
    }

    /* Collect stats from redactor */
    w->patterns_matched += redactor_patterns_matched(w->redactor);
    redactor_reset_stats(w->redactor);

    /* Wait at done barrier */
    pthread_barrier_wait(w->done_barrier);
  }

  return NULL;
}

/* Create parallel context */
ParallelCtx *parallel_create(int num_threads, PatternSet *patterns,
                             size_t max_line_size) {
  if (num_threads <= 0) {
    num_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_threads <= 0)
      num_threads = 1;
    if (num_threads > 12)
      num_threads = 12; /* Cap at 12 */
  }

  ParallelCtx *ctx = calloc(1, sizeof(ParallelCtx));
  if (!ctx)
    return NULL;

  ctx->num_threads = num_threads;
  ctx->patterns = patterns;
  ctx->max_line_size = max_line_size;
  ctx->shutdown = 0;

  /* Initialize barriers (+1 for main thread) */
  pthread_barrier_init(&ctx->start_barrier, NULL, num_threads + 1);
  pthread_barrier_init(&ctx->done_barrier, NULL, num_threads + 1);

  /* Create workers */
  ctx->workers = calloc(num_threads, sizeof(Worker));
  if (!ctx->workers) {
    free(ctx);
    return NULL;
  }

  for (int i = 0; i < num_threads; i++) {
    Worker *w = &ctx->workers[i];
    w->id = i;
    w->start_barrier = &ctx->start_barrier;
    w->done_barrier = &ctx->done_barrier;
    w->shutdown = &ctx->shutdown;

    /* Initialize thread-local arena */
    if (!arena_init(&w->arena, PLUMBR_SCRATCH_SIZE)) {
      goto cleanup;
    }

    /* Create thread-local redactor */
    w->redactor = redactor_create(&w->arena, patterns, max_line_size);
    if (!w->redactor) {
      arena_destroy(&w->arena);
      goto cleanup;
    }

    /* Start worker thread */
    if (pthread_create(&w->thread, NULL, worker_func, w) != 0) {
      arena_destroy(&w->arena);
      goto cleanup;
    }
  }

  return ctx;

cleanup:
  /* Cleanup on failure */
  ctx->shutdown = 1;
  pthread_barrier_destroy(&ctx->start_barrier);
  pthread_barrier_destroy(&ctx->done_barrier);

  for (int j = 0; j < num_threads; j++) {
    if (ctx->workers[j].thread) {
      pthread_cancel(ctx->workers[j].thread);
    }
    arena_destroy(&ctx->workers[j].arena);
  }
  free(ctx->workers);
  free(ctx);
  return NULL;
}

/* Process lines in parallel */
int parallel_process(ParallelCtx *ctx, const char **lines,
                     const size_t *lengths, char **outputs, size_t *out_lengths,
                     size_t count) {
  if (!ctx || !lines || !lengths || !outputs || !out_lengths || count == 0) {
    return -1;
  }

  /* Distribute work */
  size_t per_thread = (count + ctx->num_threads - 1) / ctx->num_threads;

  for (int i = 0; i < ctx->num_threads; i++) {
    Worker *w = &ctx->workers[i];
    w->lines = lines;
    w->lengths = lengths;
    w->outputs = outputs;
    w->out_lengths = out_lengths;
    w->start_idx = i * per_thread;
    w->end_idx = w->start_idx + per_thread;
    if (w->end_idx > count)
      w->end_idx = count;
    if (w->start_idx > count)
      w->start_idx = count;
  }

  /* Release workers (they're waiting at start barrier) */
  pthread_barrier_wait(&ctx->start_barrier);

  /* Wait for all workers to finish */
  pthread_barrier_wait(&ctx->done_barrier);

  return (int)count;
}

/* Get stats */
size_t parallel_patterns_matched(const ParallelCtx *ctx) {
  size_t total = ctx->total_patterns_matched;
  for (int i = 0; i < ctx->num_threads; i++) {
    total += ctx->workers[i].patterns_matched;
  }
  return total;
}

size_t parallel_lines_modified(const ParallelCtx *ctx) {
  size_t total = ctx->total_lines_modified;
  for (int i = 0; i < ctx->num_threads; i++) {
    total += ctx->workers[i].lines_modified;
  }
  return total;
}

/* Reset stats */
void parallel_reset_stats(ParallelCtx *ctx) {
  ctx->total_patterns_matched = parallel_patterns_matched(ctx);
  ctx->total_lines_modified = parallel_lines_modified(ctx);

  for (int i = 0; i < ctx->num_threads; i++) {
    ctx->workers[i].patterns_matched = 0;
    ctx->workers[i].lines_modified = 0;
  }
}

/* Destroy */
void parallel_destroy(ParallelCtx *ctx) {
  if (!ctx)
    return;

  /* Signal shutdown */
  ctx->shutdown = 1;

  /* Wake workers so they can exit */
  pthread_barrier_wait(&ctx->start_barrier);

  /* Wait for threads to exit */
  for (int i = 0; i < ctx->num_threads; i++) {
    pthread_join(ctx->workers[i].thread, NULL);
    arena_destroy(&ctx->workers[i].arena);
  }

  pthread_barrier_destroy(&ctx->start_barrier);
  pthread_barrier_destroy(&ctx->done_barrier);

  free(ctx->workers);
  free(ctx);
}
