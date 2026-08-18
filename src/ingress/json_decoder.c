#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/fak.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

static inline void safe_strcpy(char *dest, size_t dest_cap, const char *src) {
    if (!dest || dest_cap == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dest_cap) len = dest_cap - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && isspace((unsigned char)*p)) p++;
    return p;
}

static bool parse_strict_int64(const char *str, int64_t *out) {
    if (!str || *str == '\0') return false;
    if (*str == '+' || isspace((unsigned char)*str)) return false;
    const char *p = str;
    if (*p == '-') {
        p++;
        if (*p == '\0' || !isdigit((unsigned char)*p)) return false;
    } else {
        if (!isdigit((unsigned char)*p)) return false;
    }
    while (*p) {
        if (!isdigit((unsigned char)*p)) return false;
        p++;
    }
    char *endptr = NULL;
    errno = 0;
    long long val = strtoll(str, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || endptr == str) return false;
#if defined(LLONG_MAX) && defined(INT64_MAX) && (LLONG_MAX > INT64_MAX)
    if (val > INT64_MAX) return false;
#endif
#if defined(LLONG_MIN) && defined(INT64_MIN) && (LLONG_MIN < INT64_MIN)
    if (val < INT64_MIN) return false;
#endif
    *out = (int64_t)val;
    return true;
}

static bool parse_strict_uint64(const char *str, uint64_t *out) {
    if (!str || *str == '\0') return false;
    if (*str == '-' || *str == '+' || isspace((unsigned char)*str)) return false;
    const char *p = str;
    while (*p) {
        if (!isdigit((unsigned char)*p)) return false;
        p++;
    }
    char *endptr = NULL;
    errno = 0;
    unsigned long long val = strtoull(str, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || endptr == str) return false;
#if defined(ULLONG_MAX) && defined(UINT64_MAX) && (ULLONG_MAX > UINT64_MAX)
    if (val > UINT64_MAX) return false;
#endif
    *out = (uint64_t)val;
    return true;
}

static bool parse_strict_uint32(const char *str, uint32_t *out) {
    if (!str || *str == '\0') return false;
    if (*str == '-' || *str == '+' || isspace((unsigned char)*str)) return false;
    const char *p = str;
    while (*p) {
        if (!isdigit((unsigned char)*p)) return false;
        p++;
    }
    char *endptr = NULL;
    errno = 0;
    unsigned long long val = strtoull(str, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || endptr == str) return false;
    if (val > UINT32_MAX) return false;
    *out = (uint32_t)val;
    return true;
}

bool ifm_json_unescape(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    if (!src || !dst || dst_cap == 0) return false;
    size_t out_idx = 0;
    for (size_t i = 0; i < src_len; ++i) {
        if (out_idx + 1 >= dst_cap) return false;
        if (src[i] == '\\' && i + 1 < src_len) {
            i++;
            switch (src[i]) {
                case '"':  dst[out_idx++] = '"'; break;
                case '\\': dst[out_idx++] = '\\'; break;
                case '/':  dst[out_idx++] = '/'; break;
                case 'b':  dst[out_idx++] = '\b'; break;
                case 'f':  dst[out_idx++] = '\f'; break;
                case 'n':  dst[out_idx++] = '\n'; break;
                case 'r':  dst[out_idx++] = '\r'; break;
                case 't':  dst[out_idx++] = '\t'; break;
                default:   dst[out_idx++] = src[i]; break;
            }
        } else {
            dst[out_idx++] = src[i];
        }
    }
    dst[out_idx] = '\0';
    return true;
}

static bool parse_string_token(const char **cursor, const char *end, char *out_buf, size_t buf_cap) {
    const char *p = *cursor;
    if (p >= end || *p != '"') return false;
    p++;

    const char *start = p;
    while (p < end && *p != '"') {
        if (*p == '\\') {
            p++;
            if (p >= end) return false;
        }
        p++;
    }

    if (p >= end || *p != '"') return false;

    size_t len = p - start;
    if (!ifm_json_unescape(start, len, out_buf, buf_cap)) {
        return false;
    }

    *cursor = p + 1;
    return true;
}

static bool parse_number_or_raw(const char **cursor, const char *end, char *out_buf, size_t buf_cap) {
    const char *p = *cursor;
    const char *start = p;
    while (p < end && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p)) {
        p++;
    }
    if (p == start) return false;
    size_t len = (size_t)(p - start);
    if (len >= buf_cap) return false;
    memcpy(out_buf, start, len);
    out_buf[len] = '\0';
    *cursor = p;
    return true;
}

bool ifm_json_decode_record(const char *json_str, size_t json_len, ifm_record_t *record) {
    if (!json_str || !record) return false;

    memset(record, 0, sizeof(ifm_record_t));
    record->alloc_status = IFM_ALLOC_UNALLOCATED;
    record->variance_status = IFM_VARIANCE_DEFINED;

    const char *p = json_str;
    const char *end = json_str + json_len;

    p = skip_ws(p, end);
    if (p >= end || *p != '{') {
        record->is_faulted = true;
        record->fault_code = IFM_FAULT_JSON_SYNTAX;
        record->fault_severity = IFM_SEV_ERR;
        snprintf(record->fault_message, sizeof(record->fault_message), "Expected JSON object start '{'");
        return false;
    }
    p++;

    char key[64];
    char val_str[256];
    bool has_cost_micros = false;
    bool has_cost_decimal = false;
    ifm_micros_t parsed_cost_micros = 0;

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) break;
        if (*p == '}') {
            p++;
            break;
        }
        if (*p == ',') {
            p++;
            continue;
        }

        if (!parse_string_token(&p, end, key, sizeof(key))) {
            record->is_faulted = true;
            record->fault_code = IFM_FAULT_JSON_SYNTAX;
            record->fault_severity = IFM_SEV_ERR;
            snprintf(record->fault_message, sizeof(record->fault_message), "Malformed JSON key");
            return false;
        }

        p = skip_ws(p, end);
        if (p >= end || *p != ':') {
            record->is_faulted = true;
            record->fault_code = IFM_FAULT_JSON_SYNTAX;
            record->fault_severity = IFM_SEV_ERR;
            snprintf(record->fault_message, sizeof(record->fault_message), "Expected ':' after key %s", key);
            return false;
        }
        p++;
        p = skip_ws(p, end);

        if (p >= end) break;

        if (*p == '"') {
            if (!parse_string_token(&p, end, val_str, sizeof(val_str))) {
                record->is_faulted = true;
                record->fault_code = IFM_FAULT_JSON_SYNTAX;
                record->fault_severity = IFM_SEV_ERR;
                snprintf(record->fault_message, sizeof(record->fault_message), "Malformed string value for key %s", key);
                return false;
            }

            if (strcmp(key, "provider") == 0) {
                safe_strcpy(record->provider, sizeof(record->provider), val_str);
            } else if (strcmp(key, "provider_row_id") == 0) {
                safe_strcpy(record->provider_row_id, sizeof(record->provider_row_id), val_str);
            } else if (strcmp(key, "account_id") == 0) {
                safe_strcpy(record->account_id, sizeof(record->account_id), val_str);
            } else if (strcmp(key, "resource_id") == 0) {
                safe_strcpy(record->resource_id, sizeof(record->resource_id), val_str);
            } else if (strcmp(key, "usage_start_raw") == 0 || strcmp(key, "usage_start") == 0) {
                safe_strcpy(record->usage_start_raw, sizeof(record->usage_start_raw), val_str);
            } else if (strcmp(key, "billed_cost") == 0 || strcmp(key, "cost") == 0) {
                if (fak_parse_micros(val_str, &parsed_cost_micros)) {
                    has_cost_decimal = true;
                } else {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid decimal cost format: %.128s", val_str);
                    return false;
                }
            } else if (strcmp(key, "billed_cost_micros") == 0 || strcmp(key, "cost_micros") == 0) {
                if (parse_strict_int64(val_str, &record->billed_cost_micros)) {
                    has_cost_micros = true;
                } else {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid integer cost_micros: %.128s", val_str);
                    return false;
                }
            } else if (strcmp(key, "source_line") == 0) {
                if (!parse_strict_uint64(val_str, &record->source_line)) {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid source_line numeric: %.128s", val_str);
                    return false;
                }
            } else if (strcmp(key, "flags") == 0) {
                if (!parse_strict_uint32(val_str, &record->flags)) {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid flags numeric: %.128s", val_str);
                    return false;
                }
            } else if (strcmp(key, "cost_center_id") == 0) {
                safe_strcpy(record->cost_center_id, sizeof(record->cost_center_id), val_str);
            }
        } else if (*p == '{' || *p == '[') {
            int depth = 0;
            char open_char = *p;
            char close_char = (open_char == '{') ? '}' : ']';
            while (p < end) {
                if (*p == open_char) depth++;
                else if (*p == close_char) {
                    depth--;
                    if (depth == 0) { p++; break; }
                }
                p++;
            }
        } else {
            if (!parse_number_or_raw(&p, end, val_str, sizeof(val_str))) {
                record->is_faulted = true;
                record->fault_code = IFM_FAULT_JSON_SYNTAX;
                record->fault_severity = IFM_SEV_ERR;
                snprintf(record->fault_message, sizeof(record->fault_message), "Malformed value for key %s", key);
                return false;
            }

            if (strcmp(key, "billed_cost_micros") == 0 || strcmp(key, "cost_micros") == 0) {
                if (!parse_strict_int64(val_str, &record->billed_cost_micros)) {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid integer cost_micros: %.128s", val_str);
                    return false;
                }
                has_cost_micros = true;
            } else if (strcmp(key, "billed_cost") == 0 || strcmp(key, "cost") == 0) {
                if (fak_parse_micros(val_str, &parsed_cost_micros)) {
                    has_cost_decimal = true;
                } else {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid decimal cost numeric: %.128s", val_str);
                    return false;
                }
            } else if (strcmp(key, "source_line") == 0) {
                if (!parse_strict_uint64(val_str, &record->source_line)) {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid source_line numeric: %.128s", val_str);
                    return false;
                }
            } else if (strcmp(key, "flags") == 0) {
                if (!parse_strict_uint32(val_str, &record->flags)) {
                    record->is_faulted = true;
                    record->fault_code = IFM_FAULT_INVALID_NUMERIC;
                    record->fault_severity = IFM_SEV_ERR;
                    snprintf(record->fault_message, sizeof(record->fault_message), "Invalid flags numeric: %.128s", val_str);
                    return false;
                }
            }
        }
    }

    if (!has_cost_micros && has_cost_decimal) {
        record->billed_cost_micros = parsed_cost_micros;
        has_cost_micros = true;
    }

    record->active_spend_micros = record->billed_cost_micros;
    return true;
}

