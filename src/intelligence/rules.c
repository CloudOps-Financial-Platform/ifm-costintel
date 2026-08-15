#include "ifm_costintel/rules.h"
#include "ifm_costintel/json_decoder.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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

void ifm_rule_set_init(ifm_rule_set_t *rs) {
    if (!rs) return;
    memset(rs, 0, sizeof(ifm_rule_set_t));
    safe_strcpy(rs->config_version, sizeof(rs->config_version), "v1.0.0");
    rs->version_number = 1;
    rs->rule_count = 0;
}

bool ifm_rule_set_add(ifm_rule_set_t *rs, const ifm_allocation_rule_t *rule) {
    if (!rs || !rule || rs->rule_count >= IFM_MAX_RULES) return false;
    memcpy(&rs->rules[rs->rule_count++], rule, sizeof(ifm_allocation_rule_t));
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
    size_t len = p - start;
    if (!ifm_json_unescape(start, len, out_buf, buf_cap)) return false;
    *cursor = p + 1;
    return true;
}

static bool parse_strict_int32(const char *str, int32_t *out) {
    if (!str || *str == '\0') return false;
    char *endptr = NULL;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || endptr == str) return false;
    if (val < INT32_MIN || val > INT32_MAX) return false;
    *out = (int32_t)val;
    return true;
}

static bool parse_strict_uint32(const char *str, uint32_t *out) {
    if (!str || *str == '\0') return false;
    if (*str == '-') return false;
    char *endptr = NULL;
    errno = 0;
    unsigned long val = strtoul(str, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || endptr == str) return false;
    if (val > UINT32_MAX) return false;
    *out = (uint32_t)val;
    return true;
}

static bool parse_rule_object(const char **cursor, const char *end, ifm_allocation_rule_t *rule) {
    const char *p = *cursor;
    p = skip_ws(p, end);
    if (p >= end || *p != '{') return false;
    p++;

    memset(rule, 0, sizeof(ifm_allocation_rule_t));
    rule->priority = 100;
    rule->version = 1;

    char key[64];
    char val_str[256];

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) break;
        if (*p == '}') {
            p++;
            *cursor = p;
            return true;
        }
        if (*p == ',') {
            p++;
            continue;
        }

        if (!parse_str_val(&p, end, key, sizeof(key))) return false;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') return false;
        p++;
        p = skip_ws(p, end);

        if (*p == '"') {
            if (!parse_str_val(&p, end, val_str, sizeof(val_str))) return false;
            if (strcmp(key, "rule_id") == 0) {
                safe_strcpy(rule->rule_id, sizeof(rule->rule_id), val_str);
            } else if (strcmp(key, "match_provider") == 0 || strcmp(key, "provider") == 0) {
                safe_strcpy(rule->match_provider, sizeof(rule->match_provider), val_str);
            } else if (strcmp(key, "match_account_id") == 0 || strcmp(key, "account_id") == 0) {
                safe_strcpy(rule->match_account_id, sizeof(rule->match_account_id), val_str);
            } else if (strcmp(key, "match_resource_prefix") == 0 || strcmp(key, "resource_prefix") == 0) {
                safe_strcpy(rule->match_resource_prefix, sizeof(rule->match_resource_prefix), val_str);
            } else if (strcmp(key, "target_cost_center_id") == 0 || strcmp(key, "cost_center_id") == 0) {
                safe_strcpy(rule->target_cost_center_id, sizeof(rule->target_cost_center_id), val_str);
            }
        } else {
            const char *num_start = p;
            if (*p == '-') p++;
            if (p >= end || !isdigit((unsigned char)*p)) return false;
            while (p < end && isdigit((unsigned char)*p)) p++;
            size_t num_len = p - num_start;
            if (num_len == 0 || num_len >= sizeof(val_str)) return false;

            memcpy(val_str, num_start, num_len);
            val_str[num_len] = '\0';

            if (strcmp(key, "priority") == 0) {
                if (!parse_strict_int32(val_str, &rule->priority)) return false;
            } else if (strcmp(key, "version") == 0) {
                if (!parse_strict_uint32(val_str, &rule->version)) return false;
            }
        }
    }
    *cursor = p;
    return true;
}

bool ifm_rule_set_load_json(ifm_rule_set_t *rs, const char *json_str, size_t json_len) {
    if (!rs || !json_str) return false;
    ifm_rule_set_init(rs);

    const char *p = json_str;
    const char *end = json_str + json_len;

    const char *rules_pos = strstr(json_str, "\"rules\"");
    if (!rules_pos) return true;

    p = rules_pos + strlen("\"rules\"");
    p = skip_ws(p, end);
    if (p >= end || *p != ':') return false;
    p++;
    p = skip_ws(p, end);
    if (p >= end || *p != '[') return false;
    p++;

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == ']') {
            break;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '{') {
            ifm_allocation_rule_t rule;
            if (parse_rule_object(&p, end, &rule)) {
                ifm_rule_set_add(rs, &rule);
            } else {
                p++;
            }
        } else {
            p++;
        }
    }

    return true;
}

bool ifm_rule_set_load_file(ifm_rule_set_t *rs, const char *filepath) {
    if (!rs || !filepath) return false;
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz <= 0 || sz > 10 * 1024 * 1024) {
        fclose(fp);
        return false;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(fp);
        return false;
    }

    size_t read_bytes = fread(buf, 1, sz, fp);
    fclose(fp);
    buf[read_bytes] = '\0';

    bool ok = ifm_rule_set_load_json(rs, buf, read_bytes);
    free(buf);
    return ok;
}
