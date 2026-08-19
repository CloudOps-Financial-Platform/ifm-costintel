#include "ifm_costintel/variance.h"
#include "ifm_costintel/fak.h"
#include "ifm_costintel/json_decoder.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

static inline void safe_strcpy(char *dest, size_t dest_cap, const char *src) {
    if (!dest || dest_cap == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dest_cap) len = dest_cap - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

void ifm_baseline_table_init(ifm_baseline_table_t *table) {
    if (!table) return;
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

bool ifm_baseline_table_set(ifm_baseline_table_t *table, const char *key, ifm_micros_t baseline_micros) {
    if (!table || !key) return false;

    for (size_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i].key, key) == 0) {
            table->entries[i].baseline_micros = baseline_micros;
            return true;
        }
    }

    if (table->count >= table->capacity) {
        size_t new_cap = (table->capacity == 0) ? 64 : table->capacity * 2;
        ifm_baseline_entry_t *new_entries = (ifm_baseline_entry_t *)realloc(table->entries, new_cap * sizeof(ifm_baseline_entry_t));
        if (!new_entries) return false;
        table->entries = new_entries;
        table->capacity = new_cap;
    }

    safe_strcpy(table->entries[table->count].key, sizeof(table->entries[table->count].key), key);
    table->entries[table->count].baseline_micros = baseline_micros;
    table->count++;
    return true;
}

bool ifm_baseline_table_lookup(const ifm_baseline_table_t *table, const char *key, ifm_micros_t *out_baseline) {
    if (!table || !key || !out_baseline) return false;
    for (size_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i].key, key) == 0) {
            *out_baseline = table->entries[i].baseline_micros;
            return true;
        }
    }
    return false;
}

void ifm_baseline_table_cleanup(ifm_baseline_table_t *table) {
    if (!table) return;
    if (table->entries) {
        free(table->entries);
        table->entries = NULL;
    }
    table->count = 0;
    table->capacity = 0;
}

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && isspace((unsigned char)*p)) p++;
    return p;
}

static bool parse_str_val(const char **cursor, const char *end, char *out_buf, size_t buf_cap) {
    const char *p = *cursor;
    if (p >= end || *p != '"') return false;
    p++;
    const char *start = p;
    while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) p++;
        p++;
    }
    if (p >= end || *p != '"') return false;
    size_t len = (size_t)(p - start);
    if (!ifm_json_unescape(start, len, out_buf, buf_cap)) return false;
    *cursor = p + 1;
    return true;
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

static bool parse_baseline_object(const char **cursor, const char *end, char *out_key, size_t key_cap, ifm_micros_t *out_micros) {
    const char *p = *cursor;
    p = skip_ws(p, end);
    if (p >= end || *p != '{') return false;
    p++;

    out_key[0] = '\0';
    *out_micros = 0;
    bool has_key = false;
    bool has_micros = false;

    char k[64];
    char val_str[256];

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) return false;
        if (*p == '}') {
            p++;
            *cursor = p;
            return (has_key && has_micros);
        }
        if (*p == ',') {
            p++;
            continue;
        }

        if (!parse_str_val(&p, end, k, sizeof(k))) return false;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') return false;
        p++;
        p = skip_ws(p, end);

        if (*p == '"') {
            if (!parse_str_val(&p, end, val_str, sizeof(val_str))) return false;
            if (strcmp(k, "key") == 0 || strcmp(k, "resource_id") == 0) {
                safe_strcpy(out_key, key_cap, val_str);
                has_key = (out_key[0] != '\0');
            } else if (strcmp(k, "baseline") == 0 || strcmp(k, "cost") == 0) {
                if (!fak_parse_micros(val_str, out_micros)) return false;
                has_micros = true;
            } else if (strcmp(k, "baseline_micros") == 0) {
                if (!parse_strict_int64(val_str, out_micros)) return false;
                has_micros = true;
            } else {
                return false;
            }
        } else {
            const char *num_start = p;
            while (p < end && *p != ',' && *p != '}' && !isspace((unsigned char)*p)) p++;
            size_t num_len = (size_t)(p - num_start);
            if (num_len == 0 || num_len >= sizeof(val_str)) return false;

            memcpy(val_str, num_start, num_len);
            val_str[num_len] = '\0';

            if (strcmp(k, "baseline_micros") == 0) {
                if (!parse_strict_int64(val_str, out_micros)) return false;
                has_micros = true;
            } else if (strcmp(k, "baseline") == 0 || strcmp(k, "cost") == 0) {
                if (!fak_parse_micros(val_str, out_micros)) return false;
                has_micros = true;
            } else {
                return false;
            }
        }
    }
    return false;
}

bool ifm_baseline_table_load_json(ifm_baseline_table_t *table, const char *json_str, size_t json_len) {
    if (!table || !json_str) return false;

    const char *p = json_str;
    const char *end = json_str + json_len;

    const char *baselines_pos = strstr(json_str, "\"baselines\"");
    if (!baselines_pos) return true;

    p = baselines_pos + strlen("\"baselines\"");
    p = skip_ws(p, end);
    if (p >= end || *p != ':') goto fail;
    p++;
    p = skip_ws(p, end);
    if (p >= end || *p != '[') goto fail;
    p++;

    bool closed = false;
    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) goto fail;
        if (*p == ']') {
            closed = true;
            p++;
            break;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '{') {
            char key[256];
            ifm_micros_t micros = 0;
            if (!parse_baseline_object(&p, end, key, sizeof(key), &micros)) {
                goto fail;
            }
            if (!ifm_baseline_table_set(table, key, micros)) {
                goto fail;
            }
        } else {
            goto fail;
        }
    }

    if (!closed) goto fail;
    return true;

fail:
    ifm_baseline_table_cleanup(table);
    ifm_baseline_table_init(table);
    return false;
}

bool ifm_baseline_table_load_file(ifm_baseline_table_t *table, const char *filepath) {
    if (!table || !filepath) return false;
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return false;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }
    long sz = ftell(fp);
    if (sz < 0 || sz > 10 * 1024 * 1024) {
        fclose(fp);
        return false;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return false;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, fp);
    if (ferror(fp) || read_bytes != (size_t)sz) {
        free(buf);
        fclose(fp);
        return false;
    }
    fclose(fp);
    buf[read_bytes] = '\0';

    bool ok = ifm_baseline_table_load_json(table, buf, read_bytes);
    free(buf);
    return ok;
}

bool ifm_compute_variance(ifm_record_t *record, ifm_micros_t baseline_micros) {
    if (!record) return false;

    record->baseline_micros = baseline_micros;
    if (!fak_sub_micros(record->active_spend_micros, baseline_micros, &record->variance_delta_micros)) {
        record->is_faulted = true;
        record->fault_code = IFM_FAULT_ARITHMETIC_OVERFLOW;
        record->fault_severity = IFM_SEV_ERR;
        return false;
    }

    if (baseline_micros == 0) {
        if (record->active_spend_micros == 0) {
            record->variance_status = IFM_VARIANCE_BASELINE_ZERO_NO_CHANGE;
            record->variance_pct_micros = 0;
        } else {
            record->variance_status = IFM_VARIANCE_BASELINE_ZERO;
            record->variance_pct_micros = 0;
        }
    } else {
        record->variance_status = IFM_VARIANCE_DEFINED;
        if (!fak_div_micros(record->variance_delta_micros, baseline_micros, &record->variance_pct_micros)) {
            record->variance_pct_micros = 0;
        }
    }

    return true;
}
