#ifndef IFM_ALLOCATION_H
#define IFM_ALLOCATION_H

#include "ifm_costintel/ifm_types.h"
#include "ifm_costintel/rules.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allocates a single normalized billing record against a rule set */
void ifm_allocate_record(const ifm_rule_set_t *rs, ifm_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* IFM_ALLOCATION_H */
