/*
 * PlumbrC - Unit Tests for Pattern Loading
 *
 * Tests pattern compilation and Aho-Corasick automaton.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aho_corasick.h"
#include "arena.h"
#include "patterns.h"

/* Test helpers */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    printf("  Testing %s... ", #name);                                         \
    fflush(stdout);                                                            \
    test_##name();                                                             \
    printf("PASS\n");                                                          \
  } while (0)

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "\nFAIL: %s is false\n", #cond);                         \
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

/* Tests */
TEST(arena_basic) {
  Arena arena;
  ASSERT_TRUE(arena_init(&arena, 1024 * 1024));

  void *p1 = arena_alloc(&arena, 100);
  ASSERT_TRUE(p1 != NULL);

  void *p2 = arena_alloc(&arena, 200);
  ASSERT_TRUE(p2 != NULL);
  ASSERT_TRUE(p2 != p1);

  ASSERT_TRUE(arena_used(&arena) >= 300);

  arena_reset(&arena);
  ASSERT_EQ(0, arena_used(&arena));

  arena_destroy(&arena);
}

TEST(ac_single_pattern) {
  Arena arena;
  ASSERT_TRUE(arena_init(&arena, 16 * 1024 * 1024));

  ACAutomaton *ac = ac_create(&arena);
  ASSERT_TRUE(ac != NULL);

  ASSERT_TRUE(ac_add_pattern(ac, "hello", 5, 0));
  ASSERT_TRUE(ac_build(ac));

  /* Should find "hello" */
  ACMatch match;
  ASSERT_TRUE(ac_search_first(ac, "say hello world", 15, &match));
  ASSERT_EQ(0, match.pattern_id);
  ASSERT_EQ(5, match.length);

  /* Should not find in different text */
  ASSERT_TRUE(!ac_search_first(ac, "goodbye world", 13, &match));

  arena_destroy(&arena);
}

TEST(ac_multiple_patterns) {
  Arena arena;
  ASSERT_TRUE(arena_init(&arena, 16 * 1024 * 1024));

  ACAutomaton *ac = ac_create(&arena);
  ASSERT_TRUE(ac_add_pattern(ac, "he", 2, 0));
  ASSERT_TRUE(ac_add_pattern(ac, "she", 3, 1));
  ASSERT_TRUE(ac_add_pattern(ac, "his", 3, 2));
  ASSERT_TRUE(ac_add_pattern(ac, "hers", 4, 3));
  ASSERT_TRUE(ac_build(ac));

  /* Find all matches */
  ACMatch matches[10];
  size_t count = ac_search_all(ac, "ushers", 6, matches, 10);

  /* Should find: "she" at 1, "he" at 2, "hers" at 2 */
  ASSERT_TRUE(count >= 2);

  arena_destroy(&arena);
}

TEST(pattern_create) {
  Arena arena;
  ASSERT_TRUE(arena_init(&arena, 16 * 1024 * 1024));

  PatternSet *ps = patterns_create(&arena, 32);
  ASSERT_TRUE(ps != NULL);
  ASSERT_EQ(0, patterns_count(ps));

  ASSERT_TRUE(patterns_add(ps, "test", "AKIA", "AKIA[0-9A-Z]{16}", NULL));
  ASSERT_EQ(1, patterns_count(ps));

  ASSERT_TRUE(patterns_build(ps));

  const Pattern *p = patterns_get(ps, 0);
  ASSERT_TRUE(p != NULL);
  ASSERT_TRUE(strcmp(p->name, "test") == 0);

  patterns_destroy(ps);
  arena_destroy(&arena);
}

TEST(pattern_defaults) {
  Arena arena;
  ASSERT_TRUE(arena_init(&arena, 16 * 1024 * 1024));

  PatternSet *ps = patterns_create(&arena, 64);
  ASSERT_TRUE(ps != NULL);

  ASSERT_TRUE(patterns_add_defaults(ps));
  ASSERT_TRUE(patterns_count(ps) >= 10); /* Should have many patterns */

  ASSERT_TRUE(patterns_build(ps));

  patterns_destroy(ps);
  arena_destroy(&arena);
}

TEST(literal_extraction) {
  char literal[256];

  /* Should extract "AKIA" from regex */
  ASSERT_TRUE(
      patterns_extract_literal("AKIA[0-9A-Z]{16}", literal, sizeof(literal)));
  ASSERT_TRUE(strcmp(literal, "AKIA") == 0);

  /* Should handle anchored pattern */
  ASSERT_TRUE(patterns_extract_literal("^hello", literal, sizeof(literal)));
  ASSERT_TRUE(strcmp(literal, "hello") == 0);
}

int main(void) {
  printf("Running pattern tests...\n");

  RUN_TEST(arena_basic);
  RUN_TEST(ac_single_pattern);
  RUN_TEST(ac_multiple_patterns);
  RUN_TEST(pattern_create);
  RUN_TEST(pattern_defaults);
  RUN_TEST(literal_extraction);

  printf("\nAll tests passed!\n");
  return 0;
}
