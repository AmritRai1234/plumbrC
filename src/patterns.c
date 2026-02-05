/*
 * PlumbrC - Pattern Management Implementation
 */

#include "patterns.h"
#include "config.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

PatternSet *patterns_create(Arena *arena, size_t initial_capacity) {
  PatternSet *ps = arena_alloc(arena, sizeof(PatternSet));
  if (!ps)
    return NULL;

  ps->patterns = arena_alloc(arena, initial_capacity * sizeof(Pattern));
  if (!ps->patterns)
    return NULL;

  ps->count = 0;
  ps->capacity = initial_capacity;
  ps->automaton = ac_create(arena);
  ps->arena = arena;
  ps->built = false;

  return ps;
}

bool patterns_add(PatternSet *ps, const char *name, const char *literal,
                  const char *regex, const char *replacement) {
  if (ps->built) {
    return false; /* Cannot add after build */
  }

  if (ps->count >= ps->capacity) {
    return false; /* Full */
  }

  Pattern *p = &ps->patterns[ps->count];
  memset(p, 0, sizeof(Pattern));

  /* Copy name */
  strncpy(p->name, name, PLUMBR_MAX_PATTERN_NAME - 1);
  p->name[PLUMBR_MAX_PATTERN_NAME - 1] = '\0';

  /* Copy literal */
  if (literal && literal[0]) {
    strncpy(p->literal, literal, PLUMBR_MAX_LITERAL_LEN - 1);
    p->literal[PLUMBR_MAX_LITERAL_LEN - 1] = '\0';
    p->literal_len = strlen(p->literal);
    p->has_literal = true;
  } else {
    p->has_literal = false;
  }

  /* Compile regex */
  int errcode;
  PCRE2_SIZE erroffset;

  p->regex =
      pcre2_compile((PCRE2_SPTR)regex, PCRE2_ZERO_TERMINATED,
                    PCRE2_UTF | PCRE2_NO_UTF_CHECK, &errcode, &erroffset, NULL);

  if (!p->regex) {
    PCRE2_UCHAR errbuf[256];
    pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
    fprintf(stderr, "Pattern '%s' compile error at %zu: %s\n", name, erroffset,
            errbuf);
    return false;
  }

  /* JIT compile for speed */
  pcre2_jit_compile(p->regex, PCRE2_JIT_COMPLETE);

  /* Create match data */
  p->match_data = pcre2_match_data_create_from_pattern(p->regex, NULL);
  if (!p->match_data) {
    pcre2_code_free(p->regex);
    return false;
  }

  /* Copy replacement */
  if (replacement && replacement[0]) {
    strncpy(p->replacement, replacement, PLUMBR_MAX_REPLACEMENT_LEN - 1);
    p->replacement[PLUMBR_MAX_REPLACEMENT_LEN - 1] = '\0';
    p->replacement_len = strlen(p->replacement);
  } else {
    snprintf(p->replacement, PLUMBR_MAX_REPLACEMENT_LEN, "[REDACTED:%s]", name);
    p->replacement_len = strlen(p->replacement);
  }

  p->id = (uint32_t)ps->count;
  ps->count++;

  return true;
}

bool patterns_load_file(PatternSet *ps, const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    return false;
  }

  char line[4096];
  int line_num = 0;

  while (fgets(line, sizeof(line), f)) {
    line_num++;

    /* Skip comments and empty lines */
    char *start = line;
    while (*start && isspace(*start))
      start++;
    if (*start == '#' || *start == '\0' || *start == '\n') {
      continue;
    }

    /* Remove trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    /* Parse: name|literal|regex|replacement */
    char *name = strtok(start, "|");
    char *literal = strtok(NULL, "|");
    char *regex = strtok(NULL, "|");
    char *replacement = strtok(NULL, "|");

    if (!name || !regex) {
      /* SECURITY: Only show basename to avoid path disclosure */
      const char *base = strrchr(filename, '/');
      base = base ? base + 1 : filename;
      fprintf(
          stderr,
          "%s:%d: Invalid format (expected name|literal|regex|replacement)\n",
          base, line_num);
      continue;
    }

    /* Trim whitespace */
    while (*name && isspace(*name))
      name++;

    if (!patterns_add(ps, name, literal, regex, replacement)) {
      const char *base = strrchr(filename, '/');
      base = base ? base + 1 : filename;
      fprintf(stderr, "%s:%d: Failed to add pattern '%s'\n", base, line_num,
              name);
    }
  }

  fclose(f);
  return ps->count > 0;
}

/* Load all pattern files from a directory */
int patterns_load_directory(PatternSet *ps, const char *dirname) {
  DIR *dir = opendir(dirname);
  if (!dir) {
    return 0;
  }

  int loaded = 0;
  struct dirent *entry;
  char filepath[4096];

  while ((entry = readdir(dir)) != NULL) {
    /* Skip hidden files and directories */
    if (entry->d_name[0] == '.') {
      continue;
    }

    /* Only load .txt files */
    size_t len = strlen(entry->d_name);
    if (len < 4 || strcmp(entry->d_name + len - 4, ".txt") != 0) {
      continue;
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", dirname, entry->d_name);

    size_t before = ps->count;
    if (patterns_load_file(ps, filepath)) {
      loaded += (int)(ps->count - before);
    }
  }

  closedir(dir);
  return loaded;
}

bool patterns_build(PatternSet *ps) {
  if (ps->built) {
    return true;
  }

  /* Add literals to AC automaton */
  for (size_t i = 0; i < ps->count; i++) {
    Pattern *p = &ps->patterns[i];
    if (p->has_literal && p->literal_len > 0) {
      ac_add_pattern(ps->automaton, p->literal, p->literal_len, p->id);
    }
  }

  /* Build automaton */
  if (!ac_build(ps->automaton)) {
    return false;
  }

  ps->built = true;
  return true;
}

const Pattern *patterns_get(const PatternSet *ps, uint32_t id) {
  if (id >= ps->count) {
    return NULL;
  }
  return &ps->patterns[id];
}

size_t patterns_count(const PatternSet *ps) { return ps->count; }

void patterns_destroy(PatternSet *ps) {
  /* Free PCRE2 resources (arena handles Pattern structs) */
  for (size_t i = 0; i < ps->count; i++) {
    Pattern *p = &ps->patterns[i];
    if (p->match_data) {
      pcre2_match_data_free(p->match_data);
    }
    if (p->regex) {
      pcre2_code_free(p->regex);
    }
  }
}

/* Extract a literal from a regex pattern (best effort) */
bool patterns_extract_literal(const char *regex, char *out_literal,
                              size_t max_len) {
  size_t out_pos = 0;
  const char *p = regex;

  /* Skip anchors */
  if (*p == '^')
    p++;
  if (*p == '\\' && *(p + 1) == 'A')
    p += 2;

  while (*p && out_pos < max_len - 1) {
    /* Stop at regex metacharacters */
    if (strchr("[](){}|*+?.^$\\", *p)) {
      /* Handle escapes */
      if (*p == '\\' && *(p + 1)) {
        char next = *(p + 1);
        /* Simple escapes we can handle */
        if (next == '-' || next == '_' || next == '.' || next == '@' ||
            next == ':' || next == '/') {
          out_literal[out_pos++] = next;
          p += 2;
          continue;
        }
      }
      break; /* Stop at other metacharacters */
    }

    out_literal[out_pos++] = *p++;
  }

  out_literal[out_pos] = '\0';

  /* Need at least 3 characters for useful literal */
  return out_pos >= 3;
}

/* Default patterns for common secrets */
bool patterns_add_defaults(PatternSet *ps) {
  /* AWS Access Key */
  patterns_add(ps, "aws_access_key", "AKIA", "AKIA[0-9A-Z]{16}", NULL);

  /* AWS Secret Key */
  patterns_add(ps, "aws_secret_key", NULL, "[A-Za-z0-9/+=]{40}", NULL);

  /* GitHub Token */
  patterns_add(ps, "github_token", "ghp_", "ghp_[A-Za-z0-9]{36}", NULL);

  /* GitHub OAuth */
  patterns_add(ps, "github_oauth", "gho_", "gho_[A-Za-z0-9]{36}", NULL);

  /* Generic API Key */
  patterns_add(ps, "api_key", "api_key",
               "api[_-]?key[\"'\\s:=]+[A-Za-z0-9_-]{20,}", NULL);

  /* Generic Secret */
  patterns_add(ps, "generic_secret", "secret",
               "secret[\"'\\s:=]+[A-Za-z0-9_-]{8,}", NULL);

  /* Password in config */
  patterns_add(ps, "password", "password", "password[\"'\\s:=]+[^\\s\"']{4,}",
               NULL);

  /* Private Key Header */
  patterns_add(ps, "private_key", "-----BEGIN",
               "-----BEGIN[A-Z ]+PRIVATE KEY-----", NULL);

  /* JWT Token */
  patterns_add(ps, "jwt", "eyJ",
               "eyJ[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+", NULL);

  /* Slack Token */
  patterns_add(ps, "slack_token", "xox", "xox[baprs]-[0-9A-Za-z-]{10,}", NULL);

  /* Credit Card (basic) */
  patterns_add(ps, "credit_card", NULL,
               "\\b[0-9]{4}[- ]?[0-9]{4}[- ]?[0-9]{4}[- ]?[0-9]{4}\\b", NULL);

  /* Email Address */
  patterns_add(ps, "email", "@",
               "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}", NULL);

  /* IPv4 Address */
  patterns_add(ps, "ipv4", ".",
               "\\b[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\b", NULL);

  /* SSN */
  patterns_add(ps, "ssn", NULL, "\\b[0-9]{3}-[0-9]{2}-[0-9]{4}\\b", NULL);

  return ps->count > 0;
}
