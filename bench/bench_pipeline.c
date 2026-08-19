#define _POSIX_C_SOURCE 199309L
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include "ifm_costintel/traceability.h"
#include "ifm_costintel/rules.h"
#include "ifm_costintel/allocation.h"
#include "ifm_costintel/aggregation.h"
#include "ifm_costintel/variance.h"
#include "ifm_costintel/concentration.h"
#include "ifm_costintel/anomaly.h"
#include "ifm_costintel/reconciliation.h"
#include "ifm_costintel/telemetry.h"
#include "ifm_costintel/fak.h"
#include "ifm_costintel/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#define DEFAULT_BENCH_RECORDS 500000
#define AGG_ARENA_SIZE (2 * 1024 * 1024) /* 2 MB */

/* Rotating Multi-Cloud Corpus (16 Distinct Payloads) */
static const char *bench_corpus[] = {
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-001\",\"account_id\":\"111222333444\",\"resource_id\":\"i-ec2-prod-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":45800000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-002\",\"account_id\":\"111222333444\",\"resource_id\":\"vol-ebs-prod-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":12500000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-003\",\"account_id\":\"555666777888\",\"resource_id\":\"i-ec2-dev-02\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":8200000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-004\",\"account_id\":\"999000111222\",\"resource_id\":\"s3-data-lake\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":95000000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-005\",\"account_id\":\"sub-core-infra\",\"resource_id\":\"vm-app-eastus-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":34000000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-006\",\"account_id\":\"sub-core-infra\",\"resource_id\":\"disk-os-prod-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":6200000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-007\",\"account_id\":\"sub-data-ai\",\"resource_id\":\"blob-models-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":120000000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-008\",\"account_id\":\"sub-unmapped\",\"resource_id\":\"nic-orphan-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":1500000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-009\",\"account_id\":\"proj-analytics-prod\",\"resource_id\":\"gcs-raw-events-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":55000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-010\",\"account_id\":\"proj-analytics-prod\",\"resource_id\":\"bq-slots-analytics\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":210000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-011\",\"account_id\":\"proj-k8s-cluster\",\"resource_id\":\"gke-node-pool-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":78000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-012\",\"account_id\":\"proj-unallocated\",\"resource_id\":\"pubsub-topic-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":4300000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-013\",\"account_id\":\"111222333444\",\"resource_id\":\"i-ec2-prod-01\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":45800000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-014\",\"account_id\":\"sub-core-infra\",\"resource_id\":\"vm-app-eastus-01\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":34000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-015\",\"account_id\":\"proj-analytics-prod\",\"resource_id\":\"bq-slots-analytics\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":210000000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-016\",\"account_id\":\"999000111222\",\"resource_id\":\"s3-data-lake\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":180000000,\"flags\":0}" /* Anomaly Spike */
};

static const size_t corpus_len = sizeof(bench_corpus) / sizeof(bench_corpus[0]);

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    uint64_t total_records = DEFAULT_BENCH_RECORDS;
    if (argc > 1) {
        long val = atol(argv[1]);
        if (val > 0) total_records = (uint64_t)val;
    }

    printf("===============================================================\n");
    printf("IFM-CostIntel v1.0.0 — Full 9-Stage Production Pipeline Benchmark\n");
    printf("Target Record Count: %" PRIu64 "\n", total_records);
    printf("Workload Mode:       Rotating Multi-Cloud Corpus (16 Distinct Payloads)\n");
    printf("===============================================================\n");

    /* 1. Setup Allocation Rules */
    ifm_rule_set_t rule_set;
    ifm_rule_set_init(&rule_set);
    ifm_allocation_rule_t r1 = {.rule_id = "R-AWS-PROD", .priority = 100, .match_provider = "aws", .match_account_id = "111*", .match_resource_prefix = "*", .target_cost_center_id = "CC-INFRA-AWS", .version = 1};
    ifm_allocation_rule_t r2 = {.rule_id = "R-AZ-CORE",   .priority = 100, .match_provider = "azure", .match_account_id = "sub-core*", .match_resource_prefix = "*", .target_cost_center_id = "CC-INFRA-AZURE", .version = 1};
    ifm_allocation_rule_t r3 = {.rule_id = "R-GCP-DATA",  .priority = 100, .match_provider = "gcp", .match_account_id = "proj-analytics*", .match_resource_prefix = "*", .target_cost_center_id = "CC-DATA-GCP", .version = 1};
    ifm_allocation_rule_t r4 = {.rule_id = "R-AWS-S3",    .priority = 90,  .match_provider = "aws", .match_account_id = "*", .match_resource_prefix = "s3-", .target_cost_center_id = "CC-STORAGE-AWS", .version = 1};
    ifm_rule_set_add(&rule_set, &r1);
    ifm_rule_set_add(&rule_set, &r2);
    ifm_rule_set_add(&rule_set, &r3);
    ifm_rule_set_add(&rule_set, &r4);

    /* 2. Setup Baseline Table */
    ifm_baseline_table_t baseline_table;
    ifm_baseline_table_init(&baseline_table);
    ifm_baseline_table_set(&baseline_table, "i-ec2-prod-01", 45000000);
    ifm_baseline_table_set(&baseline_table, "vol-ebs-prod-01", 12000000);
    ifm_baseline_table_set(&baseline_table, "s3-data-lake", 90000000);
    ifm_baseline_table_set(&baseline_table, "vm-app-eastus-01", 34000000);
    ifm_baseline_table_set(&baseline_table, "blob-models-01", 120000000);
    ifm_baseline_table_set(&baseline_table, "gcs-raw-events-01", 55000000);
    ifm_baseline_table_set(&baseline_table, "bq-slots-analytics", 200000000);
    ifm_baseline_table_set(&baseline_table, "gke-node-pool-01", 75000000);

    /* 3. Setup Anomaly Rules */
    ifm_anomaly_rule_set_t anomaly_rules;
    ifm_anomaly_rule_set_init(&anomaly_rules);
    ifm_anomaly_rule_t a1 = {.rule_id = "ANOM-SPIKE-50", .threshold_pct_micros = 500000, .min_baseline_micros = 50000000, .direction = IFM_ANOMALY_DIR_SPIKE};
    ifm_anomaly_rule_t a2 = {.rule_id = "ANOM-DROP-30",  .threshold_pct_micros = 300000, .min_baseline_micros = 10000000, .direction = IFM_ANOMALY_DIR_DROP};
    ifm_anomaly_rule_set_add(&anomaly_rules, &a1);
    ifm_anomaly_rule_set_add(&anomaly_rules, &a2);

    /* 4. Setup 4-Dimensional Aggregation Subsystem */
    ifm_arena_t agg_arena;
    ifm_arena_init(&agg_arena, AGG_ARENA_SIZE);

    ifm_aggregation_table_t agg_provider = {0};
    ifm_aggregation_table_t agg_account = {0};
    ifm_aggregation_table_t agg_cost_center = {0};
    ifm_aggregation_table_t agg_resource = {0};

    bool agg_ok = ifm_agg_table_init(&agg_provider, IFM_AGG_DIM_PROVIDER, 1024, &agg_arena);
    agg_ok = agg_ok && ifm_agg_table_init(&agg_account, IFM_AGG_DIM_ACCOUNT, 1024, &agg_arena);
    agg_ok = agg_ok && ifm_agg_table_init(&agg_cost_center, IFM_AGG_DIM_COST_CENTER, 1024, &agg_arena);
    agg_ok = agg_ok && ifm_agg_table_init(&agg_resource, IFM_AGG_DIM_RESOURCE, 4096, &agg_arena);
    assert(agg_ok && "Aggregation initialization failed");

    /* 5. Setup Reconciliation Tracker */
    ifm_reconciliation_tracker_t tracker;
    ifm_reconciliation_init(&tracker);

    uint64_t total_bytes = 0;
    uint64_t anomaly_count = 0;

    /* Warm up & start timer */
    double t_start = get_time_sec();

    /* Benchmark Critical Execution Loop */
    for (uint64_t i = 0; i < total_records; ++i) {
        const char *json_payload = bench_corpus[i % corpus_len];
        size_t payload_len = strlen(json_payload);
        total_bytes += payload_len;

        ifm_record_t record;
        memset(&record, 0, sizeof(record));

        /* Stage 1: JSON Decode */
        bool decode_ok = ifm_json_decode_record(json_payload, payload_len, &record);
        if (!decode_ok) {
            record.is_faulted = true;
            record.fault_code = IFM_FAULT_JSON_SYNTAX;
        }

        /* Stage 2: Traceability Stamping */
        ifm_traceability_stamp(&record, i + 1);

        /* Stage 3: Schema Validation */
        if (!record.is_faulted && !ifm_schema_validate_record(&record)) {
            record.is_faulted = true;
            record.fault_code = IFM_FAULT_MISSING_REQUIRED_FIELD;
        }

        /* Stage 4: Rule Allocation */
        if (!record.is_faulted) {
            ifm_allocate_record(&rule_set, &record);
        }

        /* Stage 5: 4-Dimensional Aggregation */
        if (!record.is_faulted) {
            ifm_agg_table_accumulate(&agg_provider, &record);
            ifm_agg_table_accumulate(&agg_account, &record);
            ifm_agg_table_accumulate(&agg_cost_center, &record);
            ifm_agg_table_accumulate(&agg_resource, &record);
        }

        /* Stage 6: Baseline Lookup */
        ifm_micros_t baseline = 0;
        if (!record.is_faulted) {
            ifm_baseline_table_lookup(&baseline_table, record.resource_id, &baseline);
        }

        /* Stage 7: Variance Computation */
        if (!record.is_faulted) {
            ifm_compute_variance(&record, baseline);
        }

        /* Stage 8: Anomaly Evaluation */
        if (!record.is_faulted) {
            ifm_evaluate_anomalies(&anomaly_rules, &record);
            if (record.is_anomaly) anomaly_count++;
        }

        /* Stage 9: Double-Entry Reconciliation Accumulation */
        ifm_reconciliation_accumulate(&tracker, &record);
    }

    double t_end = get_time_sec();
    double duration = t_end - t_start;
    if (duration <= 0.0) duration = 0.000001;

    double throughput = (double)total_records / duration;
    double mb_per_sec = ((double)total_bytes / (1024.0 * 1024.0)) / duration;

    /* Invariant Verification */
    bool recon_ok = ifm_reconciliation_verify(&tracker);

    printf("\n================ BENCHMARK EXECUTION RESULTS ================\n");
    printf("Total Records:            %" PRIu64 "\n", total_records);
    printf("Elapsed Time:             %.4f seconds\n", duration);
    printf("Pipeline Throughput:      %.2f records/sec\n", throughput);
    printf("Data Bandwidth:           %.2f MB/sec (Total: %.2f MB)\n", mb_per_sec, (double)total_bytes / (1024.0 * 1024.0));
    printf("Anomalies Detected:       %" PRIu64 "\n", anomaly_count);
    printf("Reconciliation Check:     %s\n", recon_ok ? "PASS (100% Mathematically Conserved)" : "FAIL (INVARIANT BROKEN)");
    printf("Allocated Spend:          $%.2f (%" PRIu64 " records)\n", (double)tracker.allocated_micros / 1e6, tracker.allocated_count);
    printf("Unallocated Spend:        $%.2f (%" PRIu64 " records)\n", (double)tracker.unallocated_micros / 1e6, tracker.unallocated_count);
    printf("Faulted Spend:            $%.2f (%" PRIu64 " records)\n", (double)tracker.faulted_micros / 1e6, tracker.faulted_count);
    printf("\nAggregation Cardinalities:\n");
    printf("  Providers:    %zu\n", agg_provider.entry_count);
    printf("  Accounts:     %zu\n", agg_account.entry_count);
    printf("  Cost Centers: %zu\n", agg_cost_center.entry_count);
    printf("  Resources:    %zu\n", agg_resource.entry_count);
    printf("===============================================================\n");

    assert(recon_ok && "Fatal: Benchmark double-entry reconciliation invariant violated!");

    /* Cleanup Resources */
    ifm_baseline_table_cleanup(&baseline_table);
    ifm_agg_table_cleanup(&agg_provider);
    ifm_agg_table_cleanup(&agg_account);
    ifm_agg_table_cleanup(&agg_cost_center);
    ifm_agg_table_cleanup(&agg_resource);
    ifm_arena_destroy(&agg_arena);

    return 0;
}
