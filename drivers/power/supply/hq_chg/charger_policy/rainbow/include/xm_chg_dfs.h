/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __XM_CHG_DFS_H__
#define __XM_CHG_DFS_H__

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>

#include "mievent.h"
#include "xm_adapter_class.h"
#include "hq_fg_class.h"

#define PARAMETER_LEN_MAX               (128)
#define REPORT_PERIOD_300S              (300)
#define REPORT_PERIOD_600S              (600)
#define DFS_CHECK_5S_INTERVAL           (5000)
#define DFS_CHECK_10S_INTERVAL          (10000)

#define DFX_ID_CHG_PD_AUTH_FAIL         909001004
#define DFX_ID_CHG_CP_EN_FAIL           909001005
#define DFX_ID_CHG_NONE_STANDARD_CHG    909002001
// #define DFX_ID_CHG_CORROSION_DISCHARGE  909002002
#define DFX_ID_CHG_LPD_DETECTED         909002003
#define DFX_ID_CHG_CP_VBUS_OVP          909002004
#define DFX_ID_CHG_CP_IBUS_OCP          909002005
#define DFX_ID_CHG_CP_VBAT_OVP          909002006
#define DFX_ID_CHG_CP_IBAT_OCP          909002007
#define DFX_ID_CHG_CP_VAC_OVP           909002008
#define DFX_ID_CHG_CHG_BATT_CYCLE       909003001
#define DFX_ID_CHG_SOC_NOT_FULL         909003002
#define DFX_ID_CHG_SMART_ENDURA_TRIG    909003004
#define DFX_ID_CHG_FG_I2C_ERR           909005001
#define DFX_ID_CHG_CP_I2C_ERR           909005002
// #define DFX_ID_CHG_BATT_LINKER_ABSENT   909005003
#define DFX_ID_CHG_CP_TDIE_HOT          909005004
#define DFX_ID_CHG_VBUS_UVLO            909005006
#define DFX_ID_CHG_NOT_CHG_IN_LOW_TEMP  909005007
#define DFX_ID_CHG_NOT_CHG_IN_HIGH_TEMP 909005008
#define DFX_ID_CHG_BATT_AUTH_FAIL       909007001
#define DFX_ID_CHG_TBAT_HOT             909009001
#define DFX_ID_CHG_TBAT_COLD            909009002


enum xm_chg_dfx_type {
	CHG_DFX_DEFAULT_TYPE,
	CHG_DFX_NONE_STANDARD_CHG,
	CHG_DFX_FG_I2C_ERR,
	CHG_DFX_CP_I2C_ERR,
	CHG_DFX_CP_EN_FAIL,
	CHG_DFX_BATT_LINKER_ABSENT,
	CHG_DFX_NOT_CHG_IN_LOW_TEMP,
	CHG_DFX_NOT_CHG_IN_HIGH_TEMP,
	CHG_DFX_PD_AUTH_FAIL,
	CHG_DFX_CP_VBUS_OVP,
	CHG_DFX_CP_IBUS_OCP,
	CHG_DFX_CP_VBAT_OVP,
	CHG_DFX_CP_IBAT_OCP,
	CHG_DFX_CP_VAC_OVP,
	CHG_DFX_CP_TDIE_HOT,
	CHG_DFX_CHG_BATT_CYCLE,
	CHG_DFX_SMART_ENDURA_TRIG,
	CHG_DFX_SOC_NOT_FULL,
	CHG_DFX_BATT_AUTH_FAIL,
	CHG_DFX_TBAT_HOT,
	CHG_DFX_TBAT_COLD,
	CHG_DFX_LPD_DETECTED,
	CHG_DFX_VBUS_UVLO,
	CHG_DFX_MAX_TYPE,
};

struct xm_chg_dfx_evt_cond {
	// int evt_code;
	// char *evt_name;
	// int evt_type;
	int dfx_type;
	int dfx_id;
	uint8_t para_cnt;
	int report_cnt;
	int report_cnt_limit;
	time64_t report_time;
	int report_period_limit;
};

struct xm_chg_dfs {
	struct task_struct *report_thread;
	wait_queue_head_t report_wq;
	//struct xm_chg_dfx_evt_cond *evt_cond;
	atomic_t run_report;
	int dfx_type;
	unsigned long evt_report_bits[BITS_TO_LONGS(CHG_DFX_MAX_TYPE)];

	struct notifier_block psy_nb;
	struct notifier_block cp_nb;
	struct notifier_block fg_nb;
	struct notifier_block cm_nb;
	struct charger_dev *charger;
	struct adapter_device *pd_adapter;
	struct chargerpump_dev *cp_charger;
	struct fuel_gauge_dev *fuel_gauge;
	struct delayed_work evt_cond_check_work;
	struct mutex cond_check_lock;

	int check_interval_ms;
	int board_version;
	int tbat;
	int tbat_max;
	int tbat_min;
	int batt_auth;
	int chip_chg_status;
	int cycle_cnt;
	int last_cycle_count;
	int vbat;
	int vbat_max;
	int soc;
	int rsoc;
	int vbus_type;
	int vbus;
	int tboard;
	int adapter_plug_in;
	int master_ok;
	int adapter_svid;
	int pd_auth_fail;
	int pd_verify_done;
	int fg_status;
	// charger_ic_event
	int cp_ibat_ocp;
	int cp_ibus_ocp;
	int cp_vbus_ovp;
	int cp_vbat_ovp;
	int cp_vac_ovp;
	int cp_tdie_flt;
	int cp_enbale_fail;
	int smart_endura_trig;
	int lpd_flag;
	int tdie_temp;
	int vbus_uvlo;
	int aicl_threshold;
};

#endif /* __XM_CHG_DFS_H__ */