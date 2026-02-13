/*
 * PlumbrC - Native HTTP Server
 * High-performance REST API for log redaction
 *
 * Architecture:
 *   - epoll-based I/O on main thread
 *   - Worker thread pool (1 per vCore)
 *   - Each worker owns a libplumbr_t instance (zero contention)
 *   - No external HTTP library dependencies
 *
 * Usage: plumbr-server [OPTIONS]
 *   --port PORT       Listen port (default: 8080)
 *   --threads N       Worker threads (0=auto, default: 0)
 *   --pattern-dir DIR Pattern directory
 *   --pattern-file F  Pattern file
 *   --host ADDR       Bind address (default: 0.0.0.0)
 */

#include "libplumbr.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

/* ─── Configuration ────────────────────────────────────────── */

#define SERVER_VERSION "1.0.0"
#define MAX_EVENTS 256
#define READ_BUF_SIZE (128 * 1024)  /* 128KB per connection */
#define MAX_BODY_SIZE (1024 * 1024) /* 1MB max request body */
#define MAX_HEADER_SIZE (8 * 1024)  /* 8KB max headers */
#define QUEUE_SIZE 4096

/* ─── Global state ─────────────────────────────────────────── */

static volatile sig_atomic_t g_running = 1;
static atomic_uint_least64_t g_requests_total = 0;
static atomic_uint_least64_t g_requests_ok = 0;
static atomic_uint_least64_t g_requests_err = 0;
static atomic_uint_least64_t g_bytes_processed = 0;
static struct timespec g_start_time;

/* ─── Work queue (lock-free-ish with mutex for simplicity) ── */

typedef struct {
  int fds[QUEUE_SIZE];
  int head;
  int tail;
  int count;
  pthread_mutex_t lock;
  pthread_cond_t not_empty;
} WorkQueue;

static void queue_init(WorkQueue *q) {
  q->head = 0;
  q->tail = 0;
  q->count = 0;
  pthread_mutex_init(&q->lock, NULL);
  pthread_cond_init(&q->not_empty, NULL);
}

static void queue_push(WorkQueue *q, int fd) {
  pthread_mutex_lock(&q->lock);
  if (q->count < QUEUE_SIZE) {
    q->fds[q->tail] = fd;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
  } else {
    /* Queue full — drop connection */
    close(fd);
  }
  pthread_mutex_unlock(&q->lock);
}

static int queue_pop(WorkQueue *q) {
  pthread_mutex_lock(&q->lock);
  while (q->count == 0 && g_running) {
    pthread_cond_wait(&q->not_empty, &q->lock);
  }
  if (!g_running && q->count == 0) {
    pthread_mutex_unlock(&q->lock);
    return -1;
  }
  int fd = q->fds[q->head];
  q->head = (q->head + 1) % QUEUE_SIZE;
  q->count--;
  pthread_mutex_unlock(&q->lock);
  return fd;
}

static void queue_wake_all(WorkQueue *q) {
  pthread_mutex_lock(&q->lock);
  pthread_cond_broadcast(&q->not_empty);
  pthread_mutex_unlock(&q->lock);
}

/* ─── HTTP helpers ─────────────────────────────────────────── */

/* Find end of HTTP headers (\r\n\r\n) */
static const char *find_header_end(const char *buf, size_t len) {
  for (size_t i = 0; i + 3 < len; i++) {
    if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' &&
        buf[i + 3] == '\n') {
      return buf + i + 4;
    }
  }
  return NULL;
}

/* Parse Content-Length from headers */
static int parse_content_length(const char *headers, size_t header_len) {
  const char *p = headers;
  const char *end = headers + header_len;

  while (p < end) {
    /* Case-insensitive search for "content-length:" */
    if ((end - p > 15) && (p[0] == 'C' || p[0] == 'c') &&
        (p[1] == 'o' || p[1] == 'O') && (p[2] == 'n' || p[2] == 'N') &&
        (p[3] == 't' || p[3] == 'T') && (p[4] == 'e' || p[4] == 'E') &&
        (p[5] == 'n' || p[5] == 'N') && (p[6] == 't' || p[6] == 'T') &&
        p[7] == '-' && (p[8] == 'L' || p[8] == 'l') &&
        (p[9] == 'e' || p[9] == 'E') && (p[10] == 'n' || p[10] == 'N') &&
        (p[11] == 'g' || p[11] == 'G') && (p[12] == 't' || p[12] == 'T') &&
        (p[13] == 'h' || p[13] == 'H') && p[14] == ':') {
      /* Skip whitespace after colon */
      const char *val = p + 15;
      while (val < end && *val == ' ')
        val++;
      return atoi(val);
    }
    /* Skip to next line */
    while (p < end && *p != '\n')
      p++;
    if (p < end)
      p++; /* skip \n */
  }
  return -1;
}

/* Check if Connection: keep-alive (HTTP/1.1 default) */
static bool is_keep_alive(const char *headers, size_t header_len) {
  /* Default keep-alive for HTTP/1.1 */
  if (header_len > 8 && strstr(headers, "HTTP/1.1"))
    return true;
  /* Explicit header check */
  const char *p = headers;
  const char *end = headers + header_len;
  while (p < end) {
    if ((end - p > 10) && (p[0] == 'C' || p[0] == 'c') &&
        (p[1] == 'o' || p[1] == 'O') && (p[2] == 'n' || p[2] == 'N') &&
        (p[3] == 'n' || p[3] == 'N') && (p[4] == 'e' || p[4] == 'E') &&
        (p[5] == 'c' || p[5] == 'C') && (p[6] == 't' || p[6] == 'T') &&
        (p[7] == 'i' || p[7] == 'I') && (p[8] == 'o' || p[8] == 'O') &&
        (p[9] == 'n' || p[9] == 'N') && p[10] == ':') {
      const char *val = p + 11;
      while (val < end && *val == ' ')
        val++;
      if (strncmp(val, "keep-alive", 10) == 0 ||
          strncmp(val, "Keep-Alive", 10) == 0 ||
          strncmp(val, "Keep-alive", 10) == 0)
        return true;
      return false;
    }
    while (p < end && *p != '\n')
      p++;
    if (p < end)
      p++;
  }
  return false;
}

/* Parse method and path from request line */
static bool parse_request_line(const char *buf, char *method, size_t mlen,
                               char *path, size_t plen) {
  /* "POST /api/redact HTTP/1.1\r\n" */
  const char *sp1 = memchr(buf, ' ', 16);
  if (!sp1)
    return false;

  size_t method_len = (size_t)(sp1 - buf);
  if (method_len >= mlen)
    return false;
  memcpy(method, buf, method_len);
  method[method_len] = '\0';

  const char *path_start = sp1 + 1;
  const char *sp2 = memchr(path_start, ' ', 256);
  if (!sp2)
    return false;

  size_t path_len = (size_t)(sp2 - path_start);
  if (path_len >= plen)
    return false;
  memcpy(path, path_start, path_len);
  path[path_len] = '\0';

  return true;
}

/* Extract JSON "text" field value — minimal zero-alloc parser */
static bool extract_json_text(const char *json, size_t json_len,
                              const char **text_start, size_t *text_len) {
  /*
   * Find "text" key and extract its string value.
   * Only handles: {"text": "value"} (with possible escapes)
   */
  const char *p = json;
  const char *end = json + json_len;

  /* Find "text" key */
  const char *key = NULL;
  while (p + 5 < end) {
    if (*p == '"' && p[1] == 't' && p[2] == 'e' && p[3] == 'x' && p[4] == 't' &&
        p[5] == '"') {
      key = p + 6;
      break;
    }
    p++;
  }
  if (!key)
    return false;

  /* Skip whitespace and colon */
  while (key < end && (*key == ' ' || *key == ':' || *key == '\t'))
    key++;

  /* Must be a string */
  if (key >= end || *key != '"')
    return false;
  key++; /* skip opening quote */

  /* Find end of string (handle escapes) */
  const char *val_start = key;
  while (key < end) {
    if (*key == '\\') {
      key += 2; /* skip escape sequence */
      continue;
    }
    if (*key == '"') {
      *text_start = val_start;
      *text_len = (size_t)(key - val_start);
      return true;
    }
    key++;
  }

  return false;
}

/* Unescape JSON string in-place, returns new length */
static size_t json_unescape(char *buf, size_t len) {
  size_t r = 0, w = 0;
  while (r < len) {
    if (buf[r] == '\\' && r + 1 < len) {
      r++;
      switch (buf[r]) {
      case '"':
        buf[w++] = '"';
        break;
      case '\\':
        buf[w++] = '\\';
        break;
      case '/':
        buf[w++] = '/';
        break;
      case 'n':
        buf[w++] = '\n';
        break;
      case 'r':
        buf[w++] = '\r';
        break;
      case 't':
        buf[w++] = '\t';
        break;
      case 'b':
        buf[w++] = '\b';
        break;
      case 'f':
        buf[w++] = '\f';
        break;
      default:
        buf[w++] = buf[r];
        break;
      }
      r++;
    } else {
      buf[w++] = buf[r++];
    }
  }
  return w;
}

/* Escape string for JSON output, returns malloc'd string */
static char *json_escape(const char *input, size_t input_len, size_t *out_len) {
  /* Worst case: every char needs escaping = 6x */
  size_t max_len = input_len * 6 + 1;
  char *out = malloc(max_len);
  if (!out)
    return NULL;

  size_t w = 0;
  for (size_t i = 0; i < input_len; i++) {
    unsigned char c = (unsigned char)input[i];
    switch (c) {
    case '"':
      out[w++] = '\\';
      out[w++] = '"';
      break;
    case '\\':
      out[w++] = '\\';
      out[w++] = '\\';
      break;
    case '\n':
      out[w++] = '\\';
      out[w++] = 'n';
      break;
    case '\r':
      out[w++] = '\\';
      out[w++] = 'r';
      break;
    case '\t':
      out[w++] = '\\';
      out[w++] = 't';
      break;
    case '\b':
      out[w++] = '\\';
      out[w++] = 'b';
      break;
    case '\f':
      out[w++] = '\\';
      out[w++] = 'f';
      break;
    default:
      if (c < 0x20) {
        w += snprintf(out + w, max_len - w, "\\u%04x", c);
      } else {
        out[w++] = (char)c;
      }
    }
  }
  out[w] = '\0';
  if (out_len)
    *out_len = w;
  return out;
}

/* ─── HTTP response builders ──────────────────────────────── */

static const char *CORS_HEADERS =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
    "Access-Control-Max-Age: 86400\r\n";

/* Send a complete HTTP response */
static void send_response(int fd, int status, const char *status_text,
                          const char *content_type, const char *body,
                          size_t body_len, bool keep_alive) {
  char header[1024];
  int hlen = snprintf(header, sizeof(header),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: %s\r\n"
                      "%s"
                      "\r\n",
                      status, status_text, content_type, body_len,
                      keep_alive ? "keep-alive" : "close", CORS_HEADERS);

  /* Send header + body in one writev if possible */
  struct iovec iov[2];
  iov[0].iov_base = header;
  iov[0].iov_len = (size_t)hlen;
  iov[1].iov_base = (void *)body;
  iov[1].iov_len = body_len;

  ssize_t total = (ssize_t)hlen + (ssize_t)body_len;
  ssize_t sent = writev(fd, iov, 2);
  if (sent < total) {
    /* Partial send — try remaining */
    if (sent > 0) {
      size_t remaining = (size_t)(total - sent);
      const char *ptr;
      if ((size_t)sent < (size_t)hlen) {
        /* Still in header — just give up for simplicity */
        return;
      }
      ptr = body + (sent - hlen);
      while (remaining > 0) {
        ssize_t n = write(fd, ptr, remaining);
        if (n <= 0)
          break;
        ptr += n;
        remaining -= (size_t)n;
      }
    }
  }
}

static void send_json_error(int fd, int status, const char *status_text,
                            const char *message, bool keep_alive) {
  char body[512];
  int blen = snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
  send_response(fd, status, status_text, "application/json", body, (size_t)blen,
                keep_alive);
}

static void send_options_response(int fd) {
  char header[512];
  int hlen = snprintf(header, sizeof(header),
                      "HTTP/1.1 204 No Content\r\n"
                      "Content-Length: 0\r\n"
                      "%s"
                      "\r\n",
                      CORS_HEADERS);
  (void)!write(fd, header, (size_t)hlen);
}

/* ─── Request handlers ─────────────────────────────────────── */

/* Count patterns matched in redacted output */
static int count_patterns(const char *text, size_t len) {
  int count = 0;
  for (size_t i = 0; i + 10 < len; i++) {
    if (text[i] == '[' && text[i + 1] == 'R' && text[i + 2] == 'E' &&
        text[i + 3] == 'D' && text[i + 4] == 'A' && text[i + 5] == 'C' &&
        text[i + 6] == 'T' && text[i + 7] == 'E' && text[i + 8] == 'D' &&
        text[i + 9] == ':') {
      count++;
    }
  }
  return count;
}

/* Handle POST /api/redact */
static void handle_redact(int fd, libplumbr_t *plumbr, const char *body,
                          size_t body_len, bool keep_alive) {
  struct timespec t_start, t_end;
  clock_gettime(CLOCK_MONOTONIC, &t_start);

  /* Extract "text" from JSON */
  const char *text_start;
  size_t text_len;
  if (!extract_json_text(body, body_len, &text_start, &text_len)) {
    send_json_error(fd, 400, "Bad Request", "Missing or invalid 'text' field",
                    keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }

  if (text_len > MAX_BODY_SIZE) {
    send_json_error(fd, 413, "Payload Too Large",
                    "Input too large. Max size: 1MB", keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }

  /* Make a mutable copy for unescaping */
  char *text_buf = malloc(text_len + 1);
  if (!text_buf) {
    send_json_error(fd, 500, "Internal Server Error",
                    "Memory allocation failed", keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }
  memcpy(text_buf, text_start, text_len);
  text_buf[text_len] = '\0';

  /* Unescape JSON string */
  size_t unescaped_len = json_unescape(text_buf, text_len);

  /* Process each line through plumbr */
  /* Build output by processing line-by-line */
  size_t out_capacity = unescaped_len * 2 + 256;
  char *output = malloc(out_capacity);
  if (!output) {
    free(text_buf);
    send_json_error(fd, 500, "Internal Server Error",
                    "Memory allocation failed", keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }

  size_t out_pos = 0;
  int lines_processed = 0;
  int lines_modified = 0;

  const char *line_start = text_buf;
  const char *text_end = text_buf + unescaped_len;

  while (line_start < text_end) {
    /* Find end of line */
    const char *line_end =
        memchr(line_start, '\n', (size_t)(text_end - line_start));
    size_t line_len;
    if (line_end) {
      line_len = (size_t)(line_end - line_start);
    } else {
      line_len = (size_t)(text_end - line_start);
      line_end = text_end;
    }

    /* Redact this line */
    size_t redacted_len;
    char *redacted =
        libplumbr_redact(plumbr, line_start, line_len, &redacted_len);

    const char *result;
    size_t result_len;
    if (redacted) {
      result = redacted;
      result_len = redacted_len;
      if (result_len != line_len || memcmp(result, line_start, line_len) != 0) {
        lines_modified++;
      }
    } else {
      result = line_start;
      result_len = line_len;
    }

    /* Ensure output buffer has space */
    while (out_pos + result_len + 2 > out_capacity) {
      out_capacity *= 2;
      char *new_out = realloc(output, out_capacity);
      if (!new_out) {
        free(output);
        free(text_buf);
        if (redacted)
          free(redacted);
        send_json_error(fd, 500, "Internal Server Error",
                        "Memory allocation failed", keep_alive);
        atomic_fetch_add(&g_requests_err, 1);
        return;
      }
      output = new_out;
    }

    memcpy(output + out_pos, result, result_len);
    out_pos += result_len;

    /* Add newline between lines (not after last) */
    if (line_end < text_end) {
      output[out_pos++] = '\n';
    }

    if (redacted)
      free(redacted);

    lines_processed++;
    line_start = line_end < text_end ? line_end + 1 : text_end;
  }

  output[out_pos] = '\0';

  clock_gettime(CLOCK_MONOTONIC, &t_end);
  double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                      (t_end.tv_nsec - t_start.tv_nsec) / 1e6;

  int patterns_matched = count_patterns(output, out_pos);

  /* JSON-escape the output */
  size_t escaped_len;
  char *escaped = json_escape(output, out_pos, &escaped_len);
  free(output);
  free(text_buf);

  if (!escaped) {
    send_json_error(fd, 500, "Internal Server Error", "JSON escape failed",
                    keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }

  /* Build JSON response */
  size_t resp_size = escaped_len + 256;
  char *resp = malloc(resp_size);
  if (!resp) {
    free(escaped);
    send_json_error(fd, 500, "Internal Server Error",
                    "Memory allocation failed", keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }

  int resp_len = snprintf(resp, resp_size,
                          "{\"redacted\":\"%s\","
                          "\"stats\":{"
                          "\"lines_processed\":%d,"
                          "\"lines_modified\":%d,"
                          "\"patterns_matched\":%d,"
                          "\"processing_time_ms\":%.3f"
                          "}}",
                          escaped, lines_processed, lines_modified,
                          patterns_matched, elapsed_ms);

  free(escaped);

  send_response(fd, 200, "OK", "application/json", resp, (size_t)resp_len,
                keep_alive);
  free(resp);

  atomic_fetch_add(&g_requests_ok, 1);
  atomic_fetch_add(&g_bytes_processed, (uint_least64_t)unescaped_len);
}

/* Handle GET /health */
static void handle_health(int fd, libplumbr_t *plumbr, bool keep_alive) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  double uptime = (now.tv_sec - g_start_time.tv_sec) +
                  (now.tv_nsec - g_start_time.tv_nsec) / 1e9;

  uint64_t total = atomic_load(&g_requests_total);
  uint64_t ok = atomic_load(&g_requests_ok);
  uint64_t err = atomic_load(&g_requests_err);
  uint64_t bytes = atomic_load(&g_bytes_processed);

  size_t pattern_count = libplumbr_pattern_count(plumbr);
  const char *version = libplumbr_version();

  char body[1024];
  int blen =
      snprintf(body, sizeof(body),
               "{\"status\":\"healthy\","
               "\"version\":\"%s\","
               "\"server_version\":\"%s\","
               "\"uptime_seconds\":%.1f,"
               "\"patterns_loaded\":%zu,"
               "\"stats\":{"
               "\"requests_total\":%lu,"
               "\"requests_ok\":%lu,"
               "\"requests_error\":%lu,"
               "\"bytes_processed\":%lu,"
               "\"avg_rps\":%.1f"
               "}}",
               version, SERVER_VERSION, uptime, pattern_count,
               (unsigned long)total, (unsigned long)ok, (unsigned long)err,
               (unsigned long)bytes, uptime > 0 ? total / uptime : 0.0);

  send_response(fd, 200, "OK", "application/json", body, (size_t)blen,
                keep_alive);
}

/* Handle POST /api/redact/batch */
static void handle_redact_batch(int fd, libplumbr_t *plumbr, const char *body,
                                size_t body_len, bool keep_alive) {
  struct timespec t_start, t_end;
  clock_gettime(CLOCK_MONOTONIC, &t_start);

  /* Find "texts" array in JSON: {"texts": ["...", "..."]} */
  const char *p = body;
  const char *end = body + body_len;

  /* Find "texts" key */
  const char *key = NULL;
  while (p + 6 < end) {
    if (*p == '"' && p[1] == 't' && p[2] == 'e' && p[3] == 'x' && p[4] == 't' &&
        p[5] == 's' && p[6] == '"') {
      key = p + 7;
      break;
    }
    p++;
  }
  if (!key) {
    send_json_error(fd, 400, "Bad Request", "Missing 'texts' array field",
                    keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }

  /* Skip to array start */
  while (key < end && (*key == ' ' || *key == ':' || *key == '\t'))
    key++;
  if (key >= end || *key != '[') {
    send_json_error(fd, 400, "Bad Request", "'texts' must be an array",
                    keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }
  key++; /* skip '[' */

  /* Build response incrementally */
  size_t resp_capacity = body_len * 2 + 1024;
  char *resp = malloc(resp_capacity);
  if (!resp) {
    send_json_error(fd, 500, "Internal Server Error",
                    "Memory allocation failed", keep_alive);
    atomic_fetch_add(&g_requests_err, 1);
    return;
  }

  size_t resp_pos = 0;
  resp_pos += (size_t)snprintf(resp + resp_pos, resp_capacity - resp_pos,
                               "{\"results\":[");

  int total_items = 0;
  int total_lines = 0;
  int total_modified = 0;
  int total_patterns = 0;
  size_t total_bytes = 0;

  /* Parse each string in the array */
  p = key;
  while (p < end) {
    /* Skip whitespace, commas */
    while (p < end &&
           (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r'))
      p++;
    if (p >= end || *p == ']')
      break;
    if (*p != '"') {
      p++;
      continue;
    }
    p++; /* skip opening quote */

    /* Find end of string */
    const char *str_start = p;
    while (p < end) {
      if (*p == '\\') {
        p += 2;
        continue;
      }
      if (*p == '"')
        break;
      p++;
    }
    if (p >= end)
      break;

    size_t str_len = (size_t)(p - str_start);
    p++; /* skip closing quote */

    /* Copy and unescape */
    char *text_buf = malloc(str_len + 1);
    if (!text_buf)
      continue;
    memcpy(text_buf, str_start, str_len);
    text_buf[str_len] = '\0';
    size_t unescaped_len = json_unescape(text_buf, str_len);

    total_bytes += unescaped_len;

    /* Process line-by-line */
    size_t out_cap = unescaped_len * 2 + 128;
    char *out = malloc(out_cap);
    if (!out) {
      free(text_buf);
      continue;
    }
    size_t out_pos = 0;
    int lines = 0, modified = 0;

    const char *ls = text_buf;
    const char *te = text_buf + unescaped_len;
    while (ls < te) {
      const char *le = memchr(ls, '\n', (size_t)(te - ls));
      size_t ll = le ? (size_t)(le - ls) : (size_t)(te - ls);
      if (!le)
        le = te;

      size_t rl;
      char *red = libplumbr_redact(plumbr, ls, ll, &rl);
      const char *res = red ? red : ls;
      size_t res_len = red ? rl : ll;
      if (red && (rl != ll || memcmp(red, ls, ll) != 0))
        modified++;

      while (out_pos + res_len + 2 > out_cap) {
        out_cap *= 2;
        char *new_out = realloc(out, out_cap);
        if (!new_out) {
          free(out);
          out = NULL;
          break;
        }
        out = new_out;
      }
      if (!out) {
        if (red)
          free(red);
        break;
      }

      memcpy(out + out_pos, res, res_len);
      out_pos += res_len;
      if (le < te)
        out[out_pos++] = '\n';

      if (red)
        free(red);
      lines++;
      ls = le < te ? le + 1 : te;
    }

    if (out) {
      out[out_pos] = '\0';
      int pm = count_patterns(out, out_pos);

      /* JSON-escape output */
      size_t esc_len;
      char *esc = json_escape(out, out_pos, &esc_len);
      free(out);

      if (esc) {
        /* Ensure resp has space */
        size_t needed = esc_len + 256;
        while (resp_pos + needed > resp_capacity) {
          resp_capacity *= 2;
          char *new_resp = realloc(resp, resp_capacity);
          if (!new_resp) {
            free(esc);
            free(text_buf);
            free(resp);
            send_json_error(fd, 500, "Internal Server Error",
                            "Memory allocation failed", keep_alive);
            atomic_fetch_add(&g_requests_err, 1);
            return;
          }
          resp = new_resp;
        }

        if (total_items > 0)
          resp[resp_pos++] = ',';
        resp_pos += (size_t)snprintf(resp + resp_pos, resp_capacity - resp_pos,
                                     "{\"redacted\":\"%s\","
                                     "\"lines_processed\":%d,"
                                     "\"lines_modified\":%d,"
                                     "\"patterns_matched\":%d}",
                                     esc, lines, modified, pm);
        free(esc);

        total_items++;
        total_lines += lines;
        total_modified += modified;
        total_patterns += pm;
      }
    }
    free(text_buf);
  }

  clock_gettime(CLOCK_MONOTONIC, &t_end);
  double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                      (t_end.tv_nsec - t_start.tv_nsec) / 1e6;

  /* Close response JSON */
  size_t needed = 256;
  while (resp_pos + needed > resp_capacity) {
    resp_capacity *= 2;
    char *new_resp = realloc(resp, resp_capacity);
    if (!new_resp) {
      free(resp);
      send_json_error(fd, 500, "Internal Server Error",
                      "Memory allocation failed", keep_alive);
      atomic_fetch_add(&g_requests_err, 1);
      return;
    }
    resp = new_resp;
  }

  resp_pos += (size_t)snprintf(resp + resp_pos, resp_capacity - resp_pos,
                               "],\"stats\":{"
                               "\"items_processed\":%d,"
                               "\"total_lines\":%d,"
                               "\"total_modified\":%d,"
                               "\"total_patterns_matched\":%d,"
                               "\"processing_time_ms\":%.3f"
                               "}}",
                               total_items, total_lines, total_modified,
                               total_patterns, elapsed_ms);

  send_response(fd, 200, "OK", "application/json", resp, resp_pos, keep_alive);
  free(resp);

  atomic_fetch_add(&g_requests_ok, 1);
  atomic_fetch_add(&g_bytes_processed, (uint_least64_t)total_bytes);
}

/* ─── Connection handler ───────────────────────────────────── */

/* Read a full HTTP request, returns total bytes consumed or -1 */
static ssize_t read_full_request(int fd, char *buf, size_t buf_size) {
  size_t total = 0;
  int header_done = 0;
  int content_length = 0;
  size_t header_end_offset = 0;

  /* Set read timeout */
  struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  while (total < buf_size) {
    ssize_t n = read(fd, buf + total, buf_size - total);
    if (n <= 0) {
      if (n == 0)
        return total > 0 ? (ssize_t)total : -1; /* Connection closed */
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (total > 0)
          return (ssize_t)total;
        return -1; /* Timeout */
      }
      return -1; /* Error */
    }
    total += (size_t)n;

    if (!header_done) {
      const char *hend = find_header_end(buf, total);
      if (hend) {
        header_done = 1;
        header_end_offset = (size_t)(hend - buf);
        content_length = parse_content_length(buf, header_end_offset);
        if (content_length < 0)
          content_length = 0;

        /* Check if we already have the full body */
        size_t expected = header_end_offset + (size_t)content_length;
        if (total >= expected) {
          return (ssize_t)expected;
        }
      }
    } else {
      size_t expected = header_end_offset + (size_t)content_length;
      if (total >= expected) {
        return (ssize_t)expected;
      }
    }
  }
  return (ssize_t)total;
}

static void handle_connection(int fd, libplumbr_t *plumbr) {
  char *buf = malloc(READ_BUF_SIZE);
  if (!buf) {
    close(fd);
    return;
  }

  /* Enable TCP_NODELAY for low latency */
  int flag = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

  bool keep_alive = true;

  while (keep_alive && g_running) {
    ssize_t req_len = read_full_request(fd, buf, READ_BUF_SIZE);
    if (req_len <= 0)
      break;

    atomic_fetch_add(&g_requests_total, 1);

    /* Parse request line */
    char method[16] = {0};
    char path[256] = {0};
    if (!parse_request_line(buf, method, sizeof(method), path, sizeof(path))) {
      send_json_error(fd, 400, "Bad Request", "Invalid request line", false);
      atomic_fetch_add(&g_requests_err, 1);
      break;
    }

    /* Find header end */
    const char *body_start = find_header_end(buf, (size_t)req_len);
    size_t body_len = 0;
    if (body_start) {
      size_t header_len = (size_t)(body_start - buf);
      body_len = (size_t)req_len - header_len;
      keep_alive = is_keep_alive(buf, header_len);
    }

    /* Route request */
    if (strcmp(method, "OPTIONS") == 0) {
      send_options_response(fd);
    } else if (strcmp(method, "POST") == 0 &&
               strcmp(path, "/api/redact") == 0) {
      if (!body_start || body_len == 0) {
        send_json_error(fd, 400, "Bad Request", "Missing request body",
                        keep_alive);
        atomic_fetch_add(&g_requests_err, 1);
      } else {
        handle_redact(fd, plumbr, body_start, body_len, keep_alive);
      }
    } else if (strcmp(method, "POST") == 0 &&
               strcmp(path, "/api/redact/batch") == 0) {
      if (!body_start || body_len == 0) {
        send_json_error(fd, 400, "Bad Request", "Missing request body",
                        keep_alive);
        atomic_fetch_add(&g_requests_err, 1);
      } else {
        handle_redact_batch(fd, plumbr, body_start, body_len, keep_alive);
      }
    } else if (strcmp(method, "GET") == 0 &&
               (strcmp(path, "/health") == 0 ||
                strcmp(path, "/api/health") == 0)) {
      handle_health(fd, plumbr, keep_alive);
      atomic_fetch_add(&g_requests_ok, 1);
    } else {
      send_json_error(fd, 404, "Not Found", "Not found", keep_alive);
      atomic_fetch_add(&g_requests_err, 1);
    }
  }

  free(buf);
  close(fd);
}

/* ─── Worker thread ────────────────────────────────────────── */

typedef struct {
  int id;
  WorkQueue *queue;
  libplumbr_config_t config;
} WorkerArg;

static void *worker_thread(void *arg) {
  WorkerArg *wa = (WorkerArg *)arg;

  /* Each worker creates its own plumbr instance */
  libplumbr_t *plumbr = libplumbr_new(&wa->config);
  if (!plumbr) {
    fprintf(stderr, "[worker %d] FATAL: Failed to create plumbr instance\n",
            wa->id);
    return NULL;
  }

  fprintf(stderr, "[worker %d] Ready (%zu patterns loaded)\n", wa->id,
          libplumbr_pattern_count(plumbr));

  while (g_running) {
    int fd = queue_pop(wa->queue);
    if (fd < 0)
      break;

    handle_connection(fd, plumbr);
  }

  libplumbr_free(plumbr);
  return NULL;
}

/* ─── Signal handler ───────────────────────────────────────── */

static void signal_handler(int sig) {
  (void)sig;
  g_running = 0;
}

/* ─── Main ─────────────────────────────────────────────────── */

static void print_usage(const char *prog) {
  fprintf(stderr,
          "PlumbrC HTTP Server v%s\n\n"
          "Usage: %s [OPTIONS]\n\n"
          "Options:\n"
          "  --port PORT         Listen port (default: 8080)\n"
          "  --host ADDR         Bind address (default: 0.0.0.0)\n"
          "  --threads N         Worker threads (0=auto, default: 0)\n"
          "  --pattern-dir DIR   Load patterns from directory\n"
          "  --pattern-file FILE Load patterns from file\n"
          "  --help              Show this help\n"
          "\n"
          "API:\n"
          "  POST /api/redact       Redact text (JSON: {\"text\": \"...\"})\n"
          "  POST /api/redact/batch Batch redact (JSON: {\"texts\": [...]})\n"
          "  GET  /health           Health check + stats\n",
          SERVER_VERSION, prog);
}

int main(int argc, char *argv[]) {
  int port = 8080;
  const char *host = "0.0.0.0";
  int num_threads = 0; /* auto */
  const char *pattern_dir = NULL;
  const char *pattern_file = NULL;

  static struct option long_options[] = {
      {"port", required_argument, 0, 'p'},
      {"host", required_argument, 0, 'H'},
      {"threads", required_argument, 0, 't'},
      {"pattern-dir", required_argument, 0, 'd'},
      {"pattern-file", required_argument, 0, 'f'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "p:H:t:d:f:h", long_options, NULL)) !=
         -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    case 'H':
      host = optarg;
      break;
    case 't':
      num_threads = atoi(optarg);
      break;
    case 'd':
      pattern_dir = optarg;
      break;
    case 'f':
      pattern_file = optarg;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  /* Auto-detect thread count */
  if (num_threads <= 0) {
    num_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_threads < 1)
      num_threads = 1;
    if (num_threads > 32)
      num_threads = 32;
  }

  /* Setup signals */
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Record start time */
  clock_gettime(CLOCK_MONOTONIC, &g_start_time);

  fprintf(stderr,
          "\n"
          "╔══════════════════════════════════════════╗\n"
          "║     PlumbrC HTTP Server v%s          ║\n"
          "╠══════════════════════════════════════════╣\n"
          "║  Port:    %-29d ║\n"
          "║  Host:    %-29s ║\n"
          "║  Workers: %-29d ║\n"
          "╚══════════════════════════════════════════╝\n\n",
          SERVER_VERSION, port, host, num_threads);

  /* Create work queue */
  WorkQueue queue;
  queue_init(&queue);

  /* Prepare plumbr config */
  libplumbr_config_t plumbr_config = {
      .pattern_file = pattern_file,
      .pattern_dir = pattern_dir,
      .num_threads = 1,
      .quiet = 1,
  };

  /* Start worker threads */
  pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
  WorkerArg *args = calloc((size_t)num_threads, sizeof(WorkerArg));

  for (int i = 0; i < num_threads; i++) {
    args[i].id = i;
    args[i].queue = &queue;
    args[i].config = plumbr_config;
    pthread_create(&threads[i], NULL, worker_thread, &args[i]);
  }

  /* Create listening socket */
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    return 1;
  }

  /* Socket options */
  int reuse = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

  /* Set non-blocking for epoll accept loop */
  int flags = fcntl(listen_fd, F_GETFL, 0);
  fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons((uint16_t)port),
  };
  inet_pton(AF_INET, host, &addr.sin_addr);

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(listen_fd);
    return 1;
  }

  if (listen(listen_fd, 1024) < 0) {
    perror("listen");
    close(listen_fd);
    return 1;
  }

  fprintf(stderr, "Listening on http://%s:%d\n", host, port);
  fprintf(stderr, "Press Ctrl+C to stop\n\n");

  /* Create epoll instance */
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    perror("epoll_create1");
    close(listen_fd);
    return 1;
  }

  struct epoll_event ev = {.events = EPOLLIN, .data.fd = listen_fd};
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

  struct epoll_event events[MAX_EVENTS];

  /* Accept loop */
  while (g_running) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
    if (nfds < 0) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait");
      break;
    }

    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == listen_fd) {
        /* Accept new connections */
        while (1) {
          struct sockaddr_in client_addr;
          socklen_t client_len = sizeof(client_addr);
          int client_fd =
              accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
          if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            continue;
          }
          queue_push(&queue, client_fd);
        }
      }
    }
  }

  /* Shutdown */
  fprintf(stderr, "\nShutting down...\n");

  close(listen_fd);
  close(epoll_fd);

  /* Wake up all workers */
  queue_wake_all(&queue);

  /* Wait for workers */
  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  /* Print final stats */
  struct timespec end_time;
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double uptime = (end_time.tv_sec - g_start_time.tv_sec) +
                  (end_time.tv_nsec - g_start_time.tv_nsec) / 1e9;

  uint64_t total = atomic_load(&g_requests_total);
  uint64_t ok = atomic_load(&g_requests_ok);
  uint64_t err = atomic_load(&g_requests_err);
  uint64_t bytes = atomic_load(&g_bytes_processed);

  fprintf(stderr,
          "\n=== Server Statistics ===\n"
          "Uptime:           %.1f seconds\n"
          "Total requests:   %lu\n"
          "Successful:       %lu\n"
          "Errors:           %lu\n"
          "Bytes processed:  %lu\n"
          "Avg RPS:          %.1f\n"
          "=========================\n",
          uptime, (unsigned long)total, (unsigned long)ok, (unsigned long)err,
          (unsigned long)bytes, uptime > 0 ? total / uptime : 0.0);

  free(threads);
  free(args);

  return 0;
}
