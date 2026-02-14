/*
 * PlumbrC - Buffered I/O
 * High-performance streaming with vectorized writes
 */

#ifndef PLUMBR_IO_H
#define PLUMBR_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "config.h"

typedef struct {
  /* Input buffer */
  char read_buf[PLUMBR_READ_BUFFER_SIZE];
  size_t read_pos; /* Current read position */
  size_t read_len; /* Valid data length */
  int read_fd;     /* Input file descriptor */
  bool read_eof;   /* EOF reached */

  /* Output buffer */
  char write_buf[PLUMBR_WRITE_BUFFER_SIZE];
  size_t write_pos; /* Current write position */
  int write_fd;     /* Output file descriptor */

  /* Line buffer for incomplete lines */
  char line_carry[PLUMBR_MAX_LINE_SIZE];
  size_t line_carry_len;

  /* Stats */
  size_t bytes_read;
  size_t bytes_written;
  size_t lines_processed;
} IOContext;

/* Initialize I/O context with file descriptors */
void io_init(IOContext *ctx, int read_fd, int write_fd);

/* Read next line (returns pointer to line, sets length)
 * Returns NULL on EOF or error
 * The returned pointer is valid until next io_read_line call */
const char *io_read_line(IOContext *ctx, size_t *out_len);

/* Write data to output buffer (auto-flushes when full) */
bool io_write(IOContext *ctx, const char *data, size_t len);

/* Write line with newline */
bool io_write_line(IOContext *ctx, const char *line, size_t len);

/* Flush output buffer */
bool io_flush(IOContext *ctx);

/* Get stats */
size_t io_bytes_read(const IOContext *ctx);
size_t io_bytes_written(const IOContext *ctx);
size_t io_lines_processed(const IOContext *ctx);

#endif /* PLUMBR_IO_H */
