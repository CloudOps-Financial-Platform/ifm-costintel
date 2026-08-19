#include "ifm_costintel/anomaly.h"
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

void ifm_anomaly_rule_set_init(ifm_anomaly_rule_set_t *ars) {
    if (!ars) return;
    ars->count = 0;
}

bool ifm_anomaly_rule_set_add(ifm_anomaly_rule_set_t *ars, const ifm_anomaly_rule_t *rule) {
    if (!ars || !rule || ars->count >= 64) return false;
    memcpy(&ars->rules[ars->count++], rule, sizeof(ifm_anomaly_rule_t));
    return true;
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

static bool parse_anomaly_object(const char **cursor, const char *end, ifm_anomaly_rule_t *rule) {
    const char *p = *cursor;
    p = skip_ws(p, end);
    if (p >= end || *p != '{') return false;
    p++;

    memset(rule, 0, sizeof(ifm_anomaly_rule_t));
    bool has_rule_id = false;
    bool has_threshold = false;
    bool has_direction = false;

    char k[64];
    char val_str[256];

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) return false;
        if (*p == '}') {
            p++;
            *cursor = p;
            return (has_rule_id && has_threshold && has_direction &&
                    rule->rule_id[0] != '\0' &&
                    rule->threshold_pct_micros > 0 &&
                    rule->min_baseline_micros >= 0);
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
            if (strcmp(k, "rule_id") == 0) {
                safe_strcpy(rule->rule_id, sizeof(rule->rule_id), val_str);
                has_rule_id = (rule->rule_id[0] != '\0');
            } else if (strcmp(k, "direction") == 0) {
                if (strcasecmp(val_str, "SPIKE") == 0) {
                    rule->direction = IFM_ANOMALY_DIR_SPIKE;
                    has_direction = true;
                } else if (strcasecmp(val_str, "DROP") == 0) {
                    rule->direction = IFM_ANOMALY_DIR_DROP;
                    has_direction = true;
                } else if (strcasecmp(val_str, "BOTH") == 0) {
                    rule->direction = IFM_ANOMALY_DIR_BOTH;
                    has_direction = true;
                } else {
                    return false;
                }
            } else if (strcmp(k, "threshold_pct_micros") == 0) {
                if (!parse_strict_int64(val_str, &rule->threshold_pct_micros) || rule->threshold_pct_micros <= 0) return false;
                has_threshold = true;
            } else if (strcmp(k, "threshold_pct") == 0) {
                if (!fak_parse_micros(val_str, &rule->threshold_pct_micros) || rule->threshold_pct_micros <= 0) return false;
                has_threshold = true;
            } else if (strcmp(k, "min_baseline_micros") == 0) {
                if (!parse_strict_int64(val_str, &rule->min_baseline_micros) || rule->min_baseline_micros < 0) return false;
            } else if (strcmp(k, "min_baseline") == 0) {
                if (!fak_parse_micros(val_str, &rule->min_baseline_micros) || rule->min_baseline_micros < 0) return false;
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

            if (strcmp(k, "threshold_pct_micros") == 0) {
                if (!parse_strict_int64(val_str, &rule->threshold_pct_micros) || rule->threshold_pct_micros <= 0) return false;
                has_threshold = true;
            } else if (strcmp(k, "threshold_pct") == 0) {
                if (!fak_parse_micros(val_str, &rule->threshold_pct_micros) || rule->threshold_pct_micros <= 0) return false;
                has_threshold = true;
            } else if (strcmp(k, "min_baseline_micros") == 0) {
                if (!parse_strict_int64(val_str, &rule->min_baseline_micros) || rule->min_baseline_micros < 0) return false;
            } else if (strcmp(k, "min_baseline") == 0) {
                if (!fak_parse_micros(val_str, &rule->min_baseline_micros) || rule->min_baseline_micros < 0) return false;
            } else if (strcmp(k, "direction") == 0) {
                int64_t dir_num = 0;
                if (!parse_strict_int64(val_str, &dir_num)) return false;
                if (dir_num == 1) {
                    rule->direction = IFM_ANOMALY_DIR_SPIKE;
                    has_direction = true;
                } else if (dir_num == 2) {
                    rule->direction = IFM_ANOMALY_DIR_DROP;
                    has_direction = true;
                } else if (dir_num == 3) {
                    rule->direction = IFM_ANOMALY_DIR_BOTH;
                    has_direction = true;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    return false;
}

bool ifm_anomaly_rule_set_load_json(ifm_anomaly_rule_set_t *ars, const char *json_str, size_t json_len) {
    if (!ars || !json_str) return false;

    const char *p = json_str;
    const char *end = json_str + json_len;

    const char *anomalies_pos = strstr(json_str, "\"anomalies\"");
    if (!anomalies_pos) return true;

    p = anomalies_pos + strlen("\"anomalies\"");
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
            ifm_anomaly_rule_t rule;
            if (!parse_anomaly_object(&p, end, &rule)) {
                goto fail;
            }
            if (!ifm_anomaly_rule_set_add(ars, &rule)) {
                goto fail;
            }
        } else {
            goto fail;
        }
    }

    if (!closed) goto fail;
    return true;

fail:
    ars->count = 0;
    return false;
}

bool ifm_anomaly_rule_set_load_file(ifm_anomaly_rule_set_t *ars, const char *filepath) {
    if (!ars || !filepath) return false;
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

    bool ok = ifm_anomaly_rule_set_load_json(ars, buf, read_bytes);
    free(buf);
    return ok;
}

void ifm_evaluate_anomalies(const ifm_anomaly_rule_set_t *ars, ifm_record_t *record) {
    if (!record) return;

    record->is_anomaly = false;
    record->anomaly_rule_id[0] = '\0';
    record->anomaly_direction = IFM_ANOMALY_DIR_NONE;

    if (!ars || ars->count == 0 || record->variance_status != IFM_VARIANCE_DEFINED) {
        return;
    }

    for (size_t i = 0; i < ars->count; ++i) {
        const ifm_anomaly_rule_t *r = &ars->rules[i];

        if (record->baseline_micros < r->min_baseline_micros) {
            continue;
        }

        ifm_micros_t delta = record->variance_delta_micros;
        ifm_micros_t abs_delta;
        if (!fak_abs_micros(delta, &abs_delta)) continue;

        ifm_micros_t pct_change;
        if (!fak_div_micros(abs_delta, record->baseline_micros, &pct_change)) continue;

        if (pct_change >= r->threshold_pct_micros) {
            if (delta > 0 && (r->direction == IFM_ANOMALY_DIR_SPIKE || r->direction == IFM_ANOMALY_DIR_BOTH)) {
                record->is_anomaly = true;
                safe_strcpy(record->anomaly_rule_id, sizeof(record->anomaly_rule_id), r->rule_id);
                record->anomaly_direction = IFM_ANOMALY_DIR_SPIKE;
                return;
            } else if (delta < 0 && (r->direction == IFM_ANOMALY_DIR_DROP || r->direction == IFM_ANOMALY_DIR_BOTH)) {
                record->is_anomaly = true;
                safe_strcpy(record->anomaly_rule_id, sizeof(record->anomaly_rule_id), r->rule_id);
                record->anomaly_direction = IFM_ANOMALY_DIR_DROP;
                return;
            }
        }
    }
}
