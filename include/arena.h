/*
 * PlumbrC - Arena Allocator
 * Zero-allocation hot path through pre-allocated memory pools
 */

#ifndef PLUMBR_ARENA_H
#define PLUMBR_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t *base;     /* Base address of arena */
  size_t size;       /* Total size */
  size_t used;       /* Current usage */
  size_t high_water; /* Peak usage (for stats) */
  bool owns_memory;  /* True if we should free on destroy */
} Arena;

/* Initialize arena with given size (uses mmap) */
bool arena_init(Arena *arena, size_t size);

/* Allocate from arena (returns NULL if full) */
void *arena_alloc(Arena *arena, size_t size);

/* Reset arena (reuse memory, no syscalls) */
void arena_reset(Arena *arena);

/* Get stats */
size_t arena_used(const Arena *arena);

/* Destroy arena (free memory if owned) */
void arena_destroy(Arena *arena);

#endif /* PLUMBR_ARENA_H */
