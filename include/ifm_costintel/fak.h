#ifndef IFM_FAK_H
#define IFM_FAK_H

#include "ifm_costintel/ifm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Checked fixed-point addition: *out = a + b */
bool fak_add_micros(ifm_micros_t a, ifm_micros_t b, ifm_micros_t *out);

/* Checked fixed-point subtraction: *out = a - b */
bool fak_sub_micros(ifm_micros_t a, ifm_micros_t b, ifm_micros_t *out);

/* Checked fixed-point multiplication: *out = (a * b) / 1,000,000 */
bool fak_mul_micros(ifm_micros_t a, ifm_micros_t b, ifm_micros_t *out);

/* Checked fixed-point division: *out = (numerator * 1,000,000) / denominator */
bool fak_div_micros(ifm_micros_t numerator, ifm_micros_t denominator, ifm_micros_t *out);

/* Checked scaling: *out = (amount * multiplier) / divisor */
bool fak_scale_micros(ifm_micros_t amount, int64_t multiplier, int64_t divisor, ifm_micros_t *out);

/* Checked absolute value: *out = |a| */
bool fak_abs_micros(ifm_micros_t a, ifm_micros_t *out);

/* Checked 64-bit integer addition */
bool fak_add_int64(int64_t a, int64_t b, int64_t *out);

/* Parse decimal string into fixed-point micros without floating point (e.g. "45.80" -> 45800000) */
bool fak_parse_micros(const char *str, ifm_micros_t *out);

/* Format fixed-point micros into canonical decimal string (e.g. 45800000 -> "45.800000") */
bool fak_format_micros(ifm_micros_t micros, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* IFM_FAK_H */
