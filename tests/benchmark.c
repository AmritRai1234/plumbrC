/*
 * PlumbrC - Benchmark Suite
 *
 * Generates synthetic data and measures throughput.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "plumbr.h"

/* Get current time in seconds */
static double get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* Generate test lines */
static void generate_lines(FILE *out, size_t count, double secret_ratio) {
  const char *normal_lines[] = {
      "2024-01-01 12:00:00 INFO Application started successfully",
      "2024-01-01 12:00:01 DEBUG Processing request from client",
      "2024-01-01 12:00:02 INFO User session created",
      "2024-01-01 12:00:03 DEBUG Cache hit for key user_profile_123",
      "2024-01-01 12:00:04 INFO Request completed in 45ms",
  };

  const char *secret_lines[] = {
      "2024-01-01 12:00:00 DEBUG AWS key: AKIAIOSFODNN7EXAMPLE1234",
      "2024-01-01 12:00:01 ERROR Config: password = supersecret123",
      "2024-01-01 12:00:02 DEBUG API response for user@example.com",
      "2024-01-01 12:00:03 INFO Token: ghp_abcdefghijklmnopqrstuvwxyz123456",
      "2024-01-01 12:00:04 DEBUG JWT: "
      "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.test",
  };

  size_t num_normal = sizeof(normal_lines) / sizeof(normal_lines[0]);
  size_t num_secret = sizeof(secret_lines) / sizeof(secret_lines[0]);

  for (size_t i = 0; i < count; i++) {
    double r = (double)rand() / RAND_MAX;
    if (r < secret_ratio) {
      fprintf(out, "%s\n", secret_lines[rand() % num_secret]);
    } else {
      fprintf(out, "%s\n", normal_lines[rand() % num_normal]);
    }
  }
}

static void run_benchmark(const char *name, size_t lines, double secret_ratio) {
  printf("\n=== Benchmark: %s ===\n", name);
  printf("Lines: %zu, Secret ratio: %.0f%%\n", lines, secret_ratio * 100);

  /* Create temporary files */
  FILE *input = tmpfile();
  FILE *output = tmpfile();

  if (!input || !output) {
    fprintf(stderr, "Failed to create temp files\n");
    return;
  }

  /* Generate test data */
  printf("Generating test data... ");
  fflush(stdout);
  generate_lines(input, lines, secret_ratio);
  fflush(input);
  rewind(input);

  /* Get input size */
  fseek(input, 0, SEEK_END);
  long input_size = ftell(input);
  rewind(input);
  printf("%.2f MB\n", input_size / (1024.0 * 1024.0));

  /* Initialize plumbr */
  PlumbrConfig config;
  plumbr_config_init(&config);
  config.quiet = true;

  PlumbrContext *ctx = plumbr_create(&config);
  if (!ctx) {
    fprintf(stderr, "Failed to create context\n");
    fclose(input);
    fclose(output);
    return;
  }

  /* Run benchmark */
  printf("Processing... ");
  fflush(stdout);

  double start = get_time();
  int result = plumbr_process(ctx, input, output);
  double elapsed = get_time() - start;

  if (result != 0) {
    fprintf(stderr, "Processing failed\n");
  }

  /* Get stats */
  PlumbrStats stats = plumbr_get_stats(ctx);

  /* Report results */
  printf("Done!\n\n");
  printf("Results:\n");
  printf("  Elapsed time:     %.3f seconds\n", elapsed);
  printf("  Lines processed:  %zu\n", stats.lines_processed);
  printf("  Lines modified:   %zu (%.1f%%)\n", stats.lines_modified,
         100.0 * stats.lines_modified / stats.lines_processed);
  printf("  Patterns matched: %zu\n", stats.patterns_matched);
  printf("  Throughput:       %.0f lines/sec\n", stats.lines_per_second);
  printf("  Throughput:       %.2f MB/sec\n", stats.mb_per_second);

  /* Cleanup */
  plumbr_destroy(ctx);
  fclose(input);
  fclose(output);
}

int main(int argc, char *argv[]) {
  printf("PlumbrC Benchmark Suite\n");
  printf("=======================\n");

  /* Seed random */
  srand(time(NULL));

  /* Run benchmarks with different configurations */

  /* Small: 100K lines, 5% secrets */
  run_benchmark("Small (100K lines, 5% secrets)", 100000, 0.05);

  /* Medium: 500K lines, 10% secrets */
  run_benchmark("Medium (500K lines, 10% secrets)", 500000, 0.10);

  /* Large: 1M lines, 20% secrets */
  run_benchmark("Large (1M lines, 20% secrets)", 1000000, 0.20);

  /* Worst case: 1M lines, 100% secrets */
  run_benchmark("Worst Case (1M lines, 100% secrets)", 1000000, 1.00);

  /* Best case: 1M lines, 0% secrets */
  run_benchmark("Best Case (1M lines, 0% secrets)", 1000000, 0.00);

  printf("\nBenchmark complete!\n");
  return 0;
}
