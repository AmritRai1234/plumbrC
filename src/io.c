/*
 * PlumbrC - Buffered I/O Implementation
 */

#include "io.h"
#include "config.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

void io_init(IOContext *ctx, int read_fd, int write_fd) {
  memset(ctx, 0, sizeof(IOContext));
  ctx->read_fd = read_fd;
  ctx->write_fd = write_fd;
}

void io_init_stdio(IOContext *ctx, FILE *in, FILE *out) {
  io_init(ctx, fileno(in), fileno(out));
}

/* Internal: refill read buffer */
static bool io_refill(IOContext *ctx) {
  if (ctx->read_eof) {
    return false;
  }

  /* Move remaining data to start of buffer */
  if (ctx->read_pos > 0 && ctx->read_len > ctx->read_pos) {
    size_t remaining = ctx->read_len - ctx->read_pos;
    memmove(ctx->read_buf, ctx->read_buf + ctx->read_pos, remaining);
    ctx->read_len = remaining;
    ctx->read_pos = 0;
  } else {
    ctx->read_len = 0;
    ctx->read_pos = 0;
  }

  /* Fill rest of buffer */
  size_t space = PLUMBR_READ_BUFFER_SIZE - ctx->read_len;
  if (space == 0) {
    return true; /* Buffer full */
  }

  ssize_t n = read(ctx->read_fd, ctx->read_buf + ctx->read_len, space);

  if (n < 0) {
    if (errno == EINTR) {
      return io_refill(ctx); /* Retry on interrupt */
    }
    ctx->read_eof = true;
    return false;
  }

  if (n == 0) {
    ctx->read_eof = true;
    return ctx->read_len > ctx->read_pos; /* Still have data to process */
  }

  ctx->read_len += n;
  ctx->bytes_read += n;
  return true;
}

const char *io_read_line(IOContext *ctx, size_t *out_len) {
  /* Handle carried over partial line */
  if (ctx->line_carry_len > 0) {
    /* Find newline in current buffer */
    while (ctx->read_pos < ctx->read_len || io_refill(ctx)) {
      char *start = ctx->read_buf + ctx->read_pos;
      size_t avail = ctx->read_len - ctx->read_pos;

      char *nl = memchr(start, '\n', avail);

      if (nl != NULL) {
        size_t chunk_len = nl - start;

        /* SECURITY: Explicit bounds check to prevent overflow */
        size_t max_chunk = PLUMBR_MAX_LINE_SIZE - ctx->line_carry_len - 1;
        if (chunk_len > max_chunk) {
          /* Line too long, skip to newline */
          ctx->read_pos = (nl - ctx->read_buf) + 1;
          ctx->line_carry_len = 0;
          ctx->lines_processed++;
          return io_read_line(ctx, out_len);
        }

        size_t total_len = ctx->line_carry_len + chunk_len;

        /* Append to carry buffer - bounds already verified above */
        memcpy(ctx->line_carry + ctx->line_carry_len, start, chunk_len);
        ctx->line_carry_len = total_len;
        ctx->line_carry[total_len] = '\0';

        ctx->read_pos = (nl - ctx->read_buf) + 1;
        *out_len = total_len;
        ctx->lines_processed++;

        /* Reset carry for next call */
        ctx->line_carry_len = 0;
        return ctx->line_carry;
      }

      /* No newline - append all to carry */
      /* SECURITY: Calculate safe copy size to prevent overflow */
      size_t safe_avail = avail;
      size_t remaining_space = PLUMBR_MAX_LINE_SIZE - ctx->line_carry_len - 1;
      if (safe_avail > remaining_space) {
        /* Line too long, truncate to max size */
        safe_avail = remaining_space;
        if (safe_avail == 0) {
          ctx->line_carry_len = 0;
          ctx->read_pos = ctx->read_len;
          continue;
        }
      }

      memcpy(ctx->line_carry + ctx->line_carry_len, start, safe_avail);
      ctx->line_carry_len += safe_avail;
      ctx->read_pos = ctx->read_len;
    }

    /* EOF with data in carry buffer */
    if (ctx->line_carry_len > 0) {
      ctx->line_carry[ctx->line_carry_len] = '\0';
      *out_len = ctx->line_carry_len;
      ctx->lines_processed++;
      ctx->line_carry_len = 0;
      return ctx->line_carry;
    }

    return NULL;
  }

  /* Fast path: complete line in buffer */
  while (ctx->read_pos < ctx->read_len || io_refill(ctx)) {
    char *start = ctx->read_buf + ctx->read_pos;
    size_t avail = ctx->read_len - ctx->read_pos;

    char *nl = memchr(start, '\n', avail);

    if (nl != NULL) {
      size_t line_len = nl - start;
      *out_len = line_len;
      ctx->read_pos = (nl - ctx->read_buf) + 1;
      ctx->lines_processed++;

      /* Null terminate in place (we own the buffer) */
      *nl = '\0';
      return start;
    }

    /* No newline - need to carry over */
    if (avail > 0) {
      if (PLUMBR_UNLIKELY(avail >= PLUMBR_MAX_LINE_SIZE)) {
        /* Line too long, skip */
        ctx->read_pos = ctx->read_len;
        continue;
      }

      memcpy(ctx->line_carry, start, avail);
      ctx->line_carry_len = avail;
      ctx->read_pos = ctx->read_len;
    }
  }

  /* EOF */
  if (ctx->line_carry_len > 0) {
    ctx->line_carry[ctx->line_carry_len] = '\0';
    *out_len = ctx->line_carry_len;
    ctx->lines_processed++;
    ctx->line_carry_len = 0;
    return ctx->line_carry;
  }

  return NULL;
}

bool io_write(IOContext *ctx, const char *data, size_t len) {
  while (len > 0) {
    size_t space = PLUMBR_WRITE_BUFFER_SIZE - ctx->write_pos;

    if (space == 0) {
      if (!io_flush(ctx)) {
        return false;
      }
      space = PLUMBR_WRITE_BUFFER_SIZE;
    }

    size_t chunk = (len < space) ? len : space;
    memcpy(ctx->write_buf + ctx->write_pos, data, chunk);
    ctx->write_pos += chunk;
    data += chunk;
    len -= chunk;
  }

  return true;
}

bool io_write_line(IOContext *ctx, const char *line, size_t len) {
  if (!io_write(ctx, line, len)) {
    return false;
  }
  return io_write(ctx, "\n", 1);
}

bool io_flush(IOContext *ctx) {
  if (ctx->write_pos == 0) {
    return true;
  }

  size_t written = 0;
  while (written < ctx->write_pos) {
    ssize_t n = write(ctx->write_fd, ctx->write_buf + written,
                      ctx->write_pos - written);

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }

    written += n;
  }

  ctx->bytes_written += ctx->write_pos;
  ctx->write_pos = 0;
  return true;
}

bool io_eof(const IOContext *ctx) {
  return ctx->read_eof && ctx->read_pos >= ctx->read_len &&
         ctx->line_carry_len == 0;
}

size_t io_bytes_read(const IOContext *ctx) { return ctx->bytes_read; }

size_t io_bytes_written(const IOContext *ctx) {
  return ctx->bytes_written + ctx->write_pos; /* Include pending */
}

size_t io_lines_processed(const IOContext *ctx) { return ctx->lines_processed; }
