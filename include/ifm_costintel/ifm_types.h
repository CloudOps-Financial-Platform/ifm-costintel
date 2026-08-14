#ifndef IFM_TYPES_H
#define IFM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Financial Fixed-Point Micro-units
 * 1 Currency Unit = 1,000,000 micros ($1.00 = 1,000,000 micros)
 * All monetary calculations in IFM-CostIntel strictly use ifm_micros_t.
 */
typedef int64_t ifm_micros_t;

#define IFM_MICROS_PER_UNIT ((ifm_micros_t)1000000LL)
#define IFM_MAX_MICROS INT64_MAX
#define IFM_MIN_MICROS INT64_MIN

/* Record allocation status */
typedef enum {
    IFM_ALLOC_UNALLOCATED = 0,
    IFM_ALLOC_ALLOCATED   = 1,
    IFM_ALLOC_AMBIGUOUS   = 2,
    IFM_ALLOC_FAULTED     = 3
} ifm_alloc_status_t;

/* Severity levels */
typedef enum {
    IFM_SEV_NONE  = 0,
    IFM_SEV_WARN  = 1,
    IFM_SEV_ERR   = 2,
    IFM_SEV_FATAL = 3
} ifm_severity_t;

/* Variance measurement status */
typedef enum {
    IFM_VARIANCE_DEFINED                 = 1,
    IFM_VARIANCE_BASELINE_ZERO           = 2,
    IFM_VARIANCE_BASELINE_ZERO_NO_CHANGE = 3
} ifm_variance_status_t;

/* Concentration status */
typedef enum {
    IFM_CONCENTRATION_DEFINED     = 1,
    IFM_CONCENTRATION_NOT_DEFINED = 2
} ifm_concentration_status_t;

/* Anomaly direction */
typedef enum {
    IFM_ANOMALY_DIR_NONE  = 0,
    IFM_ANOMALY_DIR_SPIKE = 1,
    IFM_ANOMALY_DIR_DROP  = 2,
    IFM_ANOMALY_DIR_BOTH  = 3
} ifm_anomaly_dir_t;

/* Provider classification */
typedef enum {
    IFM_PROVIDER_UNKNOWN = 0,
    IFM_PROVIDER_AWS     = 1,
    IFM_PROVIDER_AZURE   = 2,
    IFM_PROVIDER_GCP     = 3,
    IFM_PROVIDER_CUSTOM  = 4
} ifm_provider_t;

/* Fault codes */
typedef enum {
    IFM_FAULT_NONE                               = 0,
    /* Ingress Faults (100-199) */
    IFM_FAULT_IO_ERROR                           = 101,
    IFM_FAULT_JSON_SYNTAX                        = 102,
    IFM_FAULT_MISSING_REQUIRED_FIELD             = 103,
    IFM_FAULT_INVALID_NUMERIC                    = 104,
    IFM_FAULT_STRING_OVERFLOW                    = 105,
    IFM_FAULT_SCHEMA_VIOLATION                   = 106,
    /* Arithmetic Faults (200-299) */
    IFM_FAULT_ARITHMETIC_OVERFLOW                = 201,
    IFM_FAULT_DIVISION_BY_ZERO                   = 202,
    /* Governance & Reconciliation Faults (300-399) */
    IFM_FAULT_RECONCILIATION_POPULATION_MISMATCH = 301,
    IFM_FAULT_RECONCILIATION_FINANCIAL_MISMATCH  = 302,
    /* Allocation Faults (400-499) */
    IFM_FAULT_ALLOCATION_CONFLICT                = 401
} ifm_fault_code_t;

/* Normalized Billing Record (Contract between Ingress, Intelligence, and Governance) */
typedef struct {
    uint64_t source_line;
    char provider[32];
    char provider_row_id[128];
    char account_id[128];
    char resource_id[256];
    char usage_start_raw[64];
    ifm_micros_t billed_cost_micros;
    uint32_t flags;

    /* Intelligence State */
    ifm_alloc_status_t alloc_status;
    char cost_center_id[128];
    char rule_id[64];
    uint32_t rule_version;

    /* Variance & Baseline */
    ifm_micros_t active_spend_micros;
    ifm_micros_t baseline_micros;
    ifm_micros_t variance_delta_micros;
    ifm_micros_t variance_pct_micros; /* 1,000,000 = 100.0% */
    ifm_variance_status_t variance_status;

    /* Anomaly State */
    bool is_anomaly;
    char anomaly_rule_id[64];
    ifm_anomaly_dir_t anomaly_direction;

    /* Governance & Fault State */
    bool is_faulted;
    ifm_fault_code_t fault_code;
    ifm_severity_t fault_severity;
    char fault_message[256];
} ifm_record_t;

#ifdef __cplusplus
}
#endif

#endif /* IFM_TYPES_H */
