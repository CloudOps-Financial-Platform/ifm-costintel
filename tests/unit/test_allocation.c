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

int main(void) {
    printf("Running Allocation Unit Tests...\n");
    test_allocation_single_match();
    test_allocation_priority_override();
    test_allocation_ambiguous();
    test_allocation_unallocated();
    printf("ALL ALLOCATION UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
