#ifndef IFM_FAULT_ENGINE_H
#define IFM_FAULT_ENGINE_H

#include "ifm_costintel/ifm_types.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FILE *dlq_file;
    uint64_t total_faults;
    uint64_t warn_count;
    uint64_t err_count;
    uint64_t fatal_count;
} ifm_fault_engine_t;

/* Initialize fault engine */
void ifm_fault_engine_init(ifm_fault_engine_t *fe, FILE *dlq_file);

/* Route record fault to DLQ */
bool ifm_fault_engine_record_fault(ifm_fault_engine_t *fe, const ifm_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* IFM_FAULT_ENGINE_H */
