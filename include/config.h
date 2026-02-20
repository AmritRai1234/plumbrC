/*
 * PlumbrC - High-Performance Log Redaction Pipeline
 * Configuration constants
 */

#ifndef PLUMBR_CONFIG_H
#define PLUMBR_CONFIG_H

#include <stddef.h>

/* Buffer sizes (L2 cache friendly) */
#define PLUMBR_READ_BUFFER_SIZE (64 * 1024)  /* 64KB */
#define PLUMBR_WRITE_BUFFER_SIZE (64 * 1024) /* 64KB */
#define PLUMBR_MAX_LINE_SIZE                                                   \
  (64 * 1024) /* 64KB max line (reduced for security) */

/* Arena allocator */
#define PLUMBR_ARENA_SIZE (128 * 1024 * 1024) /* 128MB main arena */
#define PLUMBR_SCRATCH_SIZE (1 * 1024 * 1024) /* 1MB scratch */

/* Pattern matching */
#define PLUMBR_MAX_PATTERNS 1024
#define PLUMBR_MAX_PATTERN_NAME 64
#define PLUMBR_MAX_LITERAL_LEN 256
#define PLUMBR_MAX_REPLACEMENT_LEN 128
#define PLUMBR_MAX_MATCHES_PER_LINE 64 /* Reduced for stack safety */

/* Aho-Corasick */
#define AC_ALPHABET_SIZE 256
#define AC_MAX_STATES (8 * 1024) /* 8K states — supports base + compliance */
#define PLUMBR_DFA_COMPRESSED 1  /* Use bitmap-compressed DFA (better cache) */
#define PLUMBR_TWO_TIER_AC                                                     \
  1                           /* L1-resident sentinel AC for fast line reject  \
                               */
#define PLUMBR_HOT_AC_SIZE 20 /* Patterns in L1-resident hot DFA (~24KB) */

/* I/O */
#define PLUMBR_IOV_BATCH 64 /* writev batch size */
#define PLUMBR_MAX_FILE_SIZE                                                   \
  (10ULL * 1024 * 1024 * 1024) /* 10GB rate limit                              \
                                */

/* Performance hints */
#define PLUMBR_LIKELY(x) __builtin_expect(!!(x), 1)
#define PLUMBR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define PLUMBR_PREFETCH(addr) __builtin_prefetch(addr, 0, 0)
#define PLUMBR_PREFETCH_W(addr) __builtin_prefetch(addr, 1, 0)

/* Alignment */
#define PLUMBR_CACHE_LINE 64
#define PLUMBR_ALIGNED __attribute__((aligned(PLUMBR_CACHE_LINE)))

/* Version */
#define PLUMBR_VERSION_MAJOR 1
#define PLUMBR_VERSION_MINOR 0
#define PLUMBR_VERSION_PATCH 0

#endif /* PLUMBR_CONFIG_H */
