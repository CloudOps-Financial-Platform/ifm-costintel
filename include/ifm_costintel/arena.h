#ifndef IFM_ARENA_H
#define IFM_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ifm_arena_block {
    struct ifm_arena_block *next;
    size_t capacity;
    size_t used;
    uint8_t data[];
} ifm_arena_block_t;

typedef struct {
    ifm_arena_block_t *head;
    ifm_arena_block_t *current;
    size_t default_block_size;
    size_t total_allocated;
} ifm_arena_t;

void ifm_arena_init(ifm_arena_t *arena, size_t default_block_size);
void *ifm_arena_alloc(ifm_arena_t *arena, size_t size);
void ifm_arena_reset(ifm_arena_t *arena);
void ifm_arena_destroy(ifm_arena_t *arena);

#ifdef __cplusplus
}
#endif

#endif /* IFM_ARENA_H */
