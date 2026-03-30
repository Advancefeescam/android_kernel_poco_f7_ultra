// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_TERM_RECHARGE_POLICY_H__
#define __HQ_TERM_RECHARGE_POLICY_H__

#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#include "hq_voter.h"
#include "hq_adapter_class.h"
#include "hq_charger_class.h"

#define TERM_CHARGE_STATUS     	            POWER_SUPPLY_STATUS_CHARGING
#define TERM_LOWER_LIMIT_UISOC              (100)
#define TERM_LOWER_LIMIT_FGSOC              (90)
#define TERM_FV_RATIO                       (9920)
#define TERM_ITERM_RATIO                    (11000)
#define TERM_CHECK_COUNT                    (5)
#define TERM_LOWER_LIMIT_VBAT(fv_mv)        ((fv_mv * TERM_FV_RATIO) / 10000)
#define TERM_UPPER_LIMIT_IBAT(iterm_ma)     ((iterm_ma * TERM_ITERM_RATIO) / 10000)

#define RECHARGE_CHARGE_STATUS              POWER_SUPPLY_STATUS_FULL
#define RECHARGE_UPPER_LIMIT_SOC            (100)
#define RECHARGE_FV_OFFSET                  (20)
#define RECHARGE_CHECK_COUNT                (1)
#define RECHARGE_UPPER_LIMIT_VBAT(fv_mv)    (fv_mv - RECHARGE_FV_OFFSET)

#define EEA_BUCK_RECHARGE_VOLTAGE_400MV     400
#define EEA_BUCK_RECHARGE_VOLTAGE_100MV     100
#define EEA_RECHARGE_SOC                    95
#define BUCK_RECHARGE_VOLTAGE_400MV			400
#define BUCK_RECHARGE_VOLTAGE_100MV			100
#define RECHARGE_RSOC						9700
#define CHARGE_FULL_DROP_VBUS_MV            6500

#define CHARGE_AGAIN_TEMP_LOWER_LIMIT		230
#define CHARGE_AGAIN_TEMP_UPPER_LIMIT		450

#define CHARGE_AGAIN_RANGE_NUM (4)

struct charge_again_range {
	int l_cycle;
	int u_cycle;
	int fv;
	int ichg;
	int iterm;
};

static struct charge_again_range charge_again_table[CHARGE_AGAIN_RANGE_NUM] = {
	[0] = {.l_cycle = 0, .u_cycle = 100, .fv = 4490, .ichg = 5096, .iterm = 446},
	[1] = {.l_cycle = 101, .u_cycle = 300, .fv = 4480, .ichg = 5096, .iterm = 446},
	[2] = {.l_cycle = 301, .u_cycle = 800, .fv = 4470, .ichg = 5096, .iterm = 446},
	[3] = {.l_cycle = 801, .u_cycle = INT_MAX, .fv = 4450, .ichg = 5096, .iterm = 446}
};

struct hq_term_recharge_policy {
	struct device *dev;
	struct mutex term_recharge_update_lock;

	int term_check_cnt;

	/* charging state */
	int vbus_type;
	int charge_status;
	int ibat;
	int vbat;
	int ui_soc;
	int fg_soc;
	int raw_soc;
	int tbat;
	int cycle_cnt;

	int fv;
	int iterm;
	int rchg_val;

	int pd_active;
	int board_version;
	int soft_charge_status;
	int charge_mode;

	bool uisoc_100_adapter_plugin;
	bool terminated;
	bool is_charge_done;
	bool is_charge_again;

	/* votable */
	struct votable *iterm_votable;
	struct votable *fv_votable;
	struct votable *total_fcc_votable;

	/* psy */
	struct power_supply *batt_psy;

	/* ext_dev */
	struct charger_dev *charger;
	struct adapter_dev *adapter;
	struct fuel_gauge_dev *fuel_gauge;

	struct delayed_work policy_update_work;
	struct delayed_work battery_charge_again_work;
};

/******************************************************************************/
/*                TERMINATE AND RECHARGE POLICY API FUNCTIONS                 */
/******************************************************************************/
// int hq_term_recharge_policy_init(struct charger_manager *manager);
// int hq_term_recharge_policy_deinit(struct charger_manager *manager);
// int hq_term_recharge_policy_run(struct charger_manager *manager);

#endif /* __HQ_TERM_RECHARGE_POLICY_H__ */
