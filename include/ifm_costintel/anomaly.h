#ifndef IFM_ANOMALY_H
#define IFM_ANOMALY_H

#include "ifm_costintel/ifm_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char rule_id[64];
    ifm_micros_t threshold_pct_micros; /* 200,000 = 20.0% */
    ifm_micros_t min_baseline_micros;  /* minimum baseline spend to avoid small base noise */
    ifm_anomaly_dir_t direction;       /* SPIKE, DROP, BOTH */
} ifm_anomaly_rule_t;

typedef struct {
    ifm_anomaly_rule_t rules[64];
    size_t count;
} ifm_anomaly_rule_set_t;

/* Initialize anomaly rule set */
void ifm_anomaly_rule_set_init(ifm_anomaly_rule_set_t *ars);

/* Add anomaly rule */
bool ifm_anomaly_rule_set_add(ifm_anomaly_rule_set_t *ars, const ifm_anomaly_rule_t *rule);

/* Load anomaly rules from JSON configuration string or file */
bool ifm_anomaly_rule_set_load_json(ifm_anomaly_rule_set_t *ars, const char *json_str, size_t json_len);
bool ifm_anomaly_rule_set_load_file(ifm_anomaly_rule_set_t *ars, const char *filepath);

/* Evaluate record against anomaly rules */
void ifm_evaluate_anomalies(const ifm_anomaly_rule_set_t *ars, ifm_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* IFM_ANOMALY_H */
