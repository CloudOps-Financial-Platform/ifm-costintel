#ifndef IFM_TRACEABILITY_H
#define IFM_TRACEABILITY_H

#include "ifm_costintel/ifm_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Verify and stamp lineage traceability headers */
void ifm_traceability_stamp(ifm_record_t *record, uint64_t source_line);

#ifdef __cplusplus
}
#endif

#endif /* IFM_TRACEABILITY_H */
