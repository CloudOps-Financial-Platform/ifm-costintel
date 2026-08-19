#ifndef IFM_VARIANCE_H
#define IFM_VARIANCE_H

#include "ifm_costintel/ifm_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char key[256];
    ifm_micros_t baseline_micros;
    bool occupied;
} ifm_baseline_entry_t;

typedef struct {
    ifm_baseline_entry_t *entries;
    size_t count;
    size_t capacity;
} ifm_baseline_table_t;

/* Initialize baseline table */
void ifm_baseline_table_init(ifm_baseline_table_t *table);

/* Set baseline for specific resource or dimension key */
bool ifm_baseline_table_set(ifm_baseline_table_t *table, const char *key, ifm_micros_t baseline_micros);

/* Lookup baseline */
bool ifm_baseline_table_lookup(const ifm_baseline_table_t *table, const char *key, ifm_micros_t *out_baseline);

/* Free baseline table */
void ifm_baseline_table_cleanup(ifm_baseline_table_t *table);

/* Load baselines from JSON configuration string or file */
bool ifm_baseline_table_load_json(ifm_baseline_table_t *table, const char *json_str, size_t json_len);
bool ifm_baseline_table_load_file(ifm_baseline_table_t *table, const char *filepath);

/* Compute variance for a record against its baseline */
bool ifm_compute_variance(ifm_record_t *record, ifm_micros_t baseline_micros);

#ifdef __cplusplus
}
#endif

#endif /* IFM_VARIANCE_H */
