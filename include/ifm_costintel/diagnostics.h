#ifndef IFM_DIAGNOSTICS_H
#define IFM_DIAGNOSTICS_H

#include "ifm_costintel/ifm_types.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *ifm_fault_code_str(ifm_fault_code_t code);
const char *ifm_severity_str(ifm_severity_t sev);
const char *ifm_alloc_status_str(ifm_alloc_status_t status);

void ifm_log_diagnostic(FILE *stream, ifm_severity_t sev, ifm_fault_code_t code, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* IFM_DIAGNOSTICS_H */
