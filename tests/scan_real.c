/*
 * PlumbrC Genomics — Real Data Scanner
 * Scans actual pathogen genomes downloaded from NCBI GenBank
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dna.h"

static void scan_file(dna_scanner_t *s, const char *path, const char *label) {
  printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  printf("  Scanning: %s\n", label);
  printf("  File:     %s\n", path);
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  dna_match_t *matches = NULL;
  size_t count = 0;

  int rc = dna_scan_fastq(s, path, &matches, &count);
  if (rc < 0) {
    fprintf(stderr, "  ERROR: Failed to scan %s\n", path);
    return;
  }

  dna_stats_t stats = dna_scanner_stats(s);
  printf("  Reads scanned:  %zu\n", stats.reads_scanned);
  printf("  Bases scanned:  %zu\n", stats.bases_scanned);
  printf("  Matches found:  %zu\n", count);
  if (stats.elapsed_seconds > 0) {
    printf("  Speed:          %.0f bases/sec\n", stats.bases_per_second);
  }

  dna_print_report(matches, count);

  free(matches);
}

int main(int argc, char **argv) {
  printf("╔══════════════════════════════════════════╗\n");
  printf("║  PlumbrC Genomics — Real Pathogen Scan   ║\n");
  printf("║  Powered by Aho-Corasick DFA Engine      ║\n");
  printf("╚══════════════════════════════════════════╝\n");

  dna_config_t cfg = {.quiet = 0};
  dna_scanner_t *s = dna_scanner_new(&cfg);
  if (!s) {
    fprintf(stderr, "Failed to create scanner\n");
    return 1;
  }

  /* Scan real genomes */
  const char *files[][2] = {
      {"tests/data/sarscov2.fasta", "SARS-CoV-2 (COVID-19) — NC_045512.2"},
      {"tests/data/hiv1.fasta", "HIV-1 Reference — NC_001802.1"},
      {"tests/data/mrsa_meca.fasta", "MRSA mecA Region — AB033763.2"},
      {"tests/data/ecoli_o157.fasta", "E. coli O157:H7 — NC_002695.2"},
  };

  for (int i = 0; i < 4; i++) {
    /* Create fresh scanner for each file to reset stats */
    dna_scanner_t *scanner = dna_scanner_new(&cfg);
    if (!scanner) continue;
    scan_file(scanner, files[i][0], files[i][1]);
    dna_scanner_free(scanner);
  }

  dna_scanner_free(s);

  printf("\n════════════════════════════════════════════\n");
  printf("  Scan complete. Results above.\n");
  printf("════════════════════════════════════════════\n\n");

  return 0;
}
