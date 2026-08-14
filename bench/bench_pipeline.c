#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "ifm_costintel/ifm_types.h"
#include "ifm_costintel/fak.h"
#include "ifm_costintel/arena.h"
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include "ifm_costintel/traceability.h"
#include "ifm_costintel/rules.h"
#include "ifm_costintel/allocation.h"
#include "ifm_costintel/variance.h"
#include "ifm_costintel/reconciliation.h"
#include "ifm_costintel/output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv) {
    uint64_t num_records = 250000;
    if (argc > 1) {
        num_records = (uint64_t)strtoull(argv[1], NULL, 10);
        if (num_records == 0) num_records = 250000;
    }

    printf("===============================================================\n");
    printf("IFM-CostIntel v1.0.0 — Production Pipeline Benchmark Suite\n");
    printf("Target Record Count: %" PRIu64 " records\n", num_records);
    printf("===============================================================\n");

    /* Setup Rules */
    ifm_rule_set_t rs;
    ifm_rule_set_init(&rs);

    ifm_allocation_rule_t r1 = {
        .rule_id = "RULE-AWS-COMPUTE",
        .priority = 100,
        .match_provider = "aws",
        .match_account_id = "111*",
        .match_resource_prefix = "i-",
        .target_cost_center_id = "CC-COMPUTE",
        .version = 1
    };
    ifm_allocation_rule_t r2 = {
        .rule_id = "RULE-AZURE-STORAGE",
        .priority = 80,
        .match_provider = "azure",
        .match_account_id = "sub*",
        .match_resource_prefix = "vol-",
        .target_cost_center_id = "CC-STORAGE",
        .version = 1
    };
    ifm_rule_set_add(&rs, &r1);
    ifm_rule_set_add(&rs, &r2);

    /* Sample Record Buffer */
    const char *sample_json = "{\"provider\":\"aws\",\"provider_row_id\":\"row-987654\",\"account_id\":\"111222333444\",\"resource_id\":\"i-0123456789abcdef0\",\"usage_start_raw\":\"2026-08-14T10:00:00Z\",\"billed_cost_micros\":45800000,\"flags\":0}";
    size_t sample_len = strlen(sample_json);

    ifm_reconciliation_tracker_t tracker;
    ifm_reconciliation_init(&tracker);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ifm_record_t rec;
    for (uint64_t i = 0; i < num_records; ++i) {
        /* Ingress Decode */
        if (!ifm_json_decode_record(sample_json, sample_len, &rec)) continue;
        ifm_traceability_stamp(&rec, i + 1);

        /* Schema Validation */
        if (!ifm_schema_validate_record(&rec)) continue;

        /* Intelligence: Allocation */
        ifm_allocate_record(&rs, &rec);

        /* Intelligence: Variance */
        ifm_compute_variance(&rec, 40000000);

        /* Governance: Reconciliation */
        ifm_reconciliation_accumulate(&tracker, &rec);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double start_sec = (double)start.tv_sec + (double)start.tv_nsec / 1e9;
    double end_sec = (double)end.tv_sec + (double)end.tv_nsec / 1e9;
    double duration = end_sec - start_sec;
    if (duration <= 0.0) duration = 0.000001;

    double rps = (double)num_records / duration;
    double mb_processed = ((double)(sample_len + 1) * (double)num_records) / (1024.0 * 1024.0);
    double mbps = mb_processed / duration;

    bool recon_ok = ifm_reconciliation_verify(&tracker);

    printf("RESULTS:\n");
    printf("  Duration:              %.4f seconds\n", duration);
    printf("  Throughput:            %.2f records/sec\n", rps);
    printf("  Data Throughput:       %.2f MB/sec\n", mbps);
    printf("  Reconciliation Status: %s\n", recon_ok ? "PASS (100% Conserved)" : "FAIL");
    printf("  Allocated Records:     %" PRIu64 "\n", tracker.allocated_count);
    printf("  Total Billed Micros:   %" PRId64 " ($%.2f)\n", tracker.total_input_micros, (double)tracker.total_input_micros / 1e6);
    printf("===============================================================\n");

    return recon_ok ? 0 : 1;
}
