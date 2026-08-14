#include "ifm_costintel/aggregation.h"
#include "ifm_costintel/fak.h"
#include <string.h>
#include <stdlib.h>

static inline void safe_strcpy(char *dest, size_t dest_cap, const char *src) {
    if (!dest || dest_cap == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dest_cap) len = dest_cap - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static uint64_t fnv1a_hash(const char *str) {
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool ifm_agg_table_init(ifm_aggregation_table_t *table, ifm_agg_dim_t dim, size_t bucket_count, ifm_arena_t *arena) {
    if (!table) return false;
    if (bucket_count < 16) bucket_count = 1024;
    table->dimension = dim;
    table->bucket_count = bucket_count;
    table->entry_count = 0;
    table->grand_total_micros = 0;
    table->arena = arena;

    if (arena) {
        table->buckets = (ifm_agg_entry_t **)ifm_arena_alloc(arena, bucket_count * sizeof(ifm_agg_entry_t *));
    } else {
        table->buckets = (ifm_agg_entry_t **)calloc(bucket_count, sizeof(ifm_agg_entry_t *));
    }

    if (!table->buckets) return false;
    if (arena) {
        memset(table->buckets, 0, bucket_count * sizeof(ifm_agg_entry_t *));
    }
    return true;
}

static const char *get_record_dim_key(const ifm_aggregation_table_t *table, const ifm_record_t *record) {
    switch (table->dimension) {
        case IFM_AGG_DIM_PROVIDER:
            return record->provider[0] ? record->provider : "UNKNOWN_PROVIDER";
        case IFM_AGG_DIM_ACCOUNT:
            return record->account_id[0] ? record->account_id : "UNKNOWN_ACCOUNT";
        case IFM_AGG_DIM_COST_CENTER:
            return record->cost_center_id[0] ? record->cost_center_id : "UNALLOCATED";
        case IFM_AGG_DIM_RESOURCE:
            return record->resource_id[0] ? record->resource_id : "UNKNOWN_RESOURCE";
        default:
            return "UNKNOWN";
    }
}

bool ifm_agg_table_accumulate(ifm_aggregation_table_t *table, const ifm_record_t *record) {
    if (!table || !record) return false;

    const char *key = get_record_dim_key(table, record);
    uint64_t hash = fnv1a_hash(key);
    size_t b_idx = hash % table->bucket_count;

    ifm_agg_entry_t *curr = table->buckets[b_idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            curr->record_count++;
            ifm_micros_t new_total;
            if (!fak_add_micros(curr->total_spend_micros, record->active_spend_micros, &new_total)) {
                curr->has_overflow = true;
            } else {
                curr->total_spend_micros = new_total;
            }

            ifm_micros_t new_grand;
            if (fak_add_micros(table->grand_total_micros, record->active_spend_micros, &new_grand)) {
                table->grand_total_micros = new_grand;
            }
            return true;
        }
        curr = curr->next;
    }

    ifm_agg_entry_t *new_entry = NULL;
    if (table->arena) {
        new_entry = (ifm_agg_entry_t *)ifm_arena_alloc(table->arena, sizeof(ifm_agg_entry_t));
    } else {
        new_entry = (ifm_agg_entry_t *)malloc(sizeof(ifm_agg_entry_t));
    }
    if (!new_entry) return false;

    memset(new_entry, 0, sizeof(ifm_agg_entry_t));
    safe_strcpy(new_entry->key, sizeof(new_entry->key), key);
    new_entry->record_count = 1;
    new_entry->total_spend_micros = record->active_spend_micros;
    new_entry->has_overflow = false;
    new_entry->next = table->buckets[b_idx];
    table->buckets[b_idx] = new_entry;
    table->entry_count++;

    ifm_micros_t new_grand;
    if (fak_add_micros(table->grand_total_micros, record->active_spend_micros, &new_grand)) {
        table->grand_total_micros = new_grand;
    }
    return true;
}

static int compare_agg_entries(const void *a, const void *b) {
    const ifm_agg_entry_t *ea = *(const ifm_agg_entry_t **)a;
    const ifm_agg_entry_t *eb = *(const ifm_agg_entry_t **)b;

    if (ea->total_spend_micros > eb->total_spend_micros) return -1;
    if (ea->total_spend_micros < eb->total_spend_micros) return 1;

    return strcmp(ea->key, eb->key);
}

size_t ifm_agg_table_get_sorted_entries(const ifm_aggregation_table_t *table, ifm_agg_entry_t **out_entries, size_t max_entries) {
    if (!table || !out_entries || max_entries == 0) return 0;

    size_t count = 0;
    for (size_t i = 0; i < table->bucket_count && count < max_entries; ++i) {
        ifm_agg_entry_t *curr = table->buckets[i];
        while (curr && count < max_entries) {
            out_entries[count++] = curr;
            curr = curr->next;
        }
    }

    if (count > 1) {
        qsort(out_entries, count, sizeof(ifm_agg_entry_t *), compare_agg_entries);
    }
    return count;
}

void ifm_agg_table_cleanup(ifm_aggregation_table_t *table) {
    if (!table) return;
    if (!table->arena) {
        for (size_t i = 0; i < table->bucket_count; ++i) {
            ifm_agg_entry_t *curr = table->buckets[i];
            while (curr) {
                ifm_agg_entry_t *next = curr->next;
                free(curr);
                curr = next;
            }
        }
        free(table->buckets);
    }
    table->buckets = NULL;
    table->bucket_count = 0;
    table->entry_count = 0;
}
