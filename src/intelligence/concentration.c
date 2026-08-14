#include "ifm_costintel/concentration.h"
#include "ifm_costintel/fak.h"

ifm_concentration_status_t ifm_compute_concentration(ifm_micros_t dimension_spend_micros,
                                                     ifm_micros_t total_spend_micros,
                                                     ifm_micros_t *out_concentration_micros) {
    if (!out_concentration_micros) return IFM_CONCENTRATION_NOT_DEFINED;

    if (total_spend_micros <= 0) {
        *out_concentration_micros = 0;
        return IFM_CONCENTRATION_NOT_DEFINED;
    }

    if (dimension_spend_micros <= 0) {
        *out_concentration_micros = 0;
        return IFM_CONCENTRATION_DEFINED;
    }

    if (fak_div_micros(dimension_spend_micros, total_spend_micros, out_concentration_micros)) {
        return IFM_CONCENTRATION_DEFINED;
    }

    *out_concentration_micros = 0;
    return IFM_CONCENTRATION_NOT_DEFINED;
}
