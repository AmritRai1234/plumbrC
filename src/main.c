/*
 * PlumbrC - Main Entry Point
 * High-Performance Log Redaction Pipeline
 *
 * Usage: plumbr [OPTIONS] < input > output
 */

#include "hwdetect.h"
#include "plumbr.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_usage(const char *prog) {
  fprintf(stderr, "PlumbrC v%s - High-Performance Log Redaction Pipeline\n\n",
          plumbr_version());
  fprintf(stderr, "Usage: %s [OPTIONS] < input > output\n\n", prog);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -p, --patterns FILE   Load patterns from FILE\n");
  fprintf(
      stderr,
      "  -d, --defaults        Use built-in default patterns (default: on)\n");
  fprintf(stderr,
          "  -D, --no-defaults     Disable built-in default patterns\n");
  fprintf(stderr, "  -j, --threads N       Use N worker threads (0=auto)\n");
  fprintf(stderr, "  -q, --quiet           Suppress statistics output\n");
  fprintf(stderr,
          "  -s, --stats           Print statistics to stderr (default: on)\n");
  fprintf(stderr, "  -h, --help            Show this help message\n");
  fprintf(stderr, "  -v, --version         Show version\n");
  fprintf(stderr, "  -H, --hwinfo          Show hardware detection info\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Pattern file format (one per line):\n");
  fprintf(stderr, "  name|literal|regex|replacement\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Examples:\n");
  fprintf(stderr, "  # Redact logs using default patterns\n");
  fprintf(stderr, "  %s < app.log > redacted.log\n\n", prog);
  fprintf(stderr, "  # Use custom patterns\n");
  fprintf(stderr, "  %s -p custom.txt < app.log > redacted.log\n\n", prog);
  fprintf(stderr, "  # Pipeline usage\n");
  fprintf(stderr, "  tail -f /var/log/app.log | %s | tee redacted.log\n", prog);
}

static void print_version(void) { printf("plumbr %s\n", plumbr_version()); }

int main(int argc, char *argv[]) {
  PlumbrConfig config;
  plumbr_config_init(&config);

  static struct option long_options[] = {
      {"patterns", required_argument, 0, 'p'},
      {"defaults", no_argument, 0, 'd'},
      {"no-defaults", no_argument, 0, 'D'},
      {"threads", required_argument, 0, 'j'},
      {"quiet", no_argument, 0, 'q'},
      {"stats", no_argument, 0, 's'},
      {"help", no_argument, 0, 'h'},
      {"version", no_argument, 0, 'v'},
      {"hwinfo", no_argument, 0, 'H'},
      {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "p:j:dDqshvH", long_options, NULL)) !=
         -1) {
    switch (opt) {
    case 'p':
      config.pattern_file = optarg;
      break;
    case 'd':
      config.use_defaults = true;
      break;
    case 'D':
      config.use_defaults = false;
      break;
    case 'j':
      config.num_threads = atoi(optarg);
      break;
    case 'q':
      config.quiet = true;
      break;
    case 's':
      config.stats_to_stderr = true;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    case 'v':
      print_version();
      return 0;
    case 'H': {
      HardwareInfo hw;
      hwdetect_init(&hw);
      hwdetect_print(&hw);
      return 0;
    }
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  /* Warn if stdin is a terminal */
  if (isatty(STDIN_FILENO)) {
    fprintf(
        stderr,
        "Warning: Reading from terminal. Pipe input or use Ctrl+D to end.\n");
  }

  /* Create context */
  PlumbrContext *ctx = plumbr_create(&config);
  if (!ctx) {
    fprintf(stderr, "Error: Failed to initialize plumbr\n");
    return 1;
  }

  /* Process stdin to stdout */
  int result = plumbr_process(ctx, stdin, stdout);

  /* Print stats if requested */
  if (!config.quiet && config.stats_to_stderr) {
    plumbr_print_stats(ctx, stderr);
  }

  /* Cleanup */
  plumbr_destroy(ctx);

  return result;
}
