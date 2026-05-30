/*
 * PlumbrC Genomics — Unit Tests
 *
 * Tests DNA pathogen scanning against synthetic FASTQ data.
 * Generates test data with known pathogen reads mixed in.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dna.h"

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
      fprintf(stderr, "\nFAIL: %s is false at line %d\n", #cond, __LINE__);    \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(expected, actual)                                            \
  do {                                                                         \
    if ((expected) != (actual)) {                                              \
      fprintf(stderr, "\nFAIL: Expected %zu, got %zu at line %d\n",            \
              (size_t)(expected), (size_t)(actual), __LINE__);                  \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

/* ─── Synthetic FASTQ Generator ─── */

static const char DNA_BASES[] = "ACGT";

static void random_dna(char *buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    buf[i] = DNA_BASES[rand() % 4];
  }
  buf[len] = '\0';
}

/* Generate a synthetic FASTQ file with pathogen reads planted */
static void generate_test_fastq(const char *path, size_t num_reads,
                                size_t read_len,
                                const char **pathogen_seqs,
                                size_t num_pathogens,
                                const size_t *plant_positions) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    fprintf(stderr, "Cannot create %s\n", path);
    exit(1);
  }

  char *read_buf = malloc(read_len + 256);
  char *qual_buf = malloc(read_len + 256);
  memset(qual_buf, 'I', read_len); /* Max quality */
  qual_buf[read_len] = '\0';

  for (size_t i = 0; i < num_reads; i++) {
    /* Header */
    fprintf(fp, "@READ_%08zu length=%zu\n", i, read_len);

    /* Check if this read should contain a pathogen */
    bool planted = false;
    for (size_t p = 0; p < num_pathogens; p++) {
      if (plant_positions[p] == i) {
        /* Generate random DNA, then insert pathogen signature */
        random_dna(read_buf, read_len);
        size_t sig_len = strlen(pathogen_seqs[p]);
        if (sig_len <= read_len) {
          /* Plant at position 10 within the read */
          size_t offset = 10;
          if (offset + sig_len > read_len)
            offset = 0;
          memcpy(read_buf + offset, pathogen_seqs[p], sig_len);
        }
        planted = true;
        break;
      }
    }

    if (!planted) {
      /* Generate clean random DNA (no pathogen) */
      random_dna(read_buf, read_len);
    }

    fprintf(fp, "%s\n", read_buf);
    fprintf(fp, "+\n");
    fprintf(fp, "%.*s\n", (int)read_len, qual_buf);
  }

  free(read_buf);
  free(qual_buf);
  fclose(fp);
}

/* ─── Tests ─── */

TEST(scanner_new) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  dna_stats_t stats = dna_scanner_stats(s);
  ASSERT_TRUE(stats.patterns_loaded >= 50); /* Should load all 69 patterns */

  dna_scanner_free(s);
}

TEST(scan_sequence_covid) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  /* SARS-CoV-2 spike RBD signature (from viruses.txt) */
  const char *covid_seq =
      "AAAAGGGGATGTTTGTTTTTCTTGTTTTATTGCCACTAGTCTCTAGTCAGTGTCCCCAAAGGG";

  dna_match_t *matches = NULL;
  size_t count = 0;
  int rc = dna_scan_sequence(s, covid_seq, strlen(covid_seq), &matches, &count);

  ASSERT_EQ(0, rc);
  ASSERT_TRUE(count >= 1);
  ASSERT_TRUE(strcmp(matches[0].pathogen_name, "SARS-CoV-2") == 0);
  ASSERT_TRUE(strcmp(matches[0].gene, "spike_RBD") == 0);

  free(matches);
  dna_scanner_free(s);
}

TEST(scan_sequence_no_match) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  /* Random human DNA — should NOT match any pathogen */
  const char *human_seq =
      "ATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCG";

  dna_match_t *matches = NULL;
  size_t count = 0;
  int rc =
      dna_scan_sequence(s, human_seq, strlen(human_seq), &matches, &count);

  ASSERT_EQ(0, rc);
  ASSERT_EQ(0, count);

  free(matches);
  dna_scanner_free(s);
}

TEST(scan_sequence_mrsa) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  /* MRSA mecA gene signature (from bacteria.txt) */
  const char *mrsa_seq =
      "GGGGTGGCCAATACAGGAACAGCATATGAGATAGGCATCGTTCCAAAGAATGTAAACCCC";

  dna_match_t *matches = NULL;
  size_t count = 0;
  int rc =
      dna_scan_sequence(s, mrsa_seq, strlen(mrsa_seq), &matches, &count);

  ASSERT_EQ(0, rc);
  ASSERT_TRUE(count >= 1);
  ASSERT_TRUE(strcmp(matches[0].pathogen_name, "MRSA") == 0);

  free(matches);
  dna_scanner_free(s);
}

TEST(scan_sequence_resistance) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  /* NDM-1 resistance marker (from fungi_parasites.txt) */
  const char *ndm_seq =
      "CCCCATGGAATTGCCCAATATTATGCACCCGGTCGCGAAGCTGAGCACCGCATTAGAAAAA";

  dna_match_t *matches = NULL;
  size_t count = 0;
  int rc = dna_scan_sequence(s, ndm_seq, strlen(ndm_seq), &matches, &count);

  ASSERT_EQ(0, rc);
  ASSERT_TRUE(count >= 1);
  ASSERT_TRUE(strcmp(matches[0].category, "RESISTANCE") == 0);

  free(matches);
  dna_scanner_free(s);
}

TEST(scan_fastq_synthetic) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  /* Generate synthetic FASTQ with 1000 reads, plant COVID at read #42 */
  const char *pathogens[] = {
      "ATGTTTGTTTTTCTTGTTTTATTGCCACTAGTCTCTAGTCAGTGT", /* SARS-CoV-2 spike */
  };
  size_t positions[] = {42};

  const char *test_file = "build/test_synthetic.fastq";
  generate_test_fastq(test_file, 1000, 150, pathogens, 1, positions);

  dna_match_t *matches = NULL;
  size_t count = 0;
  int rc = dna_scan_fastq(s, test_file, &matches, &count);

  ASSERT_EQ(0, rc);
  ASSERT_TRUE(count >= 1);

  /* Should find COVID in read #42 */
  bool found_covid = false;
  for (size_t i = 0; i < count; i++) {
    if (matches[i].read_id == 42 &&
        strcmp(matches[i].pathogen_name, "SARS-CoV-2") == 0) {
      found_covid = true;
    }
  }
  ASSERT_TRUE(found_covid);

  /* Print report for visual verification */
  dna_print_report(matches, count);

  dna_stats_t stats = dna_scanner_stats(s);
  ASSERT_EQ(1000, stats.reads_scanned);
  ASSERT_TRUE(stats.reads_matched >= 1);

  free(matches);
  dna_scanner_free(s);
  remove(test_file);
}

TEST(scan_fastq_multi_pathogen) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  /* Plant multiple pathogens */
  const char *pathogens[] = {
      "ATGTTTGTTTTTCTTGTTTTATTGCCACTAGTCTCTAGTCAGTGT", /* SARS-CoV-2 */
      "TGGCCAATACAGGAACAGCATATGAGATAGGCATCGTTCCAAAGAATGT", /* MRSA */
      "ATGGGTGCGAGAGCGTCAGTATTAAGCGGGGGAGAATTAGATCGATGG", /* HIV-1 */
  };
  size_t positions[] = {10, 50, 90};

  const char *test_file = "build/test_multi.fastq";
  generate_test_fastq(test_file, 200, 150, pathogens, 3, positions);

  dna_match_t *matches = NULL;
  size_t count = 0;
  int rc = dna_scan_fastq(s, test_file, &matches, &count);

  ASSERT_EQ(0, rc);
  ASSERT_TRUE(count >= 3);

  /* Print grouped report */
  dna_print_report(matches, count);

  free(matches);
  dna_scanner_free(s);
  remove(test_file);
}

TEST(null_safety) {
  ASSERT_TRUE(dna_scan_fastq(NULL, "x", NULL, NULL) == -1);
  ASSERT_TRUE(dna_scan_sequence(NULL, "x", 1, NULL, NULL) == -1);

  dna_scanner_free(NULL); /* Should not crash */
}

TEST(version) {
  const char *ver = dna_scanner_version();
  ASSERT_TRUE(ver != NULL);
  ASSERT_TRUE(strlen(ver) > 0);
}

TEST(stats) {
  dna_scanner_t *s = dna_scanner_new(NULL);
  ASSERT_TRUE(s != NULL);

  /* Scan a sequence */
  const char *seq = "ATCGATCGATCGATCGATCG";
  dna_match_t *matches = NULL;
  size_t count = 0;
  dna_scan_sequence(s, seq, strlen(seq), &matches, &count);

  dna_stats_t stats = dna_scanner_stats(s);
  ASSERT_EQ(1, stats.reads_scanned);
  ASSERT_EQ(20, stats.bases_scanned);

  free(matches);
  dna_scanner_free(s);
}

/* ─── Main ─── */

int main(void) {
  srand(42); /* Deterministic random for reproducible tests */

  printf("Running DNA pathogen scanner tests...\n");

  RUN_TEST(scanner_new);
  RUN_TEST(scan_sequence_covid);
  RUN_TEST(scan_sequence_no_match);
  RUN_TEST(scan_sequence_mrsa);
  RUN_TEST(scan_sequence_resistance);
  RUN_TEST(scan_fastq_synthetic);
  RUN_TEST(scan_fastq_multi_pathogen);
  RUN_TEST(null_safety);
  RUN_TEST(version);
  RUN_TEST(stats);

  printf("\nAll DNA tests passed!\n");
  return 0;
}
