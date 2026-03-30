// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
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

#include "hq_charger_manager.h"
#include "xm_chg_dfs.h"
#include "hq_printk.h"
#include "hq_notify.h"
#include "hqsys_pcba.h"

#define xm_err     hq_err
#define xm_warn    hq_warn
#define xm_notice  hq_notice
#define xm_info    hq_info
#define xm_debug   hq_debug

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_DFS]"
#endif

#define CHG_DFX_EVT_DECLARE(type, pata_count, cnt_limit, period_limit) \
{\
	.dfx_type = CHG_DFX_##type, \
	.dfx_id = DFX_ID_CHG_##type, \
	.para_cnt = pata_count, \
	.report_cnt = 0, \
	.report_cnt_limit = cnt_limit, \
	.report_time = 0, \
	.report_period_limit = period_limit, \
}


static struct xm_chg_dfx_evt_cond dfx_evt_cond[] = {
	[CHG_DFX_NONE_STANDARD_CHG] = CHG_DFX_EVT_DECLARE(NONE_STANDARD_CHG, 1, 1, 0),
	[CHG_DFX_PD_AUTH_FAIL] = CHG_DFX_EVT_DECLARE(PD_AUTH_FAIL, 2, 1, 0),
	[CHG_DFX_BATT_AUTH_FAIL] = CHG_DFX_EVT_DECLARE(BATT_AUTH_FAIL, 1, 1, 0),
	[CHG_DFX_CP_EN_FAIL] = CHG_DFX_EVT_DECLARE(CP_EN_FAIL, 2, 3, 0),
	[CHG_DFX_FG_I2C_ERR] = CHG_DFX_EVT_DECLARE(FG_I2C_ERR, 2, 0, REPORT_PERIOD_600S),
	[CHG_DFX_CP_I2C_ERR] = CHG_DFX_EVT_DECLARE(CP_I2C_ERR, 2, 0, REPORT_PERIOD_600S),  //only sc8541
	[CHG_DFX_SOC_NOT_FULL] = CHG_DFX_EVT_DECLARE(SOC_NOT_FULL, 4, 1, 0),
	[CHG_DFX_CHG_BATT_CYCLE] = CHG_DFX_EVT_DECLARE(CHG_BATT_CYCLE, 2, 0, 0),
	[CHG_DFX_TBAT_HOT] = CHG_DFX_EVT_DECLARE(TBAT_HOT, 5, 0, REPORT_PERIOD_300S),
	[CHG_DFX_TBAT_COLD] = CHG_DFX_EVT_DECLARE(TBAT_COLD, 5, 0, REPORT_PERIOD_300S),
	[CHG_DFX_NOT_CHG_IN_LOW_TEMP] = CHG_DFX_EVT_DECLARE(NOT_CHG_IN_LOW_TEMP, 2, 3, 0),
	[CHG_DFX_SMART_ENDURA_TRIG] = CHG_DFX_EVT_DECLARE(SMART_ENDURA_TRIG, 2, 1, 0),
	[CHG_DFX_NOT_CHG_IN_HIGH_TEMP] = CHG_DFX_EVT_DECLARE(NOT_CHG_IN_HIGH_TEMP, 2, 3, 0),
	[CHG_DFX_CP_VBUS_OVP] = CHG_DFX_EVT_DECLARE(CP_VBUS_OVP, 1, 3, 0),  //only sc8541
	[CHG_DFX_CP_IBUS_OCP] = CHG_DFX_EVT_DECLARE(CP_IBUS_OCP, 1, 3, 0),  //only sc8541
	[CHG_DFX_CP_VBAT_OVP] = CHG_DFX_EVT_DECLARE(CP_VBAT_OVP, 3, 3, 0),  //only sc8541
	[CHG_DFX_CP_IBAT_OCP] = CHG_DFX_EVT_DECLARE(CP_IBAT_OCP, 1, 3, 0),  //only sc8541
	[CHG_DFX_CP_VAC_OVP] = CHG_DFX_EVT_DECLARE(CP_VAC_OVP, 1, 3, 0),
	[CHG_DFX_VBUS_UVLO] = CHG_DFX_EVT_DECLARE(VBUS_UVLO, 4, 1, 0),
	[CHG_DFX_CP_TDIE_HOT] = CHG_DFX_EVT_DECLARE(CP_TDIE_HOT, 2, 1, 0),
	[CHG_DFX_LPD_DETECTED] = CHG_DFX_EVT_DECLARE(LPD_DETECTED, 2, 1, 0),

};

void mievent_upload(int miev_code, char *miev_param, int para_cnt)
{
	char buffer[128] = {0};
	char *p1, *p2, *key, *value;
	int intval;
	struct misight_mievent *event = cdev_tevent_alloc(miev_code);

	memcpy(buffer, miev_param, strlen(miev_param));

	xm_info("miev_code=%d para_cnt=%d\n", miev_code, para_cnt);

	p1 = buffer;
	while (p1 && (*p1 != '\0')) {
		p2 = strsep(&p1, ",");
		while (p2 && (*p2 != '\0')) {
			key = strsep(&p2, ":");
			if (key) {
				xm_info("[CHG_DFS] key:%s\n", key);
			} else {
				xm_err("[CHG_DFS] none key\n");
				key = "None";
			}

			value = strsep(&p2, ":");
			if (value) {
				xm_info("[CHG_DFS] value:%s\n", value);
			} else {
				xm_err("[CHG_DFS] none value\n");
				value = "None";
			}
		}

		if (kstrtoint(value, 10, &intval) != 0) {
			cdev_tevent_add_str(event, key, value);
			xm_info("type=string, %s:%s\n", key, value);
		} else {
			cdev_tevent_add_int(event, key, intval);
			xm_info("type=int, %s:%d\n", key, intval);
		}
	}

	cdev_tevent_write(event);
	cdev_tevent_destroy(event);

	return;
}

static void xm_chg_dfx_handle_event_report(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfx_evt_cond *dfx_evt)
{
	char para_str[PARAMETER_LEN_MAX] = {0};
	int len = 0;

	if (IS_ERR_OR_NULL(dfx_evt) || IS_ERR_OR_NULL(chg_dfs)) {
		xm_err("dfx null pointer");
		return;
	}

	switch (dfx_evt->dfx_type) {
	case CHG_DFX_PD_AUTH_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "pdAuthFail");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%x", "adapterId", chg_dfs->adapter_svid);
		break;

	case CHG_DFX_NONE_STANDARD_CHG:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "noneStandartChg");
		break;

	case CHG_DFX_FG_I2C_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "fgI2cErr");
		break;

	case CHG_DFX_CP_EN_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpEnFail");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cpId", 0);
		break;

	case CHG_DFX_CP_VAC_OVP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVacOVP");
		break;

	// case CHG_DFX_BATT_LINKER_ABSENT:
	// len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattLinkerAbsent");
	// break;

	case CHG_DFX_NOT_CHG_IN_LOW_TEMP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInLowTemp");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		break;

	case CHG_DFX_NOT_CHG_IN_HIGH_TEMP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInHighTemp");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		break;

	case CHG_DFX_CP_I2C_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpI2CErr");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "masterOk", chg_dfs->master_ok);
		//len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d","slaveOk", dfx_data_p->data_cp.slave_ok);
		break;

	case CHG_DFX_CHG_BATT_CYCLE:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "chgBattCycle");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cycleCnt", chg_dfs->cycle_cnt);
		break;

	case CHG_DFX_CP_VBAT_OVP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbatOVP");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbatMax", chg_dfs->vbat_max);
		break;

	case CHG_DFX_CP_IBAT_OCP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbatOcp");
		break;

	case CHG_DFX_CP_VBUS_OVP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbusOVP");
		break;

	case CHG_DFX_CP_IBUS_OCP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbusOcp");
		break;

	case CHG_DFX_SMART_ENDURA_TRIG:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "SmartEnduraTrig");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		break;

	case CHG_DFX_SOC_NOT_FULL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SocNotFull");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "rsoc", chg_dfs->rsoc);
		break;

	case CHG_DFX_BATT_AUTH_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattAuthFail");
		break;

	case CHG_DFX_CP_TDIE_HOT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "CpTdieHot");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "Tdie_Temp", chg_dfs->tdie_temp);
		// len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "slaveTdie", chg_dfs->slave_tdie);
		break;

	case CHG_DFX_TBAT_HOT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatHot");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMax", (chg_dfs->tbat_max / 10));
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->adapter_plug_in);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tboard", chg_dfs->tboard);
		break;

	case CHG_DFX_TBAT_COLD:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatCold");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMin", (chg_dfs->tbat_min / 10));
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->adapter_plug_in);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tboard", chg_dfs->tboard);
		break;

	case CHG_DFX_LPD_DETECTED:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "LpdDetected");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "lpdFlag", chg_dfs->lpd_flag);
		break;

	case CHG_DFX_VBUS_UVLO:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "VbusUvlo");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbus", chg_dfs->vbus);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "aiclTh", chg_dfs->aicl_threshold);
		break;

	default:
		xm_err("[HQ_CHG_DFS]: unknown type to report\n");
		return;
	}

	dfx_evt->report_cnt++;
	dfx_evt->report_time = ktime_get_seconds();

	xm_info("dfx_type = %d, dfx_id = %d, para_cnt = %d, report_cnt = %d, report_time = %llds \n",
			dfx_evt->dfx_type, dfx_evt->dfx_id, dfx_evt->para_cnt, dfx_evt->report_cnt, dfx_evt->report_time);
	mievent_upload(dfx_evt->dfx_id, para_str, dfx_evt->para_cnt);
	return;
}


static int xm_chg_dfx_report_events(struct xm_chg_dfs *chg_dfs)
{
	int evt_index = 0;

	xm_info("evt_report_bits = 0x%X\n", chg_dfs->evt_report_bits[0]);

	for_each_set_bit(evt_index, chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE) {
		xm_chg_dfx_handle_event_report(chg_dfs, &dfx_evt_cond[evt_index]);
		clear_bit(evt_index, chg_dfs->evt_report_bits);
	}

	return 0;
}

static void wake_up_report(struct xm_chg_dfs *chg_dfs)
{
	if (!atomic_cmpxchg(&chg_dfs->run_report, 0, 1)) {
		wake_up_interruptible(&chg_dfs->report_wq);
	}
}

static int xm_chg_dfx_report_thread_fn(void *data)
{
	struct xm_chg_dfs *chg_dfs = data;

	while (!kthread_should_stop()) {
		wait_event_interruptible(chg_dfs->report_wq,
			(atomic_read(&chg_dfs->run_report) || kthread_should_stop()));

		atomic_set(&chg_dfs->run_report, 0);

		xm_chg_dfx_report_events(chg_dfs);
	}

	return 0;
}

static int chg_dfs_psy_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, psy_nb);
	struct power_supply *psy = data;
	union power_supply_propval pval;
	struct charger_manager *manager = power_supply_get_drvdata(psy);

	int ret = 0;
	bool verifed = false;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	chg_dfs->adapter_svid = chg_dfs->pd_adapter->adapter_svid;
	ret = adapter_get_usbpd_verifed(chg_dfs->pd_adapter, &verifed);
	if (ret < 0) {
		hq_err("Couldn't get usbpd verifed ret=%d\n", ret);
	}

	chg_dfs->pd_verify_done = chg_dfs->pd_adapter->verify_done;
	chg_dfs->pd_auth_fail = !verifed;

	if (strcmp(psy->desc->name, "usb") == 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->vbus = pval.intval;
	}

	if (strcmp(psy->desc->name, "battery") == 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->cycle_cnt = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->tbat = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_AUTHENTIC, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->batt_auth = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->soc = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->chip_chg_status = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->vbat = pval.intval;
		chg_dfs->vbus_type = manager->vbus_type;
		chg_dfs->rsoc = manager->rsoc;
		chg_dfs->tboard = manager->board_temp;
		chg_dfs->tbat_max = max(chg_dfs->tbat, chg_dfs->tbat_max);
		chg_dfs->tbat_min = min(chg_dfs->tbat, chg_dfs->tbat_min);
	}

	if (strcmp(psy->desc->name, "sc-cp-master") == 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->tdie_temp = pval.intval;
	}

	return NOTIFY_DONE;
}

static int chg_cp_charger_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, cp_nb);

	switch (event) {
	//case CHARGER_DEV_NOTIFY_IBATOCP:
	//xm_info("cp master ibat ocp triggered\n");
	//chg_dfs->cp_ibat_ocp = true;
	//break;

	case CHARGER_DEV_NOTIFY_IBUSOCP:
		xm_info("cp master ibus ocp triggered\n");
		chg_dfs->cp_ibus_ocp = true;
		break;

	case CHARGER_DEV_NOTIFY_VBAT_OVP:
		xm_info("cp master vbat ovp triggered\n");
		chg_dfs->cp_vbat_ovp = true;
		break;

	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		xm_info("cp master vbus ovp triggered\n");
		chg_dfs->cp_vbus_ovp = true;
		break;

	case CHARGER_DEV_NOTIFY_VAC_OVP:
		xm_info("cp master vac ovp triggered\n");
		chg_dfs->cp_vac_ovp = true;
		break;

	case CHARGER_DEV_NOTIFY_TDIE_FLT:
		xm_info("cp master tdie fault triggered\n");
		chg_dfs->cp_tdie_flt = true;
		break;

	default:
		xm_info("receive event not support: %lu\n", event);
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

static int chg_dfs_fg_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	// struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, fg_nb);
	// switch (event){
	// 	case CHG_DFX_FG_I2C_ERR:
	// }
	return NOTIFY_DONE;
}

static int chg_dfs_cm_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, cm_nb);
	int i = 0;
	struct xm_chg_dfx_evt_cond *cond = NULL;

	switch (event) {
	case CHG_FW_EVT_ADAPTER_PLUGOUT: /* adapter plugout */
		xm_info("receive adapter plugout event\n");

		chg_dfs->check_interval_ms = DFS_CHECK_10S_INTERVAL;
		chg_dfs->adapter_plug_in = 0;
		chg_dfs->cp_ibat_ocp = 0;
		chg_dfs->cp_ibus_ocp = 0;
		chg_dfs->cp_vbat_ovp = 0;
		chg_dfs->cp_vbus_ovp = 0;
		chg_dfs->cp_vac_ovp = 0;
		chg_dfs->cp_enbale_fail = 0;
		chg_dfs->cp_tdie_flt = 0;
		chg_dfs->vbus_uvlo = 0;
		break;

	case CHG_FW_EVT_ADAPTER_PLUGIN: /* adapter plugin */
		xm_info("receive adapter plugin event\n");

		for (i = 0; i < ARRAY_SIZE(dfx_evt_cond); i++) {
			cond = &dfx_evt_cond[i];

			if (cond->dfx_type != CHG_DFX_CHG_BATT_CYCLE)
				cond->report_cnt = 0;
		}

		chg_dfs->check_interval_ms = DFS_CHECK_5S_INTERVAL;
		chg_dfs->adapter_plug_in = 1;
		break;

	case CHG_FW_EVT_SMART_ENDURA_TRIG:
		xm_info("receive smart endura trigger event, data = %d\n", *(bool *)data);
		chg_dfs->smart_endura_trig = (*(bool *)data);
		break;

	case CHG_FW_EVT_LPD:
		xm_info("receive lpd event, data = %d\n", *(bool *)data);
		chg_dfs->lpd_flag = (*(bool *)data);
		break;

	case CHG_FW_EVT_CP_EN:
		xm_info("receive charger_enable event, data = %d\n", (*(bool *)data));
		chg_dfs->cp_enbale_fail = !(*(bool *)data);
		break;

	case CHG_FW_EVT_VBUS_UVLO:
		xm_info("receive VBUS_UVLO event");
		chg_dfs->vbus_uvlo = true;
		break;

	case CHG_FW_EVT_IBAT_OCP:
		xm_info("receive cp ibat ocp event\n");
		chg_dfs->cp_ibat_ocp = true;
		break;

	default:
		xm_info("receive event not support: %lu\n", event);
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

void handle_evt_cond_check_work(struct work_struct *work)
{
	struct xm_chg_dfs *chg_dfs = container_of(work, struct xm_chg_dfs, evt_cond_check_work.work);
	int i = 0;
	struct xm_chg_dfx_evt_cond *cond = NULL;
	time64_t time_now;

	mutex_lock(&chg_dfs->cond_check_lock);
	time_now = ktime_get_seconds();

	if (IS_ERR_OR_NULL(chg_dfs->cp_charger)) {
		chg_dfs->master_ok = 0;
	} else {
		chargerpump_get_chip_ok(chg_dfs->cp_charger, &chg_dfs->master_ok);
	}

	chg_dfs->fg_status = fuel_gauge_check_fg_status(chg_dfs->fuel_gauge);

	xm_info("cycle_cnt: %d, tbat: %d [%d %d], vbus: %d, tdie_temp: %d, tboard: %d, batt_auth: %d, soc: %d, rsoc: %d, vbat: %d, chip_chg_status: %d, adapter_svid: 0x%x, vbus_type: %d, fg_status: %d, cp_chip_ok: %d, pd_auth_fail: %d, pd_verify_done: %d\n",
		chg_dfs->cycle_cnt, chg_dfs->tbat, chg_dfs->tbat_min, chg_dfs->tbat_max, chg_dfs->vbus, chg_dfs->tdie_temp, chg_dfs->tboard,
		chg_dfs->batt_auth, chg_dfs->soc, chg_dfs->rsoc, chg_dfs->vbat, chg_dfs->chip_chg_status, chg_dfs->adapter_svid, chg_dfs->vbus_type, chg_dfs->fg_status, chg_dfs->master_ok, chg_dfs->pd_auth_fail, chg_dfs->pd_verify_done);


	for (i = 0; i < ARRAY_SIZE(dfx_evt_cond); i++) {
		cond = &dfx_evt_cond[i];

		switch (cond->dfx_type) {
		case CHG_DFX_NONE_STANDARD_CHG:
			if (chg_dfs->vbus_type == VBUS_TYPE_FLOAT) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_NONE_STANDARD_CHG, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_NONE_STANDARD_CHG, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_PD_AUTH_FAIL:
			if (chg_dfs->adapter_svid == USB_PD_MI_SVID) {
				if (chg_dfs->pd_auth_fail && chg_dfs->pd_verify_done) {
					if (cond->report_cnt < cond->report_cnt_limit) {
						set_bit(CHG_DFX_PD_AUTH_FAIL, chg_dfs->evt_report_bits);
					} else {
						//clear_bit(CHG_DFX_PD_AUTH_FAIL, chg_dfs->evt_report_bits);
					}
				}
			}
			break;

		case CHG_DFX_LPD_DETECTED:
			if (chg_dfs->lpd_flag) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_LPD_DETECTED, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_LPD_DETECTED, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_VAC_OVP:
			if (chg_dfs->cp_vac_ovp) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_CP_VAC_OVP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_VAC_OVP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_TDIE_HOT:
			if (chg_dfs->cp_tdie_flt && chg_dfs->adapter_plug_in) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_CP_TDIE_HOT, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_TDIE_HOT, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_EN_FAIL:
			if (chg_dfs->cp_enbale_fail) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_CP_EN_FAIL, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_EN_FAIL, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_FG_I2C_ERR:
			if (chg_dfs->fg_status & FG_EER_I2C_FAIL) {
				if ((cond->report_time == 0) || (time_now - cond->report_time > cond->report_period_limit)) {
					set_bit(CHG_DFX_FG_I2C_ERR, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_FG_I2C_ERR, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_I2C_ERR:
			if (!chg_dfs->master_ok) {
				if ((cond->report_time == 0) || (time_now - cond->report_time > cond->report_period_limit)) {
					set_bit(CHG_DFX_CP_I2C_ERR, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_I2C_ERR, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_NOT_CHG_IN_LOW_TEMP:
			if (chg_dfs->tbat < 50 && chg_dfs->tbat > -100 && chg_dfs->adapter_plug_in) {
				if (chg_dfs->chip_chg_status == POWER_SUPPLY_STATUS_DISCHARGING || chg_dfs->chip_chg_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
					if (cond->report_cnt < cond->report_cnt_limit) {
						set_bit(CHG_DFX_NOT_CHG_IN_LOW_TEMP, chg_dfs->evt_report_bits);
					}
				} else {
					//clear_bit(CHG_DFX_NOT_CHG_IN_LOW_TEMP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_NOT_CHG_IN_HIGH_TEMP:
			if (chg_dfs->tbat < 550 && chg_dfs->tbat > 480 && chg_dfs->adapter_plug_in) {
				if (chg_dfs->chip_chg_status == POWER_SUPPLY_STATUS_DISCHARGING || chg_dfs->chip_chg_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
					if (cond->report_cnt < cond->report_cnt_limit) {
						set_bit(CHG_DFX_NOT_CHG_IN_HIGH_TEMP, chg_dfs->evt_report_bits);
					}
				} else {
					//clear_bit(CHG_DFX_NOT_CHG_IN_HIGH_TEMP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CHG_BATT_CYCLE:
			if (chg_dfs->cycle_cnt != chg_dfs->last_cycle_count) {
				if ((chg_dfs->cycle_cnt % 100) == 0 && chg_dfs->cycle_cnt != 0) {
					set_bit(CHG_DFX_CHG_BATT_CYCLE, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CHG_BATT_CYCLE, chg_dfs->evt_report_bits);
				}
				chg_dfs->last_cycle_count = chg_dfs->cycle_cnt;
			}
			break;

		case CHG_DFX_SOC_NOT_FULL:
			if ((chg_dfs->chip_chg_status == POWER_SUPPLY_STATUS_FULL) && (chg_dfs->soc != 100)) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_SOC_NOT_FULL, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_SOC_NOT_FULL, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_BATT_AUTH_FAIL:
			if (chg_dfs->fg_status & FG_ERR_AUTH_FAIL || chg_dfs->batt_auth == 0) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_BATT_AUTH_FAIL, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_BATT_AUTH_FAIL, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_TBAT_HOT:
			if (chg_dfs->tbat > 550) {
				if ((cond->report_time == 0) || (time_now - cond->report_time > cond->report_period_limit)) {
					set_bit(CHG_DFX_TBAT_HOT, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_TBAT_HOT, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_TBAT_COLD:
			if (chg_dfs->tbat < -100) {
				if ((cond->report_time == 0) || (time_now - cond->report_time > cond->report_period_limit)) {
					set_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_VBUS_OVP:
			if (chg_dfs->cp_vbus_ovp) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_CP_VBUS_OVP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_VBUS_OVP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_SMART_ENDURA_TRIG:
			if (chg_dfs->smart_endura_trig && chg_dfs->adapter_plug_in) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_SMART_ENDURA_TRIG, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_SMART_ENDURA_TRIG, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_IBUS_OCP:
			if (chg_dfs->cp_ibus_ocp) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_CP_IBUS_OCP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_IBUS_OCP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_VBAT_OVP:
			if (chg_dfs->cp_vbat_ovp) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_CP_VBAT_OVP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_VBAT_OVP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_IBAT_OCP:
			if (chg_dfs->cp_ibat_ocp) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_CP_IBAT_OCP, chg_dfs->evt_report_bits);
					chg_dfs->cp_ibat_ocp = false;
				} else {
					//clear_bit(CHG_DFX_CP_IBAT_OCP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_VBUS_UVLO:
			if (chg_dfs->vbus_uvlo) {
				if (cond->report_cnt < cond->report_cnt_limit) {
					set_bit(CHG_DFX_VBUS_UVLO, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_VBUS_UVLO, chg_dfs->evt_report_bits);
				}
				chg_dfs->vbus_uvlo = false;
			}
			break;

		case CHG_DFX_BATT_LINKER_ABSENT:
		case CHG_DFX_DEFAULT_TYPE:
			break;

		default:
			xm_err("[HQ_CHG_DFS]: unknown type to report\n");
		}
	}

	if (bitmap_weight(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE) > 0) {
		xm_info("%d dfx event need report\n", bitmap_weight(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE));
		wake_up_report(chg_dfs);
	}

	mutex_unlock(&chg_dfs->cond_check_lock);

	schedule_delayed_work(&chg_dfs->evt_cond_check_work, msecs_to_jiffies(chg_dfs->check_interval_ms));
}

static void xm_chg_dfs_pcba_init(struct xm_chg_dfs *chg_dfs)
{
	struct PCBA_MSG *pcba_msg = get_pcba_msg();

	chg_dfs->board_version = OTHER_VERSION;
	if (!IS_ERR_OR_NULL(pcba_msg->sku)) {
		if ((strstr(pcba_msg->sku, "eea")))
			chg_dfs->board_version = EEA_VERSION;
		xm_info("board version = %d\n", chg_dfs->board_version);
	} else {
		xm_err("get board version fail\n");
	}
}

static int xm_chg_dfs_probe(struct platform_device *pdev)
{
	struct xm_chg_dfs *chg_dfs = NULL;
	int ret = 0;

	chg_dfs = devm_kzalloc(&pdev->dev, sizeof(*chg_dfs), GFP_KERNEL);
	if (!chg_dfs)
		return -ENOMEM;

	platform_set_drvdata(pdev, chg_dfs);

	init_waitqueue_head(&chg_dfs->report_wq);

	chg_dfs->report_thread = kthread_run(xm_chg_dfx_report_thread_fn, chg_dfs,
							"xm_chg_dfx_report_thread");

	chg_dfs->cp_charger = chargerpump_find_dev_by_name("master_cp_chg");
	if (!chg_dfs->cp_charger) {
		chg_dfs->master_ok = 0;
	}
	chg_dfs->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!chg_dfs->pd_adapter) {
		return -EPROBE_DEFER;
	}
	chg_dfs->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!chg_dfs->fuel_gauge) {
		return -EPROBE_DEFER;
	}

	chg_dfs->last_cycle_count = -1;
	chg_dfs->check_interval_ms = 10000;
	chg_dfs->tbat_max = INT_MIN;
	chg_dfs->tbat_min = INT_MAX;
	chg_dfs->tboard = 0;
	chg_dfs->aicl_threshold = 4000;
	chg_dfs->adapter_svid = 0x0;
	chg_dfs->vbat_max = INT_MIN;
	chg_dfs->pd_auth_fail = false;
	chg_dfs->pd_verify_done = false;
	chg_dfs->batt_auth = false;

	xm_chg_dfs_pcba_init(chg_dfs);

	mutex_init(&chg_dfs->cond_check_lock);
	bitmap_zero(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE);
	atomic_set(&chg_dfs->run_report, 0);
	INIT_DELAYED_WORK(&chg_dfs->evt_cond_check_work, handle_evt_cond_check_work);
	schedule_delayed_work(&chg_dfs->evt_cond_check_work, msecs_to_jiffies(60000));

	chg_dfs->psy_nb.notifier_call = chg_dfs_psy_notifier_call;
	ret = power_supply_reg_notifier(&chg_dfs->psy_nb);
	if (ret < 0) {
		xm_err("couldn't register psy notifier ret = %d\n", ret);
		return ret;
	}

	chg_dfs->cp_nb.notifier_call = chg_cp_charger_notifier_call;
	ret = hq_chargerpump_notifier_register(&chg_dfs->cp_nb);
	if (ret < 0) {
		xm_err("couldn't register charger notifier, ret = %d\n", ret);
		return ret;
	}

	chg_dfs->fg_nb.notifier_call = chg_dfs_fg_notifier_call;
	ret = hq_fg_notifier_register(&chg_dfs->fg_nb);
	if (ret < 0) {
		xm_err("couldn't register fg notifier, ret = %d\n", ret);
		return ret;
	}

	chg_dfs->cm_nb.notifier_call = chg_dfs_cm_notifier_call;
	ret = hq_chargermanager_notifier_register(&chg_dfs->cm_nb);
	if (ret < 0) {
		xm_err("couldn't register charger manager notifier, ret = %d\n", ret);
		return ret;
	}

	chg_dfs->charger = charger_find_dev_by_name("primary_chg");
	if (!chg_dfs->charger) {
		xm_err("failed to find primary_chg device\n");
		return -EPROBE_DEFER;
	}

	xm_info("success...\n");

	return 0;
}

static int xm_chg_dfs_remove(struct platform_device *pdev)
{
	struct xm_chg_dfs *chg_dfs = NULL;

	chg_dfs = platform_get_drvdata(pdev);
	if (!chg_dfs)
		return 0;

	power_supply_unreg_notifier(&chg_dfs->psy_nb);
	hq_chargerpump_notifier_unregister(&chg_dfs->cp_nb);
	hq_chargermanager_notifier_unregister(&chg_dfs->cm_nb);
	hq_fg_notifier_unregister(&chg_dfs->fg_nb);

	return 0;
}

static const struct of_device_id xm_chg_dfs_of_match[] = {
	{ .compatible = "xiaomi,xm-chg-dfs", },
	{ },
};
MODULE_DEVICE_TABLE(of, xm_chg_dfs_of_match);

static struct platform_driver xm_chg_dfs_driver = {
	.probe = xm_chg_dfs_probe,
	.remove = xm_chg_dfs_remove,
	.driver = {
		.name = "xm_chg_dfs",
		.of_match_table = of_match_ptr(xm_chg_dfs_of_match),
	},
};
module_platform_driver(xm_chg_dfs_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Xiaomi Charge DFS Driver");
MODULE_AUTHOR("pengyuzhe@huaqin.com");
