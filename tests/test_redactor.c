/*
 * PlumbrC - Unit Tests for Redactor
 *
 * Tests the redaction functionality with known inputs and expected outputs.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "patterns.h"
#include "plumbr.h"
#include "redactor.h"

/* Test helpers */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    printf("  Testing %s... ", #name);                                         \
    fflush(stdout);                                                            \
    test_##name();                                                             \
    printf("PASS\n");                                                          \
  } while (0)

#define ASSERT_STR_EQ(expected, actual)                                        \
  do {                                                                         \
    if (strcmp(expected, actual) != 0) {                                       \
      fprintf(stderr, "\nFAIL: Expected '%s', got '%s'\n", expected, actual);  \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(expected, actual)                                            \
  do {                                                                         \
    if ((expected) != (actual)) {                                              \
      fprintf(stderr, "\nFAIL: Expected %zu, got %zu\n", (size_t)(expected),   \
              (size_t)(actual));                                               \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

/* Test fixtures */
static Arena test_arena;
static PatternSet *test_patterns;
static Redactor *test_redactor;

static void setup(void) {
  assert(arena_init(&test_arena, 16 * 1024 * 1024));
  test_patterns = patterns_create(&test_arena, 64);
  assert(test_patterns != NULL);

  /* Add test patterns */
  patterns_add(test_patterns, "aws_key", "AKIA", "AKIA[0-9A-Z]{16}",
               "[REDACTED:aws]");
  patterns_add(test_patterns, "password", "password",
               "password\\s*=\\s*[^\\s]+", "[REDACTED:pwd]");
  patterns_add(test_patterns, "email", "@",
               "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}",
               "[REDACTED:email]");

  assert(patterns_build(test_patterns));

  test_redactor = redactor_create(&test_arena, test_patterns, 4096);
  assert(test_redactor != NULL);
}

static void teardown(void) {
  patterns_destroy(test_patterns);
  arena_destroy(&test_arena);
}

/* Tests */
TEST(no_match) {
  const char *input = "This is a normal log line with no secrets";
  size_t out_len;
  const char *output =
      redactor_process(test_redactor, input, strlen(input), &out_len);

  /* Should return input unchanged */
  ASSERT_STR_EQ(input, output);
  ASSERT_EQ(strlen(input), out_len);
}

TEST(single_aws_key) {
  const char *input = "Found key: AKIAIOSFODNN7EXAMPLE";
  size_t out_len;
  const char *output =
      redactor_process(test_redactor, input, strlen(input), &out_len);

  /* AWS key should be redacted */
  ASSERT_STR_EQ("Found key: [REDACTED:aws]", output);
}

TEST(single_password) {
  const char *input = "Config: password = secret123";
  size_t out_len;
  const char *output =
      redactor_process(test_redactor, input, strlen(input), &out_len);

  /* Password should be redacted */
  ASSERT_STR_EQ("Config: [REDACTED:pwd]", output);
}

TEST(single_email) {
  const char *input = "Contact: user@example.com for support";
  size_t out_len;
  const char *output =
      redactor_process(test_redactor, input, strlen(input), &out_len);

  /* Email should be redacted */
  ASSERT_STR_EQ("Contact: [REDACTED:email] for support", output);
}

TEST(multiple_patterns) {
  const char *input = "Key: AKIAIOSFODNN7EXAMPLE email: admin@company.org";
  size_t out_len;
  const char *output =
      redactor_process(test_redactor, input, strlen(input), &out_len);

  /* Both should be redacted */
  ASSERT_STR_EQ("Key: [REDACTED:aws] email: [REDACTED:email]", output);
}

TEST(empty_line) {
  const char *input = "";
  size_t out_len;
  const char *output =
      redactor_process(test_redactor, input, strlen(input), &out_len);

  ASSERT_EQ(0, out_len);
}

TEST(stats) {
  /* Reset stats */
  redactor_reset_stats(test_redactor);

  /* Process some lines */
  size_t out_len;
  redactor_process(test_redactor, "normal line", 11, &out_len);
  redactor_process(test_redactor, "AKIAIOSFODNN7EXAMPLE", 20, &out_len);
  redactor_process(test_redactor, "another normal", 14, &out_len);

  ASSERT_EQ(3, redactor_lines_scanned(test_redactor));
  ASSERT_EQ(1, redactor_lines_modified(test_redactor));
}

int main(void) {
  printf("Running redactor tests...\n");

  setup();

  RUN_TEST(no_match);
  RUN_TEST(single_aws_key);
  RUN_TEST(single_password);
  RUN_TEST(single_email);
  RUN_TEST(multiple_patterns);
  RUN_TEST(empty_line);
  RUN_TEST(stats);

  teardown();

  printf("\nAll tests passed!\n");
  return 0;
}
