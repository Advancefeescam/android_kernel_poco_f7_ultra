// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_JEITA_POLICY_H__
#define __HQ_JEITA_POLICY_H__

#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>

#include "hq_voter.h"
#include "battery_auth_class.h"

enum {
	JEITA_T0,
	JEITA_T1,
	JEITA_T2,
	JEITA_T3,
	JEITA_T4,
	JEITA_T5,
	JEITA_T6,
	JEITA_T7,
	JEITA_T8,
	JEITA_T9,
	JEITA_T10,
};

struct jeita_range {
	int low;
	int high;
};

struct ichg_parameter {
	struct jeita_range vbat_range;
	int ichg;
};

struct chg_parameter {
	int ichg_para_size;
	struct ichg_parameter ichg_para[5]; // TODO: max_vbat_range_num
	int fv;
	int iterm[5]; // TODO: max_battery_num
};

struct jeita_parameter {
	/* factors effect jeita parameters */
	struct jeita_range cycle_range;
	int charge_mode;

	/* jeita parameter for current setting */
	struct chg_parameter *chg_para;
};

struct jeita_condition {
	int tbat;
	int vbat;
	int charge_mode;
	int cycle_cnt;
	int batt_id;
	int isc_status;
};

struct jeita_config {
	int ichg;
	int fv;
	int iterm;
};

struct hq_jeita_policy {
	struct device *dev;
	struct wakeup_source *jeita_ws;
	struct mutex jeita_update_lock;

	/* votable */
	struct votable *fv_votable;
	struct votable *main_icl_votable;
	struct votable *iterm_votable;
	struct votable *total_fcc_votable;

	/* psy */
	struct power_supply *batt_psy;
	struct power_supply *bms_psy;
	struct power_supply *usb_psy;

	struct delayed_work policy_update_work;

	struct jeita_condition jeita_cond;
	struct jeita_config jeita_cfg;
};

/******************************************************************************/
/*                       JEITA POLICY API FUNCTIONS                           */
/******************************************************************************/
// int hq_jeita_policy_init(struct charger_manager *manager);
// int hq_jeita_policy_deinit(struct charger_manager *manager);
// int hq_jeita_policy_run(struct charger_manager *manager);

#endif /* __HQ_JEITA_POLICY_H__ */
