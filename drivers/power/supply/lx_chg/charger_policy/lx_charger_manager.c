// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 LiXun Technology(Shanghai) Co., Ltd.
 */


#include "../charger_class/lx_charger_class.h"
#include "../charger_class/lx_cp_class.h"
#include "../charger_class/lx_fg_class.h"
#include "lx_voter.h"
#include "lx_jeita.h"
#include "lx_cp_policy.h"

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "../charger_class/xm_adapter_class.h"
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include <linux/notifier.h>
#include "../../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#endif

#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
#include "../fuelgauge/lx_fuel_algorithm.h"
#endif

#define CHARGER_MANAGER_VERSION            "1.1.1"

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_core.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_typec.h"
#endif

#include "../battery_secrete/battery_auth_class.h"

#include "lx_charger_manager.h"
#include "lx_printk.h"
#include <linux/syscore_ops.h>
#include <linux/hardware_info.h>
#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG_CM]"
#endif
#include "xm_smart_chg.h"
#if IS_ENABLED(CONFIG_MIEV)    
#include "xm_chg_dfs.h"
#endif

static struct charger_manager *g_manager = NULL;

static const char *bc12_result[] = {
	"None",
	"SDP",
	"CDP",
	"DCP",
	"FLOAT",
	"Non-Stand",
	"QC",
	"QC3",
	"QC3+",
	"PD",
	"PPS",
};

const static char * adc_name[] = {
	"VBUS","VSYS","VBAT","VAC","IBUS","IBAT","TSBUS","TSBAT","TDIE",
};

#define CHG_UEVENT_MAX_LENGHT (64)

static char *xm_chg_uevent_prefix_str[] = {
	[CHG_UEVENT_DEFAULT_TYPE]         = "CHG_UEVENT_DEFAULT_TYPE",
	[CHG_UEVENT_SOC_DECIMAL]          = "POWER_SUPPLY_SOC_DECIMAL=",
	[CHG_UEVENT_SOC_DECIMAL_RATE]     = "POWER_SUPPLY_SOC_DECIMAL_RATE=",
	[CHG_UEVENT_QUICK_CHARGE_TYPE]    = "POWER_SUPPLY_QUICK_CHARGE_TYPE=",
	[CHG_UEVENT_SHUTDOWN_DELAY]       = "POWER_SUPPLY_SHUTDOWN_DELAY=",
	[CHG_UEVENT_CONNECTOR_TEMP]       = "POWER_SUPPLY_CONNECTOR_TEMP=",
	[CHG_UEVENT_NTC_ALARM]            = "POWER_SUPPLY_NTC_ALARM=",
	[CHG_UEVENT_LPD_DETECTION]        = "POWER_SUPPLY_MOISTURE_DET_STS=",
	[CHG_UEVENT_REVERSE_QUICK_CHARGE] = "POWER_SUPPLY_REVERSE_QUICK_CHARGE=",
	[CHG_UEVENT_CC_SHORT_VBUS]        = "POWER_SUPPLY_CC_SHORT_VBUS",
};



/* TODO: change "int event_value" to union type to support more payload type */
static int do_charge_uevent_report(struct charger_manager *manager, int event_type, int event_value)
{
	char uevent_str[CHG_UEVENT_MAX_LENGHT] = {0};
	char *envp[2] = {
		uevent_str,
		NULL,
	};

	if (!manager) {
		return -EFAULT;
	}

	mutex_lock(&manager->report_lock);

	switch (event_type) {
	/* with out payload */
	case CHG_UEVENT_DEFAULT_TYPE:
		snprintf(uevent_str, sizeof(uevent_str), "%s",
			xm_chg_uevent_prefix_str[event_type]);
		break;

	/* payload with integer type */
	case CHG_UEVENT_LPD_DETECTION:
		lx_info("LPD limit!!\n");
		if(event_value){
			vote(manager->total_fcc_votable, LPD_DECTEED_VOTER, true,1500);
		} else {
			vote(manager->total_fcc_votable, LPD_DECTEED_VOTER, false,0);
		}
	case CHG_UEVENT_SOC_DECIMAL:
	case CHG_UEVENT_SOC_DECIMAL_RATE:
	case CHG_UEVENT_QUICK_CHARGE_TYPE:
	case CHG_UEVENT_SHUTDOWN_DELAY:
	case CHG_UEVENT_CONNECTOR_TEMP:
	case CHG_UEVENT_NTC_ALARM:
	case CHG_UEVENT_REVERSE_QUICK_CHARGE:
		snprintf(uevent_str, sizeof(uevent_str), "%s%d",
			xm_chg_uevent_prefix_str[event_type], event_value);
		break;
	case CHG_UEVENT_CC_SHORT_VBUS:
		snprintf(uevent_str, sizeof(uevent_str), "%s",
			xm_chg_uevent_prefix_str[event_type]);
		break;

	default:
		lx_info("event_type %d not support\n", event_type);
		mutex_unlock(&manager->report_lock);
		return -EINVAL;
	}

	kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);

	lx_info("event_type:%d, event_value:%d, uevent_str:%s\n",
		event_type, event_value, uevent_str);

	mutex_unlock(&manager->report_lock);

	return 0;
}

/**
 * xm_charge_uevent_report() - Report Xiaomi charge specific uevent to userspace
 * @event_type: Specific uevent type, reference to enum xm_chg_uevent_type
 * @event_value: Payload of uevent with integer type
 *
 * Return: On success return zero, negative integer otherwise
 */
int xm_charge_uevent_report(int event_type, int event_value)
{
	int ret = 0;

	if (!g_manager)
		return -EFAULT;

	ret = do_charge_uevent_report(g_manager, event_type, event_value);
	if (ret != 0) {
		lx_err("fail to report charge uevent\n");
	}

	return ret;
}
EXPORT_SYMBOL_GPL(xm_charge_uevent_report);

static int charger_manager_wake_thread(struct charger_manager *manager)
{
	manager->run_thread = true;
	wake_up(&manager->wait_queue);
	return 0;
}

#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
extern int lx_fuel_algo_init(struct charger_manager *manager);
#endif


static void hrtime_otg_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, hrtime_otg_work.work);
	if(manager != NULL && manager->tcpc != NULL)
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_SNK);
	lx_info("hrtime_otg_work enter\n");
}

static enum alarmtimer_restart rust_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, rust_det_work_timer);
	if(manager != NULL)
	{
		manager->ui_cc_toggle = false;
		schedule_delayed_work(&manager->hrtime_otg_work, 0);
	}
	lx_info("rust_det_work_timer_handler enter\n");
    return ALARMTIMER_NORESTART;
}

static bool judge_cell_status(struct charger_manager *manager)
{
	int c_car_offset = 0;
	int v_car_offset = 0;
	static int count;

	if (is_input_suspend(manager)) {
		lx_info("[Detcell] input_suspend enable, skip Detcell\n");
		return false;
	}

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		lx_info("get fuel_gauge is fail, skip Detcell\n");
		return false;
	}

	manager->c_car_out = fuel_gauge_get_c_car(manager->fuel_gauge);
	manager->v_car_out = fuel_gauge_get_v_car(manager->fuel_gauge);
	lx_info("[Detcell] c_car_out = %d, v_car_out = %d\n", manager->c_car_out, manager->v_car_out);

	c_car_offset = abs(manager->c_car_out - manager->c_car_in);
	v_car_offset = abs(manager->v_car_out - manager->v_car_in);
	lx_info("[Detcell] c_car_offset = %d, v_car_offset = %d\n", c_car_offset, v_car_offset);

	//18 fail，decrease to 17
	if ( v_car_offset  * 10 >= c_car_offset * 17) { //6*60 s
		if (count >= 1) {
			lx_info("[Detcell] cell is broken!\n");
			manager->half_cell = 2;
			return true;
		} else {
			count++;
			lx_info("[Detcell] Detcell count = %d\n", count);
			manager->half_cell = 1;
			return false;
		}
	} else {
		lx_info("[Detcell] Detcell cell is ok\n");
		manager->half_cell = 1;
		count = 0;
		return false;
	}

}

static enum alarmtimer_restart cell_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, cell_det_work_timer);
	if(manager != NULL)
	{
		manager->en_single_cell = judge_cell_status(manager);
		lx_info("[Detcell] cell_det_work_timer_handler enter\n");
	}

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart start_cell_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, start_cell_det_work_timer);
	if(manager != NULL && !IS_ERR_OR_NULL(manager->fuel_gauge))
	{
		lx_info("[Detcell] cell_det_work_timer_handler enter\n");

		manager->v_car_in = fuel_gauge_get_v_car(manager->fuel_gauge);
		if (manager->tbat < 0 || manager->v_car_in <= 0) {
			lx_info("[Detcell] lowtemp or vcar error exit cell_det_work_timer\n");
			return ALARMTIMER_NORESTART;
		} else {
			manager->c_car_in = fuel_gauge_get_c_car(manager->fuel_gauge);
			lx_info("[Detcell] c_car_in = %d, v_car_in = %d\n", manager->c_car_in, manager->v_car_in);

			ret = alarm_try_to_cancel(&manager->cell_det_work_timer);
			if (ret < 0) {
				lx_err("[Detcell] callback was running, skip timer\n");
			}
			ktime_now = ktime_get_boottime();
			time_now = ktime_to_timespec64(ktime_now);
			end_time.tv_sec = time_now.tv_sec + 600;
			end_time.tv_nsec = time_now.tv_nsec + 0;
			ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
			alarm_start(&manager->cell_det_work_timer, ktime);
		}
	}
	return ALARMTIMER_NORESTART;
}

int charger_manager_get_current(struct charger_manager *manager, int *curr)
{
	int val;
	int ret = 0;
	union power_supply_propval pval;

	*curr = 0;

	ret = charger_get_adc(manager->charger, CHG_ADC_IBUS, &val);
	if (ret < 0) {
		lx_err("Couldn't read input curr ret=%d\n", ret);
	} else
		*curr += val;

	if(manager->cp_master_psy){

		ret = power_supply_get_property(manager->cp_master_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			lx_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}
	if(manager->cp_slave_psy){
		ret = power_supply_get_property(manager->cp_slave_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			lx_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}
	return 0;
}
EXPORT_SYMBOL(charger_manager_get_current);

void lx_set_prop_system_temp_level(struct charger_manager *manager, char *voter_name)
{
	int rc;
	int thermal_level = 0;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	int tbat = 250;
	union power_supply_propval pval = {0,};
	struct timespec64 ts_boot;
	struct timespec64 ts_shiled;
	struct timespec64 ts;
#endif

	/* keep system_temp_level as last thermal level */
	thermal_level = manager->system_temp_level;

#ifdef KERNEL_FACTORY_BUILD
	/* force disable thermal policy in factory build */
	manager->thermal_enable = false;
#endif //KERNEL_FACTORY_BUILD

	if (manager->thermal_enable == false) {
		lx_err("thermal ibat limit is disable\n");
		return;
	}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	ktime_get_boottime_ts64(&ts);
	if ((u64)ts.tv_sec < 60) {
		lx_info("[zero_speed_bootup] ignore thermal...\n");
		manager->system_temp_level = 0;
	}
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	/*
	 * force thermal level to zero within 60s of kernel bootup.
	 * battery temperature limitation to avoid trigger
	 * hot temperature shutdown when bootup.
	 */

	rc = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0)
		lx_err("get battery temperature error.\n");
	else
		tbat = pval.intval;

	ktime_get_boottime_ts64(&ts_boot);
	ts_shiled.tv_sec = 60;
	ts_shiled.tv_nsec = 0;

	if ((timespec64_compare(&ts_boot, &ts_shiled) == -1) && (tbat < 450)) {
		lx_info("system_temp_level = %d, tbat = %d, force thermal level to zero.\n",
			manager->system_temp_level, tbat);
		thermal_level = 0;
	}
#endif

	if (thermal_level < 0)
		goto err;

	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE || !strcmp(voter_name, CALL_THERMAL_DAEMON_VOTER)) {
		if (manager->thermal_parse_flags & PD_THERM_PARSE_ERROR) {
			lx_err("pd thermal dtsi parse error\n");
			goto err;
		}

		if (thermal_level > manager->pd_thermal_levels) {
			lx_err("thermal level is invalid\n");
			goto err;
		}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		if (manager->pps_fast_mode && (manager->low_fast_ffc >= 2150)) {
			vote(manager->total_fcc_votable, voter_name, true, manager->low_fast_ffc);
		} else {
			vote(manager->total_fcc_votable, voter_name, true,
				manager->pd_thermal_mitigation[thermal_level]);
		}
#else
		vote(manager->total_fcc_votable, voter_name, true,
			manager->pd_thermal_mitigation[thermal_level]);
#endif

	} else {
		if (manager->thermal_parse_flags & QC2_THERM_PARSE_ERROR) {
			lx_err("qc thermal dtsi parse error\n");
			goto err;
		}

		if (thermal_level > manager->qc2_thermal_levels) {
			lx_err("thermal level is invalid\n");
			goto err;
		}

		vote(manager->total_fcc_votable, voter_name, true,
			manager->qc2_thermal_mitigation[thermal_level]);
	}

	rc = get_client_vote_locked(manager->total_fcc_votable, voter_name);
	lx_info("%s: thermal vote susessful val = %d, current = %d\n", voter_name, thermal_level, rc);

	return;
err:
	vote(manager->total_fcc_votable, voter_name, false, 0);
	return;
}
EXPORT_SYMBOL(lx_set_prop_system_temp_level);

static int charge_manager_thermal_init(struct charger_manager *manager)
{
	int byte_len, rc, ret = 0;
	struct device_node *node = manager->dev->of_node;

	manager->thermal_enable = of_property_read_bool(node, "lx,thermal-enable");

	if (of_find_property(node, "lx,pd-thermal-mitigation", &byte_len)) {
		manager->pd_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation)) {
			ret |= PD_THERM_PARSE_ERROR;
			lx_err("pd_thermal_mitigation kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,pd-thermal-mitigation",
				manager->pd_thermal_mitigation, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				lx_err("pd_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		lx_err("pd_thermal_mitigation not found\n");
	}

	if (of_find_property(node, "lx,qc2-thermal-mitigation", &byte_len)) {
		manager->qc2_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->qc2_thermal_mitigation)) {
			ret |= QC2_THERM_PARSE_ERROR;
			lx_err("qc2_thermal_mitigation kzalloc error\n");
		} else {
			manager->qc2_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,qc2-thermal-mitigation",
				manager->qc2_thermal_mitigation, manager->qc2_thermal_levels);
			if (rc < 0) {
				ret |= QC2_THERM_PARSE_ERROR;
				lx_err("qc2_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= QC2_THERM_PARSE_ERROR;
		lx_err("qc2_thermal_mitigation not found\n");
	}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        if (of_find_property(node, "lx,pd-thermal-mitigation-fast", &byte_len)) {
		manager->pd_thermal_mitigation_fast = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation_fast)) {
			ret |= PD_THERM_PARSE_ERROR;
			lx_err("pd_thermal_mitigation_fast kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,pd-thermal-mitigation-fast",
				manager->pd_thermal_mitigation_fast, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				lx_err("pd_thermal_mitigation_fast parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		lx_err("pd_thermal_mitigation_fast not found\n");
	}
#endif
	manager->thermal_parse_flags = ret;
	if (ret == (QC2_THERM_PARSE_ERROR | PD_THERM_PARSE_ERROR)) {
		manager->thermal_enable = false;
		ret = -EINVAL;
	}

	return ret;
}

static int main_chg_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		lx_err("the value of main fcc is error.\n");
		return value;
	}

	ret = charger_set_ichg(manager->charger, value);
	if (ret < 0) {
		lx_err("charger set ichg fail.\n");
	} else {
		lx_info("%s vote %s val=%d\n", client, votable->name, value);
	}
	return ret;
}

static int main_chg_fv_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;
	ret = charger_set_term_volt(manager->charger, value);
	if (ret < 0) {
		lx_err("charger set term volt fail.\n");
	} else {
		lx_info("%s vote %s val=%d\n", client, votable->name, value);
	}

	return ret;
}

static int main_chg_icl_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		lx_err("the value of main chg icl is error.\n");
		return value;
	}
	if (value < 100) {
		ret = charger_disable_power_path(manager->charger, true);
	} else {
		ret = charger_disable_power_path(manager->charger, false);
		ret |= charger_set_input_curr_lmt(manager->charger, value);
	}

	if (ret < 0) {
		lx_err("charger set icl fail.\n");
	} else {
		lx_info("%s vote %s val=%d\n", client, votable->name, value);
	}
	return ret;
}

static int main_chg_iterm_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_set_term_curr(manager->charger, value);
	if (ret < 0) {
		lx_err("charger set iterm fail.\n");
	} else {
		lx_info("%s vote %s val=%d\n", client, votable->name, value);
	}
	return ret;
}

static int total_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	if (!manager->cp_policy)
		return -1;

	lx_info("%s vote %s val=%d\n", client, votable->name, value);
	if (value >= FASTCHARGE_MIN_CURR && (manager->cp_policy->state == POLICY_RUNNING)) {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			lx_err("main_icl_votable not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			lx_err("main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else
			vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
	} else {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			lx_err("-->main_icl_votable2 not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, false, 0);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			lx_err("-->main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else {
			if (value >= 0)
				vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, value);
		}
	}

	return 0;
}

static int cp_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	struct chargerpump_dev *master_cp_chg = manager->master_cp_chg;
	struct chargerpump_dev *slave_cp_chg = manager->slave_cp_chg;
	int ret = 0;
	if(manager->cp_master_psy){
		ret = chargerpump_set_enable(master_cp_chg, enable);
		if (ret < 0) {
			lx_err("master_cp_chg set chg fail.\n");
		}
	}
	if(manager->cp_slave_psy){
		ret = chargerpump_set_enable(slave_cp_chg, enable);
		if (ret < 0) {
			lx_err("slave_cp_chg set chg fail.\n");
		} else {
			lx_info("%s: vote val=%d\n", votable->name, enable);
		}
	}
	return ret;
}

static int charger_manager_create_votable(struct charger_manager *manager)
{
	int ret = 0;

	if (manager->charger) {
		manager->main_fcc_votable = create_votable("MAIN_FCC", VOTE_MIN, main_chg_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			lx_err("fail create MAIN_FCC voter.\n");
			return PTR_ERR(manager->main_fcc_votable);
		}

		manager->fv_votable = create_votable("MAIN_FV", VOTE_MIN, main_chg_fv_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->fv_votable)) {
			lx_err("fail create MAIN_FV voter.\n");
			return PTR_ERR(manager->fv_votable);
		}

		manager->main_icl_votable = create_votable("MAIN_ICL", VOTE_MIN, main_chg_icl_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_icl_votable)) {
			lx_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->main_icl_votable);
		}

		manager->iterm_votable = create_votable("MAIN_ITERM", VOTE_MIN, main_chg_iterm_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->iterm_votable)) {
			lx_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->iterm_votable);
		}

		manager->total_fcc_votable = create_votable("TOTAL_FCC", VOTE_MIN, total_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->total_fcc_votable)) {
			lx_err("fail create TOTAL_FCC voter.\n");
			return PTR_ERR(manager->total_fcc_votable);
		}
	}

	if (manager->cp_master_psy || manager->cp_slave_psy) {
		manager->cp_disable_votable = create_votable("CP_DISABLE", VOTE_SET_ANY, cp_disable_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->cp_disable_votable)) {
			lx_err("fail create CP_DISABLE voter.\n");
			return PTR_ERR(manager->cp_disable_votable);
		}
	}
	return ret;
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)

static void set_cc_drp_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, set_cc_drp_work.work);
	if(manager != NULL && manager->tcpc != NULL)
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_DRP);
	lx_info("set_cc_drp_work_func enter\n");
}
static int reset_vote(struct charger_manager *manager);


static bool check_reverse_22_5(struct charger_manager *manager)
{

	if (!manager->sm.screen_state) {
		manager->ibat_check_cnt = 0;
		return false;
	}
	lx_info("[REVCHG] ibat_check_cnt is %d\n", manager->ibat_check_cnt);
	if (manager->ibat < 3000000)
		manager->ibat_check_cnt ++;
	else
		manager->ibat_check_cnt = 0;

	if (manager->ibat_check_cnt < 2)
		return false;

	return true;
}

static void update_pdo_caps(struct charger_manager *manager, int pdo_caps)
{
	struct pd_port *pd_port = &manager->tcpc->pd_port;

	lx_info("[REVCHG] the pdo_caps is %d\n", pdo_caps);
	switch(pdo_caps) {
		case REVCHG_NORMAL:
			//5V1.5A
			pd_port->local_src_cap_default.pdos[0] =  0x26019096;
			break;
		case REVCHG_QUICK_9:
			//9V1A
			pd_port->local_src_cap_default.pdos[0] =  0x2602D064;
			break;
		case REVCHG_QUICK_22_5:
			//9V2.5A
			pd_port->local_src_cap_default.pdos[0] =  0x2602D0FA;
			break;
		default:
			break;
	}

	pd_port->local_src_cap_default.nr = 1;
	tcpm_dpm_pd_soft_reset(manager->tcpc, NULL);
	return;
}

static void check_reverse_quick_charge(struct charger_manager *manager, int *status)
{

	lx_info("[REVCHG] otg_stat is %d,soc is %d, bat_temp is %d, board_temp is %d\n",
				manager->otg_stat, manager->soc, manager->tbat, manager->thermal_board_temp);

	if(!manager->otg_stat || manager->tbat < 0 || manager->soc < 30 || manager->thermal_board_temp > 400)
		*status = 0;
	else
		*status = 1;

	lx_info("%s:[REVCHG] status is %d\n", *status);
}

static void reverse_quick_charge_work_handler(struct work_struct *work)
{
	int i;
	u32 cp_ibus = 0;
	int revchg_enable = 0;
	static int reverse_power_mode = REVCHG_QUICK_9;

	struct charger_manager *manager = container_of(work,
				struct charger_manager, reverse_quick_charge_work.work);

	lx_info("enter\n");

	if (!manager->reverse_charge_wakelock->active)
		__pm_stay_awake(manager->reverse_charge_wakelock);

	check_reverse_quick_charge(manager, &revchg_enable);
	if(!revchg_enable) {
		update_pdo_caps(manager, REVCHG_NORMAL);
		return;
	}

	if (manager->last_pdo_caps != reverse_power_mode) {
		lx_info("[REVCHG] last_pdo_caps is %d, reverse_power_mode is %d\n",
			manager->last_pdo_caps, reverse_power_mode);
		update_pdo_caps(manager, REVCHG_QUICK_9);
		manager->last_pdo_caps = reverse_power_mode;
	}

	switch(reverse_power_mode) {
		case REVCHG_QUICK_9:
			lx_info("[REVCHG] in case REVCHG_QUICK_9\n");
			if (check_reverse_22_5(manager)) {
				if (!manager->revchg_bcl) {
					xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 3);// open bcl
				} else {
					reverse_power_mode = REVCHG_QUICK_22_5;
					if (manager->last_pdo_caps != reverse_power_mode) {
						lx_info("[REVCHG]2 last_pdo_caps is %d, reverse_power_mode is %d\n",
										manager->last_pdo_caps, reverse_power_mode);
						update_pdo_caps(manager, REVCHG_QUICK_22_5);
						manager->last_pdo_caps = reverse_power_mode;
					}

				}
			}
			if(!manager->sm.screen_state && manager->revchg_bcl && reverse_power_mode!= REVCHG_QUICK_22_5) {
				for(i = 0; i < 5; i++) {
					chargerpump_set_enable_adc(manager->master_cp_chg, true);
					mdelay(20); //cp adc need 10ms to update ibus
					chargerpump_get_adc_value(manager->master_cp_chg, CP_ADC_IBUS, &cp_ibus);
					lx_info("[REVCHG] i = %d, cp_ibus =%d\n", i, cp_ibus);

					if(cp_ibus > 1500)
						goto next_loop;
				}
				xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 2);// close bcl
			}
			break;
		case REVCHG_QUICK_22_5:
			lx_info("[REVCHG] in case REVCHG_QUICK_22_5\n");
			if(!manager->sm.screen_state) {
				reverse_power_mode = REVCHG_QUICK_9;
				if (manager->last_pdo_caps != reverse_power_mode) {
					lx_info("[REVCHG]3 last_pdo_caps is %d, reverse_power_mode is %d\n",
										manager->last_pdo_caps, reverse_power_mode);
					update_pdo_caps(manager, REVCHG_QUICK_9);
					manager->last_pdo_caps = reverse_power_mode;
				}
			}
			break;

		default:
			break;

	}

next_loop:
	if (revchg_enable) {
		schedule_delayed_work(&manager->reverse_quick_charge_work, msecs_to_jiffies(1000));
	} else {
		schedule_delayed_work(&manager->reverse_quick_charge_work, msecs_to_jiffies(5000));
	}
}

static int charger_manager_set_source_vbus(struct charger_manager *manager, struct tcp_ny_vbus_state vbus_state)
{
	int ret = 0;
	int status = 0;

	if (!manager || !manager->master_cp_chg || !manager->charger) {
		lx_err("failed to get master cp charger\n");
		return -ENODEV;
	}
	if (vbus_state.type & TCP_VBUS_CTRL_PD_DETECT)
		manager->pd30_source = true;

	lx_info("[REVCHG] source vbus: %dmv\n", vbus_state.mv);
	switch (vbus_state.mv)
	{
		case 0:
			ret |= charger_set_otg(manager->charger, false);
			ret |= chargerpump_set_otg_enable(manager->master_cp_chg, false);
			xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 0);
			manager->otg_stat = DIS_OTG;
			manager->pd30_source = false;
			break;
		case 5000:
			if (manager->otg_stat == PUMP_OTG) {
				ret |= chargerpump_set_otg_enable(manager->master_cp_chg, false);
				lx_info("[REVCHG] 9V revchg to 5V!\n");
			}
			ret |= charger_set_otg(manager->charger, true);
			manager->otg_stat = BUCK_OTG;
			if (manager->pd30_source) {
				check_reverse_quick_charge(manager, &status);
				xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, status);
			}
			break;
		case 9000:
			ret |= chargerpump_set_otg_enable(manager->master_cp_chg, true);;
			ret |= tcpm_notify_vbus_stable(manager->tcpc);
			mdelay(1000);
			ret |= charger_set_otg(manager->charger, false);
			manager->otg_stat = PUMP_OTG;
			lx_info("[REVCHG] 5V revchg to 9V!\n");
			break;

		default:
			break;
	}

	return ret;
}

static int charger_manager_tcpc_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct charger_manager *manager =
		container_of(nb, struct charger_manager, pd_nb);

	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	lx_info("noti event: %d %d\n", (int)event, (int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
			manager->pd_curr_max = noti->vbus_state.ma;
			manager->pd_volt_max = noti->vbus_state.mv;
			if (IS_ERR_OR_NULL(manager->main_icl_votable))
				lx_err("main_icl_votable not found\n");
			else
				vote(manager->main_icl_votable, TYPEC_SINK_VBUS_VOTER, true, manager->pd_curr_max);
			lx_info("TCP_NOTIFY_SINK_VBUS pd_curr_max = %d\n", manager->pd_curr_max);
			if (manager->pd_curr_max == 0) {
				chargerpump_set_enable(manager->master_cp_chg, false);
				lx_info("PD pd_curr_max = 0 disable cp !!!\n");
			}
		}
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		lx_info("source vbus %dmV %dmA type(0x%02X)\n",
				    noti->vbus_state.mv, noti->vbus_state.ma, noti->vbus_state.type);
		ret = charger_manager_set_source_vbus(manager, noti->vbus_state);
		if (ret)
			lx_err("charger_manager_set_source_vbus err!!!\n");
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.new_state == TYPEC_UNATTACHED)
			manager->pd_active = CHARGE_PD_INVALID;

		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(manager->tcpc)) {
			lx_err("manager or tcpc is nullptr\n");
			break;
		}

		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!manager->typec_attach) {
			lx_info("typec plug in, polarity = %d\n", noti->typec_state.polarity);
			manager->typec_attach = true;
			manager->cid_status = true;
			if(manager->ui_cc_toggle){
				ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
				if (ret < 0) {
					lx_err("callback was running, skip timer\n");
				}
				lx_info("OTG ON:typec plug in, cancel hrtimer\n");
			}

		} else if (old_state != TYPEC_UNATTACHED &&
						new_state == TYPEC_UNATTACHED &&
						manager->typec_attach) {
			lx_info("typec plug out\n");
			manager->typec_attach = false;
			manager->cid_status = false;
			manager->pd30_source = false;
			reset_vote(manager);
			if(manager->ui_cc_toggle) {
				lx_err("OTG ON:typec plug out, ui set cc toggle\n");
				schedule_delayed_work(&manager->set_cc_drp_work, msecs_to_jiffies(500));

				ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
				if (ret < 0) {
					lx_err("callback was running, skip timer\n");
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

				lx_info("OTG ON:alarm timer start:%d, %lld %ld\n", ret,
						end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&manager->rust_det_work_timer, ktime);

			}
			if (manager->last_pdo_caps != 0) {
				lx_info("[REVCHG] Plug out ,stop reverse_quick_charging\n");
				update_pdo_caps(manager, REVCHG_NORMAL);
				manager->last_pdo_caps = 0;
				manager->ibat_check_cnt = 0;
			}
			xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 0);
			cancel_delayed_work_sync(&manager->reverse_quick_charge_work);
			__pm_relax(manager->reverse_charge_wakelock);
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		manager->is_pr_swap = true;
		if (noti->swap_state.new_role == PD_ROLE_SINK)
			manager->pd_active = 10;
		break;
	case TCP_NOTIFY_DR_SWAP:
		manager->is_dr_swap = true;
		break;
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			manager->is_dr_swap = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			manager->pd_contract_update = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_PPS_ACTIVE;
			lx_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
			lxchg_psy_updata(LXCHG_DEFAULT_EVENT);
		#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			xm_uevent_report(manager);
		#endif
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_ACTIVE;
			manager->is_dr_swap = false;
			break;
		default:
			break;
		}
		lxchg_psy_updata(LXCHG_DEFAULT_EVENT);
		break;
	case TCP_NOTIFY_HARD_RESET_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			manager->is_dr_swap = false;
			break;
		}
		break;
	default:
		break;
	}
	if ( !IS_ERR_OR_NULL(manager->charger) ) {
		manager->charger->m_pd_active = manager->pd_active;
	}

	return NOTIFY_OK;
}
#endif

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
static int charger_monitor_fg_i2c_status(struct charger_manager *manager) {
	int ret = 0;
	int vbus_volt = 0;

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		lx_err("get fuel_gauge is fail\n");
		return -1;
	}

	ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (!manager->charger->real_type)
		return ret;
	if (ret) {
		charger_get_adc(manager->charger, CHG_ADC_VBUS, &vbus_volt);
		if (vbus_volt > FG_I2C_ERR_VBUS) {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 300);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 300);
		} else {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 500);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 500);
		}
	} else {
		vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
		vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
	}
	return ret;
}
#endif

#ifdef FACTORY_BUILD
#define BAT_CAPACITY_MIN 75
#define BAT_CAPACITY_MAX 80

void ato_control_soc(struct charger_manager *manager)
{
	bool ato_soc_limit = is_input_suspend_by_client(manager, ATO_SOC_LIMIT_VOTER);

	if (!ato_soc_limit && manager->soc >= BAT_CAPACITY_MAX && manager->ato_soc_user_control == false) {
		lx_err("factory_version : capacity 80 stop charger\n");
		vote(manager->main_icl_votable, ATO_SOC_LIMIT_VOTER, true, 0);
	} else if (ato_soc_limit && (manager->soc <= BAT_CAPACITY_MIN || manager->ato_soc_user_control == true)) {
		vote(manager->main_icl_votable, ATO_SOC_LIMIT_VOTER, false, 0);
		lx_err("factory_version : capacity 75 start charger\n");
	}
	lx_info("factory_version : ato_soc_limit = %d, ato_soc_user_control = %d, input_suspend = %d\n", 
		ato_soc_limit, manager->ato_soc_user_control, is_input_suspend(manager));
}
#endif

static void charger_manager_monitor(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	uint32_t adc_buf_len = 0;
	uint8_t i = 0;
	char adc_buf[MIAN_CHG_ADC_LENGTH + 1] = {0};
	uint32_t iterm = 0;
	uint32_t fv = 0;
	int ichg = 0;
	bool charge_en = 0;
	bool hiz_en = 0;
	int ibus = 0;

	const struct {
		enum power_supply_property psp;
		int *data;
	} psy_data[] = {
		{POWER_SUPPLY_PROP_CAPACITY, &manager->soc},
		{POWER_SUPPLY_PROP_VOLTAGE_NOW, &manager->vbat},
		{POWER_SUPPLY_PROP_CURRENT_NOW, &manager->ibat},
		{POWER_SUPPLY_PROP_TEMP, &manager->tbat},
		{POWER_SUPPLY_PROP_STATUS, &manager->chg_status},
	};

	for (i = 0; i < ARRAY_SIZE(psy_data); i++) {
		ret = power_supply_get_property(manager->batt_psy, psy_data[i].psp, &pval);
		if (ret < 0) {
			lx_err("get batt_psy [%d] error!\n", psy_data[i].psp);
		} else {
			*psy_data[i].data = pval.intval;
		}
	}

	charger_get_term_curr(manager->charger, &iterm);
	charger_get_term_volt(manager->charger, &fv);
	charger_get_hiz_status(manager->charger, &hiz_en);
	charger_get_chg_enabled(manager->charger, &charge_en);
	charger_get_ichg(manager->charger, &ichg);
	charger_get_input_curr_lmt(manager->charger, &ibus);

	if (!IS_ERR_OR_NULL(manager->fuel_gauge))
		manager->rsoc = fuel_gauge_get_rsoc(manager->fuel_gauge);

	ktime_get_real_ts64(&manager->ts64);
	manager->tv = *(const struct timespec *)&manager->ts64;
	manager->tv.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(manager->tv.tv_sec, &manager->tm);

	lx_info("[Battery] soc: %d, ibat: %d, vbat: %d, tbat: %d\n",
				manager->soc, manager->ibat, manager->vbat/1000, manager->tbat);
	lx_info("[CHG_REG] ibus: %d, ichg: %d, charge_en: %d, iterm: %d, fv: %d, hiz_en:%d\n",
				ibus, ichg, charge_en, iterm, fv, hiz_en);
	pr_info("[CHG_TIME][%d-%02d-%02d %02d:%02d:%02d]\n", manager->tm.tm_year + 1900, manager->tm.tm_mon + 1,
				manager->tm.tm_mday, manager->tm.tm_hour, manager->tm.tm_min, manager->tm.tm_sec);

#ifdef FACTORY_BUILD
	if (manager->adapter_plug_in)
		ato_control_soc(manager);
#endif
	power_supply_changed(manager->usb_psy);
	power_supply_changed(manager->batt_psy);
	power_supply_changed(manager->chg_psy);
#if IS_ENABLED(CONFIG_MIEV)
	power_supply_changed(manager->cp_master_psy);
#endif

	for (i = 0; i < CHG_ADC_MAX; i++) {
		ret = charger_get_adc(manager->charger, i, &manager->chg_adc[i]);
		if (ret < 0) {
			lx_info("get adc failed\n");
			continue;
		}
		adc_buf_len += sprintf(adc_buf + adc_buf_len,
						"%s: %d, ", adc_name[i], manager->chg_adc[i]);
	}

	if (adc_buf_len > MIAN_CHG_ADC_LENGTH)
		adc_buf[MIAN_CHG_ADC_LENGTH] = '\0';
	lx_info("[CHG_ADC] %s\n", adc_buf);
}

static void low_vbat_power_off(struct charger_manager *manager)
{
	int rc = 0;
	static int count = 0;
	struct timespec64 ts;

	ktime_get_boottime_ts64(&ts);

	if (!manager->charger) {
		lx_err("failed to master_charge device\n");
		return;
	}

	if ((manager->vbat < ((manager->tbat >= BATTERY_COLD_TEMP ? SHUTDOWN_DELAY_VOL_LOW : SHUTDOWN_DELAY_VOL_COLD_TEMP) - 50))
		&& manager->vbat > 2700 && (u64)ts.tv_sec > 10) {
		if (count < 3) {
			count ++;
			lx_info("count is =%d\n", count);
		} else {
			rc = charger_reset(manager->charger);
			if (rc < 0)
				lx_err("main chg reset failed.\n");
			mdelay(1000);
			lx_info("vbat under 3.25V, poweroff. vbat=%d\n", manager->vbat);
			kernel_power_off();
		}
	} else {
		count = 0;
	}
}

static void hot_tbat_power_off(struct charger_manager *manager)
{
	if ((manager->tbat >= BATTERY_HOT_TEMP)) {
		lx_err("kpoc hot temperature power off trigger: tbat: %d\n", manager->tbat);
		mdelay(1000);
		kernel_power_off();
	}
}

static void power_off_check_work(struct charger_manager *manager)
{
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int rc = 0;
#endif

	static char uevent_string[][MAX_UEVENT_LENGTH + 1] = {
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n", //28
	};

	static char *envp[] = {
		uevent_string[0],
		NULL,
	};
	if (IS_ERR_OR_NULL(manager->fuel_gauge))
		return;

	low_vbat_power_off(manager);
	hot_tbat_power_off(manager);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	rc = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (manager->soc == 1 || (rc && manager->vbat))
#else
	if (manager->soc == 1)
#endif
	{
		if ((manager->vbat >= SHUTDOWN_DELAY_VOL_LOW && manager->vbat < SHUTDOWN_DELAY_VOL_HIGH)
			&& manager->chg_status != POWER_SUPPLY_STATUS_CHARGING){
				manager->shutdown_delay = true;
		} else if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING
						&& manager->shutdown_delay) {
				manager->shutdown_delay = false;
		}
	} else {
		manager->shutdown_delay = false;
	}

	if (manager->last_shutdown_delay != manager->shutdown_delay) {
		manager->last_shutdown_delay = manager->shutdown_delay;
		power_supply_changed(manager->usb_psy);
		power_supply_changed(manager->batt_psy);
		if (manager->shutdown_delay == true) {
			strncpy(uevent_string[0] + 28, "1", MAX_UEVENT_LENGTH - 28);
			fuel_gauge_set_rsoc_update0(manager->fuel_gauge, true);
		} else {
			strncpy(uevent_string[0] + 28, "0", MAX_UEVENT_LENGTH - 28);
			fuel_gauge_set_rsoc_update0(manager->fuel_gauge, false);
		}
		mdelay(1000);
		lx_err("envp[0] = %s\n", envp[0]);
		kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);
	}
}

static int charger_manager_check_vindpm(struct charger_manager *manager, uint32_t vbat)
{
	struct charger_dev *charger = manager->charger;
	int ret = 0;
#if CHARGER_VINDPM_USE_DYNAMIC
	if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT1) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE1);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT2) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE2);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT3) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT4) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE4);
	} else {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE5);
	}
#else
	ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
#endif

	if (ret < 0){
		lx_err("Failed to set vindpm, ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static int charger_manager_check_iindpm(struct charger_manager *manager, uint32_t chg_type)
{
	int ret = 0;
	int ichg_ma = 0;
	int icl_ma = 0;

	switch (chg_type) {
	case VBUS_TYPE_FLOAT:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_NONE:
		ichg_ma = 100;
		icl_ma = 100;
		break;
	case VBUS_TYPE_SDP:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	case VBUS_TYPE_NON_STAND:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_CDP:
		ichg_ma = manager->cdp_current;
		icl_ma = manager->cdp_current;
		break;
	case VBUS_TYPE_DCP:
		ichg_ma = manager->dcp_current;
		icl_ma = manager->dcp_current;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		if (manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].active_status || manager->pd_active) {
			ichg_ma = manager->xm_outdoor_current;
			icl_ma = manager->xm_outdoor_current;
			lx_info("xm_outdoor_current or cp_charge_done ichg_ma = %d!\n",ichg_ma = manager->xm_outdoor_current);
		}
#endif
		break;
	case VBUS_TYPE_HVDCP:
		ichg_ma = manager->hvdcp_charge_current;
		icl_ma = manager->hvdcp_input_current;
		break;
	case VBUS_TYPE_HVDCP_3:
	case VBUS_TYPE_HVDCP_3P5:
		ichg_ma = manager->hvdcp3_charge_current;
		icl_ma = manager->hvdcp3_input_current;
		break;
	default:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	}

	if ((manager->pd_active == CHARGE_PD_ACTIVE && chg_type) || manager->pd_active == 10) {
		if (manager->pd_volt_max == 5000) {  //C-to-C
			ichg_ma = manager->pd_curr_max;
			icl_ma = manager->pd_curr_max;
			lx_err("c-to-c ichg_ma = %d\n", ichg_ma);
		} else {  //PD2.0
			ichg_ma = manager->pd_curr_max * PD20_ICHG_MULTIPLE / 1000;  //1.8 of fixed current
			manager->pd_curr_max = min(manager->pd_curr_max, 2000);
			icl_ma = manager->pd_curr_max;
			lx_err("fixed current ichg_ma = %d\n", ichg_ma);
		}
	}

	if (is_mtbf_mode_func() && (chg_type == VBUS_TYPE_SDP || chg_type == VBUS_TYPE_CDP)) {
		ichg_ma = MTBF_CURRENT;
		icl_ma = MTBF_CURRENT;
		lx_info("is_mtbf_mode=%d icl=%d ichg=%d\n", is_mtbf_mode_func(), icl_ma, ichg_ma);
	}

	if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
		lx_err("main_icl_votable not found\n");
		return PTR_ERR(manager->main_icl_votable);
	} else
		vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, icl_ma);

	if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
		lx_err("main_fcc_votable not found\n");
		return PTR_ERR(manager->main_fcc_votable);
	} else
		vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, ichg_ma);

	return ret;
}

static void charger_manager_timer_func(struct timer_list *timer)
{
	struct charger_manager *manager = container_of(timer,
							struct charger_manager, charger_timer);
	charger_manager_wake_thread(manager);
}

int charger_manager_start_timer(struct charger_manager *manager, uint32_t ms)
{
	del_timer(&manager->charger_timer);
	manager->charger_timer.expires = jiffies + msecs_to_jiffies(ms);
	manager->charger_timer.function = charger_manager_timer_func;
	add_timer(&manager->charger_timer);
	return 0;
}
EXPORT_SYMBOL(charger_manager_start_timer);

void disable_chg_comm_ctrl(struct charger_manager *manager, const char *client_str, bool en)
{
	if ((manager == NULL) || !manager->main_fcc_votable)
		return;

	if (en) {
		vote(manager->main_fcc_votable, client_str, true, 0);

		if (manager->cp_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(manager->cp_policy);
			lx_info("disable cp\n");
		}
	} else {
		vote(manager->main_fcc_votable, client_str, false, 0);

		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (manager->cp_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(manager->cp_policy);
			lx_info("enable cp\n");
		}
	}
}
EXPORT_SYMBOL(disable_chg_comm_ctrl);

bool is_disable_chg_by_client(struct charger_manager *manager, const char *client_str)
{
	if ((manager == NULL) || !manager->main_fcc_votable)
		return false;

	return !get_client_vote(manager->main_fcc_votable, client_str);
}
EXPORT_SYMBOL(is_disable_chg_by_client);

bool is_input_suspend_by_client(struct charger_manager *manager, const char *client_str)
{
	if ((manager == NULL) || !manager->main_icl_votable)
		return false;

	return !get_client_vote(manager->main_icl_votable, client_str);
}
EXPORT_SYMBOL(is_input_suspend_by_client);

bool is_disable_chg(struct charger_manager *manager)
{
	if ((manager == NULL) || !manager->main_fcc_votable)
		return false;

	return !get_effective_result(manager->main_fcc_votable);
}
EXPORT_SYMBOL(is_disable_chg);

bool is_input_suspend(struct charger_manager *manager)
{
	if ((manager == NULL) || !manager->main_icl_votable)
		return false;

	return !get_effective_result(manager->main_icl_votable);
}
EXPORT_SYMBOL(is_input_suspend);


static int reset_vote(struct charger_manager *manager)
{
	vote_clean(manager->main_fcc_votable);
	vote_clean(manager->total_fcc_votable);
	vote_clean(manager->main_icl_votable);
	vote_clean(manager->fv_votable);
	vote_clean(manager->iterm_votable);
	return 0;
}

static int rerun_vote(struct charger_manager *manager)
{
	rerun_election(manager->total_fcc_votable);
	rerun_election(manager->main_fcc_votable);
	rerun_election(manager->main_icl_votable);
	return 0;
}

static void charger_manager_bc12_retry_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, bc12_retry_work.work);

	lx_info("enter!\n");
	if (manager->bc12_retry_count <= 3) {
		manager->bc12_retry_count++;
		lx_info("retry count = %d\n", manager->bc12_retry_count);
		if (manager->charger->real_type != VBUS_TYPE_FLOAT && manager->charger->real_type != VBUS_TYPE_NON_STAND) {
			lx_info("bc12 retry is useful !\n");
			manager->bc12_retry_count = 0;
			return;
		}
		charger_force_dpdm(manager->charger);
		schedule_delayed_work(&manager->bc12_retry_work, msecs_to_jiffies(FLOAT_DELAY_TIME));
	} else {
		lx_err("retry count > 3, bc_type still Non-standard!\n", manager->bc12_retry_count);
#if IS_ENABLED(CONFIG_MIEV)
		xmdfs_notifier_call_chain(CHG_DFX_NONE_STANDARD_CHG, NULL);
#endif
	}
}

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
static bool get_usb_ready(struct charger_manager *manager)
{
	bool ready = true;

	if (IS_ERR_OR_NULL(manager->usb_node))
		manager->usb_node = of_parse_phandle(manager->dev->of_node, "usb", 0);
	if (!IS_ERR_OR_NULL(manager->usb_node)) {
		ready = !of_property_read_bool(manager->usb_node, "cdp-block");
		if (ready || manager->get_usb_rdy_cnt % 10 == 0)
			lx_info("usb ready = %d\n", ready);
	} else
		lx_err("usb node missing or invalid\n");

	if (ready == false && (manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT || manager->pd_active)) {
		if (manager->pd_active)
			manager->get_usb_rdy_cnt = 0;
		lx_info("cdp-block timeout or pd adapter\n");
		return true;
	}

	return ready;
}

static void wait_usb_ready_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, wait_usb_ready_work.work);

	if (get_usb_ready(manager) || manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT)
		charger_force_dpdm(manager->charger);
	else {
		manager->get_usb_rdy_cnt++;
		schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(WAIT_USB_RDY_TIME));
	}
}
#endif

#if IS_ENABLED(CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT)
static void check_battery_cycle_count(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, check_batt_cycle_count.work);
	union power_supply_propval pval = {0,};
	u32 cycle_count = 0;
	u32 auth_cycle_count = 0;
	int ret = 0;

	if (unlikely(SHUTDOWN_DELAY_VOL_LOW == 3100) &&
		unlikely(SHUTDOWN_DELAY_VOL_HIGH == 3200)) {
		lx_info("DDR stress version, stop cycle count check work");
		return;
	}

	if (manager->fg_psy == NULL) {
		manager->fg_psy = power_supply_get_by_name("bms");
		if (manager->fg_psy == NULL) {
			lx_err("manager->batt_psy is NULL\n");
			goto out;
		}
	}

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		lx_err("failed to get cycle_count prop\n");
		goto out;
	} else
		cycle_count = pval.intval;

	if (!manager->auth_dev) {
		lx_err("failed to get battery auth device\n");
		goto out;
	} else {
		ret = auth_device_get_cycle_count(manager->auth_dev, &auth_cycle_count);
		if (ret != 0) {
			lx_err("read auth cycle count error\n");
			goto out;
		}
		mdelay(20);

		ret = auth_device_set_cycle_count(manager->auth_dev, cycle_count, auth_cycle_count);
		if (ret != 0) {
			lx_err("write auth cycle count error\n");
			goto out;
		}
		mdelay(20);

		ret = auth_device_get_cycle_count(manager->auth_dev, &manager->batt_cycle);
		if (ret != 0) {
			lx_err("read auth cycle count error\n");
			goto out;
		}
	}

out:
	schedule_delayed_work(&manager->check_batt_cycle_count, msecs_to_jiffies(3600 * 1000));
}
#endif

#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
static void batt_soh20_aging_test(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
			struct charger_manager, batt_soh20_aging_test.work);

	union power_supply_propval pval = {0,};
	int ret = 0;
	static int rsoc_count;
	int fake_cycle_count = 0;
	int fake_raw_soh = 100;

	lx_err("__enter__\n");

	if (manager->batt_psy == NULL) {
		manager->batt_psy = power_supply_get_by_name("battery");
		if (manager->batt_psy == NULL) {
			lx_err("manager->batt_psy is NULL\n");
			goto out;
		}
	}
	if (manager->fuel_gauge == NULL) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (manager->fuel_gauge == NULL) {
			lx_err("manager->fuel_gauge is NULL\n");
			goto out;
		}
	}

	ret = power_supply_get_property(manager->batt_psy,
		POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		lx_err("failed to get cycle_count prop\n");
		goto out;
	} else
		fake_cycle_count = pval.intval;

	fake_raw_soh -= (fake_cycle_count/100);
	fuel_gauge_set_soh(manager->fuel_gauge, fake_raw_soh);

	ret = power_supply_get_property(manager->batt_psy,
		POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0) {
		lx_err("failed to get cycle_count prop\n");
		goto out;
	} else {
		if (pval.intval < 0)
			rsoc_count++;
	}

	if (rsoc_count > 13) {
		rsoc_count = 0;
		pval.intval = fake_cycle_count + 1;
		ret = power_supply_set_property(manager->batt_psy,
			POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret < 0) {
			lx_err("failed to get cycle_count prop\n");
			goto out;
		}
	}

out:
	schedule_delayed_work(&manager->batt_soh20_aging_test, msecs_to_jiffies(3000));
}
#endif

static void charger_manager_charger_type_detect(struct charger_manager *manager)
{
	struct votable		*fcc_votable;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;
	bool input_suspend = is_input_suspend(manager);

	fcc_votable = find_votable("TOTAL_FCC");
	if (!fcc_votable) {
		lx_err("failed to get fcc_votable\n");
		return;
	}

	charger_get_online(manager->charger, &manager->usb_online);
	charger_get_vbus_type(manager->charger, &manager->charger->real_type);
	if (manager->usb_online && manager->charger->real_type == VBUS_TYPE_DCP) {
		if (manager->pd_active == CHARGE_PD_ACTIVE)
			manager->charger->real_type = VBUS_TYPE_PD;
		else if (manager->pd_active == CHARGE_PD_PPS_ACTIVE)
			manager->charger->real_type =  VBUS_TYPE_PD_PPS;
	}
	if (manager->usb_online != manager->adapter_plug_in) {
		manager->adapter_plug_in = manager->usb_online;
		if (manager->adapter_plug_in) {
			pm_stay_awake(manager->dev);
			lx_info("adapter plug in\n");
			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
				lx_info("main_icl_votable is NULL\n");
			} else {
				vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
				lx_info("main_icl_votable icl 100\n");
			}
			if (manager->single_cell_det && !input_suspend && manager->vbat <= 4300) {
				//before start timer,try to cancle
				ret = alarm_try_to_cancel(&manager->start_cell_det_work_timer);
				if (ret < 0) {
					lx_err("[Detcell] callback was running, skip timer\n");
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 60;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
				alarm_start(&manager->start_cell_det_work_timer, ktime);
				lx_info("[Detcell] start start_cell_det_work_timer\n");
			}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			if ((manager->soc <= 20) && (manager->thermal_board_temp <= 390)) {
				manager->low_fast_plugin_flag = true;
			}
			schedule_delayed_work(&manager->xm_charge_work, msecs_to_jiffies(3000));
#endif
#if IS_ENABLED(CONFIG_LIXUN_SOFT_ITERM_SUPPORT)
			schedule_delayed_work(&manager->soft_iterm_work, msecs_to_jiffies(3000));
			manager->soft_term_check_cnt = 0;
#endif
			manager->qc_detected = false;
			charger_adc_enable(manager->charger, true);
			chargerpump_set_enable_adc(manager->master_cp_chg, true);
			chargerpump_set_enable_adc(manager->slave_cp_chg, true);
			charger_set_term(manager->charger, true);
			rerun_vote(manager);
			vote(manager->total_fcc_votable, JEITA_VOTER, true, 500);
		} else {
			chargerpump_set_enable_adc(manager->master_cp_chg, false);
			chargerpump_set_enable_adc(manager->slave_cp_chg, false);
			charger_adc_enable(manager->charger, false);
			lx_info("adapter plug out\n");
			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
				lx_info("main_icl_votable is NULL\n");
			} else {
				vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
				lx_info("main_icl_votable icl 100\n");
			}

			if(manager->single_cell_det) {
				ret = alarm_try_to_cancel(&manager->start_cell_det_work_timer);
				if (ret < 0) {
					lx_err("[Detcell] start_cell_det_work_timer callback was running, skip timer\n");
				}
				ret = alarm_try_to_cancel(&manager->cell_det_work_timer);
				if (ret < 0) {
					lx_err("[Detcell] cell_det_work_timer callback was running, skip timer\n");
				}
				lx_info(" [Detcell] alarm_try_to_cancel\n");
			}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			vote(fcc_votable, ENDURANCE_VOTER, false, 0);
			rerun_election(fcc_votable);
			manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
			cancel_delayed_work(&manager->xm_charge_work);
			manager->low_fast_plugin_flag = false;
			manager->pps_fast_mode = false;
			manager->b_flag = NORMAL;
			manager->fv_overvoltage_flag = false;
			vote(manager->total_fcc_votable, FV_OVERVOLTAGE_VOTER,false, 0);
#endif
#if IS_ENABLED(CONFIG_LIXUN_SOFT_ITERM_SUPPORT)
			manager->soft_term_status = POWER_SUPPLY_STATUS_DISCHARGING;
			cancel_delayed_work(&manager->soft_iterm_work);
			manager->soft_term_check_cnt = 0;
#endif
			manager->plug_in_soc100_flag = false;
			cancel_delayed_work_sync(&manager->bc12_retry_work);
			manager->bc12_retry_count = 0;
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			if (!IS_ERR_OR_NULL(manager->fuel_gauge))
				fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
			#endif
			chargerpump_policy_stop(manager->cp_policy);
			reset_vote(manager);
			pm_relax(manager->dev);
		}
	}

	lx_info("usb_online= %d, bc_type = %s, input_suspend = %d, pd_active = %d, single_cell = %d\n",
				manager->usb_online, bc12_result[manager->charger->real_type], input_suspend, manager->pd_active, manager->en_single_cell);

	if (!manager->adapter_plug_in)
		return;

	if (!manager->is_pr_swap) {
		switch (manager->charger->real_type) {
			case VBUS_TYPE_NONE:
				charger_force_dpdm(manager->charger);
				break;
			case VBUS_TYPE_NON_STAND:
			case VBUS_TYPE_FLOAT:
				if (manager->bc12_retry_count == 0) {
					lx_info("float type! retry\n");
					schedule_delayed_work(&manager->bc12_retry_work, msecs_to_jiffies(0));
				}
				rerun_election(manager->main_icl_votable);
				break;
			case VBUS_TYPE_SDP:
				#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
				if (!get_usb_ready(manager)) {
					if (manager->get_usb_rdy_cnt == 0)
						schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(0));
				}
				#endif
				break;
			default:
				break;
		}
	} else
		manager->charger->real_type = VBUS_TYPE_FLOAT;

	if (manager->charger->real_type == VBUS_TYPE_SDP || manager->charger->real_type == VBUS_TYPE_CDP)
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
	else
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (charger_monitor_fg_i2c_status(manager)) {
		lx_info("fg i2c error\n");
}
#endif

	if (manager->pd_contract_update) {
		manager->pd_contract_update = false;
		if (manager->cp_policy->state != POLICY_RUNNING)
			chargerpump_policy_stop(manager->cp_policy);
	}

	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (manager->cp_policy->state == POLICY_NO_START))
		chargerpump_policy_start(manager->cp_policy);

	if (manager->cp_policy->state != POLICY_RUNNING) {
		if(manager->charger->real_type == VBUS_TYPE_DCP && !manager->qc_detected && !manager->pd_active) {
			manager->qc_detected = true;
			charger_qc_identify(manager->charger, manager->qc3_mode);
		}
	}

	/*
	 * WORKAROUNG: fix samsung EP-TA200 adapter HVDCP issue,
	 * We shouldn't set icl/ichg before vbus rise to 9v,
	 * as it would trigered vindpm.
	 * TODO: add notifier chain between main_chg.ko and subpmic_xxx.ko to notify hvdcp done.
	 */
	mdelay(100);

	charger_manager_check_vindpm(manager, manager->chg_adc[CHG_ADC_VBAT]);
	charger_manager_check_iindpm(manager, manager->charger->real_type);
}

static int charger_manager_thread_fn(void *data)
{
	struct charger_manager *manager = data;
	int ret = 0;

	while (true) {
		ret = wait_event_interruptible(manager->wait_queue,
							manager->run_thread);
		if (kthread_should_stop() || ret) {
			lx_err("exits(%d)\n", ret);
			break;
		}

		manager->run_thread = false;

		charger_manager_monitor(manager);

		charger_manager_charger_type_detect(manager);

		power_off_check_work(manager);

		if (!manager->adapter_plug_in)
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME_OUT);
		else
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME);
	}
	return 0;
}

static int charger_manager_notifer_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_nb);
	switch (event) {
		case THERMAL_EVENT_BOARD_TEMP:
			manager->thermal_board_temp = *(int *)data/100;
			lx_info("get thermal_board_temp: %d\n", manager->thermal_board_temp);
			break;
		default:
			lx_err("not supported charger notifier event: %d\n", event);
			break;
	}
	charger_manager_wake_thread(manager);

	return NOTIFY_OK;
}

static void shipmode_syscore_shutdown(void)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(g_manager)) {
		lx_err("g_manager err!!\n");
		return;
	}

	lx_info("shipmode flag = %d\n", g_manager->shipmode);
	if (g_manager->shipmode) {
		ret = charger_set_shipmode(g_manager->charger, true);
		if (ret < 0)
			lx_info("set ship mode fail\n");
		else
			lx_info("set ship mode success\n");
	}
}

static struct syscore_ops shipmode_syscore_ops = {
	.shutdown = shipmode_syscore_shutdown,
};

static int charger_manager_check_core_dev(struct charger_manager *manager)
{
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!manager->tcpc) {
		lx_err("tcpc device not ready, defer\n");
		goto retry;
	}
#endif

	manager->charger = charger_find_dev_by_name("primary_chg");
	if (!manager->charger) {
		lx_err("primary_chg device not ready, defer\n");
		goto retry;
	}
	manager->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (!manager->master_cp_chg) {
		lx_err("master_cp_chg device not ready, defer\n");
		goto retry;
	}

	manager->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	if (!manager->slave_cp_chg)
		lx_err("failed to slave_cp_chg device\n");

	manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!manager->fuel_gauge) {
		lx_err("fuel_gauge device not ready, defer\n");
		goto retry;
	}

	manager->cp_master_psy = power_supply_get_by_name("sc-cp-master");
	if (!manager->cp_master_psy)
		lx_err("failed to cp_master_psy\n");

	manager->cp_slave_psy = power_supply_get_by_name("sc-cp-slave");
	if (!manager->cp_slave_psy)
		lx_err("failed to cp_slave_psy\n");

	manager->fg_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(manager->fg_psy))
		lx_err("failed to get bms psy\n");


	manager->auth_dev = get_batt_auth_by_name("secret_ic");
	if (!manager->auth_dev) {
		lx_err("failed to get battery auth device\n");
	}
	return 0;
retry:
	return -EPROBE_DEFER;
}
 
static int charger_manager_parse_dts(struct charger_manager *manager)
{
	struct device_node *node = manager->dev->of_node;
	int ret = false;
	int i;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        int retv = false;
#endif

	manager->en_floatgnd = of_property_read_bool(node, "lx_chg_manager,en_floatgnd");
#ifndef KERNEL_FACTORY_BUILD
	manager->single_cell_det = of_property_read_bool(node, "lx_chg_manager,single_cell_det");
#endif //KERNEL_FACTORY_BUILD
	lx_info("manager en_floatgnd is %d, en_detcells is %d\n", manager->en_floatgnd, manager->single_cell_det);

	ret |= of_property_read_u32(node, "lx_chg_manager,charge_control_limit_max", &manager->charge_control_limit_max);
	ret |= of_property_read_u32(node, "lx_chg_manager,QC3_mode", &manager->qc3_mode);
	ret |= of_property_read_u32(node, "lx_chg_manager,usb_charger_current", &manager->usb_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,float_charger_current", &manager->float_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,ac_charger_current", &manager->dcp_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,cdp_charger_current", &manager->cdp_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp_charger_current", &manager->hvdcp_charge_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp_input_current", &manager->hvdcp_input_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp3_charger_current", &manager->hvdcp3_charge_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp3_input_current", &manager->hvdcp3_input_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,pd2_charger_current", &manager->pd2_charge_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,pd2_input_current", &manager->pd2_input_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,input_power_over", &manager->input_power_over);
	ret |= of_property_read_string(node, "lx_chg_manager,model_name", &manager->model_name);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	ret |= of_property_read_u32(node, "lx_chg_manager,xm_outdoor_current", &manager->xm_outdoor_current);
        retv = of_property_read_u32_array(node, "lx_chg_manager,cyclecount", manager->cyclecount, CYCLE_COUNT_MAX);
        if(retv){
                lx_err("use default CYCLE_COUNT: 0\n");
                for(i = 0; i < CYCLE_COUNT_MAX; i++)
                        manager->cyclecount[i] = 0;
        }
        ret |= retv;
#endif

	if (ret)
		return false;
	else
		return true;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int screen_state_for_charger_callback(struct notifier_block *nb,
                                            unsigned long val, void *v)
{
        int blank = *(int *)v;
        struct charger_manager *manager = container_of(nb, struct charger_manager, sm.charger_panel_notifier);

        if (!(val == MTK_DISP_EARLY_EVENT_BLANK|| val == MTK_DISP_EVENT_BLANK)) {
                lx_err("event(%lu) do not need process\n", val);
                return NOTIFY_OK;
        }

        switch (blank) {
        case MTK_DISP_BLANK_UNBLANK: //power on
                manager->sm.screen_state = 0;
                lx_info("screen_state = %d\n", manager->sm.screen_state);
                break;
        case MTK_DISP_BLANK_POWERDOWN: //power off
                manager->sm.screen_state = 1;
                lx_info("screen_state = %d\n", manager->sm.screen_state);
                break;
        }
        return NOTIFY_OK;
}
#endif
static void lxchg_device_init(struct charger_manager *manager)
{
	cm_usb_psy_register(manager);
	cm_battery_psy_register(manager);
	cm_charger_psy_register(manager);
	pd_adapter_init(manager);
	chargerpump_policy_init(manager);
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	xm_pd_adapter_init(manager);
#endif
	qomcharger_policy_init(manager);
}


#define PROBE_CNT_MAX	15
static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_manager *manager;
	static int probe_cnt = 0;
	int ret = 0;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	struct timespec64 ts_boot;
	struct timespec64 ts_delay;
	struct timespec64 ts_shiled;
#endif

	lx_info("running (%s), cnt = %d\n", CHARGER_MANAGER_VERSION, ++probe_cnt);

	manager = devm_kzalloc(&pdev->dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->dev = &pdev->dev;
	g_manager = manager;

	platform_set_drvdata(pdev, manager);

	ret = charger_manager_check_core_dev(manager);
	if (ret == -EPROBE_DEFER) {
		if (probe_cnt >= PROBE_CNT_MAX) {
			lx_err("failed to get core device\n");
			goto continue_with_fail;
		}
		devm_kfree(manager->dev, manager);
		return ret;
	}
continue_with_fail:
	lxchg_device_init(manager);

	ret = charger_manager_parse_dts(manager);
	if (!ret)
		lx_err("charger_manager_parse_dts failed\n");

	charger_manager_create_votable(manager);

	manager->fake_batt_cycle = 0xFFFF;
	lx_jeita_init(manager->dev);

	ret = charge_manager_thermal_init(manager);
	if (ret < 0)
		lx_err("charge_manager_thermal_init failed, ret = %d\n", ret);

	init_waitqueue_head(&manager->wait_queue);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	INIT_DELAYED_WORK(&manager->xm_charge_work, xm_charge_work);

	manager->thermal_board_temp = 250;
	manager->charger_nb.notifier_call = charger_manager_notifer_call;
	lxchg_notifier_register(&manager->charger_nb);

	manager->sm.charger_panel_notifier.notifier_call = screen_state_for_charger_callback;
	ret = mtk_disp_notifier_register("screen state", &manager->sm.charger_panel_notifier);
	if (ret) 
		lx_err("register screen state callback fail(%d)\n", ret);

	mutex_init(&manager->report_lock);

	INIT_DELAYED_WORK(&manager->thermal_restore_work, thermal_restore_work);
	ktime_get_boottime_ts64(&ts_boot);
	ts_shiled.tv_sec = 60;
	ts_shiled.tv_nsec = 0;
	ts_delay = timespec64_sub(ts_shiled, ts_boot);
	//lx_info("ts_delay: tv_sec = %llds, tv_nsec = %ldns, jiffies = %lu\n",
	//	ts_delay.tv_sec, ts_delay.tv_nsec, timespec64_to_jiffies(&ts_delay));
	schedule_delayed_work(&manager->thermal_restore_work, timespec64_to_jiffies(&ts_delay));

#endif //CONFIG_XIAOMI_SMART_CHG

#if IS_ENABLED(CONFIG_LIXUN_SOFT_ITERM_SUPPORT)
	INIT_DELAYED_WORK(&manager->soft_iterm_work, soft_iterm_work);
#endif //CONFIG_LIXUN_SOFT_ITERM_SUPPORT

	INIT_DELAYED_WORK(&manager->bc12_retry_work, charger_manager_bc12_retry_work);
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	INIT_DELAYED_WORK(&manager->wait_usb_ready_work, wait_usb_ready_work);
#endif //CONFIG_USB_MTK_HDRC

#if IS_ENABLED(CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT)
	INIT_DELAYED_WORK(&manager->check_batt_cycle_count, check_battery_cycle_count);
	schedule_delayed_work(&manager->check_batt_cycle_count, msecs_to_jiffies(30000));
#endif //CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT

#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
	INIT_DELAYED_WORK(&manager->batt_soh20_aging_test, batt_soh20_aging_test);
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	if (IS_ERR_OR_NULL(manager->tcpc)) {
		lx_err("manager->tcpc is null\n");
	} else {
		manager->pd_nb.notifier_call = charger_manager_tcpc_notifier_call;
		ret = register_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
								TCP_NOTIFY_TYPE_ALL);
		if (ret < 0) {
			lx_err("register tcpc notifier fail(%d)\n", ret);
			return ret;
		}
	}

	alarm_init(&manager->rust_det_work_timer, ALARM_BOOTTIME,rust_det_work_timer_handler);
	INIT_DELAYED_WORK(&manager->hrtime_otg_work, hrtime_otg_work);
	INIT_DELAYED_WORK(&manager->set_cc_drp_work, set_cc_drp_work);
	INIT_DELAYED_WORK(&manager->reverse_quick_charge_work, reverse_quick_charge_work_handler);
	manager->reverse_charge_wakelock = wakeup_source_register(manager->dev, "reverse_charge suspend wakelock");
	manager->cid_status = false;
#endif //CONFIG_TCPC_CLASS

	if (manager->single_cell_det){
		alarm_init(&manager->start_cell_det_work_timer, ALARM_BOOTTIME,start_cell_det_work_timer_handler);
		alarm_init(&manager->cell_det_work_timer, ALARM_BOOTTIME,cell_det_work_timer_handler);
	}
	manager->half_cell = 1;

	device_init_wakeup(manager->dev, true);
	manager->run_thread = true;
	manager->thread = kthread_run(charger_manager_thread_fn, manager,
								"charger_manager_thread");
#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
	ret = lx_fuel_algo_init(manager);
	if (ret < 0)
		lx_err("lx_fuel_algo_init fail\n");

#endif //CONFIG_LIXUN_FUEL_ALGORITHM
#if IS_ENABLED(CONFIG_MIEV)
	xm_chg_dfs_init(manager);
#endif

	/*****************for shipmode**********************
	 *Register syscore ops, just for, when restart in shipmode,
	 *it will reproduce any device shutdown fail
	 *So first, do device_shutdown, and then syscore ops->shutdown
	 */
	register_syscore_ops(&shipmode_syscore_ops);
	hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_DEFAULT");
	lx_err("HARDWARE_BATTERY_ID is set!\n");

	lx_err("success\n");
	return 0;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);
	lx_jeita_deinit();
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	cancel_delayed_work(&manager->xm_charge_work);
#endif

#if IS_ENABLED(CONFIG_LIXUN_SOFT_ITERM_SUPPORT)
	cancel_delayed_work(&manager->soft_iterm_work);
#endif

#if IS_ENABLED(CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT)
	cancel_delayed_work(&manager->check_batt_cycle_count);
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = unregister_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
					  TCP_NOTIFY_TYPE_ALL);
	if (ret < 0)
		lx_err("unregister tcpc notifier fail(%d)\n", ret);
#endif
	lxchg_notifier_unregister(&manager->charger_nb);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        ret = mtk_disp_notifier_unregister(&manager->sm.charger_panel_notifier);
        if (ret < 0)
		lx_err("unregister screen state notifier fail(%d)\n", ret);
#endif
	return 0;
}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);
	struct charger_dev *charger = charger_find_dev_by_name("primary_chg");

	//Avoid raising VBUS to 12V after shutdown
	if (manager == NULL) {
		lx_err("manager get null!!\n");
		return;
	}

	if(manager->charger->real_type == VBUS_TYPE_HVDCP){
		ret = charger_qc2_vbus_mode(charger, 5000);
		if (ret < 0)
			lx_err("set qc 5v fail\n");
		else
			lx_info("set qc 5v success\n");
	}

	if (manager->tcpc->ops->set_dis_vconn_ov) {
		manager->tcpc->ops->set_dis_vconn_ov(manager->tcpc, true);
		lx_info("set ship mode TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV\n");
	}

	charger_disable_power_path(manager->charger, true);
}

static const struct of_device_id charger_manager_match[] = {
	{.compatible = "lixun,lx_chg_manager",},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct platform_driver charger_manager_driver = {
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.driver = {
		.name = "lx_chg_manager",
		.of_match_table = charger_manager_match,
	},
};

static int __init charger_manager_init(void)
{
	lx_err("---->\n");
	return platform_driver_register(&charger_manager_driver);
}

late_initcall(charger_manager_init);

MODULE_DESCRIPTION("LiXun Charger Manager Core");
MODULE_LICENSE("GPL v2");
/*
 * LX main_chg Release Note
 *
 *
 * 1.1.2
 *  (1)Add a flag to mark the new BC detection
 *  Therefore, a BC detection was added after the PD encryption was completed.
 *
 * 1.1.1
 * (1)Add logic to not handle negative values at main_fcc, main_icl and total_fcc
 *
 * 1.1.0
 * (1)Modify usb_psy register：replace usb_psy_desc with manager->usb_psy_desc to register usb_psy
 *
 * 1.0.9
 * (1)encapsulate two APIs : charger_manager_monitor and charger_manager_charger_type_detect
 * (2)Add low_vbat_power_off_check
 *
 * 1.0.8
 * (1) Separate sysfs[psy and other]: creat sysfs in lx_charge_sysfs.c
 *
 * 1.0.7
 * (1) Add lx_thermal func: charge_manager_thermal_init
 *
 * 1.0.6
 * (1) Add lx_jeita func: lx_jeita_init
 *
 * 1.0.5
 * (1) Do not let the system sleep during charging:pm_stay_awake/pm_relax
 * (2) Start qc detection, it must PD not active
 * (3) Start cp policy sm, need PD active=2
 * (4) Add new tcpc notifier: handle tcpc event
 *
 * 1.0.4
 * (1) Add usb psy new node: real_type, typec_orientation, otg and input_suspend
 *
 * 1.0.3
 * (1) Add usb psy new node: real_type, typec_orientation, otg and input_suspend
 *
 * 1.0.2
 * (1) Add lx voter policy: charger_manager_create_votable
 *
 * 1.0.1
 * (1) Add battery psy node: charger_manager_batt_psy_register
 *
 * 1.0.0
 * (1) Add driver for lx charger manager
 * (2) charger_manager_thread_fn todo : 1)BC12 detect 2)cp policy entry 3)power off check .etc
 */
