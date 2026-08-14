#ifndef IFM_AGGREGATION_H
#define IFM_AGGREGATION_H

#include "ifm_costintel/ifm_types.h"
#include "ifm_costintel/arena.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IFM_AGG_DIM_PROVIDER,
    IFM_AGG_DIM_ACCOUNT,
    IFM_AGG_DIM_COST_CENTER,
    IFM_AGG_DIM_RESOURCE
} ifm_agg_dim_t;

typedef struct ifm_agg_entry {
    char key[256];
    uint64_t record_count;
    ifm_micros_t total_spend_micros;
    bool has_overflow;
    struct ifm_agg_entry *next;
} ifm_agg_entry_t;

typedef struct {
    ifm_agg_dim_t dimension;
    ifm_agg_entry_t **buckets;
    size_t bucket_count;
    size_t entry_count;
    ifm_micros_t grand_total_micros;
    ifm_arena_t *arena;
} ifm_aggregation_table_t;

/* Initialize aggregation table for specific dimension */
bool ifm_agg_table_init(ifm_aggregation_table_t *table, ifm_agg_dim_t dim, size_t bucket_count, ifm_arena_t *arena);

/* Accumulate a record into the aggregation table */
bool ifm_agg_table_accumulate(ifm_aggregation_table_t *table, const ifm_record_t *record);

/* Get list of sorted entries (sorted descending by spend with key tie-break) */
size_t ifm_agg_table_get_sorted_entries(const ifm_aggregation_table_t *table, ifm_agg_entry_t **out_entries, size_t max_entries);

/* Cleanup table */
void ifm_agg_table_cleanup(ifm_aggregation_table_t *table);

#ifdef __cplusplus
}
#endif

#endif /* IFM_AGGREGATION_H */
