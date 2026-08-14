#ifndef IFM_TELEMETRY_H
#define IFM_TELEMETRY_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "ifm_costintel/ifm_types.h"
#include "ifm_costintel/reconciliation.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct timespec start_time;
    struct timespec end_time;
    double duration_seconds;
    double records_per_sec;
    double mb_per_sec;
    size_t total_bytes_read;
} ifm_telemetry_t;

/* Start telemetry timer */
void ifm_telemetry_start(ifm_telemetry_t *tel);

/* Stop telemetry timer and compute statistics */
void ifm_telemetry_stop(ifm_telemetry_t *tel, uint64_t record_count, size_t bytes_read);

/* Write summary JSON report to stream */
void ifm_telemetry_write_summary(const ifm_telemetry_t *tel,
                                 const ifm_reconciliation_tracker_t *recon,
                                 FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* IFM_TELEMETRY_H */
