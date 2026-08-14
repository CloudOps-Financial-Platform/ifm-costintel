#include "ifm_costintel/traceability.h"
#include <string.h>

void ifm_traceability_stamp(ifm_record_t *record, uint64_t source_line) {
    if (!record) return;
    if (record->source_line == 0 && source_line > 0) {
        record->source_line = source_line;
    }
}
