/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 LiXun Technology(Shanghai) Co., Ltd.
 */
#ifndef __LX_NOTIFY_H__
#define __LX_NOTIFY_H__

#include <linux/notifier.h>

enum lxchg_notifier_events {
	LXCHG_DEFAULT_EVENT,
	BUCK_EVENT_BC12_DONE,
	BUCK_EVENT_HVDCP_DONE,
	BUCK_EVENT_CHG_TIMEOUT,
	BUCK_EVENT_CID_DETECT,
	THERMAL_EVENT_BOARD_TEMP,
};

enum xmdfs_dfx_type_notifier_events {
	CHG_DFX_PD_AUTH_FAIL,
	CHG_DFX_NONE_STANDARD_CHG,
	CHG_DFX_CORROSION_DISCHARGE,
	CHG_DFX_LPD_DETECTED,
	CHG_DFX_CP_VBUS_OVP,
	CHG_DFX_CP_IBUS_OCP,
	CHG_DFX_CP_VBAT_OVP,
	CHG_DFX_CP_IBAT_OCP,
	CHG_DFX_CP_VAC_OVP,
	CHG_DFX_CP_TDIE_HOT,
	CHG_DFX_CP_ENABLE_FAIL,
	CHG_DFX_CP_I2C_ERR,
	CHG_DFX_ANTI_BURN_TRIG,
	CHG_DFX_CHG_BATT_CYCLE,
	CHG_DFX_SOC_NOT_FULL,
	CHG_DFX_SMART_ENDURA_TRIG,
	CHG_DFX_SMART_NAVI_TRIG,
	CHG_DFX_FG_I2C_ERR,
	CHG_DFX_BATT_LINKER_ABSENT,
	CHG_DFX_VBUS_UVLO,
	CHG_DFX_NOT_CHG_IN_LOW_TEMP,
	CHG_DFX_NOT_CHG_IN_HIGH_TEMP,
	CHG_DFX_VBAT_SOC_NOT_MATCH,
	CHG_DFX_SMART_ENDURA_SOC_ERR,
	CHG_DFX_SMART_NAVI_SOC_ERR,
	CHG_DFX_BATT_AUTH_FAIL,
	CHG_DFX_TBAT_HOT,
	CHG_DFX_TBAT_COLD,
	CHG_DFX_ANTI_FAIL,
	/* NOTE: add new type here */
	CHG_DFX_MAX_TYPE,
};

int lxchg_notifier_register(struct notifier_block *nb);
int lxchg_notifier_unregister(struct notifier_block *nb);
int lxchg_notifier_call_chain(unsigned long val, void *v);
int xmdfs_notifier_register(struct notifier_block *nb);
int xmdfs_notifier_unregister(struct notifier_block *nb);
int xmdfs_notifier_call_chain(unsigned long val, void *v);
void lxchg_psy_updata(enum lxchg_notifier_events evt);


#endif /* __LX_NOTIFY_H__ */
