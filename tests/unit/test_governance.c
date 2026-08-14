#undef NDEBUG
#include "ifm_costintel/reconciliation.h"
#include "ifm_costintel/fault_engine.h"
#include "ifm_costintel/telemetry.h"
#include "ifm_costintel/output.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_reconciliation_invariants(void) {
    ifm_reconciliation_tracker_t tracker;
    ifm_reconciliation_init(&tracker);

    ifm_record_t r1 = { .alloc_status = IFM_ALLOC_ALLOCATED, .billed_cost_micros = 10000000, .active_spend_micros = 10000000 };
    ifm_record_t r2 = { .alloc_status = IFM_ALLOC_UNALLOCATED, .billed_cost_micros = 5000000, .active_spend_micros = 5000000 };
    ifm_record_t r3 = { .alloc_status = IFM_ALLOC_AMBIGUOUS, .billed_cost_micros = 2000000, .active_spend_micros = 2000000 };
    ifm_record_t r4 = { .alloc_status = IFM_ALLOC_FAULTED, .is_faulted = true, .billed_cost_micros = 3000000 };

    assert(ifm_reconciliation_accumulate(&tracker, &r1));
    assert(ifm_reconciliation_accumulate(&tracker, &r2));
    assert(ifm_reconciliation_accumulate(&tracker, &r3));
    assert(ifm_reconciliation_accumulate(&tracker, &r4));

    assert(ifm_reconciliation_verify(&tracker));
    assert(tracker.population_reconciled);
    assert(tracker.financial_reconciled);
    assert(tracker.total_input_count == 4);
    assert(tracker.total_input_micros == 20000000);

    printf("  [PASS] test_reconciliation_invariants\n");
}

static void test_output_ndjson(void) {
    ifm_record_t rec = {
        .source_line = 12,
        .provider = "aws",
        .provider_row_id = "abc123",
        .account_id = "111222",
        .resource_id = "vol-01",
        .alloc_status = IFM_ALLOC_ALLOCATED,
        .cost_center_id = "CC-001",
        .rule_id = "RULE-07",
        .rule_version = 1,
        .active_spend_micros = 45800000,
        .baseline_micros = 40000000,
        .variance_delta_micros = 5800000,
        .variance_status = IFM_VARIANCE_DEFINED
    };

    char buf[2048];
    assert(ifm_format_record_ndjson(&rec, buf, sizeof(buf)));
    assert(strstr(buf, "\"source_line\":12") != NULL);
    assert(strstr(buf, "\"cost_center_id\":\"CC-001\"") != NULL);
    assert(strstr(buf, "\"active_spend_micros\":45800000") != NULL);

    printf("  [PASS] test_output_ndjson\n");
}

int main(void) {
    printf("Running Governance Unit Tests...\n");
    test_reconciliation_invariants();
    test_output_ndjson();
    printf("ALL GOVERNANCE UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
