#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "ifm_costintel/ifm_types.h"
#include "ifm_costintel/fak.h"
#include "ifm_costintel/arena.h"
#include "ifm_costintel/diagnostics.h"
#include "ifm_costintel/stream_adapter.h"
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
#include "ifm_costintel/fault_engine.h"
#include "ifm_costintel/telemetry.h"
#include "ifm_costintel/output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void print_usage(const char *prog_name) {
    fprintf(stderr, "IFM-CostIntel v1.0.0 — Production FinOps Financial Intelligence Engine\n\n");
    fprintf(stderr, "Usage: %s [OPTIONS]\n\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --config <path>     Path to JSON configuration file (rules & anomalies)\n");
    fprintf(stderr, "  --input <path>      Input NDJSON file (default: stdin)\n");
    fprintf(stderr, "  --output <path>     Output NDJSON file (default: stdout)\n");
    fprintf(stderr, "  --dlq <path>        Dead Letter Queue file (default: stderr for faults)\n");
    fprintf(stderr, "  --summary <path>    Audit summary JSON output file (default: stderr)\n");
    fprintf(stderr, "  --strict            Fail immediately with exit code 1 if any fault occurs\n");
    fprintf(stderr, "  --version           Print version and exit\n");
    fprintf(stderr, "  --help              Display this help message\n");
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    const char *input_path = NULL;
    const char *output_path = NULL;
    const char *dlq_path = NULL;
    const char *summary_path = NULL;
    bool strict_mode = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input_path = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--dlq") == 0 && i + 1 < argc) {
            dlq_path = argv[++i];
        } else if (strcmp(argv[i], "--summary") == 0 && i + 1 < argc) {
            summary_path = argv[++i];
        } else if (strcmp(argv[i], "--strict") == 0) {
            strict_mode = true;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("IFM-CostIntel v1.0.0\n");
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Open I/O Streams */
    FILE *in_fp = stdin;
    if (input_path) {
        in_fp = fopen(input_path, "rb");
        if (!in_fp) {
            fprintf(stderr, "Error: Cannot open input file '%s'\n", input_path);
            return 1;
        }
    }

    FILE *out_fp = stdout;
    if (output_path) {
        out_fp = fopen(output_path, "wb");
        if (!out_fp) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", output_path);
            if (in_fp != stdin) fclose(in_fp);
            return 1;
        }
    }

    FILE *dlq_fp = NULL;
    if (dlq_path) {
        dlq_fp = fopen(dlq_path, "wb");
        if (!dlq_fp) {
            fprintf(stderr, "Error: Cannot open DLQ file '%s'\n", dlq_path);
            if (in_fp != stdin) fclose(in_fp);
            if (out_fp != stdout) fclose(out_fp);
            return 1;
        }
    }

    FILE *summary_fp = stderr;
    if (summary_path) {
        summary_fp = fopen(summary_path, "wb");
        if (!summary_fp) {
            fprintf(stderr, "Error: Cannot open summary file '%s'\n", summary_path);
            if (in_fp != stdin) fclose(in_fp);
            if (out_fp != stdout) fclose(out_fp);
            if (dlq_fp) fclose(dlq_fp);
            return 1;
        }
    }

    /* Load Configuration */
    ifm_rule_set_t rule_set;
    ifm_rule_set_init(&rule_set);

    ifm_baseline_table_t baseline_table;
    ifm_baseline_table_init(&baseline_table);

    ifm_anomaly_rule_set_t anomaly_rules;
    ifm_anomaly_rule_set_init(&anomaly_rules);

    if (config_path) {
        if (!ifm_rule_set_load_file(&rule_set, config_path)) {
            fprintf(stderr, "Error: Failed to load allocation rules from '%s'\n", config_path);
            ifm_baseline_table_cleanup(&baseline_table);
            if (in_fp != stdin) fclose(in_fp);
            if (out_fp != stdout) fclose(out_fp);
            if (dlq_fp) fclose(dlq_fp);
            if (summary_fp != stderr && summary_fp != stdout) fclose(summary_fp);
            return 1;
        }
        if (!ifm_baseline_table_load_file(&baseline_table, config_path)) {
            fprintf(stderr, "Error: Failed to load baselines from '%s'\n", config_path);
            ifm_baseline_table_cleanup(&baseline_table);
            if (in_fp != stdin) fclose(in_fp);
            if (out_fp != stdout) fclose(out_fp);
            if (dlq_fp) fclose(dlq_fp);
            if (summary_fp != stderr && summary_fp != stdout) fclose(summary_fp);
            return 1;
        }
        if (!ifm_anomaly_rule_set_load_file(&anomaly_rules, config_path)) {
            fprintf(stderr, "Error: Failed to load anomaly rules from '%s'\n", config_path);
            ifm_baseline_table_cleanup(&baseline_table);
            if (in_fp != stdin) fclose(in_fp);
            if (out_fp != stdout) fclose(out_fp);
            if (dlq_fp) fclose(dlq_fp);
            if (summary_fp != stderr && summary_fp != stdout) fclose(summary_fp);
            return 1;
        }
    }

    /* Initialize Subsystems */
    ifm_stream_reader_t stream_reader;
    if (!ifm_stream_reader_init(&stream_reader, in_fp, IFM_STREAM_BUFFER_SIZE)) {
        fprintf(stderr, "Fatal: Failed to initialize stream adapter\n");
        ifm_baseline_table_cleanup(&baseline_table);
        if (in_fp != stdin) fclose(in_fp);
        if (out_fp != stdout) fclose(out_fp);
        if (dlq_fp) fclose(dlq_fp);
        if (summary_fp != stderr && summary_fp != stdout) fclose(summary_fp);
        return 1;
    }

    ifm_reconciliation_tracker_t reconciliation;
    ifm_reconciliation_init(&reconciliation);

    ifm_fault_engine_t fault_engine;
    ifm_fault_engine_init(&fault_engine, dlq_fp);

    ifm_telemetry_t telemetry;
    ifm_telemetry_start(&telemetry);

    /* Initialize Aggregation Subsystem */
    ifm_arena_t agg_arena;
    ifm_arena_init(&agg_arena, 65536);

    ifm_aggregation_table_t agg_provider = {0};
    ifm_aggregation_table_t agg_account = {0};
    ifm_aggregation_table_t agg_cost_center = {0};
    ifm_aggregation_table_t agg_resource = {0};

    bool agg_ok = true;
    if (!ifm_agg_table_init(&agg_provider, IFM_AGG_DIM_PROVIDER, 1024, &agg_arena)) agg_ok = false;
    if (!ifm_agg_table_init(&agg_account, IFM_AGG_DIM_ACCOUNT, 1024, &agg_arena)) agg_ok = false;
    if (!ifm_agg_table_init(&agg_cost_center, IFM_AGG_DIM_COST_CENTER, 1024, &agg_arena)) agg_ok = false;
    if (!ifm_agg_table_init(&agg_resource, IFM_AGG_DIM_RESOURCE, 4096, &agg_arena)) agg_ok = false;

    if (!agg_ok) {
        fprintf(stderr, "Fatal: Failed to initialize aggregation subsystem\n");
        ifm_stream_reader_cleanup(&stream_reader);
        ifm_baseline_table_cleanup(&baseline_table);
        ifm_agg_table_cleanup(&agg_provider);
        ifm_agg_table_cleanup(&agg_account);
        ifm_agg_table_cleanup(&agg_cost_center);
        ifm_agg_table_cleanup(&agg_resource);
        ifm_arena_destroy(&agg_arena);
        if (in_fp != stdin) fclose(in_fp);
        if (out_fp != stdout) fclose(out_fp);
        if (dlq_fp) fclose(dlq_fp);
        if (summary_fp != stderr && summary_fp != stdout) fclose(summary_fp);
        return 1;
    }

    /* Stream Processing Loop */
    char *line = NULL;
    size_t line_len = 0;
    uint64_t records_processed = 0;
    size_t total_bytes = 0;
    ifm_record_t record;

    while (ifm_stream_reader_next_line(&stream_reader, &line, &line_len)) {
        if (line_len == 0 || line[0] == '\0') continue;
        total_bytes += line_len + 1;
        uint64_t current_line_num = ifm_stream_reader_get_line_number(&stream_reader);

        /* 1. Ingress: JSON Decode */
        if (!ifm_json_decode_record(line, line_len, &record)) {
            ifm_traceability_stamp(&record, current_line_num);
            ifm_reconciliation_accumulate(&reconciliation, &record);
            ifm_fault_engine_record_fault(&fault_engine, &record);
            records_processed++;
            if (strict_mode) break;
            continue;
        }

        /* 2. Ingress: Traceability & Schema Validation */
        ifm_traceability_stamp(&record, current_line_num);
        if (!ifm_schema_validate_record(&record)) {
            ifm_reconciliation_accumulate(&reconciliation, &record);
            ifm_fault_engine_record_fault(&fault_engine, &record);
            records_processed++;
            if (strict_mode) break;
            continue;
        }

        /* 3. Intelligence: Allocation */
        ifm_allocate_record(&rule_set, &record);

        /* 4. Intelligence: Multi-Dimensional Aggregation */
        if (!record.is_faulted) {
            ifm_agg_table_accumulate(&agg_provider, &record);
            ifm_agg_table_accumulate(&agg_account, &record);
            ifm_agg_table_accumulate(&agg_cost_center, &record);
            ifm_agg_table_accumulate(&agg_resource, &record);
        }

        /* 5. Intelligence: Variance */
        ifm_micros_t baseline = 0;
        ifm_baseline_table_lookup(&baseline_table, record.resource_id, &baseline);
        ifm_compute_variance(&record, baseline);

        /* 6. Intelligence: Anomaly Detection */
        ifm_evaluate_anomalies(&anomaly_rules, &record);

        /* 7. Governance: Reconciliation & Output */
        ifm_reconciliation_accumulate(&reconciliation, &record);
        ifm_write_record_ndjson(&record, out_fp);

        records_processed++;
    }

    /* Telemetry & Final Reconciliation Verification */
    ifm_telemetry_stop(&telemetry, records_processed, total_bytes);
    bool recon_ok = ifm_reconciliation_verify(&reconciliation);

    ifm_telemetry_write_summary(&telemetry, &reconciliation, summary_fp);

    /* Cleanup */
    ifm_stream_reader_cleanup(&stream_reader);
    ifm_baseline_table_cleanup(&baseline_table);
    ifm_agg_table_cleanup(&agg_provider);
    ifm_agg_table_cleanup(&agg_account);
    ifm_agg_table_cleanup(&agg_cost_center);
    ifm_agg_table_cleanup(&agg_resource);
    ifm_arena_destroy(&agg_arena);

    if (in_fp != stdin) fclose(in_fp);
    if (out_fp != stdout) fclose(out_fp);
    if (dlq_fp) fclose(dlq_fp);
    if (summary_fp != stderr && summary_fp != stdout) fclose(summary_fp);

    if (!recon_ok) {
        fprintf(stderr, "FATAL: Financial or population reconciliation invariant failure!\n");
        return 2;
    }

    if (strict_mode && fault_engine.total_faults > 0) {
        return 1;
    }

    return 0;
}
