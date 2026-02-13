/*
 * PlumbrC - Unit Tests for Library API (libplumbr)
 *
 * Tests the public library API for embedding redaction.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libplumbr.h"

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

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "\nFAIL: %s is false\n", #cond);                         \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

/* Tests */

TEST(new_default) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);
  ASSERT_TRUE(libplumbr_pattern_count(p) >= 10);
  libplumbr_free(p);
}

TEST(redact_aws_key) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  const char *input = "key=AKIAIOSFODNN7EXAMPLE";
  size_t out_len;
  char *output = libplumbr_redact(p, input, strlen(input), &out_len);

  ASSERT_TRUE(output != NULL);
  ASSERT_TRUE(out_len > 0);
  /* Should be redacted - output should differ from input */
  ASSERT_TRUE(strstr(output, "AKIAIOSFODNN7EXAMPLE") == NULL);

  free(output);
  libplumbr_free(p);
}

TEST(redact_no_match) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  const char *input = "This is a normal log line";
  size_t out_len;
  char *output = libplumbr_redact(p, input, strlen(input), &out_len);

  ASSERT_TRUE(output != NULL);
  ASSERT_STR_EQ(input, output);
  ASSERT_EQ(strlen(input), out_len);

  free(output);
  libplumbr_free(p);
}

TEST(redact_inplace) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  char buffer[256];
  const char *input = "key=AKIAIOSFODNN7EXAMPLE";
  strcpy(buffer, input);

  int result =
      libplumbr_redact_inplace(p, buffer, strlen(input), sizeof(buffer));
  ASSERT_TRUE(result >= 0);
  /* Should be redacted */
  ASSERT_TRUE(strstr(buffer, "AKIAIOSFODNN7EXAMPLE") == NULL);

  libplumbr_free(p);
}

TEST(redact_inplace_no_match) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  char buffer[256];
  const char *input = "just a normal line";
  strcpy(buffer, input);

  int result =
      libplumbr_redact_inplace(p, buffer, strlen(input), sizeof(buffer));
  ASSERT_TRUE(result >= 0);
  ASSERT_STR_EQ(input, buffer);

  libplumbr_free(p);
}

TEST(redact_batch) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  const char *inputs[] = {"normal line", "key=AKIAIOSFODNN7EXAMPLE",
                          "another normal"};
  size_t lens[] = {11, 24, 14};
  char *outputs[3];
  size_t out_lens[3];

  int result = libplumbr_redact_batch(p, inputs, lens, outputs, out_lens, 3);
  ASSERT_EQ(3, result);

  /* First and third should be unchanged */
  ASSERT_STR_EQ("normal line", outputs[0]);
  ASSERT_STR_EQ("another normal", outputs[2]);

  /* Second should be redacted */
  ASSERT_TRUE(strstr(outputs[1], "AKIAIOSFODNN7EXAMPLE") == NULL);

  for (int i = 0; i < 3; i++) {
    free(outputs[i]);
  }
  libplumbr_free(p);
}

TEST(stats) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  /* Process some lines */
  size_t out_len;
  char *o1 = libplumbr_redact(p, "normal", 6, &out_len);
  char *o2 = libplumbr_redact(p, "AKIAIOSFODNN7EXAMPLE", 20, &out_len);
  char *o3 = libplumbr_redact(p, "also normal", 11, &out_len);

  libplumbr_stats_t stats = libplumbr_get_stats(p);
  ASSERT_EQ(3, stats.lines_processed);
  ASSERT_TRUE(stats.lines_modified >= 1);

  free(o1);
  free(o2);
  free(o3);
  libplumbr_free(p);
}

TEST(null_safety) {
  /* NULL instance should not crash */
  char *result = libplumbr_redact(NULL, "test", 4, NULL);
  ASSERT_TRUE(result == NULL);

  int inplace = libplumbr_redact_inplace(NULL, NULL, 0, 0);
  ASSERT_TRUE(inplace == -1);

  ASSERT_EQ(0, libplumbr_pattern_count(NULL));

  /* Free NULL should not crash */
  libplumbr_free(NULL);
}

TEST(version) {
  const char *ver = libplumbr_version();
  ASSERT_TRUE(ver != NULL);
  ASSERT_TRUE(strlen(ver) > 0);
  /* Should be in format "X.Y.Z" */
  ASSERT_TRUE(strchr(ver, '.') != NULL);
}

TEST(empty_input) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  size_t out_len;
  char *output = libplumbr_redact(p, "", 0, &out_len);
  ASSERT_TRUE(output != NULL);
  ASSERT_EQ(0, out_len);

  free(output);
  libplumbr_free(p);
}

TEST(oversized_input) {
  libplumbr_t *p = libplumbr_new(NULL);
  ASSERT_TRUE(p != NULL);

  /* Input larger than PLUMBR_MAX_LINE_SIZE should be rejected */
  size_t huge = 128 * 1024; /* 128KB > 64KB max */
  char *big = malloc(huge);
  ASSERT_TRUE(big != NULL);
  memset(big, 'A', huge);

  size_t out_len;
  char *output = libplumbr_redact(p, big, huge, &out_len);
  ASSERT_TRUE(output == NULL); /* Should be rejected */

  free(big);
  libplumbr_free(p);
}

int main(void) {
  printf("Running libplumbr tests...\n");

  RUN_TEST(new_default);
  RUN_TEST(redact_aws_key);
  RUN_TEST(redact_no_match);
  RUN_TEST(redact_inplace);
  RUN_TEST(redact_inplace_no_match);
  RUN_TEST(redact_batch);
  RUN_TEST(stats);
  RUN_TEST(null_safety);
  RUN_TEST(version);
  RUN_TEST(empty_input);
  RUN_TEST(oversized_input);

  printf("\nAll tests passed!\n");
  return 0;
}
