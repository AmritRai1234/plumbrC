/*
 * PlumbrC - Fuzz Harness: HTTP Parser
 *
 * Targets: parse_content_length, parse_request_line, find_header_end,
 *          is_keep_alive, extract_json_text
 * These are static functions in server.c, so we duplicate the minimal
 * parsing logic here for fuzz testing.
 *
 * Build: clang -g -O1 -fsanitize=fuzzer,address tests/fuzz_http.c \
 *        -o build/bin/fuzz_http
 *
 * Run:   ./build/bin/fuzz_http -max_len=8192 -timeout=5
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Duplicated from server.c (static functions) ─── */

#define MAX_BODY_SIZE (1024 * 1024)

static const char *find_header_end(const char *buf, size_t len) {
  for (size_t i = 0; i + 3 < len; i++) {
    if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' &&
        buf[i + 3] == '\n') {
      return buf + i + 4;
    }
  }
  return NULL;
}

static int parse_content_length(const char *headers, size_t header_len) {
  const char *p = headers;
  const char *end = headers + header_len;

  while (p < end) {
    if ((end - p > 15) && (p[0] == 'C' || p[0] == 'c') &&
        (p[1] == 'o' || p[1] == 'O') && (p[2] == 'n' || p[2] == 'N') &&
        (p[3] == 't' || p[3] == 'T') && (p[4] == 'e' || p[4] == 'E') &&
        (p[5] == 'n' || p[5] == 'N') && (p[6] == 't' || p[6] == 'T') &&
        p[7] == '-' && (p[8] == 'L' || p[8] == 'l') &&
        (p[9] == 'e' || p[9] == 'E') && (p[10] == 'n' || p[10] == 'N') &&
        (p[11] == 'g' || p[11] == 'G') && (p[12] == 't' || p[12] == 'T') &&
        (p[13] == 'h' || p[13] == 'H') && p[14] == ':') {
      const char *val = p + 15;
      while (val < end && *val == ' ')
        val++;
      char *endptr;
      long cl = strtol(val, &endptr, 10);
      /* SECURITY FIX #6: Added INT_MAX check to prevent truncation */
      if (endptr == val || cl < 0 || cl > MAX_BODY_SIZE || cl > INT_MAX) {
        return -1;
      }
      return (int)cl;
    }
    while (p < end && *p != '\n')
      p++;
    if (p < end)
      p++;
  }
  return -1;
}

static bool parse_request_line(const char *buf, char *method, size_t mlen,
                               char *path, size_t plen) {
  const char *sp1 = memchr(buf, ' ', 32);
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

static bool is_keep_alive(const char *headers, size_t header_len) {
  if (header_len > 8 && memmem(headers, header_len, "HTTP/1.1", 8) != NULL)
    return true;
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
      if ((size_t)(end - val) >= 10 && (strncmp(val, "keep-alive", 10) == 0 ||
                                        strncmp(val, "Keep-Alive", 10) == 0 ||
                                        strncmp(val, "Keep-alive", 10) == 0))
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

static bool extract_json_text(const char *json, size_t json_len,
                              const char **text_start, size_t *text_len) {
  const char *p = json;
  const char *end = json + json_len;

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

  while (key < end && (*key == ' ' || *key == ':' || *key == '\t'))
    key++;

  if (key >= end || *key != '"')
    return false;
  key++;

  const char *val_start = key;
  while (key < end) {
    if (*key == '\\') {
      /* SECURITY FIX #7: return false instead of break */
      if (key + 1 >= end)
        return false;
      key += 2;
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

/* ─── Fuzz entry point ─── */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0 || size > 8192)
    return 0;

  const char *buf = (const char *)data;

  /* Fuzz find_header_end */
  (void)find_header_end(buf, size);

  /* Fuzz parse_content_length */
  (void)parse_content_length(buf, size);

  /* Fuzz is_keep_alive */
  (void)is_keep_alive(buf, size);

  /* Fuzz parse_request_line (needs writable buffers) */
  char method[32], path[512];
  (void)parse_request_line(buf, method, sizeof(method), path, sizeof(path));

  /* Fuzz extract_json_text */
  const char *text_start;
  size_t text_len;
  (void)extract_json_text(buf, size, &text_start, &text_len);

  return 0;
}
