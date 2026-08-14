#include "ifm_costintel/allocation.h"
#include <string.h>

static inline void safe_strcpy(char *dest, size_t dest_cap, const char *src) {
    if (!dest || dest_cap == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dest_cap) len = dest_cap - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static bool match_pattern(const char *pattern, const char *value) {
    if (!pattern || pattern[0] == '\0' || strcmp(pattern, "*") == 0) {
        return true;
    }
    if (!value) return false;

    size_t plen = strlen(pattern);
    if (plen > 0 && pattern[plen - 1] == '*') {
        return (strncmp(pattern, value, plen - 1) == 0);
    }
    return (strcmp(pattern, value) == 0);
}

static bool match_prefix(const char *prefix, const char *value) {
    if (!prefix || prefix[0] == '\0' || strcmp(prefix, "*") == 0) {
        return true;
    }
    if (!value) return false;
    size_t plen = strlen(prefix);
    if (plen > 0 && prefix[plen - 1] == '*') {
        plen--;
    }
    return (strncmp(prefix, value, plen) == 0);
}

static bool rule_matches(const ifm_allocation_rule_t *rule, const ifm_record_t *rec) {
    if (!match_pattern(rule->match_provider, rec->provider)) return false;
    if (!match_pattern(rule->match_account_id, rec->account_id)) return false;
    if (!match_prefix(rule->match_resource_prefix, rec->resource_id)) return false;
    return true;
}

void ifm_allocate_record(const ifm_rule_set_t *rs, ifm_record_t *record) {
    if (!record) return;

    if (record->is_faulted) {
        record->alloc_status = IFM_ALLOC_FAULTED;
        return;
    }

    if (!rs || rs->rule_count == 0) {
        record->alloc_status = IFM_ALLOC_UNALLOCATED;
        safe_strcpy(record->cost_center_id, sizeof(record->cost_center_id), "UNALLOCATED");
        safe_strcpy(record->rule_id, sizeof(record->rule_id), "NONE");
        record->rule_version = 0;
        return;
    }

    const ifm_allocation_rule_t *best_rule = NULL;
    int32_t highest_priority = INT32_MIN;
    bool is_ambiguous = false;

    for (size_t i = 0; i < rs->rule_count; ++i) {
        const ifm_allocation_rule_t *r = &rs->rules[i];
        if (rule_matches(r, record)) {
            if (r->priority > highest_priority) {
                highest_priority = r->priority;
                best_rule = r;
                is_ambiguous = false;
            } else if (r->priority == highest_priority && best_rule != NULL) {
                if (strcmp(r->target_cost_center_id, best_rule->target_cost_center_id) != 0) {
                    is_ambiguous = true;
                }
            }
        }
    }

    if (is_ambiguous) {
        record->alloc_status = IFM_ALLOC_AMBIGUOUS;
        safe_strcpy(record->cost_center_id, sizeof(record->cost_center_id), "AMBIGUOUS");
        safe_strcpy(record->rule_id, sizeof(record->rule_id), "AMBIGUOUS");
        record->rule_version = 0;
    } else if (best_rule != NULL) {
        record->alloc_status = IFM_ALLOC_ALLOCATED;
        safe_strcpy(record->cost_center_id, sizeof(record->cost_center_id), best_rule->target_cost_center_id);
        safe_strcpy(record->rule_id, sizeof(record->rule_id), best_rule->rule_id);
        record->rule_version = best_rule->version;
    } else {
        record->alloc_status = IFM_ALLOC_UNALLOCATED;
        safe_strcpy(record->cost_center_id, sizeof(record->cost_center_id), "UNALLOCATED");
        safe_strcpy(record->rule_id, sizeof(record->rule_id), "NONE");
        record->rule_version = 0;
    }
}
