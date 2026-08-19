#undef NDEBUG
#include "ifm_costintel/aggregation.h"
#include "ifm_costintel/arena.h"
#include "ifm_costintel/concentration.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_aggregation_cost_center(void) {
    ifm_aggregation_table_t table;
    assert(ifm_agg_table_init(&table, IFM_AGG_DIM_COST_CENTER, 64, NULL));

    ifm_record_t r1 = { .cost_center_id = "CC-ENG", .active_spend_micros = 10000000 };
    ifm_record_t r2 = { .cost_center_id = "CC-ENG", .active_spend_micros = 20000000 };
    ifm_record_t r3 = { .cost_center_id = "CC-OPS", .active_spend_micros = 15000000 };

    assert(ifm_agg_table_accumulate(&table, &r1));
    assert(ifm_agg_table_accumulate(&table, &r2));
    assert(ifm_agg_table_accumulate(&table, &r3));

    assert(table.entry_count == 2);
    assert(table.grand_total_micros == 45000000);

    ifm_agg_entry_t *sorted[10];
    size_t sorted_count = ifm_agg_table_get_sorted_entries(&table, sorted, 10);
    assert(sorted_count == 2);

    /* Sorted descending by total spend: CC-ENG (30M) then CC-OPS (15M) */
    assert(strcmp(sorted[0]->key, "CC-ENG") == 0);
    assert(sorted[0]->total_spend_micros == 30000000);
    assert(sorted[0]->record_count == 2);

    /* Test concentration */
    ifm_micros_t conc0 = 0, conc1 = 0;
    assert(ifm_compute_concentration(sorted[0]->total_spend_micros, table.grand_total_micros, &conc0) == IFM_CONCENTRATION_DEFINED);
    assert(ifm_compute_concentration(sorted[1]->total_spend_micros, table.grand_total_micros, &conc1) == IFM_CONCENTRATION_DEFINED);
    /* 30/45 = 66.6666% (666666 micros) */
    assert(conc0 == 666666);
    /* 15/45 = 33.3333% (333333 micros) */
    assert(conc1 == 333333);

    assert(strcmp(sorted[1]->key, "CC-OPS") == 0);
    assert(sorted[1]->total_spend_micros == 15000000);
    assert(sorted[1]->record_count == 1);

    ifm_agg_table_cleanup(&table);
    printf("  [PASS] test_aggregation_cost_center\n");
}

static void test_aggregation_multi_dimension_and_arena(void) {
    ifm_arena_t arena;
    ifm_arena_init(&arena, 8192);

    ifm_aggregation_table_t agg_prov, agg_acc, agg_res;
    assert(ifm_agg_table_init(&agg_prov, IFM_AGG_DIM_PROVIDER, 32, &arena));
    assert(ifm_agg_table_init(&agg_acc, IFM_AGG_DIM_ACCOUNT, 32, &arena));
    assert(ifm_agg_table_init(&agg_res, IFM_AGG_DIM_RESOURCE, 32, &arena));

    ifm_record_t recs[] = {
        { .provider = "aws", .account_id = "111", .resource_id = "i-01", .active_spend_micros = 50000000 },
        { .provider = "aws", .account_id = "111", .resource_id = "i-02", .active_spend_micros = 25000000 },
        { .provider = "azure", .account_id = "sub-01", .resource_id = "vm-01", .active_spend_micros = 25000000 }
    };

    for (size_t i = 0; i < 3; ++i) {
        assert(ifm_agg_table_accumulate(&agg_prov, &recs[i]));
        assert(ifm_agg_table_accumulate(&agg_acc, &recs[i]));
        assert(ifm_agg_table_accumulate(&agg_res, &recs[i]));
    }

    assert(agg_prov.grand_total_micros == 100000000);
    assert(agg_acc.grand_total_micros == 100000000);
    assert(agg_res.grand_total_micros == 100000000);

    assert(agg_prov.entry_count == 2);
    assert(agg_acc.entry_count == 2);
    assert(agg_res.entry_count == 3);

    ifm_agg_entry_t *sorted_prov[5];
    size_t prov_cnt = ifm_agg_table_get_sorted_entries(&agg_prov, sorted_prov, 5);
    assert(prov_cnt == 2);
    assert(strcmp(sorted_prov[0]->key, "aws") == 0);
    assert(sorted_prov[0]->total_spend_micros == 75000000);

    ifm_agg_table_cleanup(&agg_prov);
    ifm_agg_table_cleanup(&agg_acc);
    ifm_agg_table_cleanup(&agg_res);
    ifm_arena_destroy(&arena);

    printf("  [PASS] test_aggregation_multi_dimension_and_arena\n");
}

int main(void) {
    printf("Running Aggregation Unit Tests...\n");
    test_aggregation_cost_center();
    test_aggregation_multi_dimension_and_arena();
    printf("ALL AGGREGATION UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
