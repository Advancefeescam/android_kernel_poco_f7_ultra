#ifndef __HQ_CHG__CUST_H__
#define __HQ_CHG__CUST_H__

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include "hq_power_supply.h"


int batt_init_other_class(struct batt_chg *chg);
void batt_release_other_class(struct batt_chg *chg);
#endif