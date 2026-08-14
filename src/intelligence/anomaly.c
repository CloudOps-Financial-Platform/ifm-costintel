#include "ifm_costintel/anomaly.h"
#include "ifm_costintel/fak.h"
#include <string.h>

static inline void safe_strcpy(char *dest, size_t dest_cap, const char *src) {
    if (!dest || dest_cap == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dest_cap) len = dest_cap - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

void ifm_anomaly_rule_set_init(ifm_anomaly_rule_set_t *ars) {
    if (!ars) return;
    ars->count = 0;
}

bool ifm_anomaly_rule_set_add(ifm_anomaly_rule_set_t *ars, const ifm_anomaly_rule_t *rule) {
    if (!ars || !rule || ars->count >= 64) return false;
    memcpy(&ars->rules[ars->count++], rule, sizeof(ifm_anomaly_rule_t));
    return true;
}

void ifm_evaluate_anomalies(const ifm_anomaly_rule_set_t *ars, ifm_record_t *record) {
    if (!record) return;

    record->is_anomaly = false;
    record->anomaly_rule_id[0] = '\0';
    record->anomaly_direction = IFM_ANOMALY_DIR_NONE;

    if (!ars || ars->count == 0 || record->variance_status != IFM_VARIANCE_DEFINED) {
        return;
    }

    for (size_t i = 0; i < ars->count; ++i) {
        const ifm_anomaly_rule_t *r = &ars->rules[i];

        if (record->baseline_micros < r->min_baseline_micros) {
            continue;
        }

        ifm_micros_t delta = record->variance_delta_micros;
        ifm_micros_t abs_delta;
        if (!fak_abs_micros(delta, &abs_delta)) continue;

        ifm_micros_t pct_change;
        if (!fak_div_micros(abs_delta, record->baseline_micros, &pct_change)) continue;

        if (pct_change >= r->threshold_pct_micros) {
            if (delta > 0 && (r->direction == IFM_ANOMALY_DIR_SPIKE || r->direction == IFM_ANOMALY_DIR_BOTH)) {
                record->is_anomaly = true;
                safe_strcpy(record->anomaly_rule_id, sizeof(record->anomaly_rule_id), r->rule_id);
                record->anomaly_direction = IFM_ANOMALY_DIR_SPIKE;
                return;
            } else if (delta < 0 && (r->direction == IFM_ANOMALY_DIR_DROP || r->direction == IFM_ANOMALY_DIR_BOTH)) {
                record->is_anomaly = true;
                safe_strcpy(record->anomaly_rule_id, sizeof(record->anomaly_rule_id), r->rule_id);
                record->anomaly_direction = IFM_ANOMALY_DIR_DROP;
                return;
            }
        }
    }
}
