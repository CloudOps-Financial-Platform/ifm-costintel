#undef NDEBUG
#include "ifm_costintel/stream_adapter.h"
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include "ifm_costintel/traceability.h"
#include "ifm_costintel/rules.h"
#include "ifm_costintel/allocation.h"
#include "ifm_costintel/variance.h"
#include "ifm_costintel/anomaly.h"
#include "ifm_costintel/aggregation.h"
#include "ifm_costintel/reconciliation.h"
#include "ifm_costintel/output.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
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

    ifm_baseline_table_t bt;
    ifm_baseline_table_init(&bt);
    ifm_baseline_table_set(&bt, "i-01", 20000000); /* $20 baseline for i-01 */

    ifm_anomaly_rule_set_t ars;
    ifm_anomaly_rule_set_init(&ars);
    ifm_anomaly_rule_t ar = {
        .rule_id = "SPIKE-50",
        .threshold_pct_micros = 500000, /* 50% */
        .min_baseline_micros = 10000000,
        .direction = IFM_ANOMALY_DIR_SPIKE
    };
    ifm_anomaly_rule_set_add(&ars, &ar);

    ifm_aggregation_table_t agg_cc;
    ifm_agg_table_init(&agg_cc, IFM_AGG_DIM_COST_CENTER, 32, NULL);

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

        ifm_agg_table_accumulate(&agg_cc, &rec);

        ifm_micros_t base = 0;
        ifm_baseline_table_lookup(&bt, rec.resource_id, &base);
        ifm_compute_variance(&rec, base);

        ifm_evaluate_anomalies(&ars, &rec);

        ifm_reconciliation_accumulate(&tracker, &rec);

        if (i == 0) {
            assert(rec.alloc_status == IFM_ALLOC_ALLOCATED);
            assert(strcmp(rec.cost_center_id, "CC-PROD") == 0);
            assert(rec.billed_cost_micros == 50000000);
            assert(rec.baseline_micros == 20000000);
            assert(rec.variance_delta_micros == 30000000);
            assert(rec.variance_pct_micros == 1500000); /* +150% */
            assert(rec.is_anomaly);
            assert(strcmp(rec.anomaly_rule_id, "SPIKE-50") == 0);
            assert(rec.anomaly_direction == IFM_ANOMALY_DIR_SPIKE);
        } else if (i == 1) {
            assert(rec.alloc_status == IFM_ALLOC_UNALLOCATED);
            assert(rec.billed_cost_micros == 20000000);
            assert(!rec.is_anomaly);
        }
    }

    assert(ifm_reconciliation_verify(&tracker));
    assert(tracker.total_input_count == 3);
    assert(tracker.allocated_count == 1);
    assert(tracker.unallocated_count == 1);
    assert(tracker.faulted_count == 1);
    assert(tracker.total_input_micros == 70000000);

    /* Aggregation validation */
    assert(agg_cc.entry_count == 2); /* CC-PROD and UNALLOCATED */
    assert(agg_cc.grand_total_micros == 70000000);

    ifm_agg_table_cleanup(&agg_cc);
    ifm_baseline_table_cleanup(&bt);

    printf("  [PASS] test_pipeline_end_to_end\n");
}

static int run_cli(const char *bin, const char *args) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s %s > /dev/null 2>&1", bin, args);
    int status = system(cmd);
    if (status == -1) return -1;
    if (WIFEXITED(status)) return (int)WEXITSTATUS(status);
    return -1;
}

static void test_cli_lifecycle(void) {
    const char *bin = getenv("IFM_BIN_PATH");
    if (!bin) {
        if (access("./ifm-costintel", X_OK) == 0) {
            bin = "./ifm-costintel";
        } else if (access("../build/ifm-costintel", X_OK) == 0) {
            bin = "../build/ifm-costintel";
        } else if (access("build/ifm-costintel", X_OK) == 0) {
            bin = "build/ifm-costintel";
        } else if (access("/home/mrcn2/ifm-costintel/build/ifm-costintel", X_OK) == 0) {
            bin = "/home/mrcn2/ifm-costintel/build/ifm-costintel";
        } else {
            fprintf(stderr, "FATAL: ifm-costintel CLI executable not found in discovery paths\n");
            assert(0 && "CLI binary ifm-costintel could not be located for lifecycle testing");
            return;
        }
    }
    assert(bin != NULL);
    assert(access(bin, X_OK) == 0);

    /* 1. Version and Help (exit 0) */
    assert(run_cli(bin, "--version") == 0);
    assert(run_cli(bin, "--help") == 0);

    /* 2. Default execution with empty input (exit 0) */
    assert(run_cli(bin, "< /dev/null") == 0);

    /* 3. Missing / Nonexistent config file (exit 1) */
    assert(run_cli(bin, "--config /nonexistent_file_xyz_12345.json < /dev/null") == 1);

    /* 4. Malformed rules configuration (exit 1) */
    FILE *f = fopen("/tmp/test_malformed_rules.json", "w");
    if (f) {
        fprintf(f, "{\"rules\": [{\"priority\": \"not_a_number\"}]}");
        fclose(f);
        assert(run_cli(bin, "--config /tmp/test_malformed_rules.json < /dev/null") == 1);
        remove("/tmp/test_malformed_rules.json");
    }

    /* 5. Malformed baselines configuration (exit 1) */
    f = fopen("/tmp/test_malformed_baselines.json", "w");
    if (f) {
        fprintf(f, "{\"baselines\": [{\"key\": \"res-1\"}]}");
        fclose(f);
        assert(run_cli(bin, "--config /tmp/test_malformed_baselines.json < /dev/null") == 1);
        remove("/tmp/test_malformed_baselines.json");
    }

    /* 6. Malformed anomalies configuration (exit 1) */
    f = fopen("/tmp/test_malformed_anomalies.json", "w");
    if (f) {
        fprintf(f, "{\"anomalies\": [{\"rule_id\": \"A1\", \"threshold_pct_micros\": 500000, \"direction\": \"INVALID\"}]}");
        fclose(f);
        assert(run_cli(bin, "--config /tmp/test_malformed_anomalies.json < /dev/null") == 1);
        remove("/tmp/test_malformed_anomalies.json");
    }

    /* 7. Valid configuration (exit 0) */
    f = fopen("/tmp/test_valid_config.json", "w");
    if (f) {
        fprintf(f, "{\"rules\": [], \"baselines\": [], \"anomalies\": []}");
        fclose(f);
        assert(run_cli(bin, "--config /tmp/test_valid_config.json < /dev/null") == 0);
        remove("/tmp/test_valid_config.json");
    }

    printf("  [PASS] test_cli_lifecycle\n");
}

int main(void) {
    printf("Running End-to-End Pipeline Integration Tests...\n");
    test_pipeline_end_to_end();
    test_cli_lifecycle();
    printf("ALL INTEGRATION TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
