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
  /* Flat DFA table — hot path, built after ac_build */
  int16_t *dfa; /* dfa[state * 256 + c] = next state (flat array) */
  ACMeta *meta; /* meta[state] = match metadata (compact) */
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
  ac->prefetch_hint = 1;     /* L2 default */

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

  /* Step 3: Build flat DFA table + compact metadata.
   * Separates hot data (transitions) from cold data (match info)
   * for optimal cache utilization. */
  size_t ns = ac->num_states;
  ac->dfa = arena_alloc(ac->arena, ns * AC_ALPHABET_SIZE * sizeof(int16_t));
  ac->meta = arena_alloc(ac->arena, ns * sizeof(ACMeta));
  if (!ac->dfa || !ac->meta)
    return false;

  for (size_t s = 0; s < ns; s++) {
    /* Copy transitions to flat DFA (int32_t → int16_t) */
    int16_t *row = &ac->dfa[s * AC_ALPHABET_SIZE];
    for (int c = 0; c < AC_ALPHABET_SIZE; c++) {
      row[c] = (int16_t)ac->states[s].goto_table[c];
    }
    /* Copy match metadata */
    ac->meta[s].output = ac->states[s].output;
    ac->meta[s].pattern_id = ac->states[s].pattern_id;
    ac->meta[s].depth = ac->states[s].depth;
    ac->meta[s].is_final = ac->states[s].is_final;
  }

  ac->built = true;
  return true;
}

void ac_set_prefetch(ACAutomaton *ac, int distance, int hint) {
  ac->prefetch_distance = distance > 0 ? distance : 1;
  ac->prefetch_hint = hint;
}

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
      while (ms != 0) {                                                        \
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

bool ac_search_first(const ACAutomaton *ac, const char *text, size_t len,
                     ACMatch *out_match) {
  if (!ac->built || len == 0) {
    return false;
  }

  const int16_t *dfa = ac->dfa;
  const ACMeta *meta = ac->meta;
  int16_t state = 0;

  for (size_t i = 0; i < len; i++) {
    uint8_t c = (uint8_t)text[i];
    state = dfa[state * AC_ALPHABET_SIZE + c];

    /* Check for match */
    int16_t match_state = state;
    while (match_state != 0) {
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

size_t ac_pattern_count(const ACAutomaton *ac) { return ac->num_patterns; }

size_t ac_state_count(const ACAutomaton *ac) { return ac->num_states; }
