/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 LiXun Technology(Shanghai) Co., Ltd.
 */
#ifndef __LX_NOTIFY_H__
#define __LX_NOTIFY_H__

#include <linux/notifier.h>

enum load_index {
	DISORDER,
	LOAD_INDEX_1,
	LOAD_INDEX_2,
	LOAD_INDEX_3,
	COMPATIBLE_MAX,
};

enum lxchg_device {
	BUCK_CHAEGER,
	MASTER_CHAEGEPUMP,
	SLAVE_CHAEGEPUMP,
	FUEL_GAUGE,
	BATTERY_SECRET,

	MAX_CHG_DEVICE,
	PROBE_DEFER, //defer event
};


enum lxchg_notifier_events {
	POWER_SUPPLY_CHANGED,
	CHARGE_EVENT_PLUG_IN,
	CHARGE_EVENT_PLUG_OUT,
	CHARGE_EVENT_BC12_DONE,
	CHARGE_EVENT_BC12_TIMEOUT,
	CHARGE_EVENT_CID_PLUGIN,
	CHARGE_EVENT_CID_PLUGOUT,
	CHARGE_EVENT_HVDCP_DONE,
	CHARGE_EVENT_SOFT_TERM,
	CHARGE_EVENT_SOFT_RECHG,
	CHARGE_EVENT_LPD_OCCUR,
	CHARGE_EVENT_LPD_RELEASE,
	BATTERY_EVENT_CYCLE_CHANGED,
	THERMAL_EVENT_BOARD_TEMP,
};

enum xmdfs_dfx_type_notifier_events {
	CHG_DFX_PD_AUTH_FAIL,
	CHG_DFX_NONE_STANDARD_CHG,
	CHG_DFX_CORROSION_DISCHARGE,
	CHG_DFX_LPD_DETECTED,
	CHG_DFX_CP_VBUS_OVP,
	CHG_DFX_CP_IBUS_OCP = 5,
	CHG_DFX_CP_VBAT_OVP,
	CHG_DFX_CP_IBAT_OCP,
	CHG_DFX_CP_VAC_OVP,
	CHG_DFX_CP_TDIE_HOT,
	CHG_DFX_CP_ENABLE_FAIL = 10,
	CHG_DFX_CP_I2C_ERR,
	CHG_DFX_ANTI_BURN_TRIG,
	CHG_DFX_CHG_BATT_CYCLE,
	CHG_DFX_SOC_NOT_FULL,
	CHG_DFX_SMART_ENDURA_TRIG = 15,
	CHG_DFX_SMART_NAVI_TRIG,
	CHG_DFX_FG_I2C_ERR,
	CHG_DFX_BATT_LINKER_ABSENT,
	CHG_DFX_VBUS_UVLO,
	CHG_DFX_NOT_CHG_IN_LOW_TEMP = 20,
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
enum xm_chg_uevent_type {
	CHG_UEVENT_DEFAULT_TYPE,
	CHG_UEVENT_SOC_DECIMAL,
	CHG_UEVENT_SOC_DECIMAL_RATE,
	CHG_UEVENT_QUICK_CHARGE_TYPE,
	CHG_UEVENT_SHUTDOWN_DELAY,
	CHG_UEVENT_CONNECTOR_TEMP,
	CHG_UEVENT_NTC_ALARM,
	CHG_UEVENT_LPD_DETECTION,
	CHG_UEVENT_REVERSE_QUICK_CHARGE,
	CHG_UEVENT_MAX_TYPE,
	CHG_UEVENT_CC_SHORT_VBUS,
};

int lxchg_notifier_register(struct notifier_block *nb);
int lxchg_notifier_unregister(struct notifier_block *nb);
int lxchg_notifier_call_chain(unsigned long val, void *v);
int xmdfs_notifier_register(struct notifier_block *nb);
int xmdfs_notifier_unregister(struct notifier_block *nb);
int xmdfs_notifier_call_chain(unsigned long val, void *v);
void lxchg_psy_updata(enum lxchg_notifier_events evt);
int xm_charge_uevent_report(int event_type, int event_value);
int xm_charge_uevents_bundle_report(int bundle_type, ...);


#endif /* __LX_NOTIFY_H__ */
