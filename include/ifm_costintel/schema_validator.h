#ifndef IFM_SCHEMA_VALIDATOR_H
#define IFM_SCHEMA_VALIDATOR_H

#include "ifm_costintel/ifm_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Validates schema constraints on normalized billing record */
bool ifm_schema_validate_record(ifm_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* IFM_SCHEMA_VALIDATOR_H */
