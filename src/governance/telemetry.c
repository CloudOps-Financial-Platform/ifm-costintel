#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "ifm_costintel/telemetry.h"
#include "ifm_costintel/fak.h"
#include <string.h>
#include <time.h>

void ifm_telemetry_start(ifm_telemetry_t *tel) {
    if (!tel) return;
    memset(tel, 0, sizeof(ifm_telemetry_t));
    clock_gettime(CLOCK_MONOTONIC, &tel->start_time);
}

void ifm_telemetry_stop(ifm_telemetry_t *tel, uint64_t record_count, size_t bytes_read) {
    if (!tel) return;
    clock_gettime(CLOCK_MONOTONIC, &tel->end_time);

    double start_sec = (double)tel->start_time.tv_sec + (double)tel->start_time.tv_nsec / 1e9;
    double end_sec = (double)tel->end_time.tv_sec + (double)tel->end_time.tv_nsec / 1e9;
    tel->duration_seconds = end_sec - start_sec;
    if (tel->duration_seconds < 0.000001) tel->duration_seconds = 0.000001;

    tel->total_bytes_read = bytes_read;
    tel->records_per_sec = (double)record_count / tel->duration_seconds;
    tel->mb_per_sec = ((double)bytes_read / (1024.0 * 1024.0)) / tel->duration_seconds;
}

void ifm_telemetry_write_summary(const ifm_telemetry_t *tel,
                                 const ifm_reconciliation_tracker_t *recon,
                                 FILE *out) {
    if (!out) out = stdout;

    uint64_t gap_count = 0;
    ifm_micros_t gap_micros = 0;
    ifm_micros_t gap_pct_micros = 0;

    if (recon) {
        gap_count = recon->unallocated_count + recon->ambiguous_count;
        if (fak_add_micros(recon->unallocated_micros, recon->ambiguous_micros, &gap_micros)) {
            if (recon->total_input_micros > 0 && gap_micros > 0) {
                if (!fak_div_micros(gap_micros, recon->total_input_micros, &gap_pct_micros)) {
                    gap_pct_micros = -1;
                }
            }
        } else {
            gap_micros = -1;
            gap_pct_micros = -1;
        }
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"software_version\": \"1.0.0\",\n");
    fprintf(out, "  \"duration_seconds\": %.6f,\n", tel ? tel->duration_seconds : 0.0);
    fprintf(out, "  \"records_per_second\": %.2f,\n", tel ? tel->records_per_sec : 0.0);
    fprintf(out, "  \"mb_per_second\": %.2f,\n", tel ? tel->mb_per_sec : 0.0);
    fprintf(out, "  \"reconciliation\": {\n");
    fprintf(out, "    \"population_reconciled\": %s,\n", (recon && recon->population_reconciled) ? "true" : "false");
    fprintf(out, "    \"financial_reconciled\": %s,\n", (recon && recon->financial_reconciled) ? "true" : "false");
    fprintf(out, "    \"total_input_count\": %" PRIu64 ",\n", recon ? recon->total_input_count : 0);
    fprintf(out, "    \"allocated_count\": %" PRIu64 ",\n", recon ? recon->allocated_count : 0);
    fprintf(out, "    \"unallocated_count\": %" PRIu64 ",\n", recon ? recon->unallocated_count : 0);
    fprintf(out, "    \"ambiguous_count\": %" PRIu64 ",\n", recon ? recon->ambiguous_count : 0);
    fprintf(out, "    \"faulted_count\": %" PRIu64 ",\n", recon ? recon->faulted_count : 0);
    fprintf(out, "    \"total_input_micros\": %" PRId64 ",\n", recon ? recon->total_input_micros : 0);
    fprintf(out, "    \"allocated_micros\": %" PRId64 ",\n", recon ? recon->allocated_micros : 0);
    fprintf(out, "    \"unallocated_micros\": %" PRId64 ",\n", recon ? recon->unallocated_micros : 0);
    fprintf(out, "    \"ambiguous_micros\": %" PRId64 ",\n", recon ? recon->ambiguous_micros : 0);
    fprintf(out, "    \"faulted_micros\": %" PRId64 "\n", recon ? recon->faulted_micros : 0);
    fprintf(out, "  },\n");
    fprintf(out, "  \"allocation_gap\": {\n");
    fprintf(out, "    \"gap_count\": %" PRIu64 ",\n", gap_count);
    fprintf(out, "    \"gap_micros\": %" PRId64 ",\n", gap_micros);
    fprintf(out, "    \"gap_pct_micros\": %" PRId64 "\n", gap_pct_micros);
    fprintf(out, "  }\n");
    fprintf(out, "}\n");
}
