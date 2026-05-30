/*
 * PlumbrC Genomics — DNA Pathogen Screening
 *
 * Scans DNA sequences (FASTQ/FASTA files or raw sequences) against
 * a panel of known pathogen signatures using the Aho-Corasick DFA engine.
 *
 * Quick start:
 *   dna_scanner_t *s = dna_scanner_new(NULL);     // Built-in pathogen panel
 *   dna_match_t *matches; size_t count;
 *   dna_scan_fastq(s, "sample.fastq", &matches, &count);
 *   dna_print_report(matches, count);
 *   free(matches);
 *   dna_scanner_free(s);
 *
 * Thread safety: same as libplumbr — one instance per thread.
 */

#ifndef PLUMBR_DNA_H
#define PLUMBR_DNA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Opaque Handle ─── */

typedef struct dna_scanner dna_scanner_t;

/* ─── Configuration ─── */

typedef struct {
  const char *pattern_dir;  /* Path to genomics pattern directory (NULL = built-in) */
  const char *pattern_file; /* Path to a specific pattern file (NULL = none) */
  int quiet;                /* Suppress progress output */
} dna_config_t;

/* ─── Match Result ─── */

typedef struct {
  const char *pathogen_name;  /* e.g., "SARS-CoV-2" */
  const char *gene;           /* e.g., "spike_RBD" */
  const char *category;       /* e.g., "PATHOGEN" or "RESISTANCE" */
  uint32_t    read_id;        /* Which read in the file (0-indexed) */
  uint32_t    position;       /* Byte position within the read */
  uint16_t    match_length;   /* Length of the matching signature */
} dna_match_t;

/* ─── Statistics ─── */

typedef struct {
  size_t reads_scanned;
  size_t reads_matched;
  size_t total_matches;
  size_t bases_scanned;
  size_t patterns_loaded;
  double elapsed_seconds;
  double reads_per_second;
  double bases_per_second;
} dna_stats_t;

/* ─── Lifecycle ─── */

/*
 * Create a DNA scanner.
 * config: Configuration (NULL = use built-in pathogen panel).
 * Returns scanner handle, or NULL on error.
 */
dna_scanner_t *dna_scanner_new(const dna_config_t *config);

/*
 * Free scanner and all resources.
 */
void dna_scanner_free(dna_scanner_t *s);

/* ─── Scanning ─── */

/*
 * Scan a FASTQ file for pathogen matches.
 *
 * s:           Scanner handle
 * fastq_path:  Path to .fastq file
 * matches:     Output array of matches (caller must free)
 * num_matches: Number of matches found
 *
 * Returns 0 on success, -1 on error.
 */
int dna_scan_fastq(dna_scanner_t *s, const char *fastq_path,
                   dna_match_t **matches, size_t *num_matches);

/*
 * Scan a raw DNA sequence.
 *
 * s:           Scanner handle
 * sequence:    DNA string (A/C/G/T/N characters)
 * seq_len:     Length of sequence
 * matches:     Output array of matches (caller must free)
 * num_matches: Number of matches found
 *
 * Returns 0 on success, -1 on error.
 */
int dna_scan_sequence(dna_scanner_t *s, const char *sequence, size_t seq_len,
                      dna_match_t **matches, size_t *num_matches);

/* ─── Reporting ─── */

/*
 * Print a human-readable report of matches to stderr.
 */
void dna_print_report(const dna_match_t *matches, size_t count);

/*
 * Get scanning statistics.
 */
dna_stats_t dna_scanner_stats(const dna_scanner_t *s);

/*
 * Get scanner version.
 */
const char *dna_scanner_version(void);

#ifdef __cplusplus
}
#endif

#endif /* PLUMBR_DNA_H */
