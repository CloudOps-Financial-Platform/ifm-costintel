#include "ifm_costintel/schema_validator.h"
#include <string.h>
#include <stdio.h>

bool ifm_schema_validate_record(ifm_record_t *record) {
    if (!record) return false;
    if (record->is_faulted) return false; /* Already faulted by decoder */

    if (record->provider[0] == '\0') {
        record->is_faulted = true;
        record->fault_code = IFM_FAULT_MISSING_REQUIRED_FIELD;
        record->fault_severity = IFM_SEV_ERR;
        snprintf(record->fault_message, sizeof(record->fault_message), "Missing required field: 'provider'");
        return false;
    }

    if (record->account_id[0] == '\0') {
        record->is_faulted = true;
        record->fault_code = IFM_FAULT_MISSING_REQUIRED_FIELD;
        record->fault_severity = IFM_SEV_ERR;
        snprintf(record->fault_message, sizeof(record->fault_message), "Missing required field: 'account_id'");
        return false;
    }

    if (record->resource_id[0] == '\0') {
        record->is_faulted = true;
        record->fault_code = IFM_FAULT_MISSING_REQUIRED_FIELD;
        record->fault_severity = IFM_SEV_ERR;
        snprintf(record->fault_message, sizeof(record->fault_message), "Missing required field: 'resource_id'");
        return false;
    }

    return true;
}
