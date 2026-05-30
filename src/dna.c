/*
 * PlumbrC Genomics — DNA Pathogen Scanner
 *
 * Reuses the existing Aho-Corasick DFA engine to scan DNA sequences
 * against known pathogen marker signatures. Same engine, different input.
 */

#include "dna.h"
#include "arena.h"
#include "config.h"
#include "patterns.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ─── Internal Structure ─── */

struct dna_scanner {
  Arena arena;
  PatternSet *patterns;
  dna_stats_t stats;

  /* Pattern name/gene/category extracted from replacement tags */
  char **pathogen_names; /* [pattern_id] -> "SARS-CoV-2" */
  char **gene_names;     /* [pattern_id] -> "spike_RBD" */
  char **categories;     /* [pattern_id] -> "PATHOGEN" */
  size_t num_patterns;
};

/* ─── Helper: parse replacement tag ─── */

/* Parse "[PATHOGEN:SARS-CoV-2:spike_RBD]" into category, name, gene */
static void parse_tag(const char *replacement, char *category, size_t cat_sz,
                      char *name, size_t name_sz, char *gene,
                      size_t gene_sz) {
  /* Default values */
  snprintf(category, cat_sz, "UNKNOWN");
  snprintf(name, name_sz, "unknown");
  snprintf(gene, gene_sz, "unknown");

  if (!replacement || replacement[0] != '[')
    return;

  const char *start = replacement + 1; /* skip '[' */
  const char *end = strchr(start, ']');
  if (!end)
    return;

  /* Copy into scratch buffer */
  char buf[256];
  size_t len = (size_t)(end - start);
  if (len >= sizeof(buf))
    len = sizeof(buf) - 1;
  memcpy(buf, start, len);
  buf[len] = '\0';

  /* Split on ':' — category:name:gene */
  char *tok1 = buf;
  char *tok2 = strchr(tok1, ':');
  if (tok2) {
    *tok2++ = '\0';
    char *tok3 = strchr(tok2, ':');
    if (tok3) {
      *tok3++ = '\0';
      snprintf(gene, gene_sz, "%s", tok3);
    }
    snprintf(name, name_sz, "%s", tok2);
  }
  snprintf(category, cat_sz, "%s", tok1);
}

/* ─── Helper: resolve data path ─── */

static bool resolve_genomics_path(const char *relative, char *out,
                                  size_t out_size) {
  /* Try relative path first (cwd) — avoids absolute path rejection */
  snprintf(out, out_size, "%s", relative);
  if (access(out, R_OK) == 0)
    return true;

  /* Try PLUMBR_DATA_DIR env var */
  const char *data_dir = getenv("PLUMBR_DATA_DIR");
  if (data_dir) {
    snprintf(out, out_size, "%s/%s", data_dir, relative);
    if (access(out, R_OK) == 0)
      return true;
  }

  /* Try relative to binary location via /proc/self/exe */
  char exe_path[4096];
  ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (n > 0) {
    exe_path[n] = '\0';
    char *last_slash = strrchr(exe_path, '/');
    if (last_slash) {
      *last_slash = '\0';
      /* Go up from build/bin to project root */
      char *slash2 = strrchr(exe_path, '/');
      if (slash2) {
        *slash2 = '\0';
        char *slash3 = strrchr(exe_path, '/');
        if (slash3) {
          *slash3 = '\0';
          snprintf(out, out_size, "%s/%s", exe_path, relative);
          if (access(out, R_OK) == 0)
            return true;
        }
      }
    }
  }

  return false;
}

/* ─── Lifecycle ─── */

dna_scanner_t *dna_scanner_new(const dna_config_t *config) {
  dna_scanner_t *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;

  if (!arena_init(&s->arena, PLUMBR_ARENA_SIZE)) {
    free(s);
    return NULL;
  }

  s->patterns = patterns_create(&s->arena, 128);
  if (!s->patterns) {
    arena_destroy(&s->arena);
    free(s);
    return NULL;
  }

  /* Load patterns */
  int loaded = 0;

  if (config && config->pattern_file) {
    loaded += patterns_load_file(s->patterns, config->pattern_file);
  }

  if (config && config->pattern_dir) {
    loaded += patterns_load_directory(s->patterns, config->pattern_dir);
  }

  if (!loaded) {
    /* Load built-in genomics patterns */
    char resolved[4096];
    if (resolve_genomics_path("patterns/genomics/viruses.txt", resolved,
                              sizeof(resolved)))
      loaded += patterns_load_file(s->patterns, resolved);
    if (resolve_genomics_path("patterns/genomics/bacteria.txt", resolved,
                              sizeof(resolved)))
      loaded += patterns_load_file(s->patterns, resolved);
    if (resolve_genomics_path("patterns/genomics/fungi_parasites.txt", resolved,
                              sizeof(resolved)))
      loaded += patterns_load_file(s->patterns, resolved);
  }

  if (!loaded) {
    if (!config || !config->quiet)
      fprintf(stderr, "[dna] ERROR: No pathogen patterns loaded\n");
    patterns_destroy(s->patterns);
    arena_destroy(&s->arena);
    free(s);
    return NULL;
  }

  /* Build the AC automaton */
  if (!patterns_build(s->patterns)) {
    patterns_destroy(s->patterns);
    arena_destroy(&s->arena);
    free(s);
    return NULL;
  }

  /* Extract pathogen names from replacement tags */
  s->num_patterns = patterns_count(s->patterns);
  s->pathogen_names = calloc(s->num_patterns, sizeof(char *));
  s->gene_names = calloc(s->num_patterns, sizeof(char *));
  s->categories = calloc(s->num_patterns, sizeof(char *));

  for (size_t i = 0; i < s->num_patterns; i++) {
    const Pattern *p = patterns_get(s->patterns, (uint32_t)i);
    if (!p)
      continue;

    char cat[64], name[128], gene[128];
    parse_tag(p->replacement, cat, sizeof(cat), name, sizeof(name), gene,
              sizeof(gene));

    s->pathogen_names[i] = strdup(name);
    s->gene_names[i] = strdup(gene);
    s->categories[i] = strdup(cat);
  }

  s->stats.patterns_loaded = s->num_patterns;

  if (!config || !config->quiet)
    fprintf(stderr, "[dna] Loaded %zu pathogen signatures\n", s->num_patterns);

  return s;
}

void dna_scanner_free(dna_scanner_t *s) {
  if (!s)
    return;

  for (size_t i = 0; i < s->num_patterns; i++) {
    free(s->pathogen_names[i]);
    free(s->gene_names[i]);
    free(s->categories[i]);
  }
  free(s->pathogen_names);
  free(s->gene_names);
  free(s->categories);

  patterns_destroy(s->patterns);
  arena_destroy(&s->arena);
  free(s);
}

/* ─── Scanning ─── */

/* Internal: scan a single DNA sequence and append matches */
static int scan_one_sequence(dna_scanner_t *s, const char *seq, size_t seq_len,
                             uint32_t read_id, dna_match_t **matches,
                             size_t *count, size_t *capacity) {
  /* Use ac_search_all to find all pathogen matches */
  ACMatch ac_matches[64]; /* Max 64 matches per read */
  size_t n =
      ac_search_all(s->patterns->automaton, seq, seq_len, ac_matches, 64);

  for (size_t i = 0; i < n; i++) {
    /* Grow matches array if needed */
    if (*count >= *capacity) {
      *capacity = (*capacity == 0) ? 64 : *capacity * 2;
      dna_match_t *tmp = realloc(*matches, *capacity * sizeof(dna_match_t));
      if (!tmp)
        return -1;
      *matches = tmp;
    }

    uint32_t pid = ac_matches[i].pattern_id;
    dna_match_t *m = &(*matches)[*count];

    m->pathogen_name =
        (pid < s->num_patterns) ? s->pathogen_names[pid] : "unknown";
    m->gene = (pid < s->num_patterns) ? s->gene_names[pid] : "unknown";
    m->category = (pid < s->num_patterns) ? s->categories[pid] : "UNKNOWN";
    m->read_id = read_id;
    m->position =
        (uint32_t)(ac_matches[i].position - ac_matches[i].length + 1);
    m->match_length = ac_matches[i].length;

    (*count)++;
  }

  /* Update stats */
  s->stats.reads_scanned++;
  s->stats.bases_scanned += seq_len;
  if (n > 0) {
    s->stats.reads_matched++;
    s->stats.total_matches += n;
  }

  return 0;
}

int dna_scan_sequence(dna_scanner_t *s, const char *sequence, size_t seq_len,
                      dna_match_t **matches, size_t *num_matches) {
  if (!s || !sequence || !matches || !num_matches)
    return -1;

  *matches = NULL;
  *num_matches = 0;

  size_t capacity = 0;
  if (scan_one_sequence(s, sequence, seq_len, 0, matches, num_matches,
                        &capacity) < 0)
    return -1;

  return 0;
}

int dna_scan_fastq(dna_scanner_t *s, const char *fastq_path,
                   dna_match_t **matches, size_t *num_matches) {
  if (!s || !fastq_path || !matches || !num_matches)
    return -1;

  *matches = NULL;
  *num_matches = 0;

  FILE *fp = fopen(fastq_path, "r");
  if (!fp) {
    fprintf(stderr, "[dna] ERROR: Cannot open %s\n", fastq_path);
    return -1;
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  char line[65536]; /* Max line length */
  size_t capacity = 0;
  uint32_t read_id = 0;
  int line_in_read = 0; /* 0=header, 1=sequence, 2='+', 3=quality */

  while (fgets(line, sizeof(line), fp)) {
    /* Remove trailing newline */
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';

    switch (line_in_read) {
    case 0: /* @header line — skip */
      if (line[0] != '@' && line[0] != '>') {
        /* Not a valid FASTQ/FASTA header, try to recover */
        continue;
      }
      line_in_read = 1;
      break;

    case 1: /* Sequence line — SCAN THIS */
      if (scan_one_sequence(s, line, len, read_id, matches, num_matches,
                            &capacity) < 0) {
        fclose(fp);
        return -1;
      }
      read_id++;

      /* Check if FASTA format (no quality lines) */
      line_in_read = 2;
      break;

    case 2: /* '+' separator line — skip */
      if (line[0] == '+') {
        line_in_read = 3;
      } else if (line[0] == '>' || line[0] == '@') {
        /* FASTA format — this is the next header */
        line_in_read = 1;
      }
      break;

    case 3: /* Quality line — skip, reset for next read */
      line_in_read = 0;
      break;
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  s->stats.elapsed_seconds =
      (double)(end.tv_sec - start.tv_sec) +
      (double)(end.tv_nsec - start.tv_nsec) / 1e9;

  if (s->stats.elapsed_seconds > 0) {
    s->stats.reads_per_second =
        (double)s->stats.reads_scanned / s->stats.elapsed_seconds;
    s->stats.bases_per_second =
        (double)s->stats.bases_scanned / s->stats.elapsed_seconds;
  }

  fclose(fp);
  return 0;
}

/* ─── Reporting ─── */

void dna_print_report(const dna_match_t *matches, size_t count) {
  if (!matches || count == 0) {
    fprintf(stderr,
            "\n╔══════════════════════════════════════╗\n"
            "║    No pathogen signatures detected   ║\n"
            "╚══════════════════════════════════════╝\n\n");
    return;
  }

  fprintf(stderr, "\n══════════════════════════════════════\n");
  fprintf(stderr, "  PATHOGEN SCREENING RESULTS\n");
  fprintf(stderr, "  %zu match(es) found\n", count);
  fprintf(stderr, "══════════════════════════════════════\n\n");

  /* Group by pathogen — count occurrences */
  typedef struct {
    const char *name;
    const char *gene;
    const char *category;
    size_t hits;
  } PathogenSummary;

  PathogenSummary summary[256];
  size_t summary_count = 0;

  for (size_t i = 0; i < count; i++) {
    /* Find or create summary entry */
    bool found = false;
    for (size_t j = 0; j < summary_count; j++) {
      if (strcmp(summary[j].name, matches[i].pathogen_name) == 0 &&
          strcmp(summary[j].gene, matches[i].gene) == 0) {
        summary[j].hits++;
        found = true;
        break;
      }
    }
    if (!found && summary_count < 256) {
      summary[summary_count].name = matches[i].pathogen_name;
      summary[summary_count].gene = matches[i].gene;
      summary[summary_count].category = matches[i].category;
      summary[summary_count].hits = 1;
      summary_count++;
    }
  }

  /* Print summary table */
  fprintf(stderr, "  %-25s %-18s %-12s %s\n", "PATHOGEN", "GENE", "TYPE",
          "HITS");
  fprintf(stderr,
          "  ──────────────────────── ────────────────── ──────────── ────\n");

  for (size_t i = 0; i < summary_count; i++) {
    const char *icon = "🦠";
    if (strcmp(summary[i].category, "RESISTANCE") == 0)
      icon = "💊";
    else if (strcmp(summary[i].category, "PATHOGEN") == 0) {
      /* Try to guess organism type from name for icon */
      if (strstr(summary[i].name, "virus") || strstr(summary[i].name, "RSV") ||
          strstr(summary[i].name, "HIV") || strstr(summary[i].name, "HBV") ||
          strstr(summary[i].name, "HCV") ||
          strstr(summary[i].name, "Influenza") ||
          strstr(summary[i].name, "SARS"))
        icon = "🧬";
    }

    fprintf(stderr, "  %s %-23s %-18s %-12s %zu\n", icon, summary[i].name,
            summary[i].gene, summary[i].category, summary[i].hits);
  }

  fprintf(stderr, "\n");
}

/* ─── Info ─── */

dna_stats_t dna_scanner_stats(const dna_scanner_t *s) {
  if (!s) {
    dna_stats_t empty = {0};
    return empty;
  }
  return s->stats;
}

const char *dna_scanner_version(void) { return "1.0.0"; }
