#include "ifm_costintel/variance.h"
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

void ifm_baseline_table_init(ifm_baseline_table_t *table) {
    if (!table) return;
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

bool ifm_baseline_table_set(ifm_baseline_table_t *table, const char *key, ifm_micros_t baseline_micros) {
    if (!table || !key) return false;

    for (size_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i].key, key) == 0) {
            table->entries[i].baseline_micros = baseline_micros;
            return true;
        }
    }

    if (table->count >= table->capacity) {
        size_t new_cap = (table->capacity == 0) ? 64 : table->capacity * 2;
        ifm_baseline_entry_t *new_entries = (ifm_baseline_entry_t *)realloc(table->entries, new_cap * sizeof(ifm_baseline_entry_t));
        if (!new_entries) return false;
        table->entries = new_entries;
        table->capacity = new_cap;
    }

    safe_strcpy(table->entries[table->count].key, sizeof(table->entries[table->count].key), key);
    table->entries[table->count].baseline_micros = baseline_micros;
    table->count++;
    return true;
}

bool ifm_baseline_table_lookup(const ifm_baseline_table_t *table, const char *key, ifm_micros_t *out_baseline) {
    if (!table || !key || !out_baseline) return false;
    for (size_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i].key, key) == 0) {
            *out_baseline = table->entries[i].baseline_micros;
            return true;
        }
    }
    return false;
}

void ifm_baseline_table_cleanup(ifm_baseline_table_t *table) {
    if (!table) return;
    if (table->entries) {
        free(table->entries);
        table->entries = NULL;
    }
    table->count = 0;
    table->capacity = 0;
}

bool ifm_compute_variance(ifm_record_t *record, ifm_micros_t baseline_micros) {
    if (!record) return false;

    record->baseline_micros = baseline_micros;
    if (!fak_sub_micros(record->active_spend_micros, baseline_micros, &record->variance_delta_micros)) {
        record->is_faulted = true;
        record->fault_code = IFM_FAULT_ARITHMETIC_OVERFLOW;
        record->fault_severity = IFM_SEV_ERR;
        return false;
    }

    if (baseline_micros == 0) {
        if (record->active_spend_micros == 0) {
            record->variance_status = IFM_VARIANCE_BASELINE_ZERO_NO_CHANGE;
            record->variance_pct_micros = 0;
        } else {
            record->variance_status = IFM_VARIANCE_BASELINE_ZERO;
            record->variance_pct_micros = 0;
        }
    } else {
        record->variance_status = IFM_VARIANCE_DEFINED;
        if (!fak_div_micros(record->variance_delta_micros, baseline_micros, &record->variance_pct_micros)) {
            record->variance_pct_micros = 0;
        }
    }

    return true;
}
