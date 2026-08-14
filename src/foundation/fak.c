#include "ifm_costintel/fak.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(__SIZEOF_INT128__)
__extension__ typedef __int128 ifm_int128_t;
#endif

bool fak_add_micros(ifm_micros_t a, ifm_micros_t b, ifm_micros_t *out) {
    if (!out) return false;
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(a, b, out);
#else
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
        return false;
    }
    *out = a + b;
    return true;
#endif
}

bool fak_sub_micros(ifm_micros_t a, ifm_micros_t b, ifm_micros_t *out) {
    if (!out) return false;
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_sub_overflow(a, b, out);
#else
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) {
        return false;
    }
    *out = a - b;
    return true;
#endif
}

bool fak_mul_micros(ifm_micros_t a, ifm_micros_t b, ifm_micros_t *out) {
    if (!out) return false;
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }
#if defined(__SIZEOF_INT128__)
    ifm_int128_t prod = (ifm_int128_t)a * (ifm_int128_t)b;
    ifm_int128_t result_128 = prod / IFM_MICROS_PER_UNIT;
    if (result_128 > INT64_MAX || result_128 < INT64_MIN) {
        return false;
    }
    *out = (ifm_micros_t)result_128;
    return true;
#else
    int64_t prod;
    if (__builtin_mul_overflow(a, b, &prod)) return false;
    *out = prod / IFM_MICROS_PER_UNIT;
    return true;
#endif
}

bool fak_div_micros(ifm_micros_t numerator, ifm_micros_t denominator, ifm_micros_t *out) {
    if (!out || denominator == 0) return false;
    if (numerator == 0) {
        *out = 0;
        return true;
    }
#if defined(__SIZEOF_INT128__)
    ifm_int128_t scaled_num = (ifm_int128_t)numerator * (ifm_int128_t)IFM_MICROS_PER_UNIT;
    ifm_int128_t result_128 = scaled_num / (ifm_int128_t)denominator;
    if (result_128 > INT64_MAX || result_128 < INT64_MIN) {
        return false;
    }
    *out = (ifm_micros_t)result_128;
    return true;
#else
    if (numerator == INT64_MIN && denominator == -1) return false;
    *out = (numerator / denominator) * IFM_MICROS_PER_UNIT;
    return true;
#endif
}

bool fak_scale_micros(ifm_micros_t amount, int64_t multiplier, int64_t divisor, ifm_micros_t *out) {
    if (!out || divisor == 0) return false;
    if (amount == 0 || multiplier == 0) {
        *out = 0;
        return true;
    }
#if defined(__SIZEOF_INT128__)
    ifm_int128_t prod = (ifm_int128_t)amount * (ifm_int128_t)multiplier;
    ifm_int128_t result_128 = prod / (ifm_int128_t)divisor;
    if (result_128 > INT64_MAX || result_128 < INT64_MIN) {
        return false;
    }
    *out = (ifm_micros_t)result_128;
    return true;
#else
    return false;
#endif
}

bool fak_abs_micros(ifm_micros_t a, ifm_micros_t *out) {
    if (!out) return false;
    if (a == INT64_MIN) return false;
    *out = (a < 0) ? -a : a;
    return true;
}

bool fak_add_int64(int64_t a, int64_t b, int64_t *out) {
    if (!out) return false;
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(a, b, out);
#else
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
        return false;
    }
    *out = a + b;
    return true;
#endif
}

bool fak_parse_micros(const char *str, ifm_micros_t *out) {
    if (!str || !out) return false;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return false;

    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    } else if (*str == '+') {
        str++;
    }

    if (*str == '\0') return false;

    int64_t integer_part = 0;
    int64_t frac_part = 0;
    int frac_digits = 0;
    bool has_digits = false;
    bool has_dot = false;

    while (*str && *str != '\0') {
        if (isdigit((unsigned char)*str)) {
            has_digits = true;
            if (!has_dot) {
                if (__builtin_mul_overflow(integer_part, 10, &integer_part)) return false;
                if (__builtin_add_overflow(integer_part, (*str - '0'), &integer_part)) return false;
            } else {
                if (frac_digits < 6) {
                    frac_part = frac_part * 10 + (*str - '0');
                    frac_digits++;
                }
            }
        } else if (*str == '.') {
            if (has_dot) return false;
            has_dot = true;
        } else if (isspace((unsigned char)*str)) {
            while (isspace((unsigned char)*str)) str++;
            if (*str != '\0') return false;
            break;
        } else {
            return false;
        }
        str++;
    }

    if (!has_digits) return false;

    while (frac_digits < 6) {
        frac_part *= 10;
        frac_digits++;
    }

    int64_t total_micros;
    if (__builtin_mul_overflow(integer_part, IFM_MICROS_PER_UNIT, &total_micros)) {
        return false;
    }
    if (__builtin_add_overflow(total_micros, frac_part, &total_micros)) {
        return false;
    }

    if (negative) {
        if (total_micros == INT64_MIN) return false;
        total_micros = -total_micros;
    }

    *out = total_micros;
    return true;
}

bool fak_format_micros(ifm_micros_t micros, char *buf, size_t buflen) {
    if (!buf || buflen < 32) return false;

    bool negative = (micros < 0);
    uint64_t abs_val;
    if (micros == INT64_MIN) {
        abs_val = (uint64_t)INT64_MAX + 1ULL;
    } else {
        abs_val = (uint64_t)(negative ? -micros : micros);
    }

    uint64_t int_val = abs_val / 1000000ULL;
    uint64_t frac_val = abs_val % 1000000ULL;

    int written;
    if (negative) {
        written = snprintf(buf, buflen, "-%" PRIu64 ".%06" PRIu64, int_val, frac_val);
    } else {
        written = snprintf(buf, buflen, "%" PRIu64 ".%06" PRIu64, int_val, frac_val);
    }

    return (written > 0 && (size_t)written < buflen);
}
