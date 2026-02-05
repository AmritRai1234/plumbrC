/*
 * PlumbrC - Arena Allocator Implementation
 */

#include "arena.h"
#include "config.h"

#include <string.h>
#include <sys/mman.h>

bool arena_init(Arena *arena, size_t size) {
  /* Use mmap for large allocations - no heap fragmentation */
  void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (mem == MAP_FAILED) {
    return false;
  }

  arena->base = (uint8_t *)mem;
  arena->size = size;
  arena->used = 0;
  arena->high_water = 0;
  arena->owns_memory = true;

  return true;
}

void arena_init_with_buffer(Arena *arena, void *buffer, size_t size) {
  arena->base = (uint8_t *)buffer;
  arena->size = size;
  arena->used = 0;
  arena->high_water = 0;
  arena->owns_memory = false;
}

void *arena_alloc(Arena *arena, size_t size) {
  /* SECURITY: Check for overflow when aligning */
  if (PLUMBR_UNLIKELY(size > SIZE_MAX - 7)) {
    return NULL;
  }

  /* Align to 8 bytes for safety */
  size_t aligned_size = (size + 7) & ~7UL;

  /* SECURITY: Check for overflow in used + aligned_size */
  if (PLUMBR_UNLIKELY(aligned_size > arena->size - arena->used)) {
    return NULL;
  }

  void *ptr = arena->base + arena->used;
  arena->used += aligned_size;

  if (arena->used > arena->high_water) {
    arena->high_water = arena->used;
  }

  return ptr;
}

void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment) {
  /* SECURITY: Validate alignment is power of 2 and non-zero */
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  size_t mask = alignment - 1;
  size_t current = (size_t)(arena->base + arena->used);
  size_t aligned = (current + mask) & ~mask;
  size_t padding = aligned - current;

  /* SECURITY: Check for overflow in padding + size */
  if (PLUMBR_UNLIKELY(padding > SIZE_MAX - size)) {
    return NULL;
  }
  size_t total = padding + size;

  /* SECURITY: Check for overflow in used + total */
  if (PLUMBR_UNLIKELY(total > arena->size - arena->used)) {
    return NULL;
  }

  void *ptr = arena->base + arena->used + padding;
  arena->used += total;

  if (arena->used > arena->high_water) {
    arena->high_water = arena->used;
  }

  return ptr;
}

void arena_reset(Arena *arena) {
  arena->used = 0;
  /* Don't reset high_water - useful for stats */
}

size_t arena_remaining(const Arena *arena) { return arena->size - arena->used; }

size_t arena_used(const Arena *arena) { return arena->used; }

size_t arena_high_water(const Arena *arena) { return arena->high_water; }

void arena_destroy(Arena *arena) {
  if (arena->owns_memory && arena->base != NULL) {
    munmap(arena->base, arena->size);
  }
  arena->base = NULL;
  arena->size = 0;
  arena->used = 0;
}

ScratchArena scratch_begin(Arena *parent) {
  ScratchArena scratch;
  scratch.parent = parent;
  scratch.saved_used = parent->used;
  return scratch;
}

void scratch_end(ScratchArena *scratch) {
  scratch->parent->used = scratch->saved_used;
}
