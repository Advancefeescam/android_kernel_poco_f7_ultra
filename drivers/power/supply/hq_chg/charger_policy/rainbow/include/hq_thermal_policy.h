/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */
#ifndef __HQ_THERMAL_POLICY_H__
#define __HQ_THERMAL_POLICY_H__

#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#include "hq_voter.h"

/******************************************************************************/
/*                  THERMAL POLICY FUNCTIONS DEFINES                          */
/******************************************************************************/
/* 
 * Func Name:  THERMAL_POLICY_SHILED_ON_BOOT
 * Func Owner: pengyuzhe@huaqin.com
 * Func Date:  2024.11.25
 * Func Desc:  if battery temperature below 45 degrees, shiled thermal policy
 * on system bootup to speed up bootup time when battery voltage is low and
 * restore thermal policy after 60s.
 */
//#define THERMAL_POLICY_SHILED_ON_BOOT

enum thermal_type {
	TEMP_THERMAL_TYPE = 0,
	CALL_THERMAL_TYPE,
	MAX_THERMAL_TYPE,
};

struct hq_thermal_policy {
	struct device *dev;
	struct mutex thermal_update_lock;

	bool thermal_enable;
	int board_temp;
	int last_thermal_level;
	int thermal_level;
	int thermal_level_max;
	int *pd_thermal_mitigation;
	int *qc2_thermal_mitigation;
	int pd_thermal_levels;
	int qc2_thermal_levels;
	int thermal_type;
	int pd_active;

	/* votable */
	struct votable *total_fcc_votable;

	/* psy */
	struct power_supply *batt_psy;

	struct delayed_work policy_update_work;

#ifdef THERMAL_POLICY_SHILED_ON_BOOT
	struct delayed_work thermal_restore_work;
#endif
};

/******************************************************************************/
/*                       THERMAL POLICY API FUNCTIONS                         */
/******************************************************************************/
// int hq_thermal_policy_init(struct charger_manager *manager);
// int hq_thermal_policy_deinit(struct charger_manager *manager);
// int hq_thermal_policy_run(struct charger_manager *manager);

#endif /* __HQ_THERMAL_POLICY_H__ */
