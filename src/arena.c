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

void arena_reset(Arena *arena) {
  /* SECURITY: Zero memory to prevent stale sensitive data exposure */
  if (arena->used > 0) {
    memset(arena->base, 0, arena->used);
  }
  arena->used = 0;
  /* Don't reset high_water - useful for stats */
}

size_t arena_used(const Arena *arena) { return arena->used; }

void arena_destroy(Arena *arena) {
  if (arena->owns_memory && arena->base != NULL) {
    munmap(arena->base, arena->size);
  }
  arena->base = NULL;
  arena->size = 0;
  arena->used = 0;
}
