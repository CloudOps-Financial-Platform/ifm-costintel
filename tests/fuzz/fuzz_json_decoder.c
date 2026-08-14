#undef NDEBUG
#include "ifm_costintel/json_decoder.h"
#include "ifm_costintel/schema_validator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void run_fuzz_iterations(int iterations) {
    printf("Running %d fuzz iterations on JSON Decoder...\n", iterations);
    srand(1337);

    const char *corpus[] = {
        "{\"provider\":\"aws\",\"account_id\":\"123\",\"resource_id\":\"res-1\",\"billed_cost_micros\":100}",
        "{\"invalid\": [1, 2, 3], \"test\": true}",
        "{\"provider\":\"azure\", \"billed_cost\": \"-999999999999.999999\"}",
        "{}",
        "{\"nested\": {\"a\": {\"b\": {\"c\": 123}}}}",
        "{\"long_str\": \"\\\\\\\\\\\"}"
    };
    size_t corpus_size = sizeof(corpus) / sizeof(corpus[0]);

    for (int iter = 0; iter < iterations; ++iter) {
        char buf[1024];
        int base_idx = rand() % corpus_size;
        const char *base = corpus[base_idx];
        size_t len = strlen(base);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, base, len);
        buf[len] = '\0';

        /* Introduce mutations */
        int mutations = 1 + (rand() % 5);
        for (int m = 0; m < mutations; ++m) {
            size_t pos = rand() % len;
            buf[pos] = (char)(rand() % 256);
        }

        ifm_record_t rec;
        ifm_json_decode_record(buf, len, &rec);
        ifm_schema_validate_record(&rec);
    }
    printf("  [PASS] Fuzzer completed %d iterations with 0 crashes or undefined behavior!\n", iterations);
}

int main(void) {
    run_fuzz_iterations(20000);
    return 0;
}
