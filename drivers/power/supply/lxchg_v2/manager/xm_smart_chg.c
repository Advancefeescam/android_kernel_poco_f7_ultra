// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Xiaomi Inc.
 * Author Tianye<tianye9@xiaomi.com>
 */
#include <linux/delay.h>
#include "lxchg_voter.h"
#include "xm_smart_chg.h"
#include "lxchg_printk.h"
#include "lx_cp_policy.h"
#if IS_ENABLED(CONFIG_MIEV)
#include "xmchg_dfs.h"
#endif
#ifdef TAG
#undef TAG
#define  TAG "[XM_SMART_CHG]"
#endif

#define XM_SMART_CHG_DEBUG 0

void set_error(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 1;
	lx_err("xm %s en_ret=%d\n", manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

void set_success(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 0;
	lx_err("xm %s en_ret=%d\n", manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

int smart_chg_is_error(struct charger_manager *manager)
{
	return manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret? true : false;
}

void handle_smart_chg_functype(struct charger_manager *manager,
	const int func_type, const int en_ret, const int func_val)
{
	switch (func_type)
	{
	case SMART_CHG_FEATURE_MIN_NUM ... SMART_CHG_FEATURE_MAX_NUM:
		manager->smart_charge[func_type].en_ret = en_ret;
		manager->smart_charge[func_type].active_status = false;
		manager->smart_charge[func_type].func_val = func_val;
		set_success(manager);
		lx_err("xm set func_type:%d, en_ret = %d\n", func_type, en_ret);
		break;
	default:
		lx_err("xm ERROR: Not supported func type: %d\n", func_type);
		set_error(manager);
		break;
	}
}

int handle_smart_chg_functype_status(struct charger_manager *manager)
{
	int i;
	int all_func_status = 0;
	all_func_status |= !!manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret;	//handle bit0

	lx_err("smart_chg: all_func_status =%#X, en_ret=%d\n",all_func_status, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);

	/* save functype[i] enable status in all_func_status bit[i] */
	for(i = SMART_CHG_FEATURE_MIN_NUM; i <= SMART_CHG_FEATURE_MAX_NUM; i++){  //handle bit1 ~ bit SMART_CHG_FEATURE_MAX_NUM
		if(manager->smart_charge[i].en_ret)
			all_func_status |= BIT_MASK(i);
		else
			all_func_status &= ~BIT_MASK(i);

		lx_err("smart_chg: type:%d, en_ret=%d, active_status=%d,func_val=%d, all_func_status=%#X\n",
			i, manager->smart_charge[i].en_ret, manager->smart_charge[i].active_status, manager->smart_charge[i].func_val,all_func_status);
	}
	lx_err("smart_chg: all_func_status:%#X\n", all_func_status);
	return all_func_status;
}

static void smart_chg_navigaition_discharge_func(struct charger_manager *manager)
{
	bool navigaition_ctrl = is_disable_chg_by_client(manager, NAVIGAITION_VOTER);

	lx_info("en_ret = %d, fun_val = %d, active_status = %d, ui_soc =%d\n",
		manager->smart_charge[SMART_CHG_NAVIGATION].en_ret,
		manager->smart_charge[SMART_CHG_NAVIGATION].func_val,
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status,
		manager->uisoc);

	if(!navigaition_ctrl && manager->smart_charge[SMART_CHG_NAVIGATION].en_ret && manager->uisoc >= manager->smart_charge[SMART_CHG_NAVIGATION].func_val) {
		lx_info("disable charge\n");
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status = true;
		xm_smart_stop_charge_ctrl(manager, NAVIGAITION_VOTER, true);
#if IS_ENABLED(CONFIG_MIEV)
		xmdfs_notifier_call_chain(CHG_DFX_SMART_NAVI_TRIG, NULL);
#endif
	} else if(navigaition_ctrl && (!manager->smart_charge[SMART_CHG_NAVIGATION].en_ret || manager->uisoc <= (manager->smart_charge[SMART_CHG_NAVIGATION].func_val - 5))) {
		lx_info("enable charge\n");
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status = false;
		xm_smart_stop_charge_ctrl(manager, NAVIGAITION_VOTER, false);
	}
}


static void smart_chg_endurance_pro_handler(struct charger_manager *manager)
{
	bool endurance_ctrl = is_disable_chg_by_client(manager, ENDURANCE_VOTER);

	lx_info("en_ret = %d, fun_val = %d, active_status = %d, ui_soc =%d\n",
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret,
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val,
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status,
		manager->uisoc);

	if(!endurance_ctrl && manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && manager->uisoc >= manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) {
		lx_info("disable charge\n");
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
		xm_smart_stop_charge_ctrl(manager, ENDURANCE_VOTER, true);
	#if IS_ENABLED(CONFIG_MIEV)
		xmdfs_notifier_call_chain(CHG_DFX_SMART_ENDURA_TRIG, NULL);
	#endif
	} else if(endurance_ctrl && (!manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret || manager->uisoc <= (manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val - 5))) {
		lx_info("enable charge\n");
		manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
		xm_smart_stop_charge_ctrl(manager, ENDURANCE_VOTER, false);
	}
}

static void smart_chg_low_fast_handler(struct charger_manager *manager)
{
	bool fast_flag = false;
	time64_t time_now = 0, delta_time = 0;
	static time64_t time_last = 0;
	static int last_level = 0;
	static bool hot_flag = false;

	if (manager->system_temp_level <= 0)
		goto err;

	lx_info("en_ret = %d, fun_val = %d, active_status = %d\n",
		manager->smart_charge[SMART_CHG_LOW_FAST].en_ret,
		manager->smart_charge[SMART_CHG_LOW_FAST].func_val,
		manager->smart_charge[SMART_CHG_LOW_FAST].active_status);


	lx_info("soc = %d, thermal_level = %d, thermal_board_temp = %d, pd_active = %d, low_fast_plugin_flag = %d, low_fast_enable = %d, screen_on = %d, b_flag = %d\n", 
                manager->uisoc, manager->system_temp_level, manager->thermal_board_temp, manager->pd_active, manager->low_fast_plugin_flag, manager->smart_charge[SMART_CHG_LOW_FAST].active_status, 
                manager->sm.screen_on, manager->b_flag);
	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (manager->uisoc <= 40) && (manager->low_fast_plugin_flag) && manager->smart_charge[SMART_CHG_LOW_FAST].en_ret) {
		if (manager->thermal_parse_flags & FFC_THERM_PARSE_ERROR) {
			lx_err("pd thermal dtsi parse error\n");
			goto err;
		}
		if (manager->system_temp_level > manager->ffc_thermal_levels) {
			lx_err("system_temp_level is invalid\n");
			goto err;
		}

		/*manager->sm.screen_on 1:bright, 0:black*/
		if (((manager->b_flag == DEFAULT_STAT) || (manager->b_flag == BLACK)) && manager->sm.screen_on) {  //black to bright
			manager->b_flag = BLACK_TO_BRIGHT;
			time_last = ktime_get_seconds();
			fast_flag = true;
			lx_err("switch to bright time_last = %d\n", time_last);
		} else if ((manager->b_flag == BLACK_TO_BRIGHT || manager->b_flag == BRIGHT) && manager->sm.screen_on) {  //still bright
			manager->b_flag = BRIGHT;
			time_now = ktime_get_seconds();
			delta_time = time_now - time_last;
			lx_err("still_bright time_now = %d, time_last = %d, delta_time = %d\n", time_now, time_last, delta_time);
			if (delta_time <= 10) {
				fast_flag = true;
				lx_err("still_bright delta_time = %d, stay fast\n", delta_time);
			} else {
				fast_flag = false;
				lx_err("still_bright delta_time = %d, exit fast\n", delta_time);
			}
		} else { //black
			manager->b_flag = BLACK;
			fast_flag = true;
			lx_err("black stay fast\n", delta_time);
		}
		/*avoid thermal_board_temp raise too fast*/
		if ((last_level == 8) && (manager->system_temp_level == 7) && (manager->thermal_board_temp > 410)){
			hot_flag = true;
			fast_flag = false;
			lx_err("avoid thermal_board_temp raise too fast, exit fast mode\n");
		} else if ((last_level == 7) && ((manager->system_temp_level == 7) || (manager->system_temp_level == 8)) && hot_flag && (manager->thermal_board_temp > 410)){
			fast_flag = false;
		} else {
			hot_flag = false;
		}

		if ((manager->thermal_board_temp > 420)){
			fast_flag = false;
		}

		if (fast_flag) {  //stay fast strategy
			manager->pps_fast_mode = true;
			manager->low_fast_ffc = manager->xmchg_low_soc_fast[manager->system_temp_level];
			vote(manager->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
			manager->smart_charge[SMART_CHG_LOW_FAST].active_status = true;
			lx_err("stay fast, low_fast_ffc = %d\n", manager->low_fast_ffc);
		}else { //exit fast strategy
			manager->pps_fast_mode = false;
			manager->low_fast_ffc = manager->ffc_thermal_mitigation[manager->system_temp_level];
			vote(manager->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
			manager->smart_charge[SMART_CHG_LOW_FAST].active_status = false;
			lx_err("exit fast, low_fast_ffc = %d\n", manager->low_fast_ffc);
		}
		last_level = manager->system_temp_level;
	}
	return;
err:
	vote(manager->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, manager->ffc_thermal_mitigation[manager->system_temp_level]);
	last_level = manager->system_temp_level;
	return;
}

static void smart_chg_outdoor_charge_handler(struct charger_manager *manager)
{
	lx_info("set smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret = %d\n",
		manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret);

	if (manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret) {
		manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].active_status = true;
	} else if (!manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret) {
		manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].active_status = false;
	}
}

void smart_chg_night_charging(struct charger_manager *manager)
{
	bool night_charging_limit = false;

	if ((manager == NULL) || !manager->icharge_votable)
		return;

	night_charging_limit = is_disable_chg_by_client(manager, NIGHT_CHARGING_VOTER);

	lx_info("night_charging = %d, soc = %d, limit = %d\n", manager->night_charging, manager->uisoc, night_charging_limit);
	if (!night_charging_limit && manager->night_charging && manager->uisoc >= 80) {
		lx_info("disable charge\n");
		xm_smart_stop_charge_ctrl(manager, NIGHT_CHARGING_VOTER, true);
	} else if(night_charging_limit && (!manager->night_charging || manager->uisoc <= 75)) {
		lx_info("enable charge\n");
		xm_smart_stop_charge_ctrl(manager, NIGHT_CHARGING_VOTER, false);
	}
}

static void monitor_fv_overvoltage(struct charger_manager *manager)
{
	static int fv_overvoltage_count = 0;
	int fcc_vote = get_effective_result(manager->total_fcc_votable);
	int fv_vote = get_effective_result(manager->fv_votable);

	lx_err("%d:%d:%d:%d:%d:%d\n", manager->fv_overvoltage_flag, fv_overvoltage_count, manager->vbat, fv_vote, manager->pd_active, fcc_vote);

	if((manager->vbat > fv_vote) && (manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (!manager->fv_overvoltage_flag) && (fcc_vote >= 3000)) {
		fv_overvoltage_count++;
		if(fv_overvoltage_count > 1000)
			fv_overvoltage_count = 3;
	}
	if(!manager->fv_overvoltage_flag && (fv_overvoltage_count >= 3) && (manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (fcc_vote >= 3000)) {
		lx_err("enable, avg_ibat = %d(%d)\n", manager->avg_ibat, manager->ibat);
		manager->fv_overvoltage_flag = true;
		fv_overvoltage_count = 0;
		fcc_vote = manager->avg_ibat - 1000;
		if (fcc_vote < 2000)
			fcc_vote = 2000;
		vote(manager->total_fcc_votable, FV_OVERVOLTAGE_VOTER,true, fcc_vote);
	}
	if(manager->fv_overvoltage_flag && (manager->pd_active != CHARGE_PD_PPS_ACTIVE))
	{
		lx_err("disable\n");
                fv_overvoltage_count = 0;
		manager->fv_overvoltage_flag = false;
		vote(manager->total_fcc_votable, FV_OVERVOLTAGE_VOTER,false, 0);
	}
}

static void monitor_smart_batt_fv(struct charger_manager *manager)
{
        int value = 0,diff_fv_val = 0;

        value = get_client_vote_locked(manager->fv_votable, JEITA_VOTER);

        diff_fv_val = max(manager->smart_batt, manager->smart_fv);
        value = value - diff_fv_val;

        vote(manager->fv_votable, SMART_BATT_VOTER, true, value);
        lx_err("smart_batt:%d, smart_fv:%d, diff_fv_val:%d, new value:%d\n", manager->smart_batt, manager->smart_fv, diff_fv_val, value);
}

void xm_charge_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work, struct charger_manager, xm_charge_work.work);
	if (manager == NULL)
		return;
	smart_chg_navigaition_discharge_func(manager);
	smart_chg_endurance_pro_handler(manager);
	smart_chg_low_fast_handler(manager);
	smart_chg_outdoor_charge_handler(manager);
	smart_chg_night_charging(manager);
	monitor_fv_overvoltage(manager);
	monitor_smart_batt_fv(manager);

	schedule_delayed_work(&manager->xm_charge_work, msecs_to_jiffies(1000));
}

void thermal_restore_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work, struct charger_manager, thermal_restore_work.work);

	if (manager == NULL)
		return;

	/*
	 * always use TEMP_THERMAL_DAEMON_VOTER within 60s of kernel bootup
	 * so use TEMP_THERMAL_DAEMON_VOTER here to restore thermal level
	 */
	lx_info("restore thermal level after kernel bootup 60s\n");
	lx_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
}
