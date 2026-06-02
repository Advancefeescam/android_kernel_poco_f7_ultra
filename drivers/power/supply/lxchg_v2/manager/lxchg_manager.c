// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 LiXun Technology(Shanghai) Co., Ltd.
 */


#include "lxchg_class.h"
#include "lxchg_voter.h"
#include "lxchg_jeita.h"
#include "lx_cp_policy.h"
#include <linux/random.h>

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "xm_adapter_class.h"
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include <linux/notifier.h>
#include "../../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#endif

#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
#include "lx_fg_algorithm_inf.h"
#endif

#define CHARGER_MANAGER_VERSION            "2.0"

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_core.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_typec.h"
#endif

#include "lxbat_auth_class.h"

#include "lxchg_manager.h"
#include "lxchg_printk.h"
#include <linux/syscore_ops.h>
#include <linux/hardware_info.h>
#include <linux/pm_qos.h>

#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG_CM]"
#endif
#include "xm_smart_chg.h"
#if IS_ENABLED(CONFIG_MIEV)    
#include "xmchg_dfs.h"
#endif
#include "lxchg_notify.h"

static struct charger_manager *g_manager = NULL;
static struct pm_qos_request cp_qos_request;

static const char *vbus_type_txt[] = {
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
	"MI_PPS",
};

static const char *chg_stat[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not charging",
	"Full"
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

static void hrtime_toggle_work_handler(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, hrtime_toggle_work.work);
	charger_soft_cid_set_toggle(manager->charger, false);
	manager->ui_cc_toggle = false;
	lx_info("enter\n");
}

static enum alarmtimer_restart rust_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, rust_det_work_timer);
	if(manager != NULL)
	{
		schedule_delayed_work(&manager->hrtime_toggle_work, 0);
		lx_info("timeout, close ui_cc_toggle and set not toggle\n");
	}
    return ALARMTIMER_NORESTART;
}

int charger_manager_get_current(struct charger_manager *manager, int *curr)
{
	int val;
	int ret = 0;
	int sub_val = 0;
	union power_supply_propval pval;

	ret = charger_get_adc(manager->charger, CHG_ADC_IBUS, &val);
	if (ret < 0) {
		lx_err("Couldn't read input curr ret=%d\n", ret);
	} else
		sub_val += val;

	if(manager->cp_master_psy){
		ret = power_supply_get_property(manager->cp_master_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			lx_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			sub_val += pval.intval;
	}
	if(manager->cp_slave_psy){
		ret = power_supply_get_property(manager->cp_slave_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			lx_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			sub_val += pval.intval;
	}

	*curr = sub_val;
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
#endif
	if (!manager->charger_online || manager->fake_board_thermal != 9999)
		return;

	/* keep system_temp_level as last thermal level */
	thermal_level = manager->system_temp_level;

#ifdef FACTORY_BUILD
	/* force disable thermal policy in factory build */
	manager->thermal_enable = false;
#endif //FACTORY_BUILD

	if (manager->thermal_enable == false) {
		lx_err("thermal ibat limit is disable\n");
		return;
	}

	if (thermal_level < 0)
		goto err;

	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE) {
		if (manager->cp_policy->cp_charge_done && manager->ffc_thermal_mitigation[thermal_level] < manager->vote_iterm) {
			lx_info("ffc_thermal_mitigation is less than vote_iterm\n");
			vote(manager->total_fcc_votable, voter_name, true, manager->vote_iterm * 120 / 100);
			return;
		}

		if (manager->thermal_parse_flags & FFC_THERM_PARSE_ERROR) {
			lx_err("pd thermal dtsi parse error\n");
			goto err;
		}

		if (thermal_level > manager->ffc_thermal_levels) {
			lx_err("thermal level is invalid\n");
			goto err;
		}

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
		vote(manager->total_fcc_votable, voter_name, true,
			manager->ffc_thermal_mitigation[thermal_level]);
	} else {
		if (manager->thermal_parse_flags & NORMAL_THERM_PARSE_ERROR) {
			lx_err("qc thermal dtsi parse error\n");
			goto err;
		}

		if (thermal_level > manager->normal_thermal_levels) {
			thermal_level = manager->normal_thermal_levels;
		}

		vote(manager->total_fcc_votable, voter_name, true,
			manager->normal_thermal_mitigation[thermal_level]);
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
	struct device_node *node = of_find_node_by_name(manager->dev->of_node, "lx_thermal");

	manager->thermal_enable = true;

	if (of_find_property(node, "lx,ffc-thermal-mitigation", &byte_len)) {
		manager->ffc_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->ffc_thermal_mitigation)) {
			ret |= FFC_THERM_PARSE_ERROR;
			lx_err("ffc_thermal_mitigation kzalloc error\n");
		} else {
			manager->ffc_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,ffc-thermal-mitigation",
				manager->ffc_thermal_mitigation, manager->ffc_thermal_levels);
			if (rc < 0) {
				ret |= FFC_THERM_PARSE_ERROR;
				lx_err("ffc_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= FFC_THERM_PARSE_ERROR;
		lx_err("ffc_thermal_mitigation not found\n");
	}

	if (of_find_property(node, "lx,normal-thermal-mitigation", &byte_len)) {
		manager->normal_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->normal_thermal_mitigation)) {
			ret |= NORMAL_THERM_PARSE_ERROR;
			lx_err("normal_thermal_mitigation kzalloc error\n");
		} else {
			manager->normal_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,normal-thermal-mitigation",
				manager->normal_thermal_mitigation, manager->normal_thermal_levels);
			if (rc < 0) {
				ret |= NORMAL_THERM_PARSE_ERROR;
				lx_err("normal_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= NORMAL_THERM_PARSE_ERROR;
		lx_err("normal_thermal_mitigation not found\n");
	}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        if (of_find_property(node, "lx,xmchg-low-soc-fast", &byte_len)) {
		manager->xmchg_low_soc_fast = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->xmchg_low_soc_fast)) {
			ret |= FFC_THERM_PARSE_ERROR;
			lx_err("xmchg_low_soc_fast kzalloc error\n");
		} else {
			manager->ffc_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,xmchg-low-soc-fast",
				manager->xmchg_low_soc_fast, manager->ffc_thermal_levels);
			if (rc < 0) {
				ret |= FFC_THERM_PARSE_ERROR;
				lx_err("xmchg_low_soc_fast parse error\n");
			}
		}
	} else {
		ret |= FFC_THERM_PARSE_ERROR;
		lx_err("xmchg_low_soc_fast not found\n");
	}

#endif
	manager->thermal_parse_flags = ret;
	if (ret == (NORMAL_THERM_PARSE_ERROR | FFC_THERM_PARSE_ERROR)) {
		manager->thermal_enable = false;
		ret = -EINVAL;
	}

	return ret;
}

static int icharge_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		lx_err("the value of icharge is error, force set 0ma\n");
		value = 0;
	}

	ret = charger_set_ichg(manager->charger, value);
	if (ret < 0) {
		lx_err("charger set ichg fail.\n");
	} else {
		lx_info("%s vote %s val=%d\n", client, votable->name, value);
	}

	return ret;
}

static int fv_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	static bool batovp_flag = false;
	int ret = 0;

	if (value < 0) {
		lx_err("the value of fv is error, return %d\n", value);
		return value;
	}

	if (manager->vbat > value && manager->cp_policy->state != POLICY_RUNNING) {
		lx_err("stop charge for batovp!\n");
		if (!batovp_flag)
			vote(manager->total_fcc_votable, STOP_CHARGE_FOR_BATOVP_VOTER, true, 0);
		else
			rerun_election(manager->total_fcc_votable);
		batovp_flag = true;
	} else {
		if (batovp_flag && manager->vbat < value -150) {
			lx_err("recharge from batovp!\n");
			vote(manager->total_fcc_votable, STOP_CHARGE_FOR_BATOVP_VOTER, false, 0);
			batovp_flag = false;
		}
	}

	ret = charger_set_term_volt(manager->charger, value);
	if (ret < 0) {
		lx_err("charger set term volt fail.\n");
	} else {
		lx_info("%s vote %s val=%d\n", client, votable->name, value);
	}
	return ret;
}


static int input_limit_vote_callback(struct votable *votable, void *data, int value, const char *client)
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

static int iterm_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		lx_err("the value of iterm is error, return %d\n", value);
		return value;
	}

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
	static int batovp_cnt = 0;
	struct charger_manager *manager = data;
	if (!manager->cp_policy)
		return -1;

	lx_info("%s vote %s val=%d\n", client, votable->name, value);
	if (value >= FASTCHARGE_MIN_CURR && (manager->cp_policy->state == POLICY_RUNNING)) {
		vote(manager->input_limit_votable, TOTAL_FFC_VOTER, true, CP_EN_MAIN_CHG_CURR);
		vote(manager->icharge_votable, TOTAL_FFC_VOTER, true, CP_EN_MAIN_CHG_CURR);
	} else {
		vote(manager->input_limit_votable, TOTAL_FFC_VOTER, false, 0);
		if (value >= 0)
			vote(manager->icharge_votable, TOTAL_FFC_VOTER, true, value);
	}

	if (value == 0 && ((!strcmp(client, STOP_CHARGE_FOR_BATOVP_VOTER) && batovp_cnt++ > 0) || !strcmp(client, JEITA_VOTER))) {
		if (!manager->not_charging) {
			lx_info("set not_charging!!\n");
			manager->not_charging = true;
		}
	} else {
		if (manager->not_charging) {
			lx_info("cancel not_charging\n");
			manager->not_charging = false;
			batovp_cnt = 0;
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
		manager->icharge_votable = create_votable("ICHG_VOTE", VOTE_MIN, icharge_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->icharge_votable)) {
			lx_err("fail create ICHG_VOTE voter.\n");
			return PTR_ERR(manager->icharge_votable);
		}

		manager->fv_votable = create_votable("FV_VOTE", VOTE_MIN, fv_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->fv_votable)) {
			lx_err("fail create FV_VOTE voter.\n");
			return PTR_ERR(manager->fv_votable);
		}

		manager->input_limit_votable = create_votable("INPUT_VOTE", VOTE_MIN, input_limit_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->input_limit_votable)) {
			lx_err("fail create INPUT_VOTE voter.\n");
			return PTR_ERR(manager->input_limit_votable);
		}

		manager->iterm_votable = create_votable("ITERM_VOTE", VOTE_MIN, iterm_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->iterm_votable)) {
			lx_err("fail create ITERM_VOTE voter.\n");
			return PTR_ERR(manager->iterm_votable);
		}

		manager->total_fcc_votable = create_votable("TOTAL_VOTE", VOTE_MIN, total_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->total_fcc_votable)) {
			lx_err("fail create TOTAL_VOTE voter.\n");
			return PTR_ERR(manager->total_fcc_votable);
		}
	}

	if (manager->cp_master_psy || manager->cp_slave_psy) {
		manager->cp_disable_votable = create_votable("CP_DISABLE_VOTE", VOTE_SET_ANY, cp_disable_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->cp_disable_votable)) {
			lx_err("fail create CP_DISABLE voter.\n");
			return PTR_ERR(manager->cp_disable_votable);
		}
	}
	return ret;
}

#define BATT_CURR_AVG_SAMPLES 7
static int cal_avg_current(int batt_current)
{
	static int samples_index = 0, samples_num = 0, batt_ma_avg_samples[BATT_CURR_AVG_SAMPLES];
	static int batt_ma_prev = 0;
	static int last_batt_ma_avg = 0;
	int min = 0;
	int max = 0;
	int sum_ma = 0;
	int i;

	if(batt_current == batt_ma_prev)
		goto unchanged;
	else
		batt_ma_prev = batt_current;

	batt_ma_avg_samples[samples_index] = batt_current;
	samples_index = (samples_index + 1) % BATT_CURR_AVG_SAMPLES;
	samples_num ++;

	if(samples_num >= BATT_CURR_AVG_SAMPLES)
		samples_num = BATT_CURR_AVG_SAMPLES;

	max = batt_ma_avg_samples[0];
	min = batt_ma_avg_samples[0];

	for (i = 0; i < samples_num; i++) {
		if (batt_ma_avg_samples[i] > max)
			max = batt_ma_avg_samples[i];
		if (batt_ma_avg_samples[i] < min)
			min = batt_ma_avg_samples[i];
	}

	for( i = 0; i <  samples_num; i++){
		sum_ma += batt_ma_avg_samples[i];
	}

	if (samples_num < 3)
		last_batt_ma_avg = sum_ma / samples_num;
	else
		last_batt_ma_avg = (sum_ma - max - min) / (samples_num - 2);

	//lx_info("min=%d, max=%d\n", min, max);
unchanged:
	return last_batt_ma_avg / 1000;
}

static void temp_compensate_by_avgibat(struct charger_manager *manager)
{
	int target;

	if (!manager->charger_online && manager->temp_compensate == 0)
		return;

	if (manager->avg_ibat > 0)
		target = manager->avg_ibat/1000;
	else
		target = 0;

	if (target > manager->temp_compensate)
		manager->temp_compensate++;
	else if (target < manager->temp_compensate)
		manager->temp_compensate--;

	lx_info("target = %d, temp_compensate = %d\n", target, manager->temp_compensate);
}
#define SMOOTH_LOOP_INTERVAL    5000  //5s
#define UISOC_KEEP100_SECOND    600   //10min
#define RSOC_KEEP100_SECOND     2000  //30min+

static void smoothed_rsoc_update(struct charger_manager *manager)
{
	int curr_rsoc = fuel_gauge_get_rsoc(manager->fuel_gauge);
	static int smoothed_rsoc = -1;
	int step = 5 + ((manager->max_uisoc - curr_rsoc) / 100);

	if (smoothed_rsoc == -1)
		smoothed_rsoc = curr_rsoc;

	if (manager->uisoc < 100 || !manager->charger_online)
		manager->rsoc_keep100_sec = 0;

	if (manager->rsoc_keep100_sec ||
		(manager->uisoc >= manager->max_uisoc / 100 &&
			smoothed_rsoc <= 10000 && manager->avg_ibat > -200 &&
			(manager->term_rsoc - curr_rsoc) < 450)) {
		if (!manager->rsoc_keep100_sec)
			smoothed_rsoc += step;
		else
			manager->rsoc_keep100_sec -= SMOOTH_LOOP_INTERVAL / 1000;

		if (manager->rsoc_keep100_sec < 0)
			manager->rsoc_keep100_sec = 0;

		if (smoothed_rsoc > 10000 || (manager->rsoc_keep100_sec && smoothed_rsoc != 10000))
			smoothed_rsoc = 10000;
	} else {
		smoothed_rsoc = max(curr_rsoc, smoothed_rsoc -= step);
	}

	lx_info("step = %d, avg_ibat = %d, uisoc = %d, term_rsoc = %d, curr_rsoc=%d, smoothed_rsoc=%d, rsoc_keep100_sec = %d\n",
		step, manager->avg_ibat, manager->uisoc, manager->term_rsoc, curr_rsoc, smoothed_rsoc, manager->rsoc_keep100_sec);

	manager->rsoc = smoothed_rsoc;
	if (manager->mtk_gauge)
		manager->mtk_gauge->gm->smooth_rsoc = smoothed_rsoc;
}

static int smoothed_uisoc_update(struct charger_manager *manager)
{
	union power_supply_propval pval;
	int ret, soc;
	static int uisoc = -1;
	static int last_uisoc = 0;
	static int ready_track = 0;
	int step = 20;
	static int track_state = NO_TRACK;

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		lx_err("failed to get capaticy prop\n");
		manager->uisoc = 50;
		return 0;
	} else
		soc = pval.intval * 100;

	if (soc < 0) {
		manager->uisoc = 50;
		manager->fg_not_ready = true;
		lx_err("soc is not ready, retry quickly\n");
		return -1;
	} else {
		manager->fg_not_ready = false;
	}

	if (uisoc == -1)
		last_uisoc = uisoc = soc;

	if (manager->uisoc_keep100_sec)
		step = 100;
	else if (track_state == RECHG_DECREASE || track_state == RECOVER_FROM_RECHG)
		step = min(100, 20 + (abs(uisoc - manager->rsoc) / 100));
	else
		step = min(100, 20 + ((manager->max_uisoc - uisoc) / 100));

	if (uisoc / 100 >= (manager->jeita->soft_soc_threshold - 10) &&
		manager->chg_status == POWER_SUPPLY_STATUS_CHARGING &&
		manager->vbat >= (manager->jeita->soft_fv_threshold - 10) &&
		manager->ibat/1000 <= (manager->jeita->soft_iterm_threshold + 100) &&
		manager->ibat/1000 > 0) {
		if (ready_track > 3)
			track_state = FULL_INCREASE;
		else
			ready_track++;
	} else if (track_state == FULL_INCREASE) {
		track_state = RECOVER_FROM_FULL;
		ready_track = 0;
	}

	if (manager->is_eea) {
		if (manager->is_charge_done || manager->uisoc_keep100_sec)
			manager->max_uisoc = 10000;
		else if (uisoc < 10000)
			manager->max_uisoc = 9900;

		if (manager->chg_status == POWER_SUPPLY_STATUS_FULL) {
			if (!manager->uisoc_keep100_sec)
				track_state = RECHG_DECREASE;
			else
				manager->uisoc_keep100_sec -= SMOOTH_LOOP_INTERVAL / 1000;

		if (manager->uisoc_keep100_sec < 0 || manager->rsoc < 9600)
			manager->uisoc_keep100_sec = 0;

		} else if (track_state == RECHG_DECREASE)
			track_state = RECOVER_FROM_RECHG;
	}

	switch (track_state) {
		case NO_TRACK:
			if (soc - uisoc > 100)
				uisoc = min(soc, uisoc += step);
			else if (uisoc - soc > 100)
				uisoc = max(soc, uisoc -= step);
			else
				uisoc = soc;
			break;
		case FULL_INCREASE:
			uisoc += step;
			break;
		case RECOVER_FROM_FULL:
			uisoc = max(soc, uisoc -= step);
			if (soc == uisoc)
				track_state = NO_TRACK;
			break;
		case RECHG_DECREASE:
			uisoc = max(manager->rsoc, uisoc -= step);
			break;
		case RECOVER_FROM_RECHG:
			uisoc = min(soc, max(manager->rsoc, uisoc += step));
			if (manager->ibat < -200)
				uisoc = min(last_uisoc, uisoc);
			if (soc == uisoc)
				track_state = NO_TRACK;
			break;
	}
	uisoc = min(manager->max_uisoc, uisoc);

	if (manager->chg_status != POWER_SUPPLY_STATUS_CHARGING &&
		manager->chg_status != POWER_SUPPLY_STATUS_FULL)
		uisoc = min(last_uisoc, uisoc);
	else if (manager->ibat > 100 && manager->chg_status == POWER_SUPPLY_STATUS_CHARGING)
		uisoc = max(last_uisoc, uisoc);

	if (uisoc < 100)
		uisoc = 100;

	last_uisoc = uisoc;

	lx_info("[%d] step = %d, ibat = %d, soc=%d(%d), uisoc=%d(%d), uisoc_keep100_sec = %d\n",
		track_state, step, manager->ibat/1000, soc, manager->rsoc, uisoc, manager->max_uisoc, manager->uisoc_keep100_sec);

	manager->uisoc = uisoc / 100;
	return 0;
}

static void smooth_work_handler(struct work_struct *work)
{
	int ret;
	struct charger_manager *manager = container_of(work,
				struct charger_manager, smooth_work.work);

	manager->avg_ibat = cal_avg_current(manager->ibat);
	temp_compensate_by_avgibat(manager);
	smoothed_rsoc_update(manager);
	ret = smoothed_uisoc_update(manager);

	if (!ret)
		schedule_delayed_work(&manager->smooth_work, msecs_to_jiffies(SMOOTH_LOOP_INTERVAL));
	else
		schedule_delayed_work(&manager->smooth_work, msecs_to_jiffies(SMOOTH_LOOP_INTERVAL / 10));
}

static void qc_detected_retry_work_handler(struct work_struct *work)
{
	static int retry_count = 0;
	struct charger_manager *manager = container_of(work,
				struct charger_manager, qc_detected_retry_work.work);

	if (retry_count < 3 && manager->real_type != VBUS_TYPE_HVDCP && manager->real_type == VBUS_TYPE_DCP) {
		retry_count++;
		charger_qc_identify(manager->charger, manager->qc3_support);
		schedule_delayed_work(&manager->qc_detected_retry_work, msecs_to_jiffies(3000));
	}
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
static void set_cc_drp_work(struct work_struct *work)
{
	int ret;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	struct charger_manager *manager = container_of(work,
				struct charger_manager, set_cc_drp_work.work);

	if(manager->ui_cc_toggle) {
		lx_info("ui_cc_toggle is open, set cc toggle\n");
		charger_soft_cid_set_toggle(manager->charger, true);

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
}
static int reset_vote(struct charger_manager *manager);

__maybe_unused
static bool check_reverse_22_5(struct charger_manager *manager)
{
	if (manager->sm.screen_on) {
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

	return false;//no support 22_5
}

static void update_pdo_caps(struct charger_manager *manager, int pdo_caps)
{
	struct pd_port *pd_port = &manager->tcpc->pd_port;

	lx_info("[REVCHG] the pdo_caps is %d\n", pdo_caps);
	switch(pdo_caps) {
		case REVCHG_LOW_POWER:
			//5V0.5A
			pd_port->local_src_cap_default.pdos[0] =  0x26019032;
			break;
		case REVCHG_LIMIT:
			//5V1A
			pd_port->local_src_cap_default.pdos[0] =  0x26019064;
			break;
		case REVCHG_NORMAL:
			//5V1.5A
			pd_port->local_src_cap_default.pdos[0] =  0x26019096;
			break;
		case REVCHG_QUICK_9:
			//9V2A
			pd_port->local_src_cap_default.pdos[0] =  0x2602D0C8;
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

static int check_reverse_quick_charge(struct charger_manager *manager, bool is_init)
{
	static int temp_bias = 0;
	int quick_charge = 0;
	lx_info("[REVCHG] otg_stat is %d,soc is %d, bat_temp is %d, board_temp is %d, temp_bias is %d\n",
				manager->otg_stat, manager->uisoc, manager->tbat, manager->thermal_board_temp, temp_bias);

	if (manager->uisoc < 30 || is_init) {
		quick_charge = 0;
		temp_bias = 0;
	} else if (manager->tbat < temp_bias || manager->thermal_board_temp > (400 - temp_bias)) {
		quick_charge = 0;
		temp_bias = 50;
	} else {
		quick_charge = 1;
		temp_bias = 0;
	}

	lx_info("[REVCHG] status is %d\n", quick_charge);
	return quick_charge;
}

static void reverse_quick_charge_work_handler(struct work_struct *work)
{
	//int i;
	//u32 cp_ibus = 0;
	int revchg_enable = 0;
	static int reverse_power_mode = REVCHG_QUICK_9;

	struct charger_manager *manager = container_of(work,
				struct charger_manager, reverse_quick_charge_work.work);

	if (!manager->reverse_charge_wakelock->active)
		__pm_stay_awake(manager->reverse_charge_wakelock);

	revchg_enable = check_reverse_quick_charge(manager, false);
	if (!revchg_enable)
		reverse_power_mode = REVCHG_NORMAL;
	else
		reverse_power_mode = REVCHG_QUICK_9;

	if (manager->last_pdo_caps != reverse_power_mode &&
		(manager->last_pdo_caps == REVCHG_QUICK_9 || reverse_power_mode == REVCHG_QUICK_9)) {
		lx_info("[REVCHG] last_pdo_caps is %d, reverse_power_mode is %d\n",
			manager->last_pdo_caps, reverse_power_mode);
		update_pdo_caps(manager, reverse_power_mode);
		manager->last_pdo_caps = reverse_power_mode;
	}
#if 0
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
			if(manager->sm.screen_on && manager->revchg_bcl && reverse_power_mode!= REVCHG_QUICK_22_5) {
				for(i = 0; i < 5; i++) {
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
			if(manager->sm.screen_on) {
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
#endif

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
	char *cp_name = NULL;
	static int reverse_power_mode = REVCHG_NORMAL;

	if (!manager || !manager->charger) {
		lx_err("failed to get master cp charger\n");
		return -ENODEV;
	}
	if (vbus_state.type & TCP_VBUS_CTRL_PD_DETECT)
		manager->pd30_source = true;
	else
		manager->pd30_source = false;

	cp_name = chargerpump_get_name(manager->master_cp_chg);

	lx_info("[REVCHG] source vbus: %dmv\n", vbus_state.mv);
	switch (vbus_state.mv)
	{
		case 0:
			ret |= charger_set_otg(manager->charger, false);
			ret |= chargerpump_set_otg_enable(manager->master_cp_chg, false);
			manager->otg_stat = DIS_OTG;
			xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 0);
			if (manager->otg_stat == PUMP_OTG && strstr(cp_name, "bq25960"))
				mdelay(1000);
			break;
		case 5000:
			if (manager->otg_stat == PUMP_OTG) {
				ret |= chargerpump_set_otg_enable(manager->master_cp_chg, false);
				lx_info("[REVCHG] 9V revchg to 5V!\n");
			} else {
				if (manager->pd30_source) {
					status = check_reverse_quick_charge(manager, false);
					xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, status);
				}
			}

			if (manager->pd30_source) {
				if (manager->uisoc < 15 && manager->ibat < -3000000)
					reverse_power_mode = REVCHG_LOW_POWER;
				else if (manager->uisoc < 45 && manager->ibat < -3000000)
					reverse_power_mode = REVCHG_LIMIT;
				else
					reverse_power_mode = REVCHG_NORMAL;
				lx_info("reverse_power_mode=%d, last_pdo_caps=%d, ibat = %d\n",
					reverse_power_mode, manager->last_pdo_caps, manager->ibat);
				if (manager->last_pdo_caps > reverse_power_mode) {
					lx_info("[REVCHG] last_pdo_caps is %d, reverse_power_mode is %d\n",
						manager->last_pdo_caps, reverse_power_mode);
					update_pdo_caps(manager, reverse_power_mode);
					manager->last_pdo_caps = reverse_power_mode;
				}
			}
			ret |= charger_set_otg(manager->charger, true);
			manager->otg_stat = BUCK_OTG;
			break;
		case 9000:
			lx_info("[REVCHG] 5V revchg to 9V!\n");
			manager->otg_stat = PUMP_OTG;
			if (strstr(cp_name, "bq25960")) {
				lx_info("[REVCHG] For bq25960 OTG ovp_gate on\n");
				ret |= chargerpump_enable_acdrv_manual(manager->master_cp_chg, true);
			}
			ret |= chargerpump_set_otg_enable(manager->master_cp_chg, true);;
			ret |= tcpm_notify_vbus_stable(manager->tcpc);
			if (strstr(cp_name, "bq25960"))
				mdelay(1000);
			ret |= charger_set_otg(manager->charger, false);
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
	int ret = 0;

	lx_info("noti event: %d %d\n", (int)event, (int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
			manager->pd_curr_max = noti->vbus_state.ma;
			manager->pd_volt_max = noti->vbus_state.mv;
			vote(manager->input_limit_votable, TYPEC_SINK_VBUS_VOTER, true, manager->pd_curr_max);
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
			charger_adc_enable(manager->charger, true);
			chargerpump_set_enable_adc(manager->master_cp_chg, true);
			chargerpump_set_enable_adc(manager->slave_cp_chg, true);
			if (new_state == TYPEC_ATTACHED_SNK ||
		     new_state == TYPEC_ATTACHED_NORP_SRC ||
		     new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		     new_state == TYPEC_ATTACHED_DBGACC_SNK) {
				lx_info("charger plug in, polarity = %d\n", noti->typec_state.polarity);
				lxchg_psy_updata(CHARGE_EVENT_PLUG_IN);
			} else if (new_state == TYPEC_ATTACHED_SRC) {
				manager->otg_online = true;
				lx_info("OTG plug in, polarity = %d\n",noti->typec_state.polarity);
				if(manager->ui_cc_toggle) {
					ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
					if (ret < 0) {
						lx_err("callback was running, skip timer\n");
					}
					lx_info("OTG ON:typec plug in, cancel hrtimer\n");
				}
			}
			manager->typec_attach = true;
		}
		else if (old_state != TYPEC_UNATTACHED &&
						new_state == TYPEC_UNATTACHED &&
						manager->typec_attach) {
			chargerpump_set_enable_adc(manager->master_cp_chg, false);
			chargerpump_set_enable_adc(manager->slave_cp_chg, false);
			if (old_state == TYPEC_ATTACHED_SNK ||
			    old_state == TYPEC_ATTACHED_NORP_SRC ||
			    old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			    old_state == TYPEC_ATTACHED_DBGACC_SNK) {
				lx_info("charger plug out\n");
				lxchg_psy_updata(CHARGE_EVENT_PLUG_OUT);
			} else if (old_state == TYPEC_ATTACHED_SRC) {
				manager->otg_online = false;
				lx_info("OTG plug out\n");
				schedule_delayed_work(&manager->set_cc_drp_work, msecs_to_jiffies(500));

				if (manager->last_pdo_caps != REVCHG_NORMAL) {
					lx_info("[REVCHG] Plug out ,stop reverse_quick_charging\n");
					manager->last_pdo_caps = REVCHG_NORMAL;
					manager->ibat_check_cnt = 0;
					update_pdo_caps(manager, REVCHG_NORMAL);
				}
				cancel_delayed_work_sync(&manager->reverse_quick_charge_work);
				check_reverse_quick_charge(manager, true);
				__pm_relax(manager->reverse_charge_wakelock);
			}
			manager->typec_attach = false;
			manager->pd30_source = false;
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		manager->is_pr_swap = true;
		if (noti->swap_state.new_role == PD_ROLE_SINK)
			manager->pd_active = CHARGE_PD_PR_SWAP;
		break;
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->real_type = VBUS_TYPE_NONE;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			manager->pd_contract_update = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_PPS_ACTIVE;
			manager->real_type = VBUS_TYPE_PD_PPS;
			if (manager->pd_adapter->verifed)
				manager->real_type = VBUS_TYPE_MI_PPS;
			lx_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			manager->charger_online = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_ACTIVE;
			manager->real_type = VBUS_TYPE_PD;
			break;
		case PD_CONNECT_HARD_RESET:
			if (manager->real_type == VBUS_TYPE_MI_PPS)
				manager->charger_online = false;
			manager->real_type = VBUS_TYPE_NONE;
			reset_vote(manager);
			if (manager->plug_in_soc100_flag) {
				lx_info("set soft iterm again!\n");
				vote(manager->total_fcc_votable, SOFT_ITERM_VOTER, true, 0);
			}
			break;
		default:
			break;
		}
		lxchg_psy_updata(POWER_SUPPLY_CHANGED);
		break;
	case TCP_NOTIFY_HARD_RESET_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			break;
		}
		break;
	case TCP_NOTIFY_SOFT_RESET:
		lx_info("tcpc received soft reset.\n");
		manager->soft_reset_state = true;
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
	if (!manager->real_type)
		return ret;
	if (ret) {
		charger_get_adc(manager->charger, CHG_ADC_VBUS, &vbus_volt);
		if (vbus_volt > FG_I2C_ERR_VBUS) {
			vote(manager->icharge_votable, FG_I2C_ERR, true, 300);
			vote(manager->input_limit_votable, FG_I2C_ERR, true, 300);
		} else {
			vote(manager->icharge_votable, FG_I2C_ERR, true, 500);
			vote(manager->input_limit_votable, FG_I2C_ERR, true, 500);
		}
	} else {
		vote(manager->icharge_votable, FG_I2C_ERR, false, 0);
		vote(manager->input_limit_votable, FG_I2C_ERR, false, 0);
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

	if (!ato_soc_limit && manager->uisoc >= BAT_CAPACITY_MAX && manager->ato_soc_user_control == false) {
		lx_err("factory_version : capacity 80 stop charger\n");
		vote(manager->input_limit_votable, ATO_SOC_LIMIT_VOTER, true, 0);
	} else if (ato_soc_limit && (manager->uisoc <= BAT_CAPACITY_MIN || manager->ato_soc_user_control == true)) {
		vote(manager->input_limit_votable, ATO_SOC_LIMIT_VOTER, false, 0);
		lx_err("factory_version : capacity 75 start charger\n");
	}
	lx_info("factory_version : ato_soc_limit = %d, ato_soc_user_control = %d, input_suspend = %d\n", 
		ato_soc_limit, manager->ato_soc_user_control, is_input_suspend(manager));
}
#endif

bool is_disable_chg_by_client(struct charger_manager *manager, const char *client_str)
{
	if ((manager == NULL) || !manager->total_fcc_votable)
		return false;

	return !get_client_vote(manager->total_fcc_votable, client_str);
}
EXPORT_SYMBOL(is_disable_chg_by_client);

bool is_input_suspend_by_client(struct charger_manager *manager, const char *client_str)
{
	if ((manager == NULL) || !manager->input_limit_votable)
		return false;

	return !get_client_vote(manager->input_limit_votable, client_str);
}
EXPORT_SYMBOL(is_input_suspend_by_client);

bool is_disable_chg(struct charger_manager *manager)
{
	if ((manager == NULL) || !manager->icharge_votable)
		return false;

	return !get_effective_result(manager->icharge_votable);
}
EXPORT_SYMBOL(is_disable_chg);

bool is_input_suspend(struct charger_manager *manager)
{
	if ((manager == NULL) || !manager->input_limit_votable)
		return false;

	return !get_effective_result(manager->input_limit_votable);
}
EXPORT_SYMBOL(is_input_suspend);

static void low_vbat_power_off(struct charger_manager *manager)
{
	static int count = 0;
	struct timespec64 ts;

	ktime_get_boottime_ts64(&ts);

	if (!manager->charger) {
		lx_err("failed to master_charge device\n");
		return;
	}

	if ((manager->vbat < ((manager->tbat >= BATTERY_COLD_TEMP ? SHUTDOWN_DELAY_VOL_LOW : SHUTDOWN_DELAY_VOL_COLD_TEMP)))
		&& (u64)ts.tv_sec > 10) {
		if (count < 3) {
			count ++;
			lx_info("count is =%d\n", count);
		} else {
			charger_reset(manager->charger);
			charger_adc_enable(manager->charger, false);
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
		charger_reset(manager->charger);
		charger_adc_enable(manager->charger, false);
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
	static int cnt = 0;

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
	if (manager->uisoc == 1 || (rc && manager->vbat))
#else
	if (manager->uisoc == 1)
#endif
	{
		lx_info("vbat = %d, cnt = %d\n", manager->vbat, cnt);
		if ((manager->vbat >= SHUTDOWN_DELAY_VOL_LOW && manager->vbat < SHUTDOWN_DELAY_VOL_HIGH)
			&& manager->chg_status != POWER_SUPPLY_STATUS_CHARGING){
			if(cnt++ > 3)
				manager->shutdown_delay = true;
		} else if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING
						&& manager->shutdown_delay) {
			manager->shutdown_delay = false;
			cnt = 0;
		}
	} else {
		manager->shutdown_delay = false;
		cnt = 0;
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

static int charger_manager_check_vindpm(struct charger_manager *manager)
{
	struct charger_dev *charger = manager->charger;
	int ret = 0;
	uint32_t vbat = manager->vbat;
	int charge_vindpm_value1;
	int charge_vindpm_value2;
#if CHARGER_VINDPM_USE_DYNAMIC
	if (manager->aicl_test == 0) {
		charge_vindpm_value1 = CHARGER_VINDPM_DYNAMIC_VALUE1;
		charge_vindpm_value2 = CHARGER_VINDPM_DYNAMIC_VALUE2;
	} else {
		charge_vindpm_value1 = manager->aicl_test;
		charge_vindpm_value2 = manager->aicl_test;
	}
	if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT1) {
		ret = charger_set_input_volt_lmt(charger, charge_vindpm_value1);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT2) {
		ret = charger_set_input_volt_lmt(charger, charge_vindpm_value2);
	} else {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE4);
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

static void charger_manager_check_iindpm(struct charger_manager *manager)
{
	int ichg_ma = 0;
	int icl_ma = 0;

	switch (manager->real_type) {
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
		if (manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].active_status) {
			ichg_ma = manager->xm_outdoor_current;
			icl_ma = manager->xm_outdoor_current;
			lx_info("xm_outdoor_current ichg_ma = %d!\n",ichg_ma);
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
	case VBUS_TYPE_PD:
	case VBUS_TYPE_PD_PPS:
	case VBUS_TYPE_MI_PPS:
		if (manager->pd_curr_max != 0) {
			if (manager->pd_volt_max == 5000) {  //C-to-C
				ichg_ma = manager->pd_curr_max;
				icl_ma = manager->pd_curr_max;
			} else {  //PD2.0
				icl_ma = min(manager->pd_curr_max, 2000);
				ichg_ma = icl_ma * PD20_ICHG_MULTIPLE / 1000;  //1.8 of fixed current
			}
		} else {
			ichg_ma = manager->dcp_current;
			icl_ma = manager->dcp_current;
		}
		lx_err("pd_max = %dmv %dma, input_ma = %d, ichg_ma = %d\n",
			manager->pd_volt_max, manager->pd_curr_max, icl_ma, ichg_ma);
		break;
	default:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	}

	if (is_mtbf_mode_func() && (manager->real_type == VBUS_TYPE_SDP || manager->real_type == VBUS_TYPE_CDP)) {
		ichg_ma = MTBF_CURRENT;
		icl_ma = MTBF_CURRENT;
		lx_info("is_mtbf_mode=%d icl=%d ichg=%d\n", is_mtbf_mode_func(), icl_ma, ichg_ma);
	}

	if (manager->delay_iindpm) {
		msleep(1500);
		lx_info("delay 1.5s for typec authenticate\n");
		manager->delay_iindpm = false;
	}

	lx_info("icl_ma = %d, ichg_ma = %d\n",icl_ma, ichg_ma);
	vote(manager->input_limit_votable, CHARGER_TYPE_VOTER, true, icl_ma);
	/* IINDPM bits are changed automatically after input source type detection is completed. So need rerun_election */
	rerun_election(manager->input_limit_votable);
	vote(manager->icharge_votable, CHARGER_TYPE_VOTER, true, ichg_ma);
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

void xm_smart_stop_charge_ctrl(struct charger_manager *manager, const char *client_str, bool en)
{
	if ((manager == NULL) || !manager->total_fcc_votable)
		return;

	if (en) {
		vote(manager->total_fcc_votable, client_str, true, 0);

		if (manager->cp_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(manager->cp_policy);
			lx_info("disable cp\n");
		}
	} else {
		vote(manager->total_fcc_votable, client_str, false, 0);

		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (manager->cp_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(manager->cp_policy);
			lx_info("enable cp\n");
		}
	}
}
EXPORT_SYMBOL(xm_smart_stop_charge_ctrl);

static int reset_vote(struct charger_manager *manager)
{
	lx_info("reset all vote!\n");
	vote_clean(manager->icharge_votable);
	vote_clean(manager->total_fcc_votable);
	vote_clean(manager->input_limit_votable);
	vote_clean(manager->iterm_votable);
	vote_clean(manager->fv_votable);
	return 0;
}

__maybe_unused
static int rerun_vote(struct charger_manager *manager)
{
	rerun_election(manager->total_fcc_votable);
	rerun_election(manager->icharge_votable);
	rerun_election(manager->input_limit_votable);
	return 0;
}

static void charger_manager_bc12_retry_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, bc12_retry_work.work);

	lx_info("enter!\n");
	if (manager->bc12_retry_count <= 6) {
		manager->bc12_retry_count++;
		lx_info("retry count = %d\n", manager->bc12_retry_count);
		if (manager->charger->bc12_type != VBUS_TYPE_FLOAT &&
			manager->charger->bc12_type != VBUS_TYPE_NON_STAND &&
			manager->charger->bc12_type != VBUS_TYPE_NONE) {
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

static void lxchg_type_detect(struct charger_manager *manager)
{
	static bool cpu_qos_request = false;

	if (manager->charger_online) {
		if (manager->pd_active == CHARGE_PD_PPS_ACTIVE &&
				manager->charger->bc12_type != VBUS_TYPE_DCP) {
			if (!manager->bc12_retry_for_mi_pd) {
				charger_force_dpdm(manager->charger);
				manager->bc12_retry_for_mi_pd = true;
				lx_info("Retry for bc_type!\n");
			}
		}

		if (!manager->is_pr_swap) {
			switch (manager->charger->bc12_type) {
				case VBUS_TYPE_NONE:
				case VBUS_TYPE_NON_STAND:
				case VBUS_TYPE_FLOAT:
					if (manager->bc12_retry_count == 0) {
						lx_info("abnormal type! retry\n");
						schedule_delayed_work(&manager->bc12_retry_work, msecs_to_jiffies(2000));
					}
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
			manager->charger->bc12_type = VBUS_TYPE_FLOAT;

		if (manager->charger->bc12_type == VBUS_TYPE_SDP || manager->charger->bc12_type == VBUS_TYPE_CDP)
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
			lx_info("cp_policy state = %d\n", manager->cp_policy->state);
			if (manager->cp_policy->state != POLICY_RUNNING)
				chargerpump_policy_stop(manager->cp_policy);
		}

		if (manager->ldp_event) {
			if (manager->real_type == VBUS_TYPE_MI_PPS)
				chargerpump_policy_stop(manager->cp_policy);
			else if (manager->real_type == VBUS_TYPE_HVDCP) {
				charger_set_qc_term_vbus(manager->charger);
				manager->real_type = VBUS_TYPE_DCP;
			}
		}

		if (manager->auth_dev && manager->master_cp_chg && !manager->ldp_event && manager->cp_policy->state != POLICY_RUNNING) {
			if(manager->real_type == VBUS_TYPE_DCP && !manager->qc_detected && !manager->pd_active) {
				manager->qc_detected = true;
				charger_qc_identify(manager->charger, manager->qc3_support);
				schedule_delayed_work(&manager->qc_detected_retry_work, msecs_to_jiffies(3000));
			}
		}

		charger_manager_check_vindpm(manager);
		charger_manager_check_iindpm(manager);

		if(manager->jeita->jeita_handle)
			manager->jeita->jeita_handle(manager->jeita);

	}

	if (manager->pd30_source && manager->otg_stat == BUCK_OTG) {
		static int reverse_power_mode = REVCHG_NORMAL;
		if ((manager->uisoc < 15 && manager->ibat < -3000000) || manager->vbat < 3450)
			reverse_power_mode = REVCHG_LOW_POWER;
		else if (manager->uisoc < 45 && manager->ibat < -3000000)
			reverse_power_mode = REVCHG_LIMIT;
		else
			reverse_power_mode = REVCHG_NORMAL;

		if (manager->last_pdo_caps > reverse_power_mode) {
			lx_info("[REVCHG] last_pdo_caps is %d, reverse_power_mode is %d\n",
				manager->last_pdo_caps, reverse_power_mode);
			update_pdo_caps(manager, reverse_power_mode);
			manager->last_pdo_caps = reverse_power_mode;
		}
	}

	if (!cpu_qos_request && manager->real_type >= VBUS_TYPE_PD) {
		cpu_qos_request = true;
		cpu_latency_qos_add_request(&cp_qos_request, 50);
		lx_info("cpu_qos_request open\n");
	} else if (cpu_qos_request && manager->real_type == VBUS_TYPE_NONE){
		cpu_qos_request = false;
		cpu_latency_qos_remove_request(&cp_qos_request);
		lx_info("cpu_qos_request close\n");
	}
}

static const char *get_vote_effective_info(struct votable *votable, int *result)
{
	*result = get_effective_result(votable);
	return get_effective_client(votable);
}

void lxchg_dump_vote_info(struct charger_manager *manager)
{
	manager->total_effect_voter = get_vote_effective_info(manager->total_fcc_votable, &manager->vote_total_fcc);
	manager->icharge_effect_voter = get_vote_effective_info(manager->icharge_votable, &manager->vote_icharge);
	manager->input_effect_voter = get_vote_effective_info(manager->input_limit_votable, &manager->vote_input_limit);
	manager->fv_effect_voter = get_vote_effective_info(manager->fv_votable, &manager->vote_fv);
	manager->iterm_effect_voter = get_vote_effective_info(manager->iterm_votable, &manager->vote_iterm);

	lx_info("total_ffc=%d[%s], ichg=%d[%s], input=%d[%s], fv=%d[%s], iterm=%d[%s]\n",
		manager->vote_total_fcc, manager->total_effect_voter,
		manager->vote_icharge, manager->icharge_effect_voter, manager->vote_input_limit,
		manager->input_effect_voter, manager->vote_fv, manager->fv_effect_voter,
		manager->vote_iterm, manager->iterm_effect_voter);
}

static void lxchg_charge_status_update(struct charger_manager *manager)
{
	int ret;
	int state = 0, status = 0;

	ret = charger_get_chg_status(manager->charger, &state, &status);
	if (ret < 0) {
		lx_err("failed to get chg status prop\n");
		return;
	}

	if (manager->above_45_temp_term ||
		(manager->cp_policy->sm == PM_STATE_CHARGERPUMP_CC_CV && status == POWER_SUPPLY_STATUS_FULL))
		status = POWER_SUPPLY_STATUS_CHARGING;

	if(manager->soft_term_status == POWER_SUPPLY_STATUS_FULL || manager->plug_in_soc100_flag)
		status = POWER_SUPPLY_STATUS_FULL;

	if ((is_input_suspend(manager) && strcmp(get_effective_client(manager->input_limit_votable), TYPEC_SINK_VBUS_VOTER))
		|| (manager->not_charging && status == POWER_SUPPLY_STATUS_CHARGING))
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;

	if (status == POWER_SUPPLY_STATUS_FULL) {
		if (manager->term_rsoc < manager->rsoc)
			manager->term_rsoc = manager->rsoc;
	} else {
		manager->term_rsoc = 0;
	}

	manager->chg_status = status;
}

static void lxchg_monitor(struct charger_manager *manager)
{
	int ret = 0;
	uint32_t adc_buf_len = 0;
	uint8_t i = 0;
	char adc_buf[MIAN_CHG_ADC_LENGTH + 1] = {0};
	int fg_coulomb = 0;
	bool input_suspend = is_input_suspend(manager);
	int no_batcell_vol = 0;
	union power_supply_propval pval;

	power_off_check_work(manager);
	lxchg_type_detect(manager);
	lxchg_charge_status_update(manager);

	charger_get_term_curr(manager->charger, &manager->iterm);
	charger_get_term_volt(manager->charger, &manager->fv);
	charger_get_hiz_status(manager->charger, &manager->hiz_en);
	charger_manager_get_current(manager, &manager->ibus);
	charger_get_adc(manager->charger, CHG_ADC_VBUS, &manager->vbus);
	power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	no_batcell_vol = pval.intval / 1000;
	charger_get_adc(manager->charger, CHG_ADC_VBAT, &manager->vbat);
	power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	manager->ibat = -pval.intval;

	if (manager->cp_policy->sm == PM_STATE_CHARGERPUMP_CC_CV) {
		manager->ichg = manager->vote_total_fcc;
		manager->ilimit = manager->cp_policy->request_curr;
	} else {
		charger_get_ichg(manager->charger, &manager->ichg);
		charger_get_input_curr_lmt(manager->charger, &manager->ilimit);
	}
	charger_is_charge_done(manager->charger, &(manager->is_charge_done));

	fg_coulomb = fuel_gauge_get_c_car(manager->fuel_gauge);

	ktime_get_real_ts64(&manager->ts64);
	manager->tv = *(const struct timespec *)&manager->ts64;
	manager->tv.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(manager->tv.tv_sec, &manager->tm);


#ifdef FACTORY_BUILD
	if (manager->charger_online)
		ato_control_soc(manager);
#else
	manager->tm.tm_hour += 8;
	if (manager->tm.tm_hour >= 24)
		manager->tm.tm_hour -= 24;
#endif
	if(0) {
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
		//lx_info("[CHG_ADC] %s\n", adc_buf);
	}
	pr_err("[%02d-%02d %02d:%02d:%02d] vbus:%d(%d),vbat:%d(%d),tbat:%d(%d),chg_status:%s(%d|%d),vbus_type:%s(%s),ibus:%d(%d),ibat:%d(%d),cp_stat:%d(%d),term:%d|%d,hiz:%d(%d),car:%d(%d),uisoc:%d(%d)\n",
			manager->tm.tm_mon + 1,	manager->tm.tm_mday, manager->tm.tm_hour, manager->tm.tm_min, manager->tm.tm_sec,
			manager->vbus,manager->charger_online, manager->vbat, no_batcell_vol, manager->tbat,manager->thermal_board_temp,
			chg_stat[manager->chg_status], manager->cp_policy->cp_charge_done, manager->is_charge_done, vbus_type_txt[manager->real_type],
			vbus_type_txt[manager->charger->bc12_type], manager->ibus,manager->ilimit, manager->ibat/1000,manager->ichg, manager->cp_policy->state,
			manager->cp_policy->sm,manager->iterm,manager->fv,manager->hiz_en,input_suspend,fg_coulomb,manager->charge_full_car, manager->uisoc,manager->rsoc);

	lxchg_dump_vote_info(manager);
}

static int charger_manager_thread_fn(void *data)
{
	struct charger_manager *manager = data;
	int ret = 0;
	int loop_time = 0;

	while (true) {
		ret = wait_event_interruptible(manager->wait_queue,
							manager->run_thread);
		if (kthread_should_stop() || ret) {
			lx_err("exits(%d)\n", ret);
			break;
		}

		manager->run_thread = false;

		lxchg_monitor(manager);

		if (manager->charger_online) {
			loop_time = CHARGER_MANAGER_LOOP_TIME;
			if (manager->tbat < TEMP_LEVEL_NEGATIVE_7 || manager->tbat > TEMP_LEVEL_42)
				loop_time = CHARGER_MANAGER_LOOP_TIME_LOW_TEMP;
		} else {
			loop_time = CHARGER_MANAGER_LOOP_TIME_OUT;
		}

		charger_manager_start_timer(manager, loop_time);
	}
	return 0;
}

static int charger_manager_notifer_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_nb);
	static bool cycle_init = true;
	int cycle_count = 0;
	int ret = 0;

	switch (event) {
		case CHARGE_EVENT_PLUG_IN:
			lx_info("adapter plug in\n");
			vote(manager->input_limit_votable, CHARGER_TYPE_VOTER, true, 100);
			manager->charger_online = true;
			manager->delay_iindpm = true;
			manager->jeita->plugin_normal_index = -1;
			manager->jeita->plugin_ffc_index = -1;
			charger_force_dpdm(manager->charger);
			pm_stay_awake(manager->dev);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			if ((manager->uisoc <= 20) && (manager->thermal_board_temp <= 390)) {
				manager->low_fast_plugin_flag = true;
			}
			schedule_delayed_work(&manager->xm_charge_work, msecs_to_jiffies(3000));
#endif
			manager->qc_detected = false;
			charger_set_term(manager->charger, true);
			if (manager->is_eea && manager->uisoc == 100) {
				vote(manager->total_fcc_votable, SOFT_ITERM_VOTER, true, 0);
				manager->plug_in_soc100_flag = true;
				manager->chg_status = POWER_SUPPLY_STATUS_FULL;
			} else
				manager->chg_status = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case CHARGE_EVENT_PLUG_OUT:
			lx_info("adapter plug out\n");
			manager->charger_online = false;
			manager->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			manager->delay_iindpm = false;
			reset_vote(manager);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			cancel_delayed_work(&manager->xm_charge_work);
			manager->low_fast_plugin_flag = false;
			manager->pps_fast_mode = false;
			manager->b_flag = DEFAULT_STAT;
			manager->fv_overvoltage_flag = false;
#endif
			manager->soft_term_status = POWER_SUPPLY_STATUS_DISCHARGING;
			manager->bc12_retry_for_mi_pd = false;
			manager->plug_in_soc100_flag = false;
			manager->above_45_temp_term = false;
			manager->jeita->normal_curr_shake = false;
			manager->jeita->fcc_curr_shake = false;
			manager->ldp_event = false;
			manager->eea_is_term = false;
			manager->not_charging = false;
			manager->temp_compensate = 0;
			manager->apdo_max = 0;
			manager->soft_reset_state = false;
			cancel_delayed_work_sync(&manager->qc_detected_retry_work);
			cancel_delayed_work_sync(&manager->bc12_retry_work);
			manager->bc12_retry_count = 0;
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			if (!IS_ERR_OR_NULL(manager->fuel_gauge))
				fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
			#endif
			chargerpump_policy_stop(manager->cp_policy);
			pm_relax(manager->dev);
			break;
		case THERMAL_EVENT_BOARD_TEMP:
			if (manager->fake_board_thermal == 9999)
				manager->thermal_board_temp = *(int *)data/100;
			break;
		case CHARGE_EVENT_CID_PLUGIN:
			manager->cid_status = true;
			break;
		case CHARGE_EVENT_CID_PLUGOUT:
			manager->cid_status = false;
			if (manager->otg_stat != DIS_OTG) {
				struct tcp_ny_vbus_state vbus_state;
				vbus_state.mv = 0;
				lx_err("otg tcpc msg not receive, use cid plug out!!\n");
				ret = charger_manager_set_source_vbus(manager, vbus_state);
				if (ret)
					lx_err("charger_manager_set_source_vbus err!!!\n");
			}
			break;
		case CHARGE_EVENT_SOFT_TERM:
			lx_info("soft iterm! fv = %d\n", manager->vote_fv);
			if (manager->vote_fv <= TEMP_45_TO_58_FV) {
				vote(manager->total_fcc_votable, SOFT_ITERM_VOTER, true, 0);
				manager->above_45_temp_term = true;
				manager->soft_term_check_cnt = 0;
			} else {
				manager->rsoc_keep100_sec = RSOC_KEEP100_SECOND;//30min+
				if (manager->is_eea)
					manager->uisoc_keep100_sec = UISOC_KEEP100_SECOND; //10min
				smoothed_uisoc_update(manager);
				smoothed_rsoc_update(manager);
				msleep(2000);
				vote(manager->total_fcc_votable, SOFT_ITERM_VOTER, true, 0);
				manager->soft_term_status = POWER_SUPPLY_STATUS_FULL;
				manager->soft_term_check_cnt = 0;
			}
			break;
		case CHARGE_EVENT_SOFT_RECHG:
			lx_info("soft recharge!\n");
			vote(manager->total_fcc_votable, SOFT_ITERM_VOTER, false, 0);
			charger_enable_chg(manager->charger, false);
			msleep(200);
			charger_enable_chg(manager->charger, true);
			manager->soft_term_status = POWER_SUPPLY_STATUS_CHARGING;
			manager->plug_in_soc100_flag = false;
			manager->above_45_temp_term = false;
			break;
		case BATTERY_EVENT_CYCLE_CHANGED:
			if (!manager->auth_dev || !manager->mtk_gauge)
				break;
			if (cycle_init) {
				cycle_init = false;
				manager->boot_gm_cycle = manager->mtk_gauge->gm->bat_cycle;
			}
			auth_device_get_cycle_count(manager->auth_dev, &manager->batt_cycle);
			cycle_count = manager->batt_cycle + (manager->mtk_gauge->gm->bat_cycle - manager->boot_gm_cycle);
			auth_device_set_cycle_count(manager->auth_dev, cycle_count, manager->batt_cycle);
			auth_device_get_cycle_count(manager->auth_dev, &manager->batt_cycle);
			lx_info("cycle_count = %d(%d)\n", cycle_count, manager->batt_cycle);
			break;
		case CHARGE_EVENT_LPD_OCCUR:
			lx_info("LPD event is occur\n");
			manager->ldp_event = true;
			break;
		default:
			break;
	}

	power_supply_changed(manager->usb_psy);
	power_supply_changed(manager->batt_psy);
	power_supply_changed(manager->chg_psy);

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

static int lxchg_check_core_device(struct charger_manager *manager)
{
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!manager->tcpc) {
		lx_err("tcpc device not ready, defer\n");
		return -EPROBE_DEFER;
	}
#endif

#if IS_ENABLED(CONFIG_MIEV)
	xm_chg_dfs_init(manager);
#endif

	manager->charger = charger_find_dev_by_name("primary_chg");
	if (!manager->charger) {
		lx_err("failed to primary_chg device\n");
	}
	manager->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (!manager->master_cp_chg) {
#if IS_ENABLED(CONFIG_MIEV)
		xmdfs_notifier_call_chain(CHG_DFX_CP_I2C_ERR, NULL);
#endif
		hardwareinfo_set_prop(HARDWARE_SUB_CHARGER_MASTER, "unknow");
		lx_err("failed to master_cp_chg device\n");
	}

	manager->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	if (!manager->slave_cp_chg)
		lx_err("failed to slave_cp_chg device\n");

	manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!manager->fuel_gauge) {
		lx_err("failed to fuel_gauge device\n");
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

	manager->mtk_gauge_psy = power_supply_get_by_name("mtk-gauge");
	if (manager->mtk_gauge_psy)
		manager->mtk_gauge = power_supply_get_drvdata(manager->mtk_gauge_psy);
	else
		lx_err("failed to get mtk_gauge psy\n");

	manager->auth_dev = get_batt_auth_by_name("secret_ic");
	if (!manager->auth_dev) {
		lx_err("failed to get battery auth device\n");
	}
	return 0;	
}

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

void cm_check_bootmode(struct charger_manager *manager)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(manager->dev->of_node, "bootmode", 0);
	if (!boot_node)
		lx_err("failed to get boot mode phandle\n");
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			lx_err("failed to get atag,boot\n");
		else {
			lx_info("size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			manager->bootmode = tag->bootmode;
		}
	}
}

#define COUNTRY_CODE_LENS	10

static int get_countrycode_from_dtb(struct charger_manager *manager)
{
	struct device_node *proj_info_node;
	const char *read_countrycode;
	char *ptr=NULL;
	int res_len=-1;
	char countrycode_word[]="androidboot.countrycode_region=";
	char countrycode[COUNTRY_CODE_LENS];

	proj_info_node  = of_find_node_by_name(NULL,"chosen");
	if (NULL == proj_info_node) {
		lx_err("[charger]:device chosen node not exist.");
		return res_len;
	}

	res_len = of_property_read_string(proj_info_node,"countrycode",&read_countrycode);
	if (res_len != 0) {
		lx_err("[charger]:device bootargs read fail:res_len=%d.",res_len);
		return res_len;
	}
	lx_err("[charger]:get chosen node and hwinfo success, hwinfo=%s\n",read_countrycode);

	ptr = strstr(read_countrycode, countrycode_word);
	if (ptr != NULL) {
		strncpy(countrycode, ptr+strlen(countrycode_word), 9); //copy the result to pointer
		countrycode[9] = '\0';
		lx_err("[charger] get result %s.\n", countrycode);
		if (!strncmp(countrycode, "eea", 3))
			manager->is_eea = true;
		return 0;
	}

	lx_err("[charger]strstr get pointer countrycode failed.");
	return -1;
}

static int charger_manager_parse_dts(struct charger_manager *manager)
{
	struct device_node *node = manager->dev->of_node;
	int ret = false;
	ret |= of_property_read_string(node, "lxchg_manager,model_name", &manager->model_name);
	ret |= of_property_read_u32(node, "lxchg_manager,qc3_support", &manager->qc3_support);
	ret |= of_property_read_u32(node, "lxchg_manager,usb_charger_current", &manager->usb_current);
	ret |= of_property_read_u32(node, "lxchg_manager,usb_iterm", &manager->usb_iterm);
	ret |= of_property_read_u32(node, "lxchg_manager,float_charger_current", &manager->float_current);
	ret |= of_property_read_u32(node, "lxchg_manager,ac_charger_current", &manager->dcp_current);
	ret |= of_property_read_u32(node, "lxchg_manager,cdp_charger_current", &manager->cdp_current);
	ret |= of_property_read_u32(node, "lxchg_manager,hvdcp_charger_current", &manager->hvdcp_charge_current);
	ret |= of_property_read_u32(node, "lxchg_manager,hvdcp_input_current", &manager->hvdcp_input_current);
	ret |= of_property_read_u32(node, "lxchg_manager,hvdcp3_charger_current", &manager->hvdcp3_charge_current);
	ret |= of_property_read_u32(node, "lxchg_manager,hvdcp3_input_current", &manager->hvdcp3_input_current);
	ret |= of_property_read_u32(node, "lxchg_manager,input_power_over", &manager->input_power_over);
	ret |= of_property_read_u32(node, "lxchg_manager,xm_outdoor_current", &manager->xm_outdoor_current);
	manager->support_ui_otg = of_property_read_bool(node, "lxchg_manager,support_ui_otg");
	lx_info("support_ui_otg is %d\n", manager->support_ui_otg);
	cm_check_bootmode(manager);
	get_countrycode_from_dtb(manager);

	if (ret)
		return false;
	else
		return true;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int screen_on_for_charger_callback(struct notifier_block *nb,
                                            unsigned long val, void *v)
{
	int blank = *(int *)v;
	struct charger_manager *manager = container_of(nb, struct charger_manager, sm.charger_panel_notifier);

	if (!(val == MTK_DISP_EARLY_EVENT_BLANK|| val == MTK_DISP_EVENT_BLANK)) {
		lx_err("event(%lu) do not need process\n", val);
		return NOTIFY_OK;
	}

	switch (blank) {
		case MTK_DISP_BLANK_UNBLANK: //screen on
			manager->sm.screen_on = 1;
			lx_info("screen on\n");
			break;
		case MTK_DISP_BLANK_POWERDOWN: //screen off
			manager->sm.screen_on = 0;
			lx_info("screen off\n");
			break;
	}
	return NOTIFY_OK;
}
#endif
static void lxchg_device_init(struct charger_manager *manager)
{
	lxchg_usb_psy_register(manager);
	lxchg_battery_psy_register(manager);
	lxchg_charger_psy_register(manager);
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

	lx_info("running (%s), cnt = %d\n", CHARGER_MANAGER_VERSION, ++probe_cnt);

	manager = devm_kzalloc(&pdev->dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->dev = &pdev->dev;
	g_manager = manager;

	platform_set_drvdata(pdev, manager);

	ret = lxchg_check_core_device(manager);
	if (ret == -EPROBE_DEFER) {
		if (probe_cnt >= PROBE_CNT_MAX) {
			lx_err("failed to get core device!!\n");
			ret = -ENOMEM;
		}
		devm_kfree(manager->dev, manager);
		return ret;
	}

	ret = charger_manager_parse_dts(manager);
	if (!ret)
		lx_err("charger_manager_parse_dts failed\n");

	charger_manager_create_votable(manager);
	lxchg_device_init(manager);

	manager->max_uisoc = 10000;
	manager->uisoc = -1;
	manager->fake_batt_cycle = 0xFFFF;
	manager->fake_tbat = 0xFFFF;
	manager->fake_soc= 0xFFFF;
	lx_jeita_init(manager);
	manager->aicl_test = 0;

	ret = charge_manager_thermal_init(manager);
	if (ret < 0)
		lx_err("charge_manager_thermal_init failed, ret = %d\n", ret);

	init_waitqueue_head(&manager->wait_queue);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	INIT_DELAYED_WORK(&manager->xm_charge_work, xm_charge_work);

	manager->thermal_board_temp = 250;
	manager->fake_board_thermal = 9999;
	manager->charger_nb.notifier_call = charger_manager_notifer_call;
	lxchg_notifier_register(&manager->charger_nb);

	manager->sm.charger_panel_notifier.notifier_call = screen_on_for_charger_callback;
	ret = mtk_disp_notifier_register("screen state", &manager->sm.charger_panel_notifier);
	if (ret) 
		lx_err("register screen state callback fail(%d)\n", ret);

	mutex_init(&manager->report_lock);

	INIT_DELAYED_WORK(&manager->thermal_restore_work, thermal_restore_work);
	schedule_delayed_work(&manager->thermal_restore_work, msecs_to_jiffies(60000));
#endif //CONFIG_XIAOMI_SMART_CHG

	INIT_DELAYED_WORK(&manager->bc12_retry_work, charger_manager_bc12_retry_work);

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	INIT_DELAYED_WORK(&manager->wait_usb_ready_work, wait_usb_ready_work);
#endif //CONFIG_USB_MTK_HDRC

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
	INIT_DELAYED_WORK(&manager->hrtime_toggle_work, hrtime_toggle_work_handler);
	INIT_DELAYED_WORK(&manager->set_cc_drp_work, set_cc_drp_work);
	INIT_DELAYED_WORK(&manager->reverse_quick_charge_work, reverse_quick_charge_work_handler);
	manager->reverse_charge_wakelock = wakeup_source_register(manager->dev, "reverse_charge suspend wakelock");
#endif //CONFIG_TCPC_CLASS
	device_init_wakeup(manager->dev, true);
	manager->run_thread = true;
	manager->thread = kthread_run(charger_manager_thread_fn, manager,
								"charger_manager_thread");
#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
	ret = lx_fuel_algo_init(manager);
	if (ret < 0)
		lx_err("lx_fuel_algo_init fail\n");

#endif //CONFIG_LIXUN_FUEL_ALGORITHM

	INIT_DELAYED_WORK(&manager->qc_detected_retry_work, qc_detected_retry_work_handler);
	INIT_DELAYED_WORK(&manager->smooth_work, smooth_work_handler);
	schedule_delayed_work(&manager->smooth_work, msecs_to_jiffies(5000));
	/*****************for shipmode**********************
	 *Register syscore ops, just for, when restart in shipmode,
	 *it will reproduce any device shutdown fail
	 *So first, do device_shutdown, and then syscore ops->shutdown
	 */
	register_syscore_ops(&shipmode_syscore_ops);
	if (manager->auth_dev) {
		int batt_id;
		batt_id = manager->auth_dev->battery_id;
		lx_err("batt_id is %d\n", batt_id);
		if (batt_id == 0) {
			  hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_1");
		} else if (batt_id == 1) {
			  hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_2");
		} else if (batt_id == 2) {
			  hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_3");
		} else {
			  hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_UNKNOWN");
		}
	} else {
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_UNKNOWN");
	}
	lx_err("HARDWARE_BATTERY_ID is set!\n");


	lx_err("success\n");
	return 0;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	cancel_delayed_work(&manager->xm_charge_work);
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

	if(manager->charger->bc12_type == VBUS_TYPE_HVDCP){
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
	charger_reset(manager->charger);
	charger_adc_enable(manager->charger, false);
	chargerpump_set_enable_adc(manager->master_cp_chg, false);
	chargerpump_set_enable_adc(manager->slave_cp_chg, false);
}

static int charger_manager_suspend(struct device *dev)
{
	struct charger_manager *manager = (struct charger_manager *)dev_get_drvdata(dev);

	lx_info("enter");
	charger_adc_enable(manager->charger, false);
	return 0;
}

static int charger_manager_resume(struct device *dev)
{
	struct charger_manager *manager = (struct charger_manager *)dev_get_drvdata(dev);

	lx_info("enter");
	charger_adc_enable(manager->charger, true);
	return 0;
}

static const struct dev_pm_ops charger_manager_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(charger_manager_suspend, charger_manager_resume)
};

static const struct of_device_id charger_manager_match[] = {
	{.compatible = "lixun,lxchg_manager",},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct platform_driver charger_manager_driver = {
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.driver = {
		.name = "lxchg_manager",
		.of_match_table = charger_manager_match,
		.pm = &charger_manager_pm_ops,
	},
};

void lxchg_devices_init(void)
{
	lxchg_class_init();

	ds28e30_secret_init();
	slg_secret_init();
	stick_secret_init();

	bq28z610_fg_init();
	mt6358_fg_init();

	sc6601_base_init();
	sc6601_charger_init();
	sc6601_cid_init();

	nu6601_base_init();
	nu6601_charger_init();

	sc8541_chargepump_init();
	bq25960_chargepump_init();
}

static int __init charger_manager_init(void)
{
	lx_info("enter\n");
	lxchg_devices_init();
	return platform_driver_register(&charger_manager_driver);
}

module_init(charger_manager_init);

MODULE_DESCRIPTION("LiXun Charger Manager Core");
MODULE_LICENSE("GPL v2");
