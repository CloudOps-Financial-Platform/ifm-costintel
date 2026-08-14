#ifndef IFM_RECONCILIATION_H
#define IFM_RECONCILIATION_H

#include "ifm_costintel/ifm_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Population counts */
    uint64_t total_input_count;
    uint64_t allocated_count;
    uint64_t unallocated_count;
    uint64_t ambiguous_count;
    uint64_t faulted_count;

    /* Financial conservation sums */
    ifm_micros_t total_input_micros;
    ifm_micros_t allocated_micros;
    ifm_micros_t unallocated_micros;
    ifm_micros_t ambiguous_micros;
    ifm_micros_t faulted_micros;

    bool population_reconciled;
    bool financial_reconciled;
    bool fatal_error;
} ifm_reconciliation_tracker_t;

/* Initialize tracker */
void ifm_reconciliation_init(ifm_reconciliation_tracker_t *tracker);

/* Accumulate record into reconciliation state */
bool ifm_reconciliation_accumulate(ifm_reconciliation_tracker_t *tracker, const ifm_record_t *record);

/* Verify population and financial conservation invariants */
bool ifm_reconciliation_verify(ifm_reconciliation_tracker_t *tracker);

#ifdef __cplusplus
}
#endif

#endif /* IFM_RECONCILIATION_H */
