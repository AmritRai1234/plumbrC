/*
 * PlumbrC Library Usage Example
 *
 * Compile:
 *   gcc -I../../include example.c -L../../build/lib -lplumbr -lpcre2-8 -o
 * example
 *
 * Or with shared library:
 *   gcc -I../../include example.c -L../../build/lib -lplumbr -lpcre2-8
 * -Wl,-rpath,../../build/lib -o example
 */

#include "libplumbr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  printf("PlumbrC Library Example\n");
  printf("Version: %s\n\n", libplumbr_version());

  /* Create PlumbrC instance with default patterns */
  libplumbr_t *p = libplumbr_new(NULL);
  if (!p) {
    fprintf(stderr, "Failed to create PlumbrC instance\n");
    return 1;
  }

  printf("Loaded %zu patterns\n\n", libplumbr_pattern_count(p));

  /* Test lines to redact */
  const char *test_lines[] = {
      "User login with api_key=sk-proj-abc123def456xyz789",
      "AWS access: AKIAIOSFODNN7EXAMPLE",
      "Database: postgres://user:password123@localhost:5432/db",
      "Normal log line with no secrets",
      "GitHub token: ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
      "Email sent to user@example.com",
      "Payment card: 4111-1111-1111-1111",
  };
  size_t num_lines = sizeof(test_lines) / sizeof(test_lines[0]);

  printf("Redacting %zu lines:\n", num_lines);
  printf("========================================\n\n");

  for (size_t i = 0; i < num_lines; i++) {
    size_t out_len;
    char *redacted =
        libplumbr_redact(p, test_lines[i], strlen(test_lines[i]), &out_len);

    if (redacted) {
      printf("Input:  %s\n", test_lines[i]);
      printf("Output: %s\n\n", redacted);
      free(redacted);
    }
  }

  /* Print stats */
  libplumbr_stats_t stats = libplumbr_get_stats(p);
  printf("========================================\n");
  printf("Statistics:\n");
  printf("  Lines processed: %zu\n", stats.lines_processed);
  printf("  Lines modified:  %zu\n", stats.lines_modified);
  printf("  Patterns matched: %zu\n", stats.patterns_matched);

  /* Cleanup */
  libplumbr_free(p);

  return 0;
}
