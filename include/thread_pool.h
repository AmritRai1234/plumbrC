/*
 * PlumbrC - Thread Pool Implementation
 * Parallel line processing for maximum throughput
 */

#ifndef PLUMBR_THREAD_POOL_H
#define PLUMBR_THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct ThreadPool ThreadPool;
typedef struct PatternSet PatternSet;

/* Work item for processing */
typedef struct {
  const char *input; /* Input line (null-terminated) */
  size_t input_len;  /* Input length */
  char *output;      /* Output buffer */
  size_t output_len; /* Output length after processing */
  size_t output_cap; /* Output buffer capacity */
  bool modified;     /* Was the line modified? */
} WorkItem;

/* Create thread pool with specified number of workers */
ThreadPool *threadpool_create(int num_threads, PatternSet *patterns,
                              size_t max_line_size);

/* Process a batch of work items in parallel */
int threadpool_process_batch(ThreadPool *pool, WorkItem *items, size_t count);

/* Get statistics */
size_t threadpool_patterns_matched(const ThreadPool *pool);
size_t threadpool_lines_modified(const ThreadPool *pool);

/* Reset statistics */
void threadpool_reset_stats(ThreadPool *pool);

/* Destroy thread pool */
void threadpool_destroy(ThreadPool *pool);

/* Get optimal thread count */
int threadpool_optimal_threads(void);

#endif /* PLUMBR_THREAD_POOL_H */
