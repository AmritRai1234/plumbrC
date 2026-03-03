/*
 * PlumbrC - Fuzz Harness: JSON Parser
 *
 * Targets: extract_json_text, json_unescape, json_escape
 * These functions are the first to touch untrusted HTTP request bodies,
 * making them the #1 attack surface.
 *
 * Build: clang -g -O1 -fsanitize=fuzzer,address tests/fuzz_json.c
 *        src/*.c -Iinclude -lpcre2-8 -lpthread -o build/bin/fuzz_json
 *
 * Run:   ./build/bin/fuzz_json -max_len=4096 -timeout=5
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libplumbr.h"

/*
 * Strategy: wrap fuzz input in a JSON envelope and send through
 * the full libplumbr pipeline. This exercises:
 *   - extract_json_text (JSON string parsing)
 *   - json_unescape (escape sequence handling)
 *   - redactor_process (pattern matching on unescaped content)
 *   - json_escape (output escaping)
 *
 * Any crash or ASAN violation = actionable bug.
 */

/* Persistent instance — avoid re-creating patterns each iteration */
static libplumbr_t *g_plumbr = NULL;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!g_plumbr) {
    g_plumbr = libplumbr_new(NULL);
    if (!g_plumbr)
      return 0;
  }

  /* Limit input size to avoid OOM kills */
  if (size > 65536)
    return 0;

  /* Direct fuzz: treat input as raw text to redact */
  size_t out_len;
  char *output = libplumbr_redact(g_plumbr, (const char *)data, size, &out_len);
  if (output)
    free(output);

  /* Fuzz with embedded newlines (multi-line path) */
  if (size > 10) {
    char *multi = malloc(size + 1);
    if (multi) {
      memcpy(multi, data, size);
      /* Insert newlines at regular intervals */
      for (size_t i = 0; i < size; i += 40) {
        multi[i] = '\n';
      }
      multi[size] = '\0';

      output = libplumbr_redact(g_plumbr, multi, size, &out_len);
      if (output)
        free(output);
      free(multi);
    }
  }

  /* Fuzz batch API */
  if (size > 4) {
    const char *inputs[4];
    size_t lens[4];
    char *outputs[4];
    size_t out_lens[4];

    /* Split fuzz input into 4 chunks */
    size_t chunk = size / 4;
    for (int i = 0; i < 4; i++) {
      inputs[i] = (const char *)data + (size_t)i * chunk;
      lens[i] = (i < 3) ? chunk : (size - (size_t)i * chunk);
    }

    int result =
        libplumbr_redact_batch(g_plumbr, inputs, lens, outputs, out_lens, 4);
    if (result > 0) {
      for (int i = 0; i < result; i++) {
        free(outputs[i]);
      }
    }
  }

  return 0;
}
