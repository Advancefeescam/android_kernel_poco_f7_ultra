/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 LiXun Technology(Shanghai) Co., Ltd.
 */

#ifndef __XM_CHG_DFS_H__
#define __XM_CHG_DFS_H__

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include "lxchg_notify.h"

/* Charge DFS configurations */
#define PARAMETER_LEN_MAX                       (128)

#define DFS_CHECK_5S_INTERVAL                   (5000)  // 5s
#define DFS_CHECK_10S_INTERVAL                  (10000) // 10s

/********************************************************/
/*               Charge DFS Events Define               */
/********************************************************/
/* Charge Speed Low */
#define CHG_DFX_PD_AUTH_FAIL_ID                 909001004
#define CHG_DFX_CP_ENABLE_FAIL_ID               909001005

/* Charge Abnormal Stop */
#define CHG_DFX_NONE_STANDARD_CHG_ID            909002001
#define CHG_DFX_CORROSION_DISCHARGE_ID          909002002
#define CHG_DFX_LPD_DETECTED_ID                 909002003
#define CHG_DFX_CP_VBUS_OVP_ID                  909002004
#define CHG_DFX_CP_IBUS_OCP_ID                  909002005
#define CHG_DFX_CP_VBAT_OVP_ID                  909002006
#define CHG_DFX_CP_IBAT_OCP_ID                  909002007
#define CHG_DFX_CP_VAC_OVP_ID                   909002008
#define CHG_DFX_ANTI_BURN_TRIG_ID               909002012

/* Cann't Report 100% SOC */
#define CHG_DFX_CHG_BATT_CYCLE_ID               909003001
#define CHG_DFX_SOC_NOT_FULL_ID                 909003002
#define CHG_DFX_SMART_ENDURA_TRIG_ID            909003004
#define CHG_DFX_SMART_NAVI_TRIG_ID              909003006

/* Cann't Charing */
#define CHG_DFX_FG_I2C_ERR_ID                   909005001
#define CHG_DFX_CP_I2C_ERR_ID                   909005002
#define CHG_DFX_BATT_LINKER_ABSENT_ID           909005003
#define CHG_DFX_CP_TDIE_HOT_ID                  909005004
#define CHG_DFX_VBUS_UVLO_ID                    909005006
#define CHG_DFX_NOT_CHG_IN_LOW_TEMP_ID          909005007
#define CHG_DFX_NOT_CHG_IN_HIGH_TEMP_ID         909005008
#define CHG_DFX_DUAL_BATT_LINKER_ABSENT_ID      909005009

/* SOC Not Accurately */
#define CHG_DFX_VBAT_SOC_NOT_MATCH_ID           909006001
#define CHG_DFX_SMART_ENDURA_SOC_ERR_ID         909006010
#define CHG_DFX_SMART_NAVI_SOC_ERR_ID           909006011

/* IC Connumicate Failed */
#define CHG_DFX_BATT_AUTH_FAIL_ID               909007001
#define CHG_DFX_CHG_BATT_AUTH_FAIL_ID           909007002
// #define CHG_DFX_CURR_I2C_ERR                 909007003 /* TODO: Should Double Check */

/* Temperature Abnormal */
#define CHG_DFX_TBAT_HOT_ID                     909009001
#define CHG_DFX_TBAT_COLD_ID                    909009002
#define CHG_DFX_ANTI_FAIL_ID                    909009003

/* Wireless Charge Spedd Low */
#define CHG_DFX_WLS_FAST_CHG_FAIL_ID            909011001
#define CHG_DFX_WLS_Q_LOW_ID                    909011002

/* Wireless Charge Abnormal Stop */
#define CHG_DFX_WLS_RX_OTP_ID                   909012001
#define CHG_DFX_WLS_RX_OVP_ID                   909012002
#define CHG_DFX_WLS_RX_OCP_ID                   909012003
#define CHG_DFX_WLS_TRX_FOD_ID                  909012004
#define CHG_DFX_WLS_TRX_OCP_ID                  909012005
#define CHG_DFX_WLS_TRX_UVLO_ID                 909012006

/* Wireless Charge Invalid */
#define CHG_DFX_WLS_TRX_I2C_ERR_ID              909013001
#define CHG_DFX_WLS_RX_I2C_ERR_ID               909013004

/* Battery Security */
#define CHG_DFX_DUAL_VBAT_DIFF_ID               909014002

/* Event specific parameters */
#define DFX_BAT_HOT_TEMP       (550)
#define DFX_BAT_COLD_TEMP      (-100)

#define REPORT_PERIOD_300S     (300)
#define REPORT_PERIOD_600S     (600)
#define DFS_DATA_CNT	5

struct pump_info {
	int tdie;
	int cp_enable_err;
	int cp_tdie_flt;
	int cp_absent_flag;
};

struct buck_info {
    int vbus;
	int aicl_threshold;
	bool adapter_plug_in;
};

struct battery_info {
	int vbat;
	int soc;
	int rsoc;
	int tbat;
	int cycle_cnt;
	int last_cycle_count;
	int chg_status;
	bool batt_auth;
};

struct dfs_info {
	int data[DFS_DATA_CNT];
};

struct xm_chg_dfs {
	struct xm_chg_dfs_evt *dfs_evt;
	struct charger_manager *manager;

	struct notifier_block pd_nb;
	struct notifier_block dfs_nb;
	struct delayed_work evt_report_check_work;
	struct mutex report_check_lock;

	struct pump_info pump;
	struct buck_info buck;
	struct battery_info battery;
	int check_interval;
};

struct xm_chg_dfs_evt {
	int dfs_type;
	int dfs_id;
	bool (*event_trig)(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt);
	bool event_called;
	struct dfs_info info;
	int report_cnt;
	int report_cnt_limit;
	time64_t report_time;
	int report_period_limit;
	char para_str[PARAMETER_LEN_MAX];
};

#endif /* __XM_CHG_DFS_H__ */
