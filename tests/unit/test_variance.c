#undef NDEBUG
#include "ifm_costintel/variance.h"
#include "ifm_costintel/concentration.h"
#include "ifm_costintel/anomaly.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_variance_computation(void) {
    ifm_record_t rec = { .active_spend_micros = 120000000 };
    assert(ifm_compute_variance(&rec, 100000000));
    assert(rec.variance_delta_micros == 20000000);
    assert(rec.variance_status == IFM_VARIANCE_DEFINED);
    assert(rec.variance_pct_micros == 200000); /* +20% */

    /* Active < Baseline */
    rec.active_spend_micros = 80000000;
    assert(ifm_compute_variance(&rec, 100000000));
    assert(rec.variance_delta_micros == -20000000);
    assert(rec.variance_pct_micros == -200000); /* -20% */

    /* Zero baseline */
    rec.active_spend_micros = 50000000;
    assert(ifm_compute_variance(&rec, 0));
    assert(rec.variance_delta_micros == 50000000);
    assert(rec.variance_status == IFM_VARIANCE_BASELINE_ZERO);
    assert(rec.variance_pct_micros == 0);

    /* Zero baseline no change */
    rec.active_spend_micros = 0;
    assert(ifm_compute_variance(&rec, 0));
    assert(rec.variance_status == IFM_VARIANCE_BASELINE_ZERO_NO_CHANGE);

    printf("  [PASS] test_variance_computation\n");
}

static void test_concentration_computation(void) {
    ifm_micros_t conc;
    ifm_concentration_status_t st = ifm_compute_concentration(25000000, 100000000, &conc);
    assert(st == IFM_CONCENTRATION_DEFINED);
    assert(conc == 250000); /* 25.0% */

    /* Zero total */
    st = ifm_compute_concentration(25000000, 0, &conc);
    assert(st == IFM_CONCENTRATION_NOT_DEFINED);

    printf("  [PASS] test_concentration_computation\n");
}

static void test_anomaly_detection(void) {
    ifm_anomaly_rule_set_t ars;
    ifm_anomaly_rule_set_init(&ars);

    ifm_anomaly_rule_t r_spike = {
        .rule_id = "ANOMALY-SPIKE-50PCT",
        .threshold_pct_micros = 500000, /* 50% */
        .min_baseline_micros = 10000000, /* $10.00 */
        .direction = IFM_ANOMALY_DIR_SPIKE
    };
    ifm_anomaly_rule_set_add(&ars, &r_spike);

    /* Record: 200M active vs 100M baseline (+100% spike) */
    ifm_record_t rec = { .active_spend_micros = 200000000 };
    ifm_compute_variance(&rec, 100000000);
    ifm_evaluate_anomalies(&ars, &rec);

    assert(rec.is_anomaly);
    assert(strcmp(rec.anomaly_rule_id, "ANOMALY-SPIKE-50PCT") == 0);
    assert(rec.anomaly_direction == IFM_ANOMALY_DIR_SPIKE);

    /* Record: 120M active vs 100M baseline (+20%, under threshold) */
    rec.active_spend_micros = 120000000;
    ifm_compute_variance(&rec, 100000000);
    ifm_evaluate_anomalies(&ars, &rec);
    assert(!rec.is_anomaly);

    printf("  [PASS] test_anomaly_detection\n");
}

static void test_baseline_json_loading(void) {
    ifm_baseline_table_t bt;
    ifm_baseline_table_init(&bt);

    const char *cfg = "{\"baselines\": [{\"key\":\"res-01\",\"baseline_micros\":45000000},{\"key\":\"res-02\",\"baseline\":\"12.500000\"}]}";
    assert(ifm_baseline_table_load_json(&bt, cfg, strlen(cfg)));
    assert(bt.count == 2);

    ifm_micros_t b1 = 0, b2 = 0;
    assert(ifm_baseline_table_lookup(&bt, "res-01", &b1) && b1 == 45000000);
    assert(ifm_baseline_table_lookup(&bt, "res-02", &b2) && b2 == 12500000);

    ifm_baseline_table_cleanup(&bt);
    printf("  [PASS] test_baseline_json_loading\n");
}

static void test_anomaly_json_loading(void) {
    ifm_anomaly_rule_set_t ars;
    ifm_anomaly_rule_set_init(&ars);

    const char *cfg = "{\"anomalies\": [{\"rule_id\":\"SPIKE-50\",\"threshold_pct_micros\":500000,\"min_baseline_micros\":5000000,\"direction\":\"SPIKE\"},{\"rule_id\":\"DROP-30\",\"threshold_pct\":\"0.300000\",\"min_baseline\":\"10.000000\",\"direction\":\"DROP\"}]}";
    assert(ifm_anomaly_rule_set_load_json(&ars, cfg, strlen(cfg)));
    assert(ars.count == 2);
    assert(strcmp(ars.rules[0].rule_id, "SPIKE-50") == 0);
    assert(ars.rules[0].direction == IFM_ANOMALY_DIR_SPIKE);
    assert(ars.rules[1].direction == IFM_ANOMALY_DIR_DROP);
    assert(ars.rules[1].threshold_pct_micros == 300000);

    printf("  [PASS] test_anomaly_json_loading\n");
}

static void test_baseline_json_loading_failures(void) {
    ifm_baseline_table_t bt;
    ifm_baseline_table_init(&bt);

    /* 1. Truncated JSON without closing bracket */
    const char *trunc_cfg = "{\"baselines\": [{\"key\":\"res-01\",\"baseline_micros\":45000000}";
    assert(!ifm_baseline_table_load_json(&bt, trunc_cfg, strlen(trunc_cfg)));
    assert(bt.count == 0);

    /* 2. Missing key */
    const char *no_key = "{\"baselines\": [{\"baseline_micros\":45000000}]}";
    assert(!ifm_baseline_table_load_json(&bt, no_key, strlen(no_key)));
    assert(bt.count == 0);

    /* 3. Missing micros */
    const char *no_micros = "{\"baselines\": [{\"key\":\"res-01\"}]}";
    assert(!ifm_baseline_table_load_json(&bt, no_micros, strlen(no_micros)));
    assert(bt.count == 0);

    /* 4. Invalid numeric format (+ sign) */
    const char *invalid_num = "{\"baselines\": [{\"key\":\"res-01\",\"baseline_micros\":\"+45000\"}]}";
    assert(!ifm_baseline_table_load_json(&bt, invalid_num, strlen(invalid_num)));
    assert(bt.count == 0);

    /* 5. Numeric overflow */
    const char *overflow_num = "{\"baselines\": [{\"key\":\"res-01\",\"baseline_micros\":\"9999999999999999999999999\"}]}";
    assert(!ifm_baseline_table_load_json(&bt, overflow_num, strlen(overflow_num)));
    assert(bt.count == 0);

    /* 6. Unknown property */
    const char *unknown_prop = "{\"baselines\": [{\"key\":\"res-01\",\"baseline_micros\":1000,\"unknown\":true}]}";
    assert(!ifm_baseline_table_load_json(&bt, unknown_prop, strlen(unknown_prop)));
    assert(bt.count == 0);

    /* 7. Partial failure: First valid, second invalid -> entire table reset */
    const char *partial_fail = "{\"baselines\": [{\"key\":\"res-01\",\"baseline_micros\":1000},{\"key\":\"res-02\"}]}";
    assert(!ifm_baseline_table_load_json(&bt, partial_fail, strlen(partial_fail)));
    assert(bt.count == 0);

    ifm_baseline_table_cleanup(&bt);
    printf("  [PASS] test_baseline_json_loading_failures\n");
}

static void test_anomaly_json_loading_failures(void) {
    ifm_anomaly_rule_set_t ars;
    ifm_anomaly_rule_set_init(&ars);

    /* 1. Missing direction */
    const char *no_dir = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, no_dir, strlen(no_dir)));
    assert(ars.count == 0);

    /* 2. Invalid direction string */
    const char *bad_dir_str = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":\"UNKNOWN\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, bad_dir_str, strlen(bad_dir_str)));
    assert(ars.count == 0);

    /* 3. Empty direction string */
    const char *empty_dir_str = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":\"\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, empty_dir_str, strlen(empty_dir_str)));
    assert(ars.count == 0);

    /* 4. Invalid numeric directions (0, 4, -1, 99) */
    const char *bad_dir_0 = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":0}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, bad_dir_0, strlen(bad_dir_0)));
    assert(ars.count == 0);

    const char *bad_dir_4 = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":4}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, bad_dir_4, strlen(bad_dir_4)));
    assert(ars.count == 0);

    const char *bad_dir_neg = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":-1}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, bad_dir_neg, strlen(bad_dir_neg)));
    assert(ars.count == 0);

    /* 5. Missing or empty rule_id */
    const char *empty_rule_id = "{\"anomalies\": [{\"rule_id\":\"\",\"threshold_pct_micros\":500000,\"direction\":\"SPIKE\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, empty_rule_id, strlen(empty_rule_id)));
    assert(ars.count == 0);

    /* 6. Non-positive threshold (0 or negative) */
    const char *zero_thresh = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":0,\"direction\":\"SPIKE\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, zero_thresh, strlen(zero_thresh)));
    assert(ars.count == 0);

    const char *neg_thresh = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":-100,\"direction\":\"SPIKE\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, neg_thresh, strlen(neg_thresh)));
    assert(ars.count == 0);

    /* 7. Negative min_baseline */
    const char *neg_base = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"min_baseline_micros\":-500,\"direction\":\"SPIKE\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, neg_base, strlen(neg_base)));
    assert(ars.count == 0);

    /* 8. Unknown property */
    const char *unknown_prop = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":\"SPIKE\",\"extra\":\"field\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, unknown_prop, strlen(unknown_prop)));
    assert(ars.count == 0);

    /* 9. Truncated array */
    const char *trunc_cfg = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":\"SPIKE\"}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, trunc_cfg, strlen(trunc_cfg)));
    assert(ars.count == 0);

    /* 10. Partial failure state reset */
    const char *partial_fail = "{\"anomalies\": [{\"rule_id\":\"R1\",\"threshold_pct_micros\":500000,\"direction\":\"SPIKE\"},{\"rule_id\":\"R2\"}]}";
    assert(!ifm_anomaly_rule_set_load_json(&ars, partial_fail, strlen(partial_fail)));
    assert(ars.count == 0);

    /* 11. Capacity limit (65 rules -> returns false, count = 0) */
    char big_cfg[16384];
    int offset = snprintf(big_cfg, sizeof(big_cfg), "{\"anomalies\": [");
    for (int i = 0; i < 65; ++i) {
        offset += snprintf(big_cfg + offset, sizeof(big_cfg) - (size_t)offset,
                           "%s{\"rule_id\":\"R%d\",\"threshold_pct_micros\":100000,\"direction\":1}",
                           (i > 0 ? "," : ""), i);
    }
    snprintf(big_cfg + offset, sizeof(big_cfg) - (size_t)offset, "]}");
    assert(!ifm_anomaly_rule_set_load_json(&ars, big_cfg, strlen(big_cfg)));
    assert(ars.count == 0);

    printf("  [PASS] test_anomaly_json_loading_failures\n");
}

static void test_anomaly_json_loading_valid_permutations(void) {
    ifm_anomaly_rule_set_t ars;
    ifm_anomaly_rule_set_init(&ars);

    const char *valid_cfg = "{\"anomalies\": ["
        "{\"rule_id\":\"SPIKE_STR\",\"threshold_pct_micros\":500000,\"min_baseline_micros\":10000000,\"direction\":\"SPIKE\"},"
        "{\"rule_id\":\"DROP_STR\",\"threshold_pct\":\"0.300000\",\"min_baseline\":\"5.000000\",\"direction\":\"drop\"},"
        "{\"rule_id\":\"BOTH_STR\",\"threshold_pct_micros\":200000,\"direction\":\"Both\"},"
        "{\"rule_id\":\"SPIKE_NUM\",\"threshold_pct_micros\":400000,\"direction\":1},"
        "{\"rule_id\":\"DROP_NUM\",\"threshold_pct_micros\":350000,\"direction\":2},"
        "{\"rule_id\":\"BOTH_NUM\",\"threshold_pct_micros\":250000,\"direction\":3}"
    "]}";

    assert(ifm_anomaly_rule_set_load_json(&ars, valid_cfg, strlen(valid_cfg)));
    assert(ars.count == 6);

    assert(strcmp(ars.rules[0].rule_id, "SPIKE_STR") == 0 && ars.rules[0].direction == IFM_ANOMALY_DIR_SPIKE && ars.rules[0].threshold_pct_micros == 500000 && ars.rules[0].min_baseline_micros == 10000000);
    assert(strcmp(ars.rules[1].rule_id, "DROP_STR") == 0 && ars.rules[1].direction == IFM_ANOMALY_DIR_DROP && ars.rules[1].threshold_pct_micros == 300000 && ars.rules[1].min_baseline_micros == 5000000);
    assert(strcmp(ars.rules[2].rule_id, "BOTH_STR") == 0 && ars.rules[2].direction == IFM_ANOMALY_DIR_BOTH && ars.rules[2].threshold_pct_micros == 200000 && ars.rules[2].min_baseline_micros == 0);
    assert(strcmp(ars.rules[3].rule_id, "SPIKE_NUM") == 0 && ars.rules[3].direction == IFM_ANOMALY_DIR_SPIKE);
    assert(strcmp(ars.rules[4].rule_id, "DROP_NUM") == 0 && ars.rules[4].direction == IFM_ANOMALY_DIR_DROP);
    assert(strcmp(ars.rules[5].rule_id, "BOTH_NUM") == 0 && ars.rules[5].direction == IFM_ANOMALY_DIR_BOTH);

    printf("  [PASS] test_anomaly_json_loading_valid_permutations\n");
}

static void test_baseline_table_basic_operations(void) {
    ifm_baseline_table_t bt;
    ifm_baseline_table_init(&bt);

    /* C. Empty table lookup */
    ifm_micros_t out_val = 12345;
    assert(!ifm_baseline_table_lookup(&bt, "res-empty", &out_val));
    assert(out_val == 12345); /* Output untouched */
    assert(!ifm_baseline_table_lookup(&bt, NULL, &out_val));
    assert(!ifm_baseline_table_lookup(NULL, "res-empty", &out_val));
    assert(!ifm_baseline_table_lookup(&bt, "res-empty", NULL));

    /* Insert new keys */
    assert(ifm_baseline_table_set(&bt, "res-01", 10000000));
    assert(bt.count == 1);
    assert(ifm_baseline_table_set(&bt, "res-02", 20000000));
    assert(bt.count == 2);

    /* Lookups */
    assert(ifm_baseline_table_lookup(&bt, "res-01", &out_val) && out_val == 10000000);
    assert(ifm_baseline_table_lookup(&bt, "res-02", &out_val) && out_val == 20000000);

    /* B. Missing key in populated table */
    out_val = 99999;
    assert(!ifm_baseline_table_lookup(&bt, "res-missing", &out_val));
    assert(out_val == 99999); /* Untouched */

    /* A. Existing duplicate-key update: count remains unchanged */
    assert(ifm_baseline_table_set(&bt, "res-01", 35000000));
    assert(bt.count == 2); /* count must NOT increase */
    assert(ifm_baseline_table_lookup(&bt, "res-01", &out_val) && out_val == 35000000);

    ifm_baseline_table_cleanup(&bt);
    assert(bt.count == 0);
    assert(bt.capacity == 0);
    assert(bt.entries == NULL);

    printf("  [PASS] test_baseline_table_basic_operations\n");
}

static void test_baseline_table_high_cardinality(void) {
    ifm_baseline_table_t bt;
    ifm_baseline_table_init(&bt);

    /* D. High-cardinality table: insert 10,000 distinct keys */
    char key_buf[64];
    for (int i = 0; i < 10000; ++i) {
        snprintf(key_buf, sizeof(key_buf), "resource-entity-%06d", i);
        ifm_micros_t val = (ifm_micros_t)(i * 1000 + 7);
        assert(ifm_baseline_table_set(&bt, key_buf, val));
    }
    assert(bt.count == 10000);

    /* E. High-cardinality lookup after rehashes */
    for (int i = 0; i < 10000; ++i) {
        snprintf(key_buf, sizeof(key_buf), "resource-entity-%06d", i);
        ifm_micros_t expected = (ifm_micros_t)(i * 1000 + 7);
        ifm_micros_t out_val = 0;
        assert(ifm_baseline_table_lookup(&bt, key_buf, &out_val));
        assert(out_val == expected);
    }

    /* Update first 5,000 keys */
    for (int i = 0; i < 5000; ++i) {
        snprintf(key_buf, sizeof(key_buf), "resource-entity-%06d", i);
        ifm_micros_t new_val = (ifm_micros_t)(i * 2000 + 13);
        assert(ifm_baseline_table_set(&bt, key_buf, new_val));
    }
    assert(bt.count == 10000); /* Count unchanged */

    /* Verify updated values */
    for (int i = 0; i < 5000; ++i) {
        snprintf(key_buf, sizeof(key_buf), "resource-entity-%06d", i);
        ifm_micros_t expected = (ifm_micros_t)(i * 2000 + 13);
        ifm_micros_t out_val = 0;
        assert(ifm_baseline_table_lookup(&bt, key_buf, &out_val));
        assert(out_val == expected);
    }

    /* Missing keys lookups */
    for (int i = 10000; i < 11000; ++i) {
        snprintf(key_buf, sizeof(key_buf), "resource-entity-%06d", i);
        ifm_micros_t out_val = -1;
        assert(!ifm_baseline_table_lookup(&bt, key_buf, &out_val));
        assert(out_val == -1);
    }

    ifm_baseline_table_cleanup(&bt);
    printf("  [PASS] test_baseline_table_high_cardinality\n");
}

static void test_baseline_table_collision_probing(void) {
    ifm_baseline_table_t bt;
    ifm_baseline_table_init(&bt);

    /* F. Collision/probing behavior with common prefix and structured suffixes */
    char k[128];
    for (int i = 0; i < 1000; ++i) {
        snprintf(k, sizeof(k), "clustered_prefix_zone_us_east_1_sub_network_interface_res_%d", i);
        assert(ifm_baseline_table_set(&bt, k, (ifm_micros_t)(i + 1)));
    }
    assert(bt.count == 1000);

    for (int i = 0; i < 1000; ++i) {
        snprintf(k, sizeof(k), "clustered_prefix_zone_us_east_1_sub_network_interface_res_%d", i);
        ifm_micros_t val = 0;
        assert(ifm_baseline_table_lookup(&bt, k, &val));
        assert(val == (ifm_micros_t)(i + 1));
    }

    ifm_baseline_table_cleanup(&bt);
    printf("  [PASS] test_baseline_table_collision_probing\n");
}

static void test_baseline_table_lifecycle_reinit(void) {
    ifm_baseline_table_t bt;

    /* G. Repeated cleanup/reinitialization cycles */
    for (int cycle = 0; cycle < 5; ++cycle) {
        ifm_baseline_table_init(&bt);
        assert(bt.count == 0);
        assert(bt.capacity == 0);

        char k[32];
        for (int i = 0; i < 50; ++i) {
            snprintf(k, sizeof(k), "cycle_%d_item_%d", cycle, i);
            assert(ifm_baseline_table_set(&bt, k, 1000000));
        }
        assert(bt.count == 50);

        for (int i = 0; i < 50; ++i) {
            snprintf(k, sizeof(k), "cycle_%d_item_%d", cycle, i);
            ifm_micros_t v = 0;
            assert(ifm_baseline_table_lookup(&bt, k, &v) && v == 1000000);
        }

        ifm_baseline_table_cleanup(&bt);
        assert(bt.count == 0);
        assert(bt.capacity == 0);
        assert(bt.entries == NULL);

        /* Idempotent cleanup */
        ifm_baseline_table_cleanup(&bt);
    }

    printf("  [PASS] test_baseline_table_lifecycle_reinit\n");
}

int main(void) {
    printf("Running Variance, Concentration & Anomaly Unit Tests...\n");
    test_variance_computation();
    test_concentration_computation();
    test_anomaly_detection();
    test_baseline_json_loading();
    test_baseline_json_loading_failures();
    test_baseline_table_basic_operations();
    test_baseline_table_high_cardinality();
    test_baseline_table_collision_probing();
    test_baseline_table_lifecycle_reinit();
    test_anomaly_json_loading();
    test_anomaly_json_loading_failures();
    test_anomaly_json_loading_valid_permutations();
    printf("ALL VARIANCE & ANOMALY UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
