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

int main(void) {
    printf("Running Variance, Concentration & Anomaly Unit Tests...\n");
    test_variance_computation();
    test_concentration_computation();
    test_anomaly_detection();
    printf("ALL VARIANCE & ANOMALY UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
