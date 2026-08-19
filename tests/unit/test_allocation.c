#undef NDEBUG
#include "ifm_costintel/rules.h"
#include "ifm_costintel/allocation.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_allocation_single_match(void) {
    ifm_rule_set_t rs;
    ifm_rule_set_init(&rs);

    ifm_allocation_rule_t r1 = {
        .rule_id = "RULE-01",
        .priority = 100,
        .match_provider = "aws",
        .match_account_id = "111*",
        .match_resource_prefix = "i-",
        .target_cost_center_id = "CC-COMPUTE",
        .version = 1
    };
    ifm_rule_set_add(&rs, &r1);

    ifm_record_t rec = {
        .provider = "aws",
        .account_id = "111222333444",
        .resource_id = "i-0123456789",
        .billed_cost_micros = 50000000
    };

    ifm_allocate_record(&rs, &rec);
    assert(rec.alloc_status == IFM_ALLOC_ALLOCATED);
    assert(strcmp(rec.cost_center_id, "CC-COMPUTE") == 0);
    assert(strcmp(rec.rule_id, "RULE-01") == 0);
    printf("  [PASS] test_allocation_single_match\n");
}

static void test_allocation_priority_override(void) {
    ifm_rule_set_t rs;
    ifm_rule_set_init(&rs);

    ifm_allocation_rule_t r_low = {
        .rule_id = "RULE-LOW",
        .priority = 50,
        .match_provider = "aws",
        .match_account_id = "*",
        .target_cost_center_id = "CC-GENERAL",
        .version = 1
    };
    ifm_allocation_rule_t r_high = {
        .rule_id = "RULE-HIGH",
        .priority = 200,
        .match_provider = "aws",
        .match_account_id = "999*",
        .target_cost_center_id = "CC-VIP",
        .version = 2
    };
    ifm_rule_set_add(&rs, &r_low);
    ifm_rule_set_add(&rs, &r_high);

    ifm_record_t rec = {
        .provider = "aws",
        .account_id = "999888777",
        .resource_id = "vol-01",
        .billed_cost_micros = 10000000
    };

    ifm_allocate_record(&rs, &rec);
    assert(rec.alloc_status == IFM_ALLOC_ALLOCATED);
    assert(strcmp(rec.cost_center_id, "CC-VIP") == 0);
    assert(strcmp(rec.rule_id, "RULE-HIGH") == 0);
    printf("  [PASS] test_allocation_priority_override\n");
}

static void test_allocation_ambiguous(void) {
    ifm_rule_set_t rs;
    ifm_rule_set_init(&rs);

    ifm_allocation_rule_t r1 = {
        .rule_id = "RULE-A",
        .priority = 100,
        .match_provider = "aws",
        .target_cost_center_id = "CC-A",
        .version = 1
    };
    ifm_allocation_rule_t r2 = {
        .rule_id = "RULE-B",
        .priority = 100,
        .match_provider = "aws",
        .target_cost_center_id = "CC-B",
        .version = 1
    };
    ifm_rule_set_add(&rs, &r1);
    ifm_rule_set_add(&rs, &r2);

    ifm_record_t rec = {
        .provider = "aws",
        .account_id = "12345",
        .resource_id = "db-01",
        .billed_cost_micros = 10000000
    };

    ifm_allocate_record(&rs, &rec);
    assert(rec.alloc_status == IFM_ALLOC_AMBIGUOUS);
    assert(strcmp(rec.cost_center_id, "AMBIGUOUS") == 0);
    printf("  [PASS] test_allocation_ambiguous\n");
}

static void test_allocation_unallocated(void) {
    ifm_rule_set_t rs;
    ifm_rule_set_init(&rs);

    ifm_allocation_rule_t r1 = {
        .rule_id = "RULE-GCP",
        .priority = 100,
        .match_provider = "gcp",
        .target_cost_center_id = "CC-GCP",
        .version = 1
    };
    ifm_rule_set_add(&rs, &r1);

    ifm_record_t rec = {
        .provider = "aws",
        .account_id = "12345",
        .resource_id = "db-01",
        .billed_cost_micros = 10000000
    };

    ifm_allocate_record(&rs, &rec);
    assert(rec.alloc_status == IFM_ALLOC_UNALLOCATED);
    assert(strcmp(rec.cost_center_id, "UNALLOCATED") == 0);
    printf("  [PASS] test_allocation_unallocated\n");
}

static void test_rules_numeric_safety(void);
int main(void) {
    printf("Running Allocation Unit Tests...\n");
    test_allocation_single_match();
    test_allocation_priority_override();
    test_allocation_ambiguous();
    test_allocation_unallocated();
    test_rules_numeric_safety();
    printf("ALL ALLOCATION UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}

static void test_rules_numeric_safety(void) {
    ifm_rule_set_t rs;
    const char *valid_json = "{\"rules\":[{\"rule_id\":\"R1\",\"priority\":250,\"version\":2}]}";
    assert(ifm_rule_set_load_json(&rs, valid_json, strlen(valid_json)));
    assert(rs.rule_count == 1);
    assert(rs.rules[0].priority == 250);
    assert(rs.rules[0].version == 2);

    const char *negative_priority = "{\"rules\":[{\"rule_id\":\"R2\",\"priority\":-50,\"version\":1}]}";
    assert(ifm_rule_set_load_json(&rs, negative_priority, strlen(negative_priority)));
    assert(rs.rules[0].priority == -50);

    const char *overflow_priority = "{\"rules\":[{\"rule_id\":\"R3\",\"priority\":99999999999999999}]}";
    ifm_rule_set_init(&rs);
    assert(!ifm_rule_set_load_json(&rs, overflow_priority, strlen(overflow_priority)));
    assert(rs.rule_count == 0);

    const char *garbage_syntax = "{\"rules\":[{\"rule_id\":\"R4\",\"priority\":1-23}]}";
    ifm_rule_set_init(&rs);
    assert(!ifm_rule_set_load_json(&rs, garbage_syntax, strlen(garbage_syntax)));
    assert(rs.rule_count == 0);

    /* --- Version Boundary Assertions --- */
    const char *negative_version = "{\"rules\":[{\"rule_id\":\"R5\",\"priority\":100,\"version\":-1}]}";
    ifm_rule_set_init(&rs);
    assert(!ifm_rule_set_load_json(&rs, negative_version, strlen(negative_version)));
    assert(rs.rule_count == 0);

    const char *overflow_version = "{\"rules\":[{\"rule_id\":\"R6\",\"priority\":100,\"version\":4294967296}]}";
    ifm_rule_set_init(&rs);
    assert(!ifm_rule_set_load_json(&rs, overflow_version, strlen(overflow_version)));
    assert(rs.rule_count == 0);

    const char *garbage_version = "{\"rules\":[{\"rule_id\":\"R7\",\"priority\":100,\"version\":1-2}]}";
    ifm_rule_set_init(&rs);
    assert(!ifm_rule_set_load_json(&rs, garbage_version, strlen(garbage_version)));
    assert(rs.rule_count == 0);

    /* --- Plus Sign Rejection Assertions --- */
    const char *plus_priority = "{\"rules\":[{\"rule_id\":\"R8\",\"priority\":+123}]}";
    ifm_rule_set_init(&rs);
    assert(!ifm_rule_set_load_json(&rs, plus_priority, strlen(plus_priority)));
    assert(rs.rule_count == 0);

    const char *plus_version = "{\"rules\":[{\"rule_id\":\"R9\",\"priority\":100,\"version\":+123}]}";
    ifm_rule_set_init(&rs);
    assert(!ifm_rule_set_load_json(&rs, plus_version, strlen(plus_version)));
    assert(rs.rule_count == 0);

    printf("  [PASS] test_rules_numeric_safety\n");
}
