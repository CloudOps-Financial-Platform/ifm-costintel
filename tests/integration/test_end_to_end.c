#undef NDEBUG
#include "ifm_costintel/stream_adapter.h"
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include "ifm_costintel/traceability.h"
#include "ifm_costintel/rules.h"
#include "ifm_costintel/allocation.h"
#include "ifm_costintel/variance.h"
#include "ifm_costintel/reconciliation.h"
#include "ifm_costintel/output.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_pipeline_end_to_end(void) {
    ifm_rule_set_t rs;
    ifm_rule_set_init(&rs);

    ifm_allocation_rule_t r1 = {
        .rule_id = "RULE-PROD",
        .priority = 100,
        .match_provider = "aws",
        .match_account_id = "111*",
        .target_cost_center_id = "CC-PROD",
        .version = 1
    };
    ifm_rule_set_add(&rs, &r1);

    ifm_reconciliation_tracker_t tracker;
    ifm_reconciliation_init(&tracker);

    const char *input_lines[] = {
        "{\"provider\":\"aws\",\"account_id\":\"111222\",\"resource_id\":\"i-01\",\"billed_cost_micros\":50000000}",
        "{\"provider\":\"azure\",\"account_id\":\"sub-01\",\"resource_id\":\"vm-01\",\"billed_cost\":\"20.000000\"}",
        "{\"provider\":\"invalid_syntax"
    };

    size_t line_count = 3;
    for (size_t i = 0; i < line_count; ++i) {
        ifm_record_t rec;
        if (!ifm_json_decode_record(input_lines[i], strlen(input_lines[i]), &rec)) {
            ifm_traceability_stamp(&rec, i + 1);
            ifm_reconciliation_accumulate(&tracker, &rec);
            continue;
        }

        ifm_traceability_stamp(&rec, i + 1);
        if (!ifm_schema_validate_record(&rec)) {
            ifm_reconciliation_accumulate(&tracker, &rec);
            continue;
        }

        ifm_allocate_record(&rs, &rec);
        ifm_compute_variance(&rec, 0);
        ifm_reconciliation_accumulate(&tracker, &rec);

        if (i == 0) {
            assert(rec.alloc_status == IFM_ALLOC_ALLOCATED);
            assert(strcmp(rec.cost_center_id, "CC-PROD") == 0);
            assert(rec.billed_cost_micros == 50000000);
        } else if (i == 1) {
            assert(rec.alloc_status == IFM_ALLOC_UNALLOCATED);
            assert(rec.billed_cost_micros == 20000000);
        }
    }

    assert(ifm_reconciliation_verify(&tracker));
    assert(tracker.total_input_count == 3);
    assert(tracker.allocated_count == 1);
    assert(tracker.unallocated_count == 1);
    assert(tracker.faulted_count == 1);
    assert(tracker.total_input_micros == 70000000);

    printf("  [PASS] test_pipeline_end_to_end\n");
}

int main(void) {
    printf("Running End-to-End Pipeline Integration Tests...\n");
    test_pipeline_end_to_end();
    printf("ALL INTEGRATION TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
