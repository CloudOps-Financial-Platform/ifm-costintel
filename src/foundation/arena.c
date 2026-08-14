#include "ifm_costintel/arena.h"
#include <stdlib.h>
#include <string.h>

#define IFM_ARENA_ALIGNMENT 8
#define IFM_ARENA_ALIGN(size) (((size) + (IFM_ARENA_ALIGNMENT - 1)) & ~(IFM_ARENA_ALIGNMENT - 1))

static ifm_arena_block_t *ifm_arena_create_block(size_t capacity) {
    ifm_arena_block_t *block = (ifm_arena_block_t *)malloc(sizeof(ifm_arena_block_t) + capacity);
    if (!block) return NULL;
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
    return block;
}

void ifm_arena_init(ifm_arena_t *arena, size_t default_block_size) {
    if (!arena) return;
    if (default_block_size < 4096) default_block_size = 65536;
    arena->default_block_size = default_block_size;
    arena->head = ifm_arena_create_block(default_block_size);
    arena->current = arena->head;
    arena->total_allocated = arena->head ? default_block_size : 0;
}

void *ifm_arena_alloc(ifm_arena_t *arena, size_t size) {
    if (!arena || size == 0) return NULL;
    size_t aligned_size = IFM_ARENA_ALIGN(size);

    if (arena->current && (arena->current->capacity - arena->current->used >= aligned_size)) {
        void *ptr = arena->current->data + arena->current->used;
        arena->current->used += aligned_size;
        return ptr;
    }

    size_t new_cap = (aligned_size > arena->default_block_size) ? aligned_size : arena->default_block_size;
    ifm_arena_block_t *new_block = NULL;

    if (arena->current && arena->current->next) {
        if (arena->current->next->capacity >= aligned_size) {
            new_block = arena->current->next;
            new_block->used = 0;
        }
    }

    if (!new_block) {
        new_block = ifm_arena_create_block(new_cap);
        if (!new_block) return NULL;
        if (arena->current) {
            new_block->next = arena->current->next;
            arena->current->next = new_block;
        } else {
            arena->head = new_block;
        }
        arena->total_allocated += new_cap;
    }

    arena->current = new_block;
    void *ptr = new_block->data + new_block->used;
    new_block->used += aligned_size;
    return ptr;
}

void ifm_arena_reset(ifm_arena_t *arena) {
    if (!arena) return;
    ifm_arena_block_t *curr = arena->head;
    while (curr) {
        curr->used = 0;
        curr = curr->next;
    }
    arena->current = arena->head;
}

void ifm_arena_destroy(ifm_arena_t *arena) {
    if (!arena) return;
    ifm_arena_block_t *curr = arena->head;
    while (curr) {
        ifm_arena_block_t *next = curr->next;
        free(curr);
        curr = next;
    }
    arena->head = NULL;
    arena->current = NULL;
    arena->total_allocated = 0;
}
