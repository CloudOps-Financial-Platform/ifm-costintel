#include "ifm_costintel/fault_engine.h"
#include "ifm_costintel/diagnostics.h"
#include "ifm_costintel/fak.h"
#include <string.h>

void ifm_fault_engine_init(ifm_fault_engine_t *fe, FILE *dlq_file) {
    if (!fe) return;
    fe->dlq_file = dlq_file;
    fe->total_faults = 0;
    fe->warn_count = 0;
    fe->err_count = 0;
    fe->fatal_count = 0;
}

bool ifm_fault_engine_record_fault(ifm_fault_engine_t *fe, const ifm_record_t *record) {
    if (!fe || !record) return false;

    fe->total_faults++;
    if (record->fault_severity == IFM_SEV_WARN) fe->warn_count++;
    else if (record->fault_severity == IFM_SEV_ERR) fe->err_count++;
    else if (record->fault_severity == IFM_SEV_FATAL) fe->fatal_count++;

    if (fe->dlq_file) {
        char cost_buf[32];
        fak_format_micros(record->billed_cost_micros, cost_buf, sizeof(cost_buf));

        fprintf(fe->dlq_file,
            "{\"source_line\":%" PRIu64
            ",\"provider\":\"%s\""
            ",\"provider_row_id\":\"%s\""
            ",\"account_id\":\"%s\""
            ",\"resource_id\":\"%s\""
            ",\"billed_cost_micros\":%" PRId64
            ",\"fault_code\":\"%s\""
            ",\"fault_severity\":\"%s\""
            ",\"fault_message\":\"%s\"}\n",
            record->source_line,
            record->provider,
            record->provider_row_id,
            record->account_id,
            record->resource_id,
            record->billed_cost_micros,
            ifm_fault_code_str(record->fault_code),
            ifm_severity_str(record->fault_severity),
            record->fault_message
        );
        fflush(fe->dlq_file);
    }
    return true;
}
