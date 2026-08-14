#ifndef IFM_OUTPUT_H
#define IFM_OUTPUT_H

#include "ifm_costintel/ifm_types.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Format record to canonical NDJSON string */
bool ifm_format_record_ndjson(const ifm_record_t *record, char *buf, size_t buflen);

/* Write record directly to output stream */
bool ifm_write_record_ndjson(const ifm_record_t *record, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* IFM_OUTPUT_H */
