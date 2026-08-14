#undef NDEBUG
#include "ifm_costintel/fak.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_fak_addition(void) {
    ifm_micros_t out;
    assert(fak_add_micros(1000000, 2000000, &out) && out == 3000000);
    assert(fak_add_micros(-500000, 1000000, &out) && out == 500000);
    assert(fak_add_micros(-500000, -500000, &out) && out == -1000000);
    /* Boundary overflow */
    assert(!fak_add_micros(INT64_MAX, 1, &out));
    assert(!fak_add_micros(INT64_MIN, -1, &out));
    printf("  [PASS] fak_addition\n");
}

static void test_fak_subtraction(void) {
    ifm_micros_t out;
    assert(fak_sub_micros(3000000, 1000000, &out) && out == 2000000);
    assert(fak_sub_micros(1000000, 3000000, &out) && out == -2000000);
    assert(!fak_sub_micros(INT64_MIN, 1, &out));
    assert(!fak_sub_micros(INT64_MAX, -1, &out));
    printf("  [PASS] fak_subtraction\n");
}

static void test_fak_multiplication(void) {
    ifm_micros_t out;
    /* $2.50 * $4.00 = $10.00 */
    assert(fak_mul_micros(2500000, 4000000, &out) && out == 10000000);
    /* $0.50 * $0.50 = $0.25 */
    assert(fak_mul_micros(500000, 500000, &out) && out == 250000);
    /* $10.00 * -$2.00 = -$20.00 */
    assert(fak_mul_micros(10000000, -2000000, &out) && out == -20000000);
    printf("  [PASS] fak_multiplication\n");
}

static void test_fak_division(void) {
    ifm_micros_t out;
    /* $10.00 / 2.0 = $5.00 */
    assert(fak_div_micros(10000000, 2000000, &out) && out == 5000000);
    /* Division by zero */
    assert(!fak_div_micros(10000000, 0, &out));
    printf("  [PASS] fak_division\n");
}

static void test_fak_parsing_formatting(void) {
    ifm_micros_t out;
    char buf[64];

    assert(fak_parse_micros("45.80", &out) && out == 45800000);
    assert(fak_parse_micros("0.000001", &out) && out == 1);
    assert(fak_parse_micros("-12.345678", &out) && out == -12345678);
    assert(fak_parse_micros("100", &out) && out == 100000000);
    assert(fak_parse_micros("  +5.50  ", &out) && out == 5500000);

    /* Invalid string tests */
    assert(!fak_parse_micros("abc", &out));
    assert(!fak_parse_micros("12.34.56", &out));
    assert(!fak_parse_micros("", &out));

    /* Formatting */
    assert(fak_format_micros(45800000, buf, sizeof(buf)));
    assert(strcmp(buf, "45.800000") == 0);
    assert(fak_format_micros(-12345678, buf, sizeof(buf)));
    assert(strcmp(buf, "-12.345678") == 0);
    assert(fak_format_micros(0, buf, sizeof(buf)));
    assert(strcmp(buf, "0.000000") == 0);

    printf("  [PASS] fak_parsing_formatting\n");
}

int main(void) {
    printf("Running FAK Unit Tests...\n");
    test_fak_addition();
    test_fak_subtraction();
    test_fak_multiplication();
    test_fak_division();
    test_fak_parsing_formatting();
    printf("ALL FAK UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
