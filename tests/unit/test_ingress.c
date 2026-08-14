#undef NDEBUG
#include "ifm_costintel/stream_adapter.h"
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include "ifm_costintel/traceability.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

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

int main(void) {
    printf("Running Ingress Unit Tests...\n");
    test_json_decoder_valid();
    test_json_decoder_decimal_cost();
    test_json_decoder_missing_required();
    test_json_decoder_malformed();
    printf("ALL INGRESS UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
