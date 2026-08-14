#include "ifm_costintel/diagnostics.h"
#include <stdarg.h>

const char *ifm_fault_code_str(ifm_fault_code_t code) {
    switch (code) {
        case IFM_FAULT_NONE: return "NONE";
        case IFM_FAULT_IO_ERROR: return "IO_ERROR";
        case IFM_FAULT_JSON_SYNTAX: return "JSON_SYNTAX";
        case IFM_FAULT_MISSING_REQUIRED_FIELD: return "MISSING_REQUIRED_FIELD";
        case IFM_FAULT_INVALID_NUMERIC: return "INVALID_NUMERIC";
        case IFM_FAULT_STRING_OVERFLOW: return "STRING_OVERFLOW";
        case IFM_FAULT_SCHEMA_VIOLATION: return "SCHEMA_VIOLATION";
        case IFM_FAULT_ARITHMETIC_OVERFLOW: return "ARITHMETIC_OVERFLOW";
        case IFM_FAULT_DIVISION_BY_ZERO: return "DIVISION_BY_ZERO";
        case IFM_FAULT_RECONCILIATION_POPULATION_MISMATCH: return "RECONCILIATION_POPULATION_MISMATCH";
        case IFM_FAULT_RECONCILIATION_FINANCIAL_MISMATCH: return "RECONCILIATION_FINANCIAL_MISMATCH";
        case IFM_FAULT_ALLOCATION_CONFLICT: return "ALLOCATION_CONFLICT";
        default: return "UNKNOWN_FAULT";
    }
}

const char *ifm_severity_str(ifm_severity_t sev) {
    switch (sev) {
        case IFM_SEV_NONE: return "NONE";
        case IFM_SEV_WARN: return "WARN";
        case IFM_SEV_ERR: return "ERROR";
        case IFM_SEV_FATAL: return "FATAL";
        default: return "UNKNOWN_SEV";
    }
}

const char *ifm_alloc_status_str(ifm_alloc_status_t status) {
    switch (status) {
        case IFM_ALLOC_UNALLOCATED: return "UNALLOCATED";
        case IFM_ALLOC_ALLOCATED: return "ALLOCATED";
        case IFM_ALLOC_AMBIGUOUS: return "AMBIGUOUS";
        case IFM_ALLOC_FAULTED: return "FAULTED";
        default: return "UNKNOWN_STATUS";
    }
}

void ifm_log_diagnostic(FILE *stream, ifm_severity_t sev, ifm_fault_code_t code, const char *fmt, ...) {
    if (!stream) stream = stderr;
    fprintf(stream, "[%s] [%s] ", ifm_severity_str(sev), ifm_fault_code_str(code));
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
    fprintf(stream, "\n");
}
