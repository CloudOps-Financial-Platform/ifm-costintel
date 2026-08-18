#undef NDEBUG
#include "ifm_costintel/stream_adapter.h"
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include "ifm_costintel/traceability.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

static void test_json_decoder_valid(void) {
    const char *json_line = "{\"provider\":\"aws\",\"provider_row_id\":\"row-123\",\"account_id\":\"123456789012\",\"resource_id\":\"i-0abcd1234ef567890\",\"usage_start_raw\":\"2026-08-01T00:00:00Z\",\"billed_cost_micros\":45800000,\"flags\":0}";
    ifm_record_t rec;
    assert(ifm_json_decode_record(json_line, strlen(json_line), &rec));
    assert(!rec.is_faulted);
    assert(strcmp(rec.provider, "aws") == 0);
    assert(strcmp(rec.provider_row_id, "row-123") == 0);
    assert(strcmp(rec.account_id, "123456789012") == 0);
    assert(strcmp(rec.resource_id, "i-0abcd1234ef567890") == 0);
    assert(rec.billed_cost_micros == 45800000);

    assert(ifm_schema_validate_record(&rec));
    printf("  [PASS] test_json_decoder_valid\n");
}

static void test_json_decoder_decimal_cost(void) {
    const char *json_line = "{\"provider\":\"azure\",\"account_id\":\"sub-001\",\"resource_id\":\"vm-app\",\"billed_cost\":\"12.500000\"}";
    ifm_record_t rec;
    assert(ifm_json_decode_record(json_line, strlen(json_line), &rec));
    assert(!rec.is_faulted);
    assert(rec.billed_cost_micros == 12500000);
    assert(ifm_schema_validate_record(&rec));
    printf("  [PASS] test_json_decoder_decimal_cost\n");
}

static void test_json_decoder_missing_required(void) {
    const char *json_line = "{\"provider\":\"aws\",\"billed_cost_micros\":1000000}";
    ifm_record_t rec;
    assert(ifm_json_decode_record(json_line, strlen(json_line), &rec));
    assert(!ifm_schema_validate_record(&rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_MISSING_REQUIRED_FIELD);
    printf("  [PASS] test_json_decoder_missing_required\n");
}

static void test_json_decoder_malformed(void) {
    const char *json_line = "{provider: invalid_json";
    ifm_record_t rec;
    assert(!ifm_json_decode_record(json_line, strlen(json_line), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_JSON_SYNTAX);
    printf("  [PASS] test_json_decoder_malformed\n");
}

static void test_json_decoder_numeric_hardening(void) {
    ifm_record_t rec;

    /* --- Valid Edge Cases --- */
    /* 1. Zero billed_cost_micros, source_line, and flags */
    const char *zero_case = "{\"provider\":\"aws\",\"provider_row_id\":\"r1\",\"account_id\":\"a1\",\"resource_id\":\"res1\",\"usage_start_raw\":\"2026-08-01\",\"billed_cost_micros\":0,\"source_line\":0,\"flags\":0}";
    assert(ifm_json_decode_record(zero_case, strlen(zero_case), &rec));
    assert(!rec.is_faulted);
    assert(rec.billed_cost_micros == 0);
    assert(rec.source_line == 0);
    assert(rec.flags == 0);

    /* 2. Maximum allowable positive micro-units (INT64_MAX) */
    const char *max_int64_case = "{\"provider\":\"aws\",\"provider_row_id\":\"r1\",\"account_id\":\"a1\",\"resource_id\":\"res1\",\"usage_start_raw\":\"2026-08-01\",\"billed_cost_micros\":9223372036854775807}";
    assert(ifm_json_decode_record(max_int64_case, strlen(max_int64_case), &rec));
    assert(!rec.is_faulted);
    assert(rec.billed_cost_micros == INT64_MAX);

    /* 3. Minimum allowable negative micro-units (INT64_MIN) */
    const char *min_int64_case = "{\"provider\":\"aws\",\"provider_row_id\":\"r1\",\"account_id\":\"a1\",\"resource_id\":\"res1\",\"usage_start_raw\":\"2026-08-01\",\"billed_cost_micros\":-9223372036854775808}";
    assert(ifm_json_decode_record(min_int64_case, strlen(min_int64_case), &rec));
    assert(!rec.is_faulted);
    assert(rec.billed_cost_micros == INT64_MIN);

    /* 4. Maximum allowable unsigned bounds (UINT64_MAX source_line, UINT32_MAX flags) */
    const char *max_unsigned_case = "{\"provider\":\"aws\",\"provider_row_id\":\"r1\",\"account_id\":\"a1\",\"resource_id\":\"res1\",\"usage_start_raw\":\"2026-08-01\",\"billed_cost_micros\":100,\"source_line\":18446744073709551615,\"flags\":4294967295}";
    assert(ifm_json_decode_record(max_unsigned_case, strlen(max_unsigned_case), &rec));
    assert(!rec.is_faulted);
    assert(rec.source_line == UINT64_MAX);
    assert(rec.flags == UINT32_MAX);

    /* --- Positive / Negative Overflow Assertions --- */
    /* 5. billed_cost_micros > INT64_MAX */
    const char *overflow_pos = "{\"provider\":\"aws\",\"billed_cost_micros\":9223372036854775808}";
    assert(!ifm_json_decode_record(overflow_pos, strlen(overflow_pos), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *overflow_huge = "{\"provider\":\"aws\",\"billed_cost_micros\":999999999999999999999999999999999}";
    assert(!ifm_json_decode_record(overflow_huge, strlen(overflow_huge), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* 6. billed_cost_micros < INT64_MIN */
    const char *overflow_neg = "{\"provider\":\"aws\",\"billed_cost_micros\":-9223372036854775809}";
    assert(!ifm_json_decode_record(overflow_neg, strlen(overflow_neg), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *underflow_huge = "{\"provider\":\"aws\",\"billed_cost_micros\":-999999999999999999999999999999999}";
    assert(!ifm_json_decode_record(underflow_huge, strlen(underflow_huge), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* 7. source_line > UINT64_MAX */
    const char *overflow_source_line = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"source_line\":18446744073709551616}";
    assert(!ifm_json_decode_record(overflow_source_line, strlen(overflow_source_line), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* 8. flags > UINT32_MAX */
    const char *overflow_flags = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"flags\":4294967296}";
    assert(!ifm_json_decode_record(overflow_flags, strlen(overflow_flags), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* --- Unsigned Wrap-Around Prevention --- */
    /* 9. source_line negative */
    const char *neg_source_line = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"source_line\":-5}";
    assert(!ifm_json_decode_record(neg_source_line, strlen(neg_source_line), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* 10. flags negative */
    const char *neg_flags = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"flags\":-1}";
    assert(!ifm_json_decode_record(neg_flags, strlen(neg_flags), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* --- Malformed Numbers & Signs --- */
    /* 11. Trailing garbage in billed_cost_micros */
    const char *trailing_garbage = "{\"provider\":\"aws\",\"billed_cost_micros\":123a}";
    assert(!ifm_json_decode_record(trailing_garbage, strlen(trailing_garbage), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* 12. Trailing garbage in source_line / flags */
    const char *trailing_sl = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"source_line\":10x}";
    assert(!ifm_json_decode_record(trailing_sl, strlen(trailing_sl), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *trailing_fl = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"flags\":1ff}";
    assert(!ifm_json_decode_record(trailing_fl, strlen(trailing_fl), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* 13. Plus sign rejection */
    const char *plus_micros = "{\"provider\":\"aws\",\"billed_cost_micros\":+123}";
    assert(!ifm_json_decode_record(plus_micros, strlen(plus_micros), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *multi_plus = "{\"provider\":\"aws\",\"billed_cost_micros\":++50}";
    assert(!ifm_json_decode_record(multi_plus, strlen(multi_plus), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *plus_sl = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"source_line\":+10}";
    assert(!ifm_json_decode_record(plus_sl, strlen(plus_sl), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *plus_fl = "{\"provider\":\"aws\",\"billed_cost_micros\":100,\"flags\":+5}";
    assert(!ifm_json_decode_record(plus_fl, strlen(plus_fl), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    /* 14. String numeric form edge cases */
    const char *empty_str_cost = "{\"provider\":\"aws\",\"billed_cost_micros\":\"\"}";
    assert(!ifm_json_decode_record(empty_str_cost, strlen(empty_str_cost), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *ws_str_cost = "{\"provider\":\"aws\",\"billed_cost_micros\":\"   \"}";
    assert(!ifm_json_decode_record(ws_str_cost, strlen(ws_str_cost), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *malformed_str_cost = "{\"provider\":\"aws\",\"billed_cost_micros\":\"123a\"}";
    assert(!ifm_json_decode_record(malformed_str_cost, strlen(malformed_str_cost), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    const char *empty_decimal = "{\"provider\":\"aws\",\"billed_cost\":\"\"}";
    assert(!ifm_json_decode_record(empty_decimal, strlen(empty_decimal), &rec));
    assert(rec.is_faulted);
    assert(rec.fault_code == IFM_FAULT_INVALID_NUMERIC);

    printf("  [PASS] test_json_decoder_numeric_hardening\n");
}

int main(void) {
    printf("Running Ingress Unit Tests...\n");
    test_json_decoder_valid();
    test_json_decoder_decimal_cost();
    test_json_decoder_missing_required();
    test_json_decoder_malformed();
    test_json_decoder_numeric_hardening();
    printf("ALL INGRESS UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
