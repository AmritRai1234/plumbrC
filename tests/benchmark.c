/*
 * PlumbrC - Benchmark Suite
 *
 * Measures throughput at various thread counts and data sizes.
 * Supports human-readable and JSON output for CI regression detection.
 *
 * Usage:
 *   benchmark_suite              # Human-readable output
 *   benchmark_suite --json       # Machine-readable JSON
 *   benchmark_suite --threads 8  # Fixed thread count
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "plumbr.h"

/* ─── Timing ─── */

static double get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* ─── Data generation ─── */

static const char *normal_lines[] = {
    "2024-01-15T10:30:00.123Z INFO  [http-worker-7] Request "
    "GET /api/v2/users/12345 completed in 23ms status=200",
    "2024-01-15T10:30:00.124Z DEBUG [cache-mgr] Cache hit for key "
    "session:user:98765 ttl=3600",
    "2024-01-15T10:30:00.125Z INFO  [scheduler] Task "
    "cleanup-expired-sessions completed, removed 42 entries",
    "2024-01-15T10:30:00.126Z WARN  [conn-pool] Connection pool at "
    "85% capacity (170/200), consider scaling",
    "2024-01-15T10:30:00.127Z INFO  [metrics] System health: "
    "cpu=34% mem=67% disk=45% goroutines=1247",
    "2024-01-15T10:30:00.128Z DEBUG [auth] Token refresh for "
    "session 8f3a2b1c, new expiry in 3600s",
    "2024-01-15T10:30:00.129Z INFO  [gateway] Upstream "
    "backend-svc-03:8080 responded in 12ms",
    "2024-01-15T10:30:00.130Z DEBUG [db] Query SELECT * FROM "
    "events WHERE ts > $1 returned 156 rows in 8ms",
};

static const char *secret_lines[] = {
    "2024-01-15T10:30:00.131Z ERROR [config] AWS credentials: "
    "access_key=AKIAIOSFODNN7EXAMPLE1234 region=us-east-1",
    "2024-01-15T10:30:00.132Z DEBUG [auth] Login attempt "
    "password=SuperSecret123! for user admin@corp.com",
    "2024-01-15T10:30:00.133Z WARN  [webhook] GitHub token "
    "ghp_ABCDEFghijklmnopQRSTUVwxyz1234567890 expired",
    "2024-01-15T10:30:00.134Z ERROR [api] Private key: "
    "-----BEGIN RSA PRIVATE KEY----- MIIEpAIB...",
    "2024-01-15T10:30:00.135Z DEBUG [jwt] Token: "
    "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.abcdef",
    "2024-01-15T10:30:00.136Z INFO  [payment] Processing card "
    "4111111111111111 exp=12/26 cvv=123",
    "2024-01-15T10:30:00.137Z DEBUG [smtp] Sending to "
    "john.doe@company.com from noreply@service.io",
    "2024-01-15T10:30:00.138Z ERROR [config] Database connection: "
    "postgres://admin:p4ssw0rd@db.internal:5432/prod",
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static void generate_lines(FILE *out, size_t count, double secret_ratio) {
  for (size_t i = 0; i < count; i++) {
    double r = (double)(rand() % 10000) / 10000.0;
    if (r < secret_ratio) {
      fprintf(out, "%s\n", secret_lines[i % ARRAY_LEN(secret_lines)]);
    } else {
      fprintf(out, "%s\n", normal_lines[i % ARRAY_LEN(normal_lines)]);
    }
  }
}

/* ─── Benchmark runner ─── */

typedef struct {
  const char *name;
  size_t lines;
  double secret_ratio;
  int threads;
  /* Results */
  double elapsed;
  double lines_per_sec;
  double mb_per_sec;
  size_t lines_modified;
  size_t patterns_matched;
  size_t patterns_loaded;
  long input_bytes;
} BenchResult;

static int run_benchmark(BenchResult *br, int quiet) {
  if (!quiet) {
    fprintf(stderr, "  %-40s ", br->name);
    fflush(stderr);
  }

  /* Create temp files */
  FILE *input = tmpfile();
  FILE *output = tmpfile();
  if (!input || !output) {
    fprintf(stderr, "tmpfile failed\n");
    return -1;
  }

  /* Generate data */
  srand(42); /* Deterministic for reproducibility */
  generate_lines(input, br->lines, br->secret_ratio);
  fflush(input);
  fseek(input, 0, SEEK_END);
  br->input_bytes = ftell(input);
  rewind(input);

  /* Configure */
  PlumbrConfig config;
  plumbr_config_init(&config);
  config.quiet = true;
  config.num_threads = br->threads;

  PlumbrContext *ctx = plumbr_create(&config);
  if (!ctx) {
    fprintf(stderr, "plumbr_create failed\n");
    fclose(input);
    fclose(output);
    return -1;
  }

  /* Warmup run */
  rewind(input);
  plumbr_process(ctx, input, output);
  plumbr_destroy(ctx);

  /* Timed run */
  rewind(input);
  rewind(output);
  ctx = plumbr_create(&config);
  if (!ctx) {
    fclose(input);
    fclose(output);
    return -1;
  }

  double start = get_time();
  plumbr_process(ctx, input, output);
  br->elapsed = get_time() - start;

  PlumbrStats stats = plumbr_get_stats(ctx);
  br->lines_per_sec = stats.lines_per_second;
  br->mb_per_sec = stats.mb_per_second;
  br->lines_modified = stats.lines_modified;
  br->patterns_matched = stats.patterns_matched;
  br->patterns_loaded = stats.patterns_loaded;

  if (!quiet) {
    fprintf(stderr, "%8.0f lines/sec  %6.1f MB/s  (%.3fs)\n", br->lines_per_sec,
            br->mb_per_sec, br->elapsed);
  }

  plumbr_destroy(ctx);
  fclose(input);
  fclose(output);
  return 0;
}

/* ─── Output ─── */

static void print_json(BenchResult *results, int count) {
  printf("[\n");
  for (int i = 0; i < count; i++) {
    BenchResult *r = &results[i];
    printf("  {\"name\": \"%s\", \"threads\": %d, \"lines\": %zu, "
           "\"secret_pct\": %.0f, \"patterns\": %zu, "
           "\"lines_per_sec\": %.0f, \"mb_per_sec\": %.1f, "
           "\"elapsed_sec\": %.3f, \"lines_modified\": %zu, "
           "\"input_mb\": %.1f}%s\n",
           r->name, r->threads, r->lines, r->secret_ratio * 100,
           r->patterns_loaded, r->lines_per_sec, r->mb_per_sec, r->elapsed,
           r->lines_modified, r->input_bytes / (1024.0 * 1024.0),
           (i < count - 1) ? "," : "");
  }
  printf("]\n");
}

static void print_table(BenchResult *results, int count) {
  printf("\n╔══════════════════════════════════════════════════════"
         "══════════════════════════════╗\n");
  printf("║  PlumbrC Benchmark Results %-25s       v%s  ║\n", "",
         plumbr_version());
  printf("╠══════════════════════════════════════════════════════"
         "══════════════════════════════╣\n");
  printf("║  %-40s %8s  %8s  %8s  %6s  ║\n", "Test", "Lines/s", "MB/s", "Time",
         "Thrd");
  printf("╠══════════════════════════════════════════════════════"
         "══════════════════════════════╣\n");

  for (int i = 0; i < count; i++) {
    BenchResult *r = &results[i];
    char lps[16];
    if (r->lines_per_sec >= 1000000)
      snprintf(lps, sizeof(lps), "%.2fM", r->lines_per_sec / 1000000.0);
    else
      snprintf(lps, sizeof(lps), "%.0fK", r->lines_per_sec / 1000.0);

    char thr[8];
    if (r->threads == 0)
      snprintf(thr, sizeof(thr), "auto");
    else
      snprintf(thr, sizeof(thr), "%d", r->threads);

    printf("║  %-40s %8s  %6.1f  %7.3fs  %6s  ║\n", r->name, lps, r->mb_per_sec,
           r->elapsed, thr);
  }

  printf("╠══════════════════════════════════════════════════════"
         "══════════════════════════════╣\n");
  printf("║  Patterns: %zu%-60s║\n", results[0].patterns_loaded, "");
  printf("╚══════════════════════════════════════════════════════"
         "══════════════════════════════╝\n");
}

/* ─── Main ─── */

int main(int argc, char *argv[]) {
  int json_mode = 0;
  int fixed_threads = -1; /* -1 = run all thread configs */

  static struct option opts[] = {{"json", no_argument, 0, 'j'},
                                 {"threads", required_argument, 0, 't'},
                                 {"help", no_argument, 0, 'h'},
                                 {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "jt:h", opts, NULL)) != -1) {
    switch (opt) {
    case 'j':
      json_mode = 1;
      break;
    case 't':
      fixed_threads = atoi(optarg);
      break;
    case 'h':
      printf("Usage: benchmark_suite [--json] [--threads N]\n");
      return 0;
    default:
      return 1;
    }
  }

  if (!json_mode) {
    fprintf(stderr, "\nPlumbrC Benchmark Suite v%s\n", plumbr_version());
    fprintf(stderr, "═══════════════════════════════\n\n");
  }

  /* Define benchmark configurations */
  BenchResult results[32];
  int count = 0;

  /* Thread configurations to test */
  int thread_configs[] = {1, 0}; /* 1T, auto */
  int num_tconfigs = 2;
  if (fixed_threads >= 0) {
    thread_configs[0] = fixed_threads;
    num_tconfigs = 1;
  }

  for (int tc = 0; tc < num_tconfigs; tc++) {
    int threads = thread_configs[tc];
    const char *tname = threads == 0 ? "auto" : (threads == 1 ? "1T" : "NT");

    if (!json_mode && num_tconfigs > 1) {
      fprintf(stderr, "── %s ──\n",
              threads == 0 ? "Auto threads" : "Single thread");
    }

    /* 1M clean lines */
    char name[128];
    snprintf(name, sizeof(name), "1M clean (%s)", tname);
    results[count] = (BenchResult){.name = strdup(name),
                                   .lines = 1000000,
                                   .secret_ratio = 0.0,
                                   .threads = threads};
    run_benchmark(&results[count], json_mode);
    count++;

    /* 1M 10% secrets */
    snprintf(name, sizeof(name), "1M 10%% secrets (%s)", tname);
    results[count] = (BenchResult){.name = strdup(name),
                                   .lines = 1000000,
                                   .secret_ratio = 0.10,
                                   .threads = threads};
    run_benchmark(&results[count], json_mode);
    count++;

    /* 1M 100% secrets (worst case) */
    snprintf(name, sizeof(name), "1M 100%% secrets (%s)", tname);
    results[count] = (BenchResult){.name = strdup(name),
                                   .lines = 1000000,
                                   .secret_ratio = 1.0,
                                   .threads = threads};
    run_benchmark(&results[count], json_mode);
    count++;

    /* 5M enterprise mix (5% secrets) */
    snprintf(name, sizeof(name), "5M enterprise (%s)", tname);
    results[count] = (BenchResult){.name = strdup(name),
                                   .lines = 5000000,
                                   .secret_ratio = 0.05,
                                   .threads = threads};
    run_benchmark(&results[count], json_mode);
    count++;

    if (!json_mode && tc < num_tconfigs - 1)
      fprintf(stderr, "\n");
  }

  /* Output */
  if (json_mode) {
    print_json(results, count);
  } else {
    print_table(results, count);
  }

  /* Cleanup */
  for (int i = 0; i < count; i++)
    free((char *)results[i].name);

  return 0;
}
