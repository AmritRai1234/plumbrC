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

/* Initialize arena with external memory */
void arena_init_with_buffer(Arena *arena, void *buffer, size_t size);

/* Allocate from arena (returns NULL if full) */
void *arena_alloc(Arena *arena, size_t size);

/* Allocate aligned memory */
void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);

/* Reset arena (reuse memory, no syscalls) */
void arena_reset(Arena *arena);

/* Get remaining space */
size_t arena_remaining(const Arena *arena);

/* Get stats */
size_t arena_used(const Arena *arena);
size_t arena_high_water(const Arena *arena);

/* Destroy arena (free memory if owned) */
void arena_destroy(Arena *arena);

/*
 * Scratch arena - for temporary allocations within a scope
 * Usage:
 *   ScratchArena scratch = scratch_begin(&main_arena);
 *   void *tmp = arena_alloc(&scratch.arena, 1024);
 *   // use tmp...
 *   scratch_end(&scratch);  // automatically resets
 */
typedef struct {
  Arena *parent;
  size_t saved_used;
} ScratchArena;

ScratchArena scratch_begin(Arena *parent);
void scratch_end(ScratchArena *scratch);

#endif /* PLUMBR_ARENA_H */
