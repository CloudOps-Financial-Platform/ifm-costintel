#ifndef IFM_JSON_DECODER_H
#define IFM_JSON_DECODER_H

#include "ifm_costintel/ifm_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JSON_TOK_NONE,
    JSON_TOK_OBJECT_START,
    JSON_TOK_OBJECT_END,
    JSON_TOK_ARRAY_START,
    JSON_TOK_ARRAY_END,
    JSON_TOK_STRING,
    JSON_TOK_NUMBER,
    JSON_TOK_BOOL,
    JSON_TOK_NULL,
    JSON_TOK_COLON,
    JSON_TOK_COMMA,
    JSON_TOK_ERROR
} ifm_json_token_type_t;

/* Parse a single line NDJSON billing record into ifm_record_t */
bool ifm_json_decode_record(const char *json_str, size_t json_len, ifm_record_t *record);

/* Helper to unescape JSON string in-place or into buffer */
bool ifm_json_unescape(const char *src, size_t src_len, char *dst, size_t dst_cap);

#ifdef __cplusplus
}
#endif

#endif /* IFM_JSON_DECODER_H */
