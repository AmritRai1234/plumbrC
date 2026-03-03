/*
 * PlumbrC - Fuzz Harness: Redactor Core
 *
 * Targets: redactor_process, verify_ac_matches, redactor_apply
 * Tests the Aho-Corasick + PCRE2 two-phase pipeline with adversarial inputs.
 *
 * Build: clang -g -O1 -fsanitize=fuzzer,address tests/fuzz_redactor.c
 *        src/*.c -Iinclude -lpcre2-8 -lpthread -o build/bin/fuzz_redactor
 *
 * Run:   ./build/bin/fuzz_redactor -max_len=65536 -timeout=10
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "patterns.h"
#include "redactor.h"

static Arena g_arena;
static PatternSet *g_patterns = NULL;
static Redactor *g_redactor = NULL;

static void init_once(void) {
  if (g_redactor)
    return;

  if (!arena_init(&g_arena, 64 * 1024 * 1024))
    return;

  g_patterns = patterns_create(&g_arena, 64);
  if (!g_patterns)
    return;

  /* Add patterns that exercise different regex features */
  patterns_add(g_patterns, "aws_key", "AKIA", "AKIA[0-9A-Z]{16}",
               "[REDACTED:aws_access_key]");
  patterns_add(g_patterns, "password", "password", "password\\s*=\\s*[^\\s]+",
               "[REDACTED:password]");
  patterns_add(g_patterns, "email", "@",
               "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}",
               "[REDACTED:email]");
  patterns_add(g_patterns, "ipv4", ".", "\\b([0-9]{1,3}\\.){3}[0-9]{1,3}\\b",
               "[REDACTED:ipv4]");
  patterns_add(g_patterns, "jwt", "eyJ", "eyJ[A-Za-z0-9_-]{10,}",
               "[REDACTED:jwt]");
  patterns_add(g_patterns, "github_pat", "ghp_", "ghp_[A-Za-z0-9]{36}",
               "[REDACTED:github_pat]");

  if (!patterns_build(g_patterns))
    return;

  g_redactor = redactor_create(&g_arena, g_patterns, 128 * 1024);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  init_once();
  if (!g_redactor)
    return 0;

  /* Cap to PLUMBR_MAX_LINE_SIZE */
  if (size > 65536)
    size = 65536;

  size_t out_len;
  const char *output =
      redactor_process(g_redactor, (const char *)data, size, &out_len);
  (void)output;

  /* Also test with known pattern prefixes embedded in fuzz data */
  if (size > 20) {
    /* Inject a partial AWS key to exercise AC matching + PCRE2 verify */
    char *mutated = malloc(size + 20);
    if (mutated) {
      memcpy(mutated, "AKIA", 4);
      memcpy(mutated + 4, data, size);
      const char *out2 =
          redactor_process(g_redactor, mutated, size + 4, &out_len);
      (void)out2;
      free(mutated);
    }
  }

  return 0;
}
