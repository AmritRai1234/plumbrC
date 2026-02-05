/*
 * PlumbrC - Public API
 * High-Performance Log Redaction Pipeline
 */

#ifndef PLUMBR_H
#define PLUMBR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"

/* Forward declarations */
typedef struct PlumbrContext PlumbrContext;

/* Statistics */
typedef struct {
  size_t bytes_read;
  size_t bytes_written;
  size_t lines_processed;
  size_t lines_modified;
  size_t patterns_matched;
  size_t patterns_loaded;
  double elapsed_seconds;
  double lines_per_second;
  double mb_per_second;
} PlumbrStats;

/* Configuration options */
typedef struct {
  const char *pattern_file; /* Path to pattern file (NULL for defaults) */
  bool use_defaults;        /* Use built-in default patterns */
  bool quiet;               /* Suppress stats output */
  bool stats_to_stderr;     /* Print stats to stderr */
  size_t buffer_size;       /* I/O buffer size (0 = default) */
  int num_threads;          /* Worker threads (0 = auto, 1 = single-threaded) */
} PlumbrConfig;

/* Initialize with default config */
void plumbr_config_init(PlumbrConfig *config);

/* Create context with configuration */
PlumbrContext *plumbr_create(const PlumbrConfig *config);

/* Process stream (main entry point) */
int plumbr_process(PlumbrContext *ctx, FILE *in, FILE *out);

/* Process with file descriptors */
int plumbr_process_fd(PlumbrContext *ctx, int in_fd, int out_fd);

/* Get statistics */
PlumbrStats plumbr_get_stats(const PlumbrContext *ctx);

/* Print stats to stream */
void plumbr_print_stats(const PlumbrContext *ctx, FILE *out);

/* Free resources */
void plumbr_destroy(PlumbrContext *ctx);

/* Version string */
const char *plumbr_version(void);

#endif /* PLUMBR_H */
