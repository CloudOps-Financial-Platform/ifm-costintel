#ifndef IFM_RULES_H
#define IFM_RULES_H

#include "ifm_costintel/ifm_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IFM_MAX_RULES 1024

typedef struct {
    char rule_id[64];
    int32_t priority;
    char match_provider[32];
    char match_account_id[128];
    char match_resource_prefix[256];
    char target_cost_center_id[128];
    uint32_t version;
} ifm_allocation_rule_t;

typedef struct {
    char config_version[64];
    uint32_t version_number;
    ifm_allocation_rule_t rules[IFM_MAX_RULES];
    size_t rule_count;
} ifm_rule_set_t;

/* Initialize rule set */
void ifm_rule_set_init(ifm_rule_set_t *rs);

/* Add rule manually */
bool ifm_rule_set_add(ifm_rule_set_t *rs, const ifm_allocation_rule_t *rule);

/* Load rule set from JSON configuration string or file */
bool ifm_rule_set_load_json(ifm_rule_set_t *rs, const char *json_str, size_t json_len);
bool ifm_rule_set_load_file(ifm_rule_set_t *rs, const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* IFM_RULES_H */
