/*
 * PlumbrC - Thread Pool Implementation
 * Uses pthreads for parallel line processing
 * Fixed version with proper synchronization using generation counter
 */

#include "thread_pool.h"
#include "arena.h"
#include "config.h"
#include "patterns.h"
#include "redactor.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Per-thread context */
typedef struct {
  pthread_t thread;
  int id;

  /* Thread-local redactor */
  Arena arena;
  Redactor *redactor;

  /* Work assignment */
  WorkItem *items;  /* Array of work items */
  size_t start_idx; /* Start index in batch */
  size_t end_idx;   /* End index in batch */

  /* Synchronization - using pointers to pool's sync primitives */
  pthread_mutex_t *mutex;
  pthread_cond_t *work_cond;
  pthread_cond_t *done_cond;

  atomic_uint *generation;  /* Current work generation */
  atomic_int *workers_done; /* How many workers finished current gen */
  atomic_int *shutdown;     /* Shutdown flag */

  unsigned int my_gen; /* Last generation this worker processed */

  /* Statistics */
  size_t patterns_matched;
  size_t lines_modified;
} WorkerContext;

struct ThreadPool {
  int num_threads;
  WorkerContext *workers;

  /* Shared pattern set (read-only after init) */
  PatternSet *patterns;
  size_t max_line_size;

  /* Synchronization */
  pthread_mutex_t mutex;
  pthread_cond_t work_cond; /* Signal workers: new work available */
  pthread_cond_t done_cond; /* Signal main: workers finished */

  atomic_uint generation;  /* Incremented for each new batch */
  atomic_int workers_done; /* Count of workers finished current gen */
  atomic_int shutdown;

  /* Aggregate statistics */
  size_t total_patterns_matched;
  size_t total_lines_modified;
};

/* Worker thread function */
static void *worker_thread(void *arg) {
  WorkerContext *ctx = (WorkerContext *)arg;
  ctx->my_gen = 0;

  while (1) {
    pthread_mutex_lock(ctx->mutex);

    /* Wait for new work (generation > my_gen) or shutdown */
    while (atomic_load(ctx->generation) <= ctx->my_gen &&
           !atomic_load(ctx->shutdown)) {
      pthread_cond_wait(ctx->work_cond, ctx->mutex);
    }

    pthread_mutex_unlock(ctx->mutex);

    /* Check for shutdown */
    if (atomic_load(ctx->shutdown)) {
      break;
    }

    /* Update my generation */
    ctx->my_gen = atomic_load(ctx->generation);

    /* Process assigned work items */
    for (size_t i = ctx->start_idx; i < ctx->end_idx; i++) {
      WorkItem *item = &ctx->items[i];

      size_t out_len;
      const char *output = redactor_process(ctx->redactor, item->input,
                                            item->input_len, &out_len);

      /* Copy to output buffer */
      if (out_len < item->output_cap) {
        memcpy(item->output, output, out_len);
        item->output[out_len] = '\0';
        item->output_len = out_len;
        item->modified = (out_len != item->input_len ||
                          memcmp(output, item->input, out_len) != 0);

        if (item->modified) {
          ctx->lines_modified++;
        }
      }
    }

    /* Update stats from redactor */
    ctx->patterns_matched += redactor_patterns_matched(ctx->redactor);
    redactor_reset_stats(ctx->redactor);

    /* Signal completion */
    pthread_mutex_lock(ctx->mutex);
    int done = atomic_fetch_add(ctx->workers_done, 1) + 1;
    if (done >= (int)atomic_load(ctx->generation) /
                    1) { /* All done check will be in main */
      pthread_cond_signal(ctx->done_cond);
    }
    pthread_mutex_unlock(ctx->mutex);
  }

  return NULL;
}

int threadpool_optimal_threads(void) {
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (cores < 1)
    cores = 1;
  if (cores > 16)
    cores = 16; /* Cap at 16 for diminishing returns */
  return (int)cores;
}

ThreadPool *threadpool_create(int num_threads, PatternSet *patterns,
                              size_t max_line_size) {
  if (num_threads <= 0) {
    num_threads = threadpool_optimal_threads();
  }

  ThreadPool *pool = calloc(1, sizeof(ThreadPool));
  if (!pool)
    return NULL;

  pool->num_threads = num_threads;
  pool->patterns = patterns;
  pool->max_line_size = max_line_size;

  /* Initialize synchronization */
  pthread_mutex_init(&pool->mutex, NULL);
  pthread_cond_init(&pool->work_cond, NULL);
  pthread_cond_init(&pool->done_cond, NULL);

  atomic_store(&pool->generation, 0);
  atomic_store(&pool->workers_done, 0);
  atomic_store(&pool->shutdown, 0);

  /* Create workers */
  pool->workers = calloc(num_threads, sizeof(WorkerContext));
  if (!pool->workers) {
    free(pool);
    return NULL;
  }

  for (int i = 0; i < num_threads; i++) {
    WorkerContext *w = &pool->workers[i];
    w->id = i;
    w->my_gen = 0;

    /* Initialize thread-local arena */
    if (!arena_init(&w->arena, PLUMBR_SCRATCH_SIZE)) {
      /* Cleanup on failure */
      for (int j = 0; j < i; j++) {
        arena_destroy(&pool->workers[j].arena);
      }
      free(pool->workers);
      free(pool);
      return NULL;
    }

    /* Create thread-local redactor */
    w->redactor = redactor_create(&w->arena, patterns, max_line_size);
    if (!w->redactor) {
      arena_destroy(&w->arena);
      for (int j = 0; j < i; j++) {
        arena_destroy(&pool->workers[j].arena);
      }
      free(pool->workers);
      free(pool);
      return NULL;
    }

    /* Setup synchronization pointers */
    w->mutex = &pool->mutex;
    w->work_cond = &pool->work_cond;
    w->done_cond = &pool->done_cond;
    w->generation = &pool->generation;
    w->workers_done = &pool->workers_done;
    w->shutdown = &pool->shutdown;

    /* Start worker thread */
    if (pthread_create(&w->thread, NULL, worker_thread, w) != 0) {
      arena_destroy(&w->arena);
      for (int j = 0; j < i; j++) {
        arena_destroy(&pool->workers[j].arena);
        pthread_cancel(pool->workers[j].thread);
      }
      free(pool->workers);
      free(pool);
      return NULL;
    }
  }

  return pool;
}

int threadpool_process_batch(ThreadPool *pool, WorkItem *items, size_t count) {
  if (!pool || !items || count == 0)
    return -1;

  /* Distribute work among threads */
  size_t items_per_thread = (count + pool->num_threads - 1) / pool->num_threads;

  pthread_mutex_lock(&pool->mutex);

  for (int i = 0; i < pool->num_threads; i++) {
    WorkerContext *w = &pool->workers[i];
    w->items = items;
    w->start_idx = i * items_per_thread;
    w->end_idx = w->start_idx + items_per_thread;
    if (w->end_idx > count)
      w->end_idx = count;
    if (w->start_idx >= count) {
      w->start_idx = 0;
      w->end_idx = 0; /* No work for this thread */
    }
  }

  /* Reset completion counter and increment generation */
  atomic_store(&pool->workers_done, 0);
  atomic_fetch_add(&pool->generation, 1);

  /* Wake up all workers */
  pthread_cond_broadcast(&pool->work_cond);

  /* Wait for all workers to complete */
  while (atomic_load(&pool->workers_done) < pool->num_threads) {
    pthread_cond_wait(&pool->done_cond, &pool->mutex);
  }

  pthread_mutex_unlock(&pool->mutex);

  return 0;
}

size_t threadpool_patterns_matched(const ThreadPool *pool) {
  size_t total = pool->total_patterns_matched;
  for (int i = 0; i < pool->num_threads; i++) {
    total += pool->workers[i].patterns_matched;
  }
  return total;
}

size_t threadpool_lines_modified(const ThreadPool *pool) {
  size_t total = pool->total_lines_modified;
  for (int i = 0; i < pool->num_threads; i++) {
    total += pool->workers[i].lines_modified;
  }
  return total;
}

void threadpool_reset_stats(ThreadPool *pool) {
  pool->total_patterns_matched = threadpool_patterns_matched(pool);
  pool->total_lines_modified = threadpool_lines_modified(pool);

  for (int i = 0; i < pool->num_threads; i++) {
    pool->workers[i].patterns_matched = 0;
    pool->workers[i].lines_modified = 0;
  }
}

void threadpool_destroy(ThreadPool *pool) {
  if (!pool)
    return;

  /* Signal shutdown */
  pthread_mutex_lock(&pool->mutex);
  atomic_store(&pool->shutdown, 1);
  pthread_cond_broadcast(&pool->work_cond);
  pthread_mutex_unlock(&pool->mutex);

  /* Wait for threads to exit */
  for (int i = 0; i < pool->num_threads; i++) {
    pthread_join(pool->workers[i].thread, NULL);
    arena_destroy(&pool->workers[i].arena);
  }

  /* Cleanup synchronization */
  pthread_mutex_destroy(&pool->mutex);
  pthread_cond_destroy(&pool->work_cond);
  pthread_cond_destroy(&pool->done_cond);

  free(pool->workers);
  free(pool);
}
