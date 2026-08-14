#include "ifm_costintel/reconciliation.h"
#include "ifm_costintel/fak.h"
#include <string.h>

void ifm_reconciliation_init(ifm_reconciliation_tracker_t *tracker) {
    if (!tracker) return;
    memset(tracker, 0, sizeof(ifm_reconciliation_tracker_t));
}

bool ifm_reconciliation_accumulate(ifm_reconciliation_tracker_t *tracker, const ifm_record_t *record) {
    if (!tracker || !record) return false;

    tracker->total_input_count++;
    if (!fak_add_micros(tracker->total_input_micros, record->billed_cost_micros, &tracker->total_input_micros)) {
        tracker->fatal_error = true;
        return false;
    }

    if (record->is_faulted || record->alloc_status == IFM_ALLOC_FAULTED) {
        tracker->faulted_count++;
        if (!fak_add_micros(tracker->faulted_micros, record->billed_cost_micros, &tracker->faulted_micros)) {
            tracker->fatal_error = true;
            return false;
        }
    } else if (record->alloc_status == IFM_ALLOC_ALLOCATED) {
        tracker->allocated_count++;
        if (!fak_add_micros(tracker->allocated_micros, record->active_spend_micros, &tracker->allocated_micros)) {
            tracker->fatal_error = true;
            return false;
        }
    } else if (record->alloc_status == IFM_ALLOC_AMBIGUOUS) {
        tracker->ambiguous_count++;
        if (!fak_add_micros(tracker->ambiguous_micros, record->active_spend_micros, &tracker->ambiguous_micros)) {
            tracker->fatal_error = true;
            return false;
        }
    } else {
        /* UNALLOCATED */
        tracker->unallocated_count++;
        if (!fak_add_micros(tracker->unallocated_micros, record->active_spend_micros, &tracker->unallocated_micros)) {
            tracker->fatal_error = true;
            return false;
        }
    }

    return true;
}

bool ifm_reconciliation_verify(ifm_reconciliation_tracker_t *tracker) {
    if (!tracker || tracker->fatal_error) return false;

    /* 1. Verify Population Invariant */
    uint64_t state_sum = tracker->allocated_count + tracker->unallocated_count +
                         tracker->ambiguous_count + tracker->faulted_count;
    tracker->population_reconciled = (tracker->total_input_count == state_sum);

    /* 2. Verify Financial Conservation Invariant */
    ifm_micros_t money_sum = 0;
    if (!fak_add_micros(money_sum, tracker->allocated_micros, &money_sum)) return false;
    if (!fak_add_micros(money_sum, tracker->unallocated_micros, &money_sum)) return false;
    if (!fak_add_micros(money_sum, tracker->ambiguous_micros, &money_sum)) return false;
    if (!fak_add_micros(money_sum, tracker->faulted_micros, &money_sum)) return false;

    tracker->financial_reconciled = (tracker->total_input_micros == money_sum);

    return (tracker->population_reconciled && tracker->financial_reconciled);
}
