/*
 * PlumbrC - Unit Tests for I/O Layer
 *
 * Tests buffered I/O edge cases: empty input, no trailing newline,
 * multiple lines, and write buffering.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io.h"

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

#define ASSERT_STR_EQ(expected, actual)                                        \
  do {                                                                         \
    if (strcmp(expected, actual) != 0) {                                       \
      fprintf(stderr, "\nFAIL: Expected '%s', got '%s'\n", expected, actual);  \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

/* Helper: create a pipe with data written to the write end */
static int make_pipe_with_data(const char *data, size_t len) {
  int fds[2];
  assert(pipe(fds) == 0);

  /* Write data and close write end */
  ssize_t written = write(fds[1], data, len);
  assert((size_t)written == len);
  close(fds[1]);

  return fds[0]; /* Return read end */
}

/* Tests */

TEST(empty_input) {
  int read_fd = make_pipe_with_data("", 0);
  int write_fds[2];
  assert(pipe(write_fds) == 0);

  IOContext ctx;
  io_init(&ctx, read_fd, write_fds[1]);

  size_t line_len;
  const char *line = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line == NULL);
  ASSERT_EQ(0, io_lines_processed(&ctx));

  close(read_fd);
  close(write_fds[0]);
  close(write_fds[1]);
}

TEST(single_line_with_newline) {
  const char *data = "hello world\n";
  int read_fd = make_pipe_with_data(data, strlen(data));
  int write_fds[2];
  assert(pipe(write_fds) == 0);

  IOContext ctx;
  io_init(&ctx, read_fd, write_fds[1]);

  size_t line_len;
  const char *line = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line != NULL);
  ASSERT_STR_EQ("hello world", line);
  ASSERT_EQ(11, line_len);

  /* Should be EOF */
  line = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line == NULL);

  ASSERT_EQ(1, io_lines_processed(&ctx));

  close(read_fd);
  close(write_fds[0]);
  close(write_fds[1]);
}

TEST(single_line_no_newline) {
  const char *data = "no trailing newline";
  int read_fd = make_pipe_with_data(data, strlen(data));
  int write_fds[2];
  assert(pipe(write_fds) == 0);

  IOContext ctx;
  io_init(&ctx, read_fd, write_fds[1]);

  size_t line_len;
  const char *line = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line != NULL);
  ASSERT_STR_EQ("no trailing newline", line);
  ASSERT_EQ(19, line_len);

  /* Should be EOF */
  line = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line == NULL);

  ASSERT_EQ(1, io_lines_processed(&ctx));

  close(read_fd);
  close(write_fds[0]);
  close(write_fds[1]);
}

TEST(multiple_lines) {
  const char *data = "line one\nline two\nline three\n";
  int read_fd = make_pipe_with_data(data, strlen(data));
  int write_fds[2];
  assert(pipe(write_fds) == 0);

  IOContext ctx;
  io_init(&ctx, read_fd, write_fds[1]);

  size_t line_len;

  const char *line1 = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line1 != NULL);
  ASSERT_STR_EQ("line one", line1);

  const char *line2 = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line2 != NULL);
  ASSERT_STR_EQ("line two", line2);

  const char *line3 = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(line3 != NULL);
  ASSERT_STR_EQ("line three", line3);

  const char *eof = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(eof == NULL);

  ASSERT_EQ(3, io_lines_processed(&ctx));

  close(read_fd);
  close(write_fds[0]);
  close(write_fds[1]);
}

TEST(write_and_flush) {
  int read_fds[2];
  int write_fds[2];
  assert(pipe(read_fds) == 0);
  assert(pipe(write_fds) == 0);

  IOContext ctx;
  io_init(&ctx, read_fds[0], write_fds[1]);

  ASSERT_TRUE(io_write_line(&ctx, "hello", 5));
  ASSERT_TRUE(io_write_line(&ctx, "world", 5));
  ASSERT_TRUE(io_flush(&ctx));

  /* Read back what was written */
  char buf[256];
  close(write_fds[1]); /* Close write end so read can complete */
  ssize_t n = read(write_fds[0], buf, sizeof(buf) - 1);
  ASSERT_TRUE(n > 0);
  buf[n] = '\0';

  ASSERT_STR_EQ("hello\nworld\n", buf);
  ASSERT_EQ(12, io_bytes_written(&ctx));

  close(read_fds[0]);
  close(read_fds[1]);
  close(write_fds[0]);
}

TEST(empty_lines) {
  const char *data = "\n\n\n";
  int read_fd = make_pipe_with_data(data, strlen(data));
  int write_fds[2];
  assert(pipe(write_fds) == 0);

  IOContext ctx;
  io_init(&ctx, read_fd, write_fds[1]);

  size_t line_len;

  /* Should get 3 empty lines */
  for (int i = 0; i < 3; i++) {
    const char *line = io_read_line(&ctx, &line_len);
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(0, line_len);
  }

  const char *eof = io_read_line(&ctx, &line_len);
  ASSERT_TRUE(eof == NULL);

  ASSERT_EQ(3, io_lines_processed(&ctx));

  close(read_fd);
  close(write_fds[0]);
  close(write_fds[1]);
}

TEST(bytes_tracking) {
  const char *data = "line1\nline2\n";
  int read_fd = make_pipe_with_data(data, strlen(data));
  int write_fds[2];
  assert(pipe(write_fds) == 0);

  IOContext ctx;
  io_init(&ctx, read_fd, write_fds[1]);

  size_t line_len;
  io_read_line(&ctx, &line_len);
  io_read_line(&ctx, &line_len);
  io_read_line(&ctx, &line_len); /* EOF */

  ASSERT_EQ(12, io_bytes_read(&ctx));

  close(read_fd);
  close(write_fds[0]);
  close(write_fds[1]);
}

int main(void) {
  printf("Running I/O tests...\n");

  RUN_TEST(empty_input);
  RUN_TEST(single_line_with_newline);
  RUN_TEST(single_line_no_newline);
  RUN_TEST(multiple_lines);
  RUN_TEST(write_and_flush);
  RUN_TEST(empty_lines);
  RUN_TEST(bytes_tracking);

  printf("\nAll tests passed!\n");
  return 0;
}
