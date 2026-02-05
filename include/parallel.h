/*
 * PlumbrC - Parallel Pipeline (Fixed)
 * Uses pthread barriers for reliable synchronization
 * Optimized for AMD Ryzen with AVX2
 */

#ifndef PLUMBR_PARALLEL_H
#define PLUMBR_PARALLEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
struct PatternSet;
struct Redactor;

/* Parallel processing context */
typedef struct ParallelCtx ParallelCtx;

/* Create parallel context */
ParallelCtx *parallel_create(int num_threads, struct PatternSet *patterns,
                             size_t max_line_size);

/* Process lines in parallel
 * Returns number of lines processed, -1 on error
 *
 * lines: array of line pointers
 * lengths: array of line lengths
 * outputs: array of output buffers (pre-allocated)
 * out_lengths: filled with output lengths
 * count: number of lines
 */
int parallel_process(ParallelCtx *ctx, const char **lines,
                     const size_t *lengths, char **outputs, size_t *out_lengths,
                     size_t count);

/* Get stats */
size_t parallel_patterns_matched(const ParallelCtx *ctx);
size_t parallel_lines_modified(const ParallelCtx *ctx);

/* Reset stats */
void parallel_reset_stats(ParallelCtx *ctx);

/* Destroy */
void parallel_destroy(ParallelCtx *ctx);

#endif /* PLUMBR_PARALLEL_H */
