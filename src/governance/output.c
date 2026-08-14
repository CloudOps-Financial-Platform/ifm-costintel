#include "ifm_costintel/output.h"
#include "ifm_costintel/diagnostics.h"
#include <stdio.h>
#include <string.h>

static const char *variance_status_str(ifm_variance_status_t st) {
    switch (st) {
        case IFM_VARIANCE_DEFINED: return "DEFINED";
        case IFM_VARIANCE_BASELINE_ZERO: return "BASELINE_ZERO";
        case IFM_VARIANCE_BASELINE_ZERO_NO_CHANGE: return "BASELINE_ZERO_NO_CHANGE";
        default: return "UNKNOWN";
    }
}

static const char *anomaly_dir_str(ifm_anomaly_dir_t dir) {
    switch (dir) {
        case IFM_ANOMALY_DIR_SPIKE: return "SPIKE";
        case IFM_ANOMALY_DIR_DROP: return "DROP";
        case IFM_ANOMALY_DIR_BOTH: return "BOTH";
        default: return "NONE";
    }
}

bool ifm_format_record_ndjson(const ifm_record_t *rec, char *buf, size_t buflen) {
    if (!rec || !buf || buflen == 0) return false;

    int written = snprintf(buf, buflen,
        "{"
        "\"source_line\":%" PRIu64 ","
        "\"provider\":\"%s\","
        "\"provider_row_id\":\"%s\","
        "\"account_id\":\"%s\","
        "\"resource_id\":\"%s\","
        "\"usage_start_raw\":\"%s\","
        "\"allocation_status\":\"%s\","
        "\"cost_center_id\":\"%s\","
        "\"rule_id\":\"%s\","
        "\"rule_version\":%" PRIu32 ","
        "\"active_spend_micros\":%" PRId64 ","
        "\"baseline_micros\":%" PRId64 ","
        "\"variance_delta_micros\":%" PRId64 ","
        "\"variance_status\":\"%s\","
        "\"is_anomaly\":%s,"
        "\"anomaly_rule_id\":\"%s\","
        "\"anomaly_direction\":\"%s\""
        "}",
        rec->source_line,
        rec->provider,
        rec->provider_row_id,
        rec->account_id,
        rec->resource_id,
        rec->usage_start_raw,
        ifm_alloc_status_str(rec->alloc_status),
        rec->cost_center_id,
        rec->rule_id,
        rec->rule_version,
        rec->active_spend_micros,
        rec->baseline_micros,
        rec->variance_delta_micros,
        variance_status_str(rec->variance_status),
        rec->is_anomaly ? "true" : "false",
        rec->anomaly_rule_id,
        anomaly_dir_str(rec->anomaly_direction)
    );

    return (written > 0 && (size_t)written < buflen);
}

bool ifm_write_record_ndjson(const ifm_record_t *record, FILE *out) {
    if (!record || !out) return false;
    char line_buf[2048];
    if (!ifm_format_record_ndjson(record, line_buf, sizeof(line_buf))) return false;
    fprintf(out, "%s\n", line_buf);
    return true;
}
