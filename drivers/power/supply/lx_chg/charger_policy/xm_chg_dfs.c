// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2025 LiXun Technology(Shanghai) Co., Ltd.
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/stddef.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include "../charger_class/lx_fg_class.h"
#include "../charger_class/lx_cp_class.h"
#include "lx_charger_manager.h"
#include "xm_chg_dfs.h"
#include "lx_printk.h"
#include <linux/err.h>
#include "mievent.h"

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_core.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_typec.h"
#endif

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "../charger_class/xm_adapter_class.h"
#endif


#ifdef TAG
#undef TAG
#define TAG "[LX_CHG_DFS]"
#endif

#define CHG_DFX_EVT_DECLARE(type, function, cnt_limit, period_limit) \
{\
	.dfs_type = type, \
	.event_trig = function, \
	.dfs_id = type##_ID, \
	.report_cnt_limit = cnt_limit, \
	.report_period_limit = period_limit, \
}
#define USB_PD_MI_SVID			0x2717

static bool pd_auth_fail_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;
	int adapter_svid = chg_dfs->manager->pd_adapter->adapter_svid;
	bool verify_done = chg_dfs->manager->pd_adapter->verify_done;
	bool verfied = chg_dfs->manager->pd_adapter->verifed;

	lx_info("svid=%d, verify_done = %d, verifed = %d\n", adapter_svid, verify_done, verfied);
	if (adapter_svid == USB_PD_MI_SVID && verify_done && !verfied) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "PdAuthFail");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:0x%x", "adapterId", adapter_svid);
		return true;
	}
	return false;
}

static bool cp_enable_fail_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpEnFail");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cpId", 0);
		return true;
	}
	return false;
}

static bool unstandard_chg_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NoneStandartChg");
		return true;
	}
	return false;
}

static bool corrosion_discharge_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CorrosionDischarge");
		return true;
	}
	return false;
}

static bool lpd_detected_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "LpdDetected");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "lpdFlag", dfs_evt->event_called);
		return true;
	}
	return false;
}

static bool cp_vbus_ovp_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbusOVP");
		return true;
	}
	return false;
}

static bool cp_ibus_ocp_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbusOCP");
		return true;
	}
	return false;
}

static bool cp_vbat_ovp_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;
	

	if (dfs_evt->event_called) {
		int vbat = dfs_evt->info.data[0];
		int vbat_max = dfs_evt->info.data[1];

		vbat_max = max(vbat, vbat_max);
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbatOVP");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", vbat);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbatMax", vbat_max);
		return true;
	}
	return false;
}

static bool cp_ibat_ocp_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbatOCP");
		return true;
	}
	return false;
}

static bool cp_vac_ovp_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVacOVP");
		return true;
	}
	return false;
}

static bool anti_burn_trig_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		int temp = dfs_evt->info.data[0];
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "AntiBurnTrig");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tconn", temp);
		return true;
	}
	return false;
}

static bool chg_batt_cycle_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (chg_dfs->battery.cycle_cnt != chg_dfs->battery.last_cycle_count) {
		if ((chg_dfs->battery.cycle_cnt % 100) == 0) {
			memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
			len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "chgBattCycle");
			len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cycleCnt", chg_dfs->battery.cycle_cnt);
			return true;
		}
		chg_dfs->battery.last_cycle_count = chg_dfs->battery.cycle_cnt;
	}
	return false;
}

static bool soc_not_full_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if ((chg_dfs->battery.chg_status== POWER_SUPPLY_STATUS_FULL) && (chg_dfs->battery.soc != 100)) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SocNotFull");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->battery.vbat);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->battery.soc);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "rsoc", chg_dfs->battery.rsoc);
		return true;
	}
	return false;
}

static bool smart_endura_trig_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "SmartEnduraTrig");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->battery.soc);
		return true;
	}
	return false;
}

static bool smart_navi_trig_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "SmartNaviTrig");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->battery.soc);
		return true;
	}
	return false;
}

static bool fg_i2c_err_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "fgI2cErr");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->battery.soc);
		return true;
	}
	return false;
}


static bool cp_i2c_err_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpI2CErr");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d","masterOk", 0);
		return true;
	}
	return false;
}

static bool batt_linker_absent_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattLinkerAbsent");
		return true;
	}
	return false;
}

static bool cp_tdie_hot_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "CpTdieHot");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "masterTdie", chg_dfs->pump.tdie);
		return true;
	}
	return false;
}

static bool vbus_uvlo_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "VbusUvlo");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->battery.vbat);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbus", chg_dfs->buck.vbus);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "aiclTh", chg_dfs->buck.aicl_threshold);
		return true;
	}
	return false;
}

static bool not_chg_in_low_temp_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (chg_dfs->battery.tbat < 50 && chg_dfs->battery.tbat > -100 && chg_dfs->buck.adapter_plug_in &&
		chg_dfs->battery.chg_status != POWER_SUPPLY_STATUS_CHARGING && chg_dfs->battery.chg_status != POWER_SUPPLY_STATUS_FULL) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInLowTemp");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->battery.tbat);
		return true;
	}
	return false;
}

static bool not_chg_in_high_temp_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (chg_dfs->battery.tbat < 550 && chg_dfs->battery.tbat > 480 && chg_dfs->buck.adapter_plug_in &&
		chg_dfs->battery.chg_status != POWER_SUPPLY_STATUS_CHARGING && chg_dfs->battery.chg_status != POWER_SUPPLY_STATUS_FULL) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInHighTemp");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->battery.tbat);
		return true;
	}
	return false;
}

static bool soc_not_match_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "VbatSocNotMatch");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->battery.vbat);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->battery.soc);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cyclecnt", chg_dfs->battery.cycle_cnt);
		return true;
	}
	return false;
}

static bool smart_endura_soc_err_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SmartEnduraSocErr");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->battery.soc);
		return true;
	}
	return false;
}

static bool navi_soc_err_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SmartNaviSocErr");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->battery.soc);
		return true;
	}
	return false;
}

static bool batt_auth_fail_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattAuthFail");
		return true;
	}
	return false;
}

static bool tbat_hot_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (chg_dfs->battery.tbat > DFX_BAT_HOT_TEMP) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatHot");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->battery.tbat);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMax", (chg_dfs->battery.tbat / 10));
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->buck.adapter_plug_in);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tboard", chg_dfs->manager->thermal_board_temp);
		return true;
	}
	return false;
}

static bool tbat_cold_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (chg_dfs->battery.tbat < DFX_BAT_COLD_TEMP) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatCold");
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->battery.tbat);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMin", (chg_dfs->battery.tbat / 10));
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->buck.adapter_plug_in);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tboard", chg_dfs->manager->thermal_board_temp);
		return true;
	}
	return false;
}

static bool anti_fail_check(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	int len = 0;

	if (dfs_evt->event_called) {
		memset(dfs_evt->para_str, 0, PARAMETER_LEN_MAX);
		len += scnprintf((dfs_evt->para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "AntiFail");
		return true;
	}
	return false;
}

static struct xm_chg_dfs_evt dfs_evts[CHG_DFX_MAX_TYPE] = {
	CHG_DFX_EVT_DECLARE(CHG_DFX_PD_AUTH_FAIL, pd_auth_fail_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_ENABLE_FAIL, cp_enable_fail_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_NONE_STANDARD_CHG, unstandard_chg_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CORROSION_DISCHARGE, corrosion_discharge_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_LPD_DETECTED, lpd_detected_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_VBUS_OVP, cp_vbus_ovp_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_IBUS_OCP, cp_ibus_ocp_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_VBAT_OVP, cp_vbat_ovp_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_IBAT_OCP, cp_ibat_ocp_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_VAC_OVP, cp_vac_ovp_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_ANTI_BURN_TRIG, anti_burn_trig_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CHG_BATT_CYCLE, chg_batt_cycle_check, 0, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_SOC_NOT_FULL, soc_not_full_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_SMART_ENDURA_TRIG, smart_endura_trig_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_SMART_NAVI_TRIG, smart_navi_trig_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_FG_I2C_ERR, fg_i2c_err_check, 0, REPORT_PERIOD_600S),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_I2C_ERR, cp_i2c_err_check, 0, REPORT_PERIOD_600S),
	CHG_DFX_EVT_DECLARE(CHG_DFX_BATT_LINKER_ABSENT, batt_linker_absent_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_CP_TDIE_HOT, cp_tdie_hot_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_VBUS_UVLO, vbus_uvlo_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_NOT_CHG_IN_LOW_TEMP, not_chg_in_low_temp_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_NOT_CHG_IN_HIGH_TEMP, not_chg_in_high_temp_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_VBAT_SOC_NOT_MATCH, soc_not_match_check, 3, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_SMART_ENDURA_SOC_ERR, smart_endura_soc_err_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_SMART_NAVI_SOC_ERR, navi_soc_err_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_BATT_AUTH_FAIL, batt_auth_fail_check, 1, 0),
	CHG_DFX_EVT_DECLARE(CHG_DFX_TBAT_HOT, tbat_hot_check, 0, REPORT_PERIOD_300S),
	CHG_DFX_EVT_DECLARE(CHG_DFX_TBAT_COLD, tbat_cold_check, 0, REPORT_PERIOD_300S),
	CHG_DFX_EVT_DECLARE(CHG_DFX_ANTI_FAIL, anti_fail_check, 1, 0),
};

void mievent_upload(int miev_code, char *miev_param)
{
	char buffer[128] = {0};
	char *p1, *p2, *key, *value;
	int intval;
	struct misight_mievent *event = cdev_tevent_alloc(miev_code);

	memcpy(buffer, miev_param, strlen(miev_param));

	lx_info("miev_code=%d\n", miev_code);

	p1 = buffer;
	while (p1 && (*p1 != '\0')) {
		p2 = strsep(&p1, ",");
		while (p2 && (*p2 != '\0')) {
			key = strsep(&p2, ":");
			if (key) {
				lx_info("[CHG_DFS] key:%s\n", key);
			} else {
				lx_err("[CHG_DFS] none key\n");
				key = "None";
			}

			value = strsep(&p2, ":");
			if (value) {
				lx_info("[CHG_DFS] value:%s\n", value);
			} else {
				lx_err("[CHG_DFS] none value\n");
				value = "None";
			}
		}

		if (kstrtoint(value, 10, &intval) != 0) {
			cdev_tevent_add_str(event, key, value);
			lx_info("type=string, %s:%s\n", key, value);
		} else {
			cdev_tevent_add_int(event, key, intval);
			lx_info("type=int, %s:%d\n", key, intval);
		}
	}

	cdev_tevent_write(event);
	cdev_tevent_destroy(event);

	return;
}

static void xm_chg_dfx_handle_event_report(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfs_evt *dfs_evt)
{
	time64_t time_now = ktime_get_seconds();

	if (dfs_evt->event_trig(chg_dfs, dfs_evt) &&
		(dfs_evt->report_cnt_limit == 0 || dfs_evt->report_cnt < dfs_evt->report_cnt_limit) &&
		(dfs_evt->report_period_limit == 0 || dfs_evt->report_time == 0 || (time_now - dfs_evt->report_time > dfs_evt->report_period_limit))) {
		lx_info("dfs_type = %d, dfs_id = 0x%x, report_cnt = %d, report_time = %llds, para_str = %s\n",
					dfs_evt->dfs_type, dfs_evt->dfs_id, dfs_evt->report_cnt, dfs_evt->report_time, dfs_evt->para_str);
		mievent_upload(dfs_evt->dfs_id, dfs_evt->para_str);
		dfs_evt->event_called = false;
		dfs_evt->report_cnt++;
		dfs_evt->report_time = ktime_get_seconds();
	}
}
static int chg_dfs_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
    struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, dfs_nb);

	mutex_lock(&chg_dfs->report_check_lock);
	if (data != NULL)
		memcpy(&dfs_evts[event].info, data, sizeof(struct dfs_info));
	dfs_evts[event].event_called = true;
	xm_chg_dfx_handle_event_report(chg_dfs, &dfs_evts[event]);
	mutex_unlock(&chg_dfs->report_check_lock);

	return NOTIFY_DONE;
}

void handle_evt_report_check_work(struct work_struct *work)
{
	int i = 0;
	int ret = 0;
	struct xm_chg_dfs *chg_dfs = container_of(work, struct xm_chg_dfs, evt_report_check_work.work);
	struct charger_manager *manager = chg_dfs->manager;
	union power_supply_propval pval;

	if (manager->batt_psy) {
		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (!ret)
			chg_dfs->battery.cycle_cnt = pval.intval;

		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
		if (!ret)
			chg_dfs->battery.chg_status = pval.intval;

		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (!ret)
			chg_dfs->battery.soc = pval.intval;

		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (!ret)
			chg_dfs->battery.vbat = pval.intval;

		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (!ret)
			chg_dfs->battery.tbat = pval.intval;
	}

	if (manager->fuel_gauge) {
		chg_dfs->battery.rsoc = fuel_gauge_get_rsoc(manager->fuel_gauge);
	}
	if (manager->master_cp_chg) {
		chargerpump_get_adc_value(manager->master_cp_chg, CP_ADC_TDIE, &chg_dfs->pump.tdie);
	}

	mutex_lock(&chg_dfs->report_check_lock);

	lx_info("cycle_cnt: %d, chg_status: %d , soc: %d, rsoc: %d, vbat: %d, tbat: %d, chg_type: %d, vbus: %d, tdie: %d\n",
		chg_dfs->battery.cycle_cnt, chg_dfs->battery.chg_status, chg_dfs->battery.soc, chg_dfs->battery.rsoc, chg_dfs->battery.vbat,
		chg_dfs->battery.tbat, manager->charger->real_type,chg_dfs->buck.vbus,chg_dfs->pump.tdie);

	for (i = 0; i < ARRAY_SIZE(dfs_evts); i++) {
		xm_chg_dfx_handle_event_report(chg_dfs, &dfs_evts[i]);
	}

	mutex_unlock(&chg_dfs->report_check_lock);

	schedule_delayed_work(&chg_dfs->evt_report_check_work, msecs_to_jiffies(chg_dfs->check_interval));
}

static int pd_tcp_notifier_dfs_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct xm_chg_dfs *chg_dfs;

	chg_dfs = container_of(nb, struct xm_chg_dfs, pd_nb);
	lx_info("PD charger event:%d %d\n", (int)event, (int)noti->pd_state.connected);

	switch(event) {
		case TCP_NOTIFY_TYPEC_STATE:
			if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
				(noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
				noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
				noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
				noti->typec_state.new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
				int i;
				lx_info("receive adapter plugin event\n");

				for (i = 0; i < ARRAY_SIZE(dfs_evts); i++) {
					dfs_evts[i].report_cnt = 0;
					dfs_evts[i].event_called = 0;
				}
				chg_dfs->check_interval = DFS_CHECK_5S_INTERVAL;
				chg_dfs->buck.adapter_plug_in = true;
			} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
				noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
				noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
				noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
				noti->typec_state.old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
				noti->typec_state.new_state == TYPEC_UNATTACHED) {
				lx_info("receive adapter plugout event\n");
				chg_dfs->check_interval = DFS_CHECK_10S_INTERVAL;
				chg_dfs->buck.adapter_plug_in = false;
			}
			break;
	}

	return 0;
}

int xm_chg_dfs_init(struct charger_manager *manager)
{
	struct xm_chg_dfs *chg_dfs = NULL;
	int ret = 0;

	chg_dfs = devm_kzalloc(manager->dev, sizeof(*chg_dfs), GFP_KERNEL);
	if (!chg_dfs)
		return -ENOMEM;

	chg_dfs->manager = manager;

	if (!manager->tcpc)
		return -ENOMEM;
	chg_dfs->pd_nb.notifier_call = pd_tcp_notifier_dfs_call;
	ret = register_tcp_dev_notifier(manager->tcpc, &chg_dfs->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		lx_err("couldn't register tcpc notifier, ret = %d\n", ret);
	}

	chg_dfs->dfs_nb.notifier_call = chg_dfs_notifier_call;
	ret = xmdfs_notifier_register(&chg_dfs->dfs_nb);
	if (ret < 0) {
		lx_err("couldn't register dfs notifier, ret = %d\n", ret);
	}

	chg_dfs->check_interval = DFS_CHECK_10S_INTERVAL;
	mutex_init(&chg_dfs->report_check_lock);
	INIT_DELAYED_WORK(&chg_dfs->evt_report_check_work, handle_evt_report_check_work);
	schedule_delayed_work(&chg_dfs->evt_report_check_work, 0);

	lx_info("success...\n");

	return 0;
}

