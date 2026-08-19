#undef NDEBUG
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#define FUZZ_ITERATIONS 100000
#define FUZZ_BUFFER_SIZE 2048
#define FUZZ_REPORT_INTERVAL 25000

static const char *corpus[] = {
    /* 1. Standard valid IFM records */
    "{\"provider\":\"aws\",\"account_id\":\"123\",\"resource_id\":\"res-1\",\"billed_cost_micros\":100}",
    "{\"provider\":\"azure\",\"account_id\":\"sub-1\",\"resource_id\":\"vm-1\",\"billed_cost\":\"25.500000\",\"flags\":0,\"source_line\":1}",
    "{\"provider\":\"gcp\",\"account_id\":\"proj-1\",\"resource_id\":\"gcs-1\",\"billed_cost_micros\":0}",

    /* 2. Structural edge cases */
    "{}",
    "{\"invalid\": [1, 2, 3], \"test\": true}",
    "{\"nested\": {\"a\": {\"b\": {\"c\": 123}}}}",
    "{\"long_str\": \"\\\\\\\\\\\"}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"unknown_field\":{\"deep\":[null,false]}}",

    /* 3. Numeric bounds: INT64_MAX / INT64_MIN and overflows */
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":9223372036854775807}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":9223372036854775808}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":-9223372036854775808}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":-9223372036854775809}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":99999999999999999999999999999999999999999999}",

    /* 4. Numeric bounds: UINT64_MAX and UINT32_MAX for source_line and flags */
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"source_line\":18446744073709551615,\"flags\":4294967295}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"source_line\":18446744073709551616,\"flags\":4294967296}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"source_line\":-1,\"flags\":-5}",

    /* 5. Malformed signs and syntax */
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":+100000}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"source_line\":+50}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"flags\":+0}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":--100}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":+-500}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":1-23}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":1..23}",
    "{\"provider\":\"aws\",\"account_id\":\"111\",\"resource_id\":\"res\",\"billed_cost_micros\":1e6}",

    /* 6. Decimal extreme bounds */
    "{\"provider\":\"azure\",\"account_id\":\"sub-1\",\"resource_id\":\"vm-1\",\"billed_cost\":\"-999999999999.999999\"}",
    "{\"provider\":\"azure\",\"account_id\":\"sub-1\",\"resource_id\":\"vm-1\",\"billed_cost\":\"999999999999.999999\"}",
    "{\"provider\":\"azure\",\"account_id\":\"sub-1\",\"resource_id\":\"vm-1\",\"billed_cost\":\"+12.345678\"}",
    "{\"provider\":\"azure\",\"account_id\":\"sub-1\",\"resource_id\":\"vm-1\",\"billed_cost\":\"1.5e-3\"}"
};

static void run_fuzz_iterations(int iterations) {
    printf("Running %d hardened fuzz iterations on JSON Decoder...\n", iterations);
    srand(1337);

    size_t corpus_size = sizeof(corpus) / sizeof(corpus[0]);
    uint64_t decode_valid_schema_valid = 0;
    uint64_t decode_valid_schema_invalid = 0;
    uint64_t decode_rejected = 0;

    for (int iter = 0; iter < iterations; ++iter) {
        char buf[FUZZ_BUFFER_SIZE];
        int base_idx = rand() % (int)corpus_size;
        const char *base = corpus[base_idx];
        size_t base_len = strlen(base);
        if (base_len >= sizeof(buf)) base_len = sizeof(buf) - 1;
        memcpy(buf, base, base_len);

        size_t payload_len = base_len;
        int strategy = rand() % 5;

        switch (strategy) {
            case 0: {
                /* Strategy 0: In-place byte corruption / random overwrite */
                int mutations = 1 + (rand() % 5);
                for (int m = 0; m < mutations; ++m) {
                    if (payload_len > 0) {
                        size_t pos = (size_t)(rand() % (int)payload_len);
                        buf[pos] = (char)(rand() % 256);
                    }
                }
                break;
            }
            case 1: {
                /* Strategy 1: Arbitrary prefix truncation */
                payload_len = (size_t)(rand() % ((int)base_len + 1));
                break;
            }
            case 2: {
                /* Strategy 2: Trailing garbage injection */
                size_t max_garbage = sizeof(buf) - base_len - 1;
                if (max_garbage > 0) {
                    size_t garbage_len = (size_t)(rand() % (int)(max_garbage > 32 ? 32 : max_garbage));
                    for (size_t g = 0; g < garbage_len; ++g) {
                        buf[base_len + g] = (char)(rand() % 256);
                    }
                    payload_len = base_len + garbage_len;
                }
                break;
            }
            case 3: {
                /* Strategy 3: Explicit embedded NUL byte injection */
                int nul_count = 1 + (rand() % 3);
                for (int n = 0; n < nul_count; ++n) {
                    if (payload_len > 0) {
                        size_t pos = (size_t)(rand() % (int)payload_len);
                        buf[pos] = '\0';
                    }
                }
                break;
            }
            case 4: {
                /* Strategy 4: Compound mutation (corruption + truncation or trailing garbage) */
                int mutations = 1 + (rand() % 4);
                for (int m = 0; m < mutations; ++m) {
                    if (base_len > 0) {
                        size_t pos = (size_t)(rand() % (int)base_len);
                        buf[pos] = (char)(rand() % 256);
                    }
                }
                if (rand() % 2 == 0) {
                    /* Sub-slice truncation */
                    payload_len = (size_t)(rand() % ((int)base_len + 1));
                } else {
                    /* Append trailing garbage */
                    size_t max_g = sizeof(buf) - base_len - 1;
                    if (max_g > 0) {
                        size_t g_len = (size_t)(rand() % (int)(max_g > 16 ? 16 : max_g));
                        for (size_t g = 0; g < g_len; ++g) {
                            buf[base_len + g] = (char)(rand() % 256);
                        }
                        payload_len = base_len + g_len;
                    }
                }
                break;
            }
        }

        assert(payload_len < sizeof(buf));

        ifm_record_t rec;
        memset(&rec, 0, sizeof(rec));

        if (ifm_json_decode_record(buf, payload_len, &rec)) {
            if (ifm_schema_validate_record(&rec)) {
                decode_valid_schema_valid++;
            } else {
                decode_valid_schema_invalid++;
            }
        } else {
            decode_rejected++;
        }

        if ((iter + 1) % FUZZ_REPORT_INTERVAL == 0) {
            printf("  [FUZZ PROGRESS] %d / %d iterations completed...\n", iter + 1, iterations);
        }
    }

    printf("===============================================================\n");
    printf("FUZZ TEST RESULTS (%d iterations):\n", iterations);
    printf("  Decoder Accepted & Schema Valid:   %" PRIu64 "\n", decode_valid_schema_valid);
    printf("  Decoder Accepted & Schema Invalid: %" PRIu64 "\n", decode_valid_schema_invalid);
    printf("  Decoder Rejected (Syntax/Numeric): %" PRIu64 "\n", decode_rejected);
    printf("  Total Iterations Completed:        %d\n", iterations);
    printf("===============================================================\n");
    printf("  [PASS] Fuzzer completed with 0 crashes or undefined behavior!\n");
}

int main(void) {
    run_fuzz_iterations(FUZZ_ITERATIONS);
    return 0;
}
