/*
 * PlumbrC Library API
 * High-performance log redaction — embed in any application.
 *
 * Quick start:
 *   #include <plumbr/libplumbr.h>
 *
 *   libplumbr_t *p = libplumbr_new(NULL);
 *   char *safe = libplumbr_redact(p, "api_key=secret123", 17, NULL);
 *   printf("%s\n", safe);  // "api_key=[REDACTED:api_key]"
 *   libplumbr_free_string(safe);
 *   libplumbr_free(p);
 *
 * Thread safety:
 *   Each libplumbr_t instance must be used from a single thread.
 *   Create separate instances for concurrent use.
 */

#ifndef LIBPLUMBR_H
#define LIBPLUMBR_H

#include <stddef.h>
#include <sys/types.h> /* ssize_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ─── */

typedef enum {
  PLUMBR_OK                 =  0,
  PLUMBR_ERR_ALLOC          = -1, /* Memory allocation failed */
  PLUMBR_ERR_PATTERN        = -2, /* Invalid pattern or pattern file */
  PLUMBR_ERR_INPUT_TOO_LARGE= -3, /* Input exceeds PLUMBR_MAX_LINE_SIZE */
  PLUMBR_ERR_BUFFER_TOO_SMALL=-4, /* Output buffer too small */
  PLUMBR_ERR_NULL_INPUT     = -5, /* NULL pointer passed */
} libplumbr_error_t;

/* ─── Opaque Handle ─── */

typedef struct libplumbr libplumbr_t;

/* ─── Configuration ─── */

typedef struct {
  const char *pattern_file; /* Path to pattern file (NULL = defaults) */
  const char *pattern_dir;  /* Path to pattern directory (NULL = none) */
  const char *compliance;   /* Compliance profiles: hipaa,pci,gdpr,soc2,all */
  int num_threads;          /* Worker threads (0 = auto) */
  int quiet;                /* Suppress stats output */
} libplumbr_config_t;

/* ─── Statistics ─── */

typedef struct {
  size_t lines_processed;
  size_t lines_modified;
  size_t patterns_matched;
  size_t bytes_processed;
  double elapsed_seconds;
} libplumbr_stats_t;

/* ─── Lifecycle ─── */

/*
 * Create a new PlumbrC instance.
 *
 * config: Configuration options (NULL for defaults — 14 built-in patterns).
 * Returns: Handle, or NULL on error (check libplumbr_last_error()).
 */
libplumbr_t *libplumbr_new(const libplumbr_config_t *config);

/*
 * Free a PlumbrC instance and all associated resources.
 */
void libplumbr_free(libplumbr_t *p);

/* ─── Redaction ─── */

/*
 * Redact a single line (allocating).
 *
 * Returns a newly allocated string. Caller must free with libplumbr_free_string().
 * Returns NULL on error.
 *
 * p:          PlumbrC handle
 * input:      Input bytes (need not be null-terminated)
 * input_len:  Length of input
 * output_len: If non-NULL, receives output length
 */
char *libplumbr_redact(libplumbr_t *p, const char *input, size_t input_len,
                       size_t *output_len);

/*
 * Redact into a caller-owned buffer (zero-allocation).
 *
 * Ideal for FFI bindings (Rust, Go) that manage their own memory.
 * Returns the number of bytes written, or a negative libplumbr_error_t on error.
 *
 * p:       PlumbrC handle
 * input:   Input bytes
 * in_len:  Input length
 * output:  Caller-allocated output buffer
 * out_cap: Capacity of output buffer
 */
ssize_t libplumbr_redact_into(libplumbr_t *p, const char *input, size_t in_len,
                              char *output, size_t out_cap);

/*
 * Redact a line in-place.
 *
 * Returns the new length, or a negative libplumbr_error_t on error.
 *
 * p:        PlumbrC handle
 * buffer:   Input/output buffer
 * len:      Input length
 * capacity: Buffer capacity
 */
ssize_t libplumbr_redact_inplace(libplumbr_t *p, char *buffer, size_t len,
                                 size_t capacity);

/*
 * Redact multiple lines (batch).
 *
 * Each output[i] is newly allocated; caller must free with libplumbr_free_string().
 * Returns number of lines processed, or -1 on error.
 */
int libplumbr_redact_batch(libplumbr_t *p, const char **inputs,
                           const size_t *input_lens, char **outputs,
                           size_t *output_lens, size_t count);

/*
 * Redact a newline-separated buffer (bulk API).
 *
 * Processes all lines in one call — ideal for FFI bindings that want to
 * avoid per-line call overhead. Returns newly allocated result.
 * Caller must free with libplumbr_free_string().
 */
char *libplumbr_redact_buffer(libplumbr_t *p, const char *input,
                              size_t input_len, size_t *output_len);

/* ─── Memory ─── */

/*
 * Free a string returned by libplumbr_redact() or libplumbr_redact_buffer().
 * Always use this instead of free() when calling from FFI.
 */
void libplumbr_free_string(char *str);

/* ─── Info & Stats ─── */

libplumbr_stats_t libplumbr_get_stats(const libplumbr_t *p);
void              libplumbr_reset_stats(libplumbr_t *p);
size_t            libplumbr_pattern_count(const libplumbr_t *p);
const char       *libplumbr_version(void);
int               libplumbr_is_threadsafe(void);

/*
 * Get the last error code (per-thread).
 * Reset to PLUMBR_OK after each successful call.
 */
libplumbr_error_t libplumbr_last_error(void);

/*
 * Get a human-readable error string.
 */
const char *libplumbr_error_string(libplumbr_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLUMBR_H */
