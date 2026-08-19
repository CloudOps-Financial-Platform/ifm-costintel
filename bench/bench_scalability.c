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

#define AGG_ARENA_SIZE (2 * 1024 * 1024) /* 2 MB */
#define NUM_CARDINALITIES 5
#define NUM_MODES 3

typedef enum {
    MODE_UNIFORM = 0,
    MODE_TAIL    = 1,
    MODE_MISS    = 2
} lookup_mode_t;

static const char *mode_names[NUM_MODES] = {
    "Uniform",
    "Tail   ",
    "Miss   "
};

static const size_t cardinalities[NUM_CARDINALITIES] = {
    100,
    1000,
    10000,
    50000,
    100000
};

/* 16 Distinct Multi-Cloud Payloads with Controlled Resource Keys */
static const char *bench_corpus[] = {
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-001\",\"account_id\":\"111222333444\",\"resource_id\":\"res-cloud-00\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":45800000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-002\",\"account_id\":\"111222333444\",\"resource_id\":\"res-cloud-01\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":12500000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-003\",\"account_id\":\"555666777888\",\"resource_id\":\"res-cloud-02\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":8200000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-004\",\"account_id\":\"999000111222\",\"resource_id\":\"res-cloud-03\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":95000000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-005\",\"account_id\":\"sub-core-infra\",\"resource_id\":\"res-cloud-04\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":34000000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-006\",\"account_id\":\"sub-core-infra\",\"resource_id\":\"res-cloud-05\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":6200000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-007\",\"account_id\":\"sub-data-ai\",\"resource_id\":\"res-cloud-06\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":120000000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-008\",\"account_id\":\"sub-unmapped\",\"resource_id\":\"res-cloud-07\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":1500000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-009\",\"account_id\":\"proj-analytics-prod\",\"resource_id\":\"res-cloud-08\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":55000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-010\",\"account_id\":\"proj-analytics-prod\",\"resource_id\":\"res-cloud-09\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":210000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-011\",\"account_id\":\"proj-k8s-cluster\",\"resource_id\":\"res-cloud-10\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":78000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-012\",\"account_id\":\"proj-unallocated\",\"resource_id\":\"res-cloud-11\",\"usage_start_raw\":\"2026-08-19T00:00:00Z\",\"billed_cost_micros\":4300000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-013\",\"account_id\":\"111222333444\",\"resource_id\":\"res-cloud-12\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":45800000,\"flags\":0}",
    "{\"provider\":\"azure\",\"provider_row_id\":\"row-014\",\"account_id\":\"sub-core-infra\",\"resource_id\":\"res-cloud-13\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":34000000,\"flags\":0}",
    "{\"provider\":\"gcp\",\"provider_row_id\":\"row-015\",\"account_id\":\"proj-analytics-prod\",\"resource_id\":\"res-cloud-14\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":210000000,\"flags\":0}",
    "{\"provider\":\"aws\",\"provider_row_id\":\"row-016\",\"account_id\":\"999000111222\",\"resource_id\":\"res-cloud-15\",\"usage_start_raw\":\"2026-08-19T01:00:00Z\",\"billed_cost_micros\":180000000,\"flags\":0}"
};

static const size_t corpus_count = sizeof(bench_corpus) / sizeof(bench_corpus[0]);

static const char *target_keys[16] = {
    "res-cloud-00", "res-cloud-01", "res-cloud-02", "res-cloud-03",
    "res-cloud-04", "res-cloud-05", "res-cloud-06", "res-cloud-07",
    "res-cloud-08", "res-cloud-09", "res-cloud-10", "res-cloud-11",
    "res-cloud-12", "res-cloud-13", "res-cloud-14", "res-cloud-15"
};

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void populate_baseline_table(ifm_baseline_table_t *table, size_t n, lookup_mode_t mode) {
    ifm_baseline_table_init(table);

    if (mode == MODE_UNIFORM) {
        size_t interval = (n > 16) ? (n / 16) : 1;
        size_t placed = 0;
        for (size_t i = 0; i < n; ++i) {
            char key[64];
            if (placed < 16 && (i % interval == 0 || (n - i <= 16 - placed))) {
                snprintf(key, sizeof(key), "%s", target_keys[placed++]);
            } else {
                snprintf(key, sizeof(key), "res-baseline-%06zu", i);
            }
            ifm_baseline_table_set(table, key, 40000000);
        }
    } else if (mode == MODE_TAIL) {
        size_t tail_start = (n >= 16) ? (n - 16) : 0;
        size_t placed = 0;
        for (size_t i = 0; i < n; ++i) {
            char key[64];
            if (i >= tail_start && placed < 16) {
                snprintf(key, sizeof(key), "%s", target_keys[placed++]);
            } else {
                snprintf(key, sizeof(key), "res-baseline-%06zu", i);
            }
            ifm_baseline_table_set(table, key, 40000000);
        }
    } else { /* MODE_MISS */
        for (size_t i = 0; i < n; ++i) {
            char key[64];
            snprintf(key, sizeof(key), "res-baseline-%06zu", i);
            ifm_baseline_table_set(table, key, 40000000);
        }
    }
}

/* Tier 1: Isolated Baseline Lookup Micro-Benchmark */
static double run_micro_benchmark(const ifm_baseline_table_t *table, size_t lookups, volatile ifm_micros_t *sink) {
    ifm_micros_t out_val = 0;
    ifm_micros_t accum = 0;

    double t0 = get_time_sec();
    for (size_t i = 0; i < lookups; ++i) {
        const char *k = target_keys[i % 16];
        if (ifm_baseline_table_lookup(table, k, &out_val)) {
            accum += out_val;
        }
    }
    double t1 = get_time_sec();
    *sink = accum;

    double dt = t1 - t0;
    if (dt <= 0.0) dt = 0.000000001;
    return (dt * 1e9) / (double)lookups; /* ns per lookup */
}

/* Tier 2: Full 9-Stage Pipeline Macro-Benchmark */
static void run_macro_benchmark(const ifm_baseline_table_t *table,
                                uint64_t records,
                                double *out_duration,
                                double *out_throughput,
                                double *out_mbps,
                                bool *out_recon_ok) {
    /* Rules */
    ifm_rule_set_t rule_set;
    ifm_rule_set_init(&rule_set);
    ifm_allocation_rule_t r1 = {.rule_id = "R-AWS", .priority = 100, .match_provider = "aws", .match_account_id = "111*", .match_resource_prefix = "*", .target_cost_center_id = "CC-INFRA-AWS", .version = 1};
    ifm_allocation_rule_t r2 = {.rule_id = "R-AZ",  .priority = 100, .match_provider = "azure", .match_account_id = "sub-core*", .match_resource_prefix = "*", .target_cost_center_id = "CC-INFRA-AZURE", .version = 1};
    ifm_allocation_rule_t r3 = {.rule_id = "R-GCP", .priority = 100, .match_provider = "gcp", .match_account_id = "proj-analytics*", .match_resource_prefix = "*", .target_cost_center_id = "CC-DATA-GCP", .version = 1};
    ifm_rule_set_add(&rule_set, &r1);
    ifm_rule_set_add(&rule_set, &r2);
    ifm_rule_set_add(&rule_set, &r3);

    /* Anomalies */
    ifm_anomaly_rule_set_t anomaly_rules;
    ifm_anomaly_rule_set_init(&anomaly_rules);
    ifm_anomaly_rule_t a1 = {.rule_id = "ANOM-SPIKE-50", .threshold_pct_micros = 500000, .min_baseline_micros = 50000000, .direction = IFM_ANOMALY_DIR_SPIKE};
    ifm_anomaly_rule_set_add(&anomaly_rules, &a1);

    /* 4D Aggregation */
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
    assert(agg_ok);

    /* Reconciliation */
    ifm_reconciliation_tracker_t tracker;
    ifm_reconciliation_init(&tracker);

    uint64_t total_bytes = 0;
    double t_start = get_time_sec();

    for (uint64_t i = 0; i < records; ++i) {
        const char *payload = bench_corpus[i % corpus_count];
        size_t payload_len = strlen(payload);
        total_bytes += payload_len;

        ifm_record_t record;
        memset(&record, 0, sizeof(record));

        /* Stage 1: JSON Decode */
        if (!ifm_json_decode_record(payload, payload_len, &record)) {
            record.is_faulted = true;
            record.fault_code = IFM_FAULT_JSON_SYNTAX;
        }

        /* Stage 2: Traceability */
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

        /* Stage 5: 4D Aggregation */
        if (!record.is_faulted) {
            ifm_agg_table_accumulate(&agg_provider, &record);
            ifm_agg_table_accumulate(&agg_account, &record);
            ifm_agg_table_accumulate(&agg_cost_center, &record);
            ifm_agg_table_accumulate(&agg_resource, &record);
        }

        /* Stage 6: Baseline Lookup */
        ifm_micros_t baseline = 0;
        if (!record.is_faulted) {
            ifm_baseline_table_lookup(table, record.resource_id, &baseline);
        }

        /* Stage 7: Variance Computation */
        if (!record.is_faulted) {
            ifm_compute_variance(&record, baseline);
        }

        /* Stage 8: Anomaly Evaluation */
        if (!record.is_faulted) {
            ifm_evaluate_anomalies(&anomaly_rules, &record);
        }

        /* Stage 9: Reconciliation */
        ifm_reconciliation_accumulate(&tracker, &record);
    }

    double t_end = get_time_sec();
    double duration = t_end - t_start;
    if (duration <= 0.0) duration = 0.000001;

    *out_duration = duration;
    *out_throughput = (double)records / duration;
    *out_mbps = ((double)total_bytes / (1024.0 * 1024.0)) / duration;
    *out_recon_ok = ifm_reconciliation_verify(&tracker);

    ifm_agg_table_cleanup(&agg_provider);
    ifm_agg_table_cleanup(&agg_account);
    ifm_agg_table_cleanup(&agg_cost_center);
    ifm_agg_table_cleanup(&agg_resource);
    ifm_arena_destroy(&agg_arena);
}

int main(int argc, char **argv) {
    uint64_t records_per_tier = 50000;
    size_t micro_lookups = 20000;

    if (argc > 1) {
        long v = atol(argv[1]);
        if (v > 0) records_per_tier = (uint64_t)v;
    }

    printf("=========================================================================\n");
    printf("IFM-CostIntel v1.0.0 — Sprint 3C Scalability Profiler\n");
    printf("Records per Tier: %" PRIu64 " | Micro-Lookups: %zu\n", records_per_tier, micro_lookups);
    printf("=========================================================================\n\n");
    fflush(stdout);

    typedef struct {
        size_t n;
        lookup_mode_t mode;
        double ns_lookup;
        double rec_per_sec;
        double mbps;
        double retained_pct;
        bool recon_ok;
    } result_entry_t;

    result_entry_t results[NUM_CARDINALITIES * NUM_MODES];
    size_t res_idx = 0;
    volatile ifm_micros_t sink = 0;

    double base_throughput[NUM_MODES] = {0.0, 0.0, 0.0};

    for (size_t c = 0; c < NUM_CARDINALITIES; ++c) {
        size_t n = cardinalities[c];

        for (int m = 0; m < NUM_MODES; ++m) {
            lookup_mode_t mode = (lookup_mode_t)m;

            ifm_baseline_table_t table;
            populate_baseline_table(&table, n, mode);

            /* Tier 1: Micro-Benchmark */
            double ns_lookup = run_micro_benchmark(&table, micro_lookups, &sink);

            /* Tier 2: Macro-Benchmark */
            double duration = 0.0;
            double throughput = 0.0;
            double mbps = 0.0;
            bool recon_ok = false;

            run_macro_benchmark(&table, records_per_tier, &duration, &throughput, &mbps, &recon_ok);
            assert(recon_ok && "Reconciliation failed during scalability profiler run!");

            if (c == 0) {
                base_throughput[m] = throughput;
            }

            double retained = (base_throughput[m] > 0.0) ? (throughput / base_throughput[m]) * 100.0 : 100.0;

            results[res_idx].n = n;
            results[res_idx].mode = mode;
            results[res_idx].ns_lookup = ns_lookup;
            results[res_idx].rec_per_sec = throughput;
            results[res_idx].mbps = mbps;
            results[res_idx].retained_pct = retained;
            results[res_idx].recon_ok = recon_ok;
            res_idx++;

            printf("  [COMPLETED] Baseline N=%-6zu | Mode: %s | Lookup: %10.2f ns | Pipeline: %10.2f rec/s | Retained: %6.2f%%\n",
                   n, mode_names[m], ns_lookup, throughput, retained);
            fflush(stdout);

            ifm_baseline_table_cleanup(&table);
        }
    }

    /* Print Formatted Markdown Summary Table */
    printf("\n======================== SPRINT 3C EXPERIMENTAL RESULTS ========================\n\n");
    printf("| Baseline N | Lookup Mode | Isolated ns/lookup | Pipeline rec/sec | Bandwidth (MB/s) | Retained %% | Reconciliation |\n");
    printf("|:-----------|:------------|:-------------------|:-----------------|:-----------------|:-----------|:---------------|\n");

    for (size_t i = 0; i < res_idx; ++i) {
        printf("| %-10zu | %-11s | %18.2f | %16.2f | %16.2f | %9.2f%% | %-14s |\n",
               results[i].n,
               mode_names[results[i].mode],
               results[i].ns_lookup,
               results[i].rec_per_sec,
               results[i].mbps,
               results[i].retained_pct,
               results[i].recon_ok ? "PASS (100%)" : "FAIL");
    }

    printf("\n================================================================================\n");
    fflush(stdout);
    return 0;
}
