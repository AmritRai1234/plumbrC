/*
 * PlumbrC - Aho-Corasick Implementation
 *
 * Standard AC automaton with:
 * - Compact state representation
 * - Failure link optimization
 * - Output link chains for overlapping matches
 */

#include "aho_corasick.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

/* State representation */
typedef struct ACState {
  int32_t
      goto_table[AC_ALPHABET_SIZE]; /* Transition table (-1 = no transition) */
  int32_t fail;                     /* Failure link */
  int32_t output;                   /* Output link (for chained outputs) */
  uint32_t pattern_id;              /* Pattern ID if this is final state */
  uint16_t depth;                   /* Depth in trie (= pattern length) */
  bool is_final;                    /* True if accepting state */
} ACState;

/* Compact match metadata — cold path, only accessed on matches */
typedef struct {
  int32_t output;      /* Output link for chained matches */
  uint32_t pattern_id; /* Pattern ID if final state */
  uint16_t depth;      /* Pattern length */
  bool is_final;       /* True if accepting state */
} ACMeta;

#if PLUMBR_DFA_COMPRESSED
/* Per-state index into compressed data buffer */
typedef struct {
  uint32_t offset;         /* Byte offset into compressed buffer */
  uint8_t num_transitions; /* Number of non-default transitions */
} RowIndex;
#endif

struct ACAutomaton {
  ACState *states; /* Build-time state array (dense goto_table) */
  size_t num_states;
  size_t capacity;
  size_t num_patterns;
  bool built;
  Arena *arena;
  /* Runtime prefetch tuning (set by hwdetect) */
  int prefetch_distance;
  int prefetch_hint;
  bool force_flat; /* When true, skip compression — use flat 256-wide DFA */
  /* Flat DFA table — hot path, built after ac_build */
  int16_t *dfa;      /* dfa[state * 256 + c] = next state (flat array) */
  ACMeta *meta;      /* meta[state] = match metadata (compact) */
  size_t dfa_memory; /* Total DFA memory in bytes */
#if PLUMBR_DFA_COMPRESSED
  /* Compressed DFA — root stays flat, rest use bitmap-indexed sparse rows */
  int16_t *root_row;      /* Flat 256-wide row for state 0 (hot path) */
  uint8_t *compressed;    /* Compressed row data buffer */
  RowIndex *row_index;    /* Per-state offset/count index */
  size_t compressed_size; /* Total compressed buffer size */
#endif
};

/* Queue for BFS during build */
typedef struct {
  int32_t *data;
  size_t head;
  size_t tail;
  size_t capacity;
} Queue;

static bool queue_init(Queue *q, Arena *arena, size_t capacity) {
  q->data = arena_alloc(arena, capacity * sizeof(int32_t));
  if (!q->data)
    return false;
  q->head = 0;
  q->tail = 0;
  q->capacity = capacity;
  return true;
}

static void queue_push(Queue *q, int32_t value) { q->data[q->tail++] = value; }

static int32_t queue_pop(Queue *q) { return q->data[q->head++]; }

static bool queue_empty(const Queue *q) { return q->head >= q->tail; }

ACAutomaton *ac_create(Arena *arena) {
  ACAutomaton *ac = arena_alloc(arena, sizeof(ACAutomaton));
  if (!ac)
    return NULL;

  ac->arena = arena;
  ac->capacity = AC_MAX_STATES;
  ac->states = arena_alloc(arena, ac->capacity * sizeof(ACState));
  if (!ac->states)
    return NULL;

  /* Initialize root state */
  ac->num_states = 1;
  ACState *root = &ac->states[0];
  memset(root->goto_table, -1, sizeof(root->goto_table));
  root->fail = 0;
  root->output = -1;
  root->pattern_id = 0;
  root->depth = 0;
  root->is_final = false;

  ac->num_patterns = 0;
  ac->built = false;
  ac->prefetch_distance = 1; /* Safe default */
  ac->prefetch_hint = 0;     /* L1 locality */
  ac->force_flat = false;

  return ac;
}

static int32_t ac_new_state(ACAutomaton *ac, uint16_t depth) {
  if (ac->num_states >= ac->capacity) {
    return -1; /* Out of states */
  }

  int32_t state_id = (int32_t)ac->num_states++;
  ACState *state = &ac->states[state_id];

  memset(state->goto_table, -1, sizeof(state->goto_table));
  state->fail = 0;
  state->output = -1;
  state->pattern_id = 0;
  state->depth = depth;
  state->is_final = false;

  return state_id;
}

bool ac_add_pattern(ACAutomaton *ac, const char *pattern, size_t len,
                    uint32_t pattern_id) {
  if (ac->built) {
    return false; /* Cannot add after build */
  }

  if (len == 0) {
    return false; /* Empty pattern not allowed */
  }

  int32_t state = 0;

  for (size_t i = 0; i < len; i++) {
    uint8_t c = (uint8_t)pattern[i];
    int32_t next = ac->states[state].goto_table[c];

    if (next == -1) {
      next = ac_new_state(ac, (uint16_t)(i + 1));
      if (next == -1) {
        return false; /* Out of states */
      }
      ac->states[state].goto_table[c] = next;
    }

    state = next;
  }

  /* Mark as final state */
  ac->states[state].is_final = true;
  ac->states[state].pattern_id = pattern_id;
  ac->num_patterns++;

  return true;
}

bool ac_build(ACAutomaton *ac) {
  if (ac->built) {
    return true; /* Already built */
  }

  Queue q;
  if (!queue_init(&q, ac->arena, ac->num_states)) {
    return false;
  }

  /* Step 1: Complete root — all missing transitions loop back to root */
  ACState *root = &ac->states[0];
  for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
    int32_t s = root->goto_table[c];
    if (s != -1) {
      ac->states[s].fail = 0;
      queue_push(&q, s);
    } else {
      root->goto_table[c] = 0; /* Stay at root */
    }
  }

  /* Step 2: BFS with DFA completion.
   * For each state r and character c:
   *   - If r has a transition for c → set failure link as usual
   *   - If r does NOT have a transition for c → copy from failure state
   * Since BFS processes level-by-level and root is fully complete,
   * failure states are always already completed when we reach deeper states.
   * Result: every goto_table[c] is valid → zero failure link chasing at search
   * time. */
  while (!queue_empty(&q)) {
    int32_t r = queue_pop(&q);
    ACState *state_r = &ac->states[r];

    for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
      int32_t s = state_r->goto_table[c];

      if (s != -1) {
        /* Real transition exists — compute failure link */
        queue_push(&q, s);

        /* Failure link = follow parent's fail until we find a transition for c
         */
        int32_t f = state_r->fail;
        ac->states[s].fail = ac->states[f].goto_table[c];
        /* Safe: f's goto_table is already DFA-completed (BFS order) */

        /* Build output links */
        int32_t fail_s = ac->states[s].fail;
        if (ac->states[fail_s].is_final) {
          ac->states[s].output = fail_s;
        } else {
          ac->states[s].output = ac->states[fail_s].output;
        }
      } else {
        /* No transition — DFA completion: copy from failure state */
        state_r->goto_table[c] = ac->states[state_r->fail].goto_table[c];
      }
    }
  }

  /* Step 3: Build DFA tables + compact metadata.
   * Separates hot data (transitions) from cold data (match info)
   * for optimal cache utilization. */
  size_t ns = ac->num_states;

  /* SECURITY: Guard against int16_t truncation in DFA table.
   * State IDs stored as int16_t — must fit in [-32768, 32767]. */
  if (ns > 32767) {
    fprintf(stderr,
            "AC build error: %zu states exceeds int16_t max (32767). "
            "Reduce pattern count.\n",
            ns);
    return false;
  }

  /* Always build metadata array */
  ac->meta = arena_alloc(ac->arena, ns * sizeof(ACMeta));
  if (!ac->meta)
    return false;

  for (size_t s = 0; s < ns; s++) {
    ac->meta[s].output = ac->states[s].output;
    ac->meta[s].pattern_id = ac->states[s].pattern_id;
    ac->meta[s].depth = ac->states[s].depth;
    ac->meta[s].is_final = ac->states[s].is_final;
  }

#if PLUMBR_DFA_COMPRESSED
  if (ac->force_flat) {
    /* Force flat DFA for this automaton (used by hot AC for L1 speed).
     * Each transition = single array lookup: dfa[state * 256 + c] */
    ac->dfa = arena_alloc(ac->arena, ns * AC_ALPHABET_SIZE * sizeof(int16_t));
    if (!ac->dfa)
      return false;
    for (size_t s = 0; s < ns; s++) {
      int16_t *row = &ac->dfa[s * AC_ALPHABET_SIZE];
      for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
        row[c] = (int16_t)ac->states[s].goto_table[c];
      }
    }
    ac->dfa_memory =
        ns * AC_ALPHABET_SIZE * sizeof(int16_t) + ns * sizeof(ACMeta);
    ac->root_row = NULL;
    ac->compressed = NULL;
    ac->row_index = NULL;
  } else {
    /* Build compressed DFA: root row flat, rest bitmap-indexed sparse rows.
     *
     * For each non-root state, find the most common target (default_state),
     * build a 256-bit bitmap of chars with non-default transitions, and
     * pack only the non-default transitions densely.
     *
     * Layout per compressed row:
     *   int16_t default_state    (2 bytes)
     *   uint64_t bitmap[4]       (32 bytes)  — 256-bit bitmask
     *   int16_t transitions[N]   (N * 2 bytes) — only non-default
     */

    /* Root row stays flat (hit on every byte, usually many unique transitions)
     */
    ac->root_row = arena_alloc(ac->arena, AC_ALPHABET_SIZE * sizeof(int16_t));
    if (!ac->root_row)
      return false;
    for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
      ac->root_row[c] = (int16_t)ac->states[0].goto_table[c];
    }

    /* Index array for all states (root included for uniform indexing) */
    ac->row_index = arena_alloc(ac->arena, ns * sizeof(RowIndex));
    if (!ac->row_index)
      return false;

    /* Per-state default targets — stored temporarily for the two-pass build */
    int32_t *def_targets = arena_alloc(ac->arena, ns * sizeof(int32_t));
    if (!def_targets)
      return false;

    /* First pass: find default target per state and compute compressed sizes.
     * For each state, the "default target" is the goto_table entry that appears
     * most often across all 256 characters. We find it by scanning the
     * 256-entry table — no auxiliary array indexed by state count needed. */
    size_t total_compressed = 0;
    ac->row_index[0].offset = 0;
    ac->row_index[0].num_transitions = 0; /* Root uses flat row */
    def_targets[0] = 0;

    for (size_t s = 1; s < ns; s++) {
      /* Find most common target by comparing each entry to candidates.
       * Since most rows have 250+ identical entries (the failure default),
       * we just pick the first goto target and count how many match it,
       * then check if any other target appears more. In practice, the
       * failure-inherited target dominates (>95% of entries). */
      int32_t candidate = ac->states[s].goto_table[0];
      int candidate_count = 0;
      for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
        if (ac->states[s].goto_table[c] == candidate) {
          candidate_count++;
        }
      }

      /* If candidate doesn't have majority, scan for a better one.
       * This is rare but handles the edge case correctly. */
      if (candidate_count <= 128) {
        for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
          int32_t t = ac->states[s].goto_table[c];
          if (t != candidate) {
            int cnt = 0;
            for (int c2 = 0; c2 < AC_ALPHABET_SIZE; c2++) {
              if (ac->states[s].goto_table[c2] == t)
                cnt++;
            }
            if (cnt > candidate_count) {
              candidate = t;
              candidate_count = cnt;
              if (cnt > 128)
                break; /* Majority found */
            }
          }
        }
      }

      def_targets[s] = candidate;

      /* Count non-default transitions */
      uint8_t num_trans = 0;
      for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
        if (ac->states[s].goto_table[c] != candidate) {
          num_trans++;
        }
      }

      ac->row_index[s].offset = (uint32_t)total_compressed;
      ac->row_index[s].num_transitions = num_trans;
      /* Row size: 2 (default) + 32 (bitmap) + num_trans * 2 (transitions) */
      total_compressed += 2 + 32 + (size_t)num_trans * 2;
    }

    /* Allocate compressed buffer */
    ac->compressed = arena_alloc(ac->arena, total_compressed);
    if (!ac->compressed)
      return false;
    ac->compressed_size = total_compressed;

    /* Second pass: fill compressed data using stored def_targets */
    for (size_t s = 1; s < ns; s++) {
      uint8_t *row = ac->compressed + ac->row_index[s].offset;
      int32_t def_target = def_targets[s];

      /* Write default_state */
      int16_t def16 = (int16_t)def_target;
      memcpy(row, &def16, 2);

      /* Build bitmap and pack transitions */
      uint64_t bitmap[4] = {0, 0, 0, 0};
      int16_t *trans = (int16_t *)(row + 2 + 32); /* After default + bitmap */
      int trans_idx = 0;

      for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
        int32_t target = ac->states[s].goto_table[c];
        if (target != def_target) {
          bitmap[c >> 6] |= (1ULL << (c & 63));
          trans[trans_idx++] = (int16_t)target;
        }
      }

      /* Write bitmap */
      memcpy(row + 2, bitmap, 32);
    }

    /* DFA memory = root row + index + compressed data + metadata */
    ac->dfa_memory = AC_ALPHABET_SIZE * sizeof(int16_t) /* root_row */
                     + ns * sizeof(RowIndex)            /* row_index */
                     + total_compressed                 /* compressed */
                     + ns * sizeof(ACMeta);             /* meta */

    /* Also keep flat dfa pointer NULL so old code paths don't use it */
    ac->dfa = NULL;
  } /* end !force_flat */

#else /* !PLUMBR_DFA_COMPRESSED — original flat DFA */

  ac->dfa = arena_alloc(ac->arena, ns * AC_ALPHABET_SIZE * sizeof(int16_t));
  if (!ac->dfa)
    return false;

  for (size_t s = 0; s < ns; s++) {
    int16_t *row = &ac->dfa[s * AC_ALPHABET_SIZE];
    for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
      row[c] = (int16_t)ac->states[s].goto_table[c];
    }
  }

  ac->dfa_memory =
      ns * AC_ALPHABET_SIZE * sizeof(int16_t) + ns * sizeof(ACMeta);

#endif /* PLUMBR_DFA_COMPRESSED */

  ac->built = true;
  return true;
}

void ac_set_prefetch(ACAutomaton *ac, int distance, int hint) {
  ac->prefetch_distance = distance;
  ac->prefetch_hint = hint;
}

void ac_set_force_flat(ACAutomaton *ac) {
  if (ac)
    ac->force_flat = true;
}

#if PLUMBR_DFA_COMPRESSED

/*
 * Compressed DFA lookup — bitmap-indexed sparse row.
 * Uses popcount to find the index of a character's transition.
 *
 * Row layout: [default_state:2B] [bitmap:32B] [transitions:N*2B]
 */
static inline int16_t compressed_lookup(const uint8_t *compressed,
                                        const RowIndex *idx, int16_t state,
                                        uint8_t c) {
  const uint8_t *row = compressed + idx[state].offset;

  /* Read default_state */
  int16_t def;
  memcpy(&def, row, 2);

  /* Read bitmap */
  const uint64_t *bm = (const uint64_t *)(row + 2);
  int word = c >> 6; /* Which 64-bit word (0-3) */
  int bit = c & 63;  /* Which bit in word */

  if (!(bm[word] & (1ULL << bit)))
    return def; /* Default transition — fast path */

  /* Count set bits below this position = index into packed transitions */
  int pos = 0;
  for (int w = 0; w < word; w++)
    pos += __builtin_popcountll(bm[w]);
  pos += __builtin_popcountll(bm[word] & ((1ULL << bit) - 1));

  const int16_t *trans = (const int16_t *)(row + 2 + 32);
  return trans[pos];
}

void ac_search(const ACAutomaton *ac, const char *text, size_t len,
               ACMatchCallback callback, void *user_data) {
  if (!ac->built || len == 0)
    return;

  const ACMeta *meta = ac->meta;

  /* Fast path: flat DFA (used by force_flat hot AC) */
  if (ac->dfa) {
    const int16_t *dfa = ac->dfa;
    int16_t state = 0;
    for (size_t i = 0; i < len; i++) {
      state = dfa[state * AC_ALPHABET_SIZE + (uint8_t)text[i]];
      int16_t ms = state;
      while (ms > 0) {
        if (meta[ms].is_final) {
          ACMatch m;
          m.position = i;
          m.pattern_id = meta[ms].pattern_id;
          m.length = meta[ms].depth;
          if (!callback(&m, user_data))
            return;
        }
        ms = (int16_t)meta[ms].output;
      }
    }
    return;
  }

  /* Compressed DFA path */
  const int16_t *root = ac->root_row;
  const uint8_t *comp = ac->compressed;
  const RowIndex *ridx = ac->row_index;
  int16_t state = 0;

  for (size_t i = 0; i < len; i++) {
    uint8_t c = (uint8_t)text[i];

    /* Root state: flat 256-wide lookup (hot path, ~50% of transitions) */
    if (state == 0) {
      state = root[c];
    } else {
      /* Non-root: compressed bitmap lookup */
      state = compressed_lookup(comp, ridx, state, c);
    }

    /* Prefetch next compressed row header */
    if (state != 0) {
      __builtin_prefetch(comp + ridx[state].offset, 0, 1);
    }

    /* Check for matches (cold path) */
    int16_t ms = state;
    while (ms > 0) {
      if (meta[ms].is_final) {
        ACMatch m;
        m.position = i;
        m.pattern_id = meta[ms].pattern_id;
        m.length = meta[ms].depth;
        if (!callback(&m, user_data))
          return;
      }
      ms = (int16_t)meta[ms].output;
    }
  }
}

#else /* !PLUMBR_DFA_COMPRESSED — original flat DFA search */

/*
 * Inner search loop macro — uses flat DFA table for cache-optimal transitions.
 * dfa[state * 256 + c] = next state (int16_t, contiguous memory).
 * Match metadata in separate cold array, only touched on actual matches.
 * LOCALITY is a compile-time constant (3=L1, 1=L2).
 */
#define AC_SEARCH_LOOP(ac, text, len, callback, user_data, pf_dist, LOCALITY)  \
  do {                                                                         \
    const int16_t *dfa = (ac)->dfa;                                            \
    const ACMeta *meta = (ac)->meta;                                           \
    int16_t state = 0;                                                         \
    for (size_t i = 0; i < (len); i++) {                                       \
      uint8_t c = (uint8_t)(text)[i];                                          \
      state = dfa[state * AC_ALPHABET_SIZE + c]; /* flat array lookup */       \
      if (i + (pf_dist) < (len)) {                                             \
        uint8_t fc = (uint8_t)(text)[i + (pf_dist)];                           \
        __builtin_prefetch(&dfa[state * AC_ALPHABET_SIZE + fc], 0, LOCALITY);  \
      }                                                                        \
      int16_t ms = state;                                                      \
      while (ms > 0) {                                                         \
        if (meta[ms].is_final) {                                               \
          ACMatch m;                                                           \
          m.position = i;                                                      \
          m.pattern_id = meta[ms].pattern_id;                                  \
          m.length = meta[ms].depth;                                           \
          if (!(callback)(&m, (user_data)))                                    \
            return;                                                            \
        }                                                                      \
        ms = (int16_t)meta[ms].output;                                         \
      }                                                                        \
    }                                                                          \
  } while (0)

void ac_search(const ACAutomaton *ac, const char *text, size_t len,
               ACMatchCallback callback, void *user_data) {
  if (!ac->built || len == 0) {
    return;
  }

  const int pf_dist = ac->prefetch_distance;

  /* Dispatch once — no branch inside the hot loop */
  if (ac->prefetch_hint == 0) {
    AC_SEARCH_LOOP(ac, text, len, callback, user_data, pf_dist, 3); /* L1 */
  } else {
    AC_SEARCH_LOOP(ac, text, len, callback, user_data, pf_dist, 1); /* L2 */
  }
}
#endif /* PLUMBR_DFA_COMPRESSED */

bool ac_search_first(const ACAutomaton *ac, const char *text, size_t len,
                     ACMatch *out_match) {
  if (!ac->built || len == 0) {
    return false;
  }

  const ACMeta *meta = ac->meta;
  int16_t state = 0;

#if PLUMBR_DFA_COMPRESSED
  const int16_t *root = ac->root_row;
  const uint8_t *comp = ac->compressed;
  const RowIndex *ridx = ac->row_index;
#else
  const int16_t *dfa = ac->dfa;
#endif

  for (size_t i = 0; i < len; i++) {
    uint8_t c = (uint8_t)text[i];

#if PLUMBR_DFA_COMPRESSED
    state = (state == 0) ? root[c] : compressed_lookup(comp, ridx, state, c);
#else
    state = dfa[state * AC_ALPHABET_SIZE + c];
#endif

    /* Check for match */
    int16_t match_state = state;
    while (match_state > 0) {
      if (meta[match_state].is_final) {
        out_match->position = i;
        out_match->pattern_id = meta[match_state].pattern_id;
        out_match->length = meta[match_state].depth;
        return true;
      }
      match_state = (int16_t)meta[match_state].output;
    }
  }

  return false;
}

/* Callback context for ac_search_all */
typedef struct {
  ACMatch *matches;
  size_t count;
  size_t max;
} SearchAllContext;

static bool search_all_callback(const ACMatch *match, void *user_data) {
  SearchAllContext *ctx = user_data;
  if (ctx->count >= ctx->max) {
    return false; /* Stop - buffer full */
  }
  ctx->matches[ctx->count++] = *match;
  return true;
}

size_t ac_search_all(const ACAutomaton *ac, const char *text, size_t len,
                     ACMatch *matches, size_t max_matches) {
  SearchAllContext ctx = {.matches = matches, .count = 0, .max = max_matches};

  ac_search(ac, text, len, search_all_callback, &ctx);
  return ctx.count;
}

bool ac_search_has_match(const ACAutomaton *ac, const char *text, size_t len) {
  if (!ac->built || len == 0)
    return false;

  const ACMeta *meta = ac->meta;
  int16_t state = 0;

  /* Fast path: flat DFA (force_flat automata) */
  if (ac->dfa) {
    const int16_t *dfa = ac->dfa;
    for (size_t i = 0; i < len; i++) {
      state = dfa[state * AC_ALPHABET_SIZE + (uint8_t)text[i]];
      int16_t ms = state;
      while (ms > 0) {
        if (meta[ms].is_final)
          return true;
        ms = (int16_t)meta[ms].output;
      }
    }
    return false;
  }

#if PLUMBR_DFA_COMPRESSED
  const int16_t *root = ac->root_row;
  const uint8_t *comp = ac->compressed;
  const RowIndex *ridx = ac->row_index;
#else
  const int16_t *dfa = ac->dfa;
#endif

  for (size_t i = 0; i < len; i++) {
    uint8_t c = (uint8_t)text[i];

#if PLUMBR_DFA_COMPRESSED
    state = (state == 0) ? root[c] : compressed_lookup(comp, ridx, state, c);
#else
    state = dfa[state * AC_ALPHABET_SIZE + c];
#endif

    /* Check for any match — return immediately on first hit */
    int16_t ms = state;
    while (ms > 0) {
      if (meta[ms].is_final)
        return true;
      ms = (int16_t)meta[ms].output;
    }
  }

  return false;
}

size_t ac_dfa_memory(const ACAutomaton *ac) { return ac ? ac->dfa_memory : 0; }

const int16_t *ac_get_root_transitions(const ACAutomaton *ac) {
  if (!ac) {
    return NULL;
  }
#if PLUMBR_DFA_COMPRESSED
  if (ac->root_row)
    return ac->root_row;
  /* force_flat automata: root = first 256 entries of flat DFA */
  if (ac->dfa)
    return &ac->dfa[0];
  return NULL;
#else
  if (!ac->dfa)
    return NULL;
  return &ac->dfa[0]; /* State 0 (root) row */
#endif
}
