#undef NDEBUG
#include "ifm_costintel/aggregation.h"
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

    assert(strcmp(sorted[1]->key, "CC-OPS") == 0);
    assert(sorted[1]->total_spend_micros == 15000000);
    assert(sorted[1]->record_count == 1);

    ifm_agg_table_cleanup(&table);
    printf("  [PASS] test_aggregation_cost_center\n");
}

int main(void) {
    printf("Running Aggregation Unit Tests...\n");
    test_aggregation_cost_center();
    printf("ALL AGGREGATION UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
