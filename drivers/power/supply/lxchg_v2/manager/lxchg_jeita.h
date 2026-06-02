/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __LENOVO_JEITA_H__
#define __LENOVO_JEITA_H__

#include "lx_cp_policy.h"
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "xm_adapter_class.h"
#endif
#include "lxchg_class.h"
#include "lxchg_class.h"
#include "lxchg_manager.h"
#include "lxbat_auth_class.h"

#define MAX_STEP_JEITA_NUM          10
#define COLD_RECHG_VOLT_OFFSET      200
#define TEMP_45_TO_58_FV            4100
#define CURRENT_NOW_1A              500
#define WARM_RECHG_VOLT_OFFSET      130
#define TEMP_LEVEL_NEGATIVE_10      -100
#define TEMP_LEVEL_NEGATIVE_7       -70
#define TEMP_LEVEL_15               150
#define TEMP_LEVEL_20               200
#define TEMP_LEVEL_35               350
#define TEMP_LEVEL_40               400
#define TEMP_LEVEL_42               420
#define TEMP_LEVEL_45               450
#define TEMP_LEVEL_48               480
#define TEMP_LEVEL_58               580

#define TEMP_SHAKE_OFFSET           20
#define TERM_DELTA_CV               8
#define HEAVY_LOAD_VOLTAGE          4470
#define JEITA_NORMAL_CFG_LINE_LEN   9
#define JEITA_FCC_CFG_LINE_LEN      11
#define JEITA_RECHG_LINE_LEN        3

#define LOW_TEMP_RECHG_OFFSET       200
#define NOR_TEMP_RECHG_OFFSET       100
#define CHG_CYCLE_RESTORE_VOL       100
#define BATT_CYCLE_COUNT_OVER_100   100
#define BATT_CYCLE_COUNT_OVER_800   800

#define ST_THRESHOLD_LOW_LIMIT_FV(fv_mv) (fv_mv * 9900 / 10000)
#define ST_THRESHOLD_LOW_LIMIT_SDP_FV(fv_mv) (fv_mv * 9960 / 10000)
#define ST_THRESHOLD_LOW_LIMIT_ITERM_SDP(iterm_ma) (iterm_ma * 100 / 100)
#define ST_THRESHOLD_LOW_LIMIT_ITERM(iterm_ma) (iterm_ma * 105 / 100)
#define HW_THRESHOLD_LOW_LIMIT_ITERM(iterm_ma) (iterm_ma * 95 / 100)
#define SW_THRESHOLD_LIMIT_ITERM(iterm_ma) (iterm_ma * 110 / 100)

#define BATT_MA_AVG_SAMPLES             8

int lx_jeita_init(struct charger_manager *manager);
void lx_jeita_deinit(void);

/******************** LX ***********************/
#define LX_SDP_HARD_ITERM_STEP         50

#endif /* __STEP_CHG_H__ */
