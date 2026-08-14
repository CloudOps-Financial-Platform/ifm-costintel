#ifndef IFM_CONCENTRATION_H
#define IFM_CONCENTRATION_H

#include "ifm_costintel/ifm_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute concentration ratio: dimension_spend / total_spend */
ifm_concentration_status_t ifm_compute_concentration(ifm_micros_t dimension_spend_micros,
                                                     ifm_micros_t total_spend_micros,
                                                     ifm_micros_t *out_concentration_micros);

#ifdef __cplusplus
}
#endif

#endif /* IFM_CONCENTRATION_H */
