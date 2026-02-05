/*
 * PlumbrC Library API
 * Embed log redaction into your application
 *
 * Usage:
 *   #include <plumbr/libplumbr.h>
 *
 *   libplumbr_t *p = libplumbr_new(NULL);  // Use default patterns
 *   char *safe = libplumbr_redact(p, "api_key=secret123", 17, NULL);
 *   printf("%s\n", safe);  // "api_key=[REDACTED:api_key]"
 *   free(safe);
 *   libplumbr_free(p);
 */

#ifndef LIBPLUMBR_H
#define LIBPLUMBR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle */
typedef struct libplumbr libplumbr_t;

/* Configuration options */
typedef struct {
  const char *pattern_file; /* Path to pattern file (NULL = defaults) */
  const char *pattern_dir;  /* Path to pattern directory (NULL = none) */
  int num_threads;          /* Worker threads (0 = auto) */
  int quiet;                /* Suppress stats output */
} libplumbr_config_t;

/* Statistics */
typedef struct {
  size_t lines_processed;
  size_t lines_modified;
  size_t patterns_matched;
  size_t bytes_processed;
  double elapsed_seconds;
} libplumbr_stats_t;

/*
 * Create a new PlumbrC instance
 *
 * config: Configuration options (NULL for defaults)
 * Returns: Handle or NULL on error
 */
libplumbr_t *libplumbr_new(const libplumbr_config_t *config);

/*
 * Redact a single line
 *
 * p: PlumbrC handle
 * input: Input string (does not need to be null-terminated)
 * input_len: Length of input string
 * output_len: If non-NULL, receives output length
 *
 * Returns: Newly allocated redacted string (caller must free)
 *          Returns NULL on error
 */
char *libplumbr_redact(libplumbr_t *p, const char *input, size_t input_len,
                       size_t *output_len);

/*
 * Redact a line in-place (if buffer is large enough)
 *
 * p: PlumbrC handle
 * buffer: Input/output buffer
 * len: Input length
 * capacity: Buffer capacity
 *
 * Returns: New length, or -1 if buffer too small
 */
int libplumbr_redact_inplace(libplumbr_t *p, char *buffer, size_t len,
                             size_t capacity);

/*
 * Redact multiple lines (batch processing)
 *
 * p: PlumbrC handle
 * inputs: Array of input strings
 * input_lens: Array of input lengths
 * outputs: Array to receive output strings (caller must free each)
 * output_lens: Array to receive output lengths
 * count: Number of lines
 *
 * Returns: Number of lines processed, -1 on error
 */
int libplumbr_redact_batch(libplumbr_t *p, const char **inputs,
                           const size_t *input_lens, char **outputs,
                           size_t *output_lens, size_t count);

/*
 * Get statistics
 */
libplumbr_stats_t libplumbr_get_stats(const libplumbr_t *p);

/*
 * Reset statistics
 */
void libplumbr_reset_stats(libplumbr_t *p);

/*
 * Get number of loaded patterns
 */
size_t libplumbr_pattern_count(const libplumbr_t *p);

/*
 * Get version string
 */
const char *libplumbr_version(void);

/*
 * Free PlumbrC instance
 */
void libplumbr_free(libplumbr_t *p);

/*
 * Thread-safe: Is the library thread-safe?
 * Returns: 1 if thread-safe, 0 otherwise
 */
int libplumbr_is_threadsafe(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLUMBR_H */
