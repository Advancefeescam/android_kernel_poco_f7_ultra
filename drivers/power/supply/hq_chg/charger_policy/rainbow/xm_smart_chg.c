// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */
#include "hq_charger_manager.h"
#include "xm_smart_chg.h"
#include "hq_printk.h"
#include "hq_notify.h"

#define xm_err     hq_err
#define xm_warn    hq_warn
#define xm_notice  hq_notice
#define xm_info    hq_info
#define xm_debug   hq_debug

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_SMART_CHG]"
#endif

static int smart_chg_navigaition_discarge_func(struct xm_smart_chg *smart_chg)
{
	/* domestic support is not needed
	struct smart_chg_func *this = NULL;
	int soc_threshold = 0;
	int soc = 0;

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	this = &(smart_chg->funcs[SMART_CHG_NAVI_DISCHARGE]);
	soc = smart_chg->soc;
	soc_threshold = this->func_val;
	if ((this->func_on && (soc >= soc_threshold)) ||
		(((soc > soc_threshold - 5) && (soc < soc_threshold)) && this->active_flag)) {
		this->active_flag = true;
		xm_info("navigation discharge function active\n");
	} else if (((!this->func_on || (soc <= soc_threshold - 5)) && this->active_flag) ||
		(!this->func_on && !this->active_flag)) {
		this->active_flag = false;
		xm_info("navigation discharge function deactive\n");
	}

	smart_chg->stop_charge = this->active_flag;
	vote(smart_chg->total_fcc_votable, NAVIGATION_VOTER, this->active_flag, 0);
	//rerun_election(smart_chg->total_fcc_votable);
	xm_info("soc: %d, func[on: %d val: %d active: %d]\n",
		smart_chg->soc, this->func_on, this->func_val, this->active_flag);
	 */

	return 0;
}

static int smart_chg_outdoor_charge_func(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *this = NULL;
	struct charger_manager *manager = NULL;

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	manager = dev_get_drvdata(smart_chg->dev);

	this = &(smart_chg->funcs[SMART_CHG_OUTDOOR_CHARGE]);

	if (this->func_on && (smart_chg->vbus_type == VBUS_TYPE_DCP)) {
		this->active_flag = true;
		manager->outdoor_chg = true;
		xm_debug("outdoor charge function active\n");
	} else {
		this->active_flag = false;
		manager->outdoor_chg = false;
		xm_debug("outdoor charge function deactive\n");
	}

	xm_info("vbus_type: %d, pd_active: %d, func[on: %d val: %d active: %d]\n",
		smart_chg->vbus_type, smart_chg->pd_active,
		this->func_on, this->func_val, this->active_flag);

	return 0;
}

static int smart_chg_low_battery_fast_charge_func(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *this = NULL;
	time64_t time_now = 0, delta_time = 0;
	static time64_t time_last = 0;

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	this = &(smart_chg->funcs[SMART_CHG_LOW_BATT_FAST_CHG]);

	if (this->func_on) {
		if ((smart_chg->pd_active == CHARGE_PD_PPS_ACTIVE) &&
		    (smart_chg->plugin_soc <= 20) &&
		    (smart_chg->plugin_board_temp <= 390) &&
		    (smart_chg->soc <= 40 )) {
				if(((smart_chg->back_flag == SCREEN_STATE_UNKONW) || (smart_chg->back_flag == SCREEN_STATE_BLACK)) && !smart_chg->screen_state) {  //black to bright
					smart_chg->back_flag = SCREEN_STATE_BLACK_TO_BRIGHT;
					time_last = ktime_get_seconds();
					this->active_flag = true;
					pr_err("%s switch to bright time_last = %lld\n", __func__, time_last);
				}  else if ((smart_chg->back_flag == SCREEN_STATE_BLACK_TO_BRIGHT || smart_chg->back_flag == SCREEN_STATE_BRIGHT) && !smart_chg->screen_state) {  //still bright
					smart_chg->back_flag = SCREEN_STATE_BRIGHT;
					time_now = ktime_get_seconds();
					delta_time = time_now - time_last;
					pr_err("%s still_bright time_now = %lld, time_last = %lld, delta_time = %lld\n", __func__, time_now, time_last, delta_time);
					if(delta_time <= 10) {
						this->active_flag = true;
						pr_err("%s still_bright delta_time = %lld, stay fast\n", __func__, delta_time);
					} else {
						this->active_flag = false;
						pr_err("%s still_bright delta_time = %lld, exit fast\n", __func__, delta_time);
					}
				} else { //black
					smart_chg->back_flag = SCREEN_STATE_BLACK;
					this->active_flag = true;
					pr_err("%s black stay fast, delta_time = %lld\n", __func__, delta_time);
				}
		}

		if((smart_chg->board_temp >= 420) || (smart_chg->soc > 40)){
			this->active_flag = false;
		}
	} else {
		this->active_flag = false;
	}

	if (this->active_flag) {
		smart_chg->pps_fast_mode = true;
		if ((smart_chg->soc > 37) && (smart_chg->board_temp > 410)) {
			if(smart_chg->low_fast_ffc >= 4300){
				smart_chg->low_fast_ffc = 4000;
			}
			xm_info("%s stay fast but low_fast_ffc = 4500, board_temp = %d, low_fast_ffc = %d\n", __func__, smart_chg->board_temp, smart_chg->low_fast_ffc);
		} else if ((smart_chg->soc > 38) && (smart_chg->board_temp > 380)) {
			if (smart_chg->low_fast_ffc <= 4000) {
					smart_chg->low_fast_ffc = 4000;
			} else {
					 smart_chg->low_fast_ffc = 4200;
			}
			xm_info("%s stay fast but cool down, board_temp = %d, low_fast_ffc = %d\n", __func__, smart_chg->board_temp, smart_chg->low_fast_ffc);
		}
		vote(smart_chg->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, smart_chg->low_fast_ffc);
		xm_info("%s stay fastchg, low_fast_ffc = %d\n", __func__, smart_chg->low_fast_ffc);
	} else {
		smart_chg->pps_fast_mode = false;
		vote(smart_chg->total_fcc_votable, TEMP_THERMAL_DAEMON_VOTER, true, smart_chg->low_fast_ffc);
		xm_info("low battery fast charge function deactive\n");
	}

	xm_info("soc: %d, pd_active: %d, plugin_soc: %d, board_temp: %d, screen_state: %d, back_flag:%d, func[on: %d val: %d active: %d]\n",
		smart_chg->soc, smart_chg->pd_active, smart_chg->plugin_soc,
		smart_chg->board_temp, smart_chg->screen_state, smart_chg->back_flag,
		this->func_on, this->func_val, this->active_flag);

	return 0;
}

static int smart_chg_long_charge_protect_func(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *this = NULL;
	int soc_threshold = 0;
	int soc = 0;

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	this = &(smart_chg->funcs[SMART_CHG_LONG_CHG_PROTECT]);
	soc = smart_chg->soc;
	soc_threshold = this->func_val;

	if ((this->func_on && (soc >= soc_threshold)) ||
		(((soc > soc_threshold - 5) && (soc < soc_threshold)) && this->active_flag)) {
		this->active_flag = true;
		hq_chargermanager_notifier_call_chain(CHG_FW_EVT_SMART_ENDURA_TRIG, &(this->active_flag));
		xm_debug("long charge protect function active\n");
	} else if (((!this->func_on || (soc <= soc_threshold - 5)) && this->active_flag) ||
		(!this->func_on && !this->active_flag)) {
		this->active_flag = false;
		hq_chargermanager_notifier_call_chain(CHG_FW_EVT_SMART_ENDURA_TRIG, &(this->active_flag));
		xm_debug("long charge protect function deactive\n");
	}
	/* update stop charge status for sysfs */
	smart_chg->stop_charge = this->active_flag;

	vote(smart_chg->total_fcc_votable, ENDURANCE_VOTER, this->active_flag, 0);
	rerun_election(smart_chg->total_fcc_votable);

	xm_info("soc: %d, func[on: %d val: %d active: %d]\n",
		smart_chg->soc, this->func_on, this->func_val, this->active_flag);

	return 0;
}

static int smart_chg_update_state(struct xm_smart_chg *smart_chg)
{
	union power_supply_propval pval = {0};
	int ret = 0;
	struct charger_manager *manager = NULL;
	struct hq_thermal_policy *policy = NULL;

	manager = dev_get_drvdata(smart_chg->dev);
	if (IS_ERR_OR_NULL(manager)) {
		xm_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	policy = manager->thermal_policy;
	if (IS_ERR_OR_NULL(policy)) {
		xm_err("failed to get policy from manager\n");
		return -EFAULT;
	}

	smart_chg->vbus_type = manager->vbus_type;

	smart_chg->pd_active = manager->pd_active;

	smart_chg->thermal_level = manager->thermal_policy->thermal_level;

	smart_chg->board_temp = manager->board_temp;

	smart_chg->screen_state = manager->screen_state;

	manager->pps_fast_mode = smart_chg->pps_fast_mode;

	smart_chg->low_fast_ffc = policy->pd_thermal_mitigation[smart_chg->thermal_level];

	ret = power_supply_get_property(smart_chg->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		xm_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}
	smart_chg->soc = pval.intval;

	xm_info("soc: %d, vbus_type: %d, pd_active: %d, thermal_level: %d, board_temp: %d, screen_state: %d\n",
		smart_chg->soc, smart_chg->vbus_type, smart_chg->pd_active,
		smart_chg->thermal_level, smart_chg->board_temp, smart_chg->screen_state);

	return 0;
}

void handle_smart_chg_work(struct work_struct *work)
{
	struct xm_smart_chg *smart_chg = container_of(work, struct xm_smart_chg, smart_chg_work.work);

	mutex_lock(&smart_chg->smart_chg_work_lock);

	smart_chg_update_state(smart_chg);

	smart_chg_navigaition_discarge_func(smart_chg);
	smart_chg_outdoor_charge_func(smart_chg);
	smart_chg_low_battery_fast_charge_func(smart_chg);
	smart_chg_long_charge_protect_func(smart_chg);

	mutex_unlock(&smart_chg->smart_chg_work_lock);

	schedule_delayed_work(&smart_chg->smart_chg_work, msecs_to_jiffies(3000));
}

static int smart_chg_on_plugin_routines(struct xm_smart_chg *smart_chg)
{
	union power_supply_propval pval = {0};
	int ret = 0;
	struct charger_manager *manager = NULL;

	manager = dev_get_drvdata(smart_chg->dev);
	if (IS_ERR_OR_NULL(manager)) {
		xm_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	/* record soc on plugin */
	ret = power_supply_get_property(smart_chg->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		xm_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}
	smart_chg->plugin_soc = pval.intval;

	/* record board temperature on plugin */
	smart_chg->plugin_board_temp = manager->board_temp;

	smart_chg->stop_charge = false;

	return 0;
}

static int smart_chg_on_plugout_routines(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *long_chg_protect = &(smart_chg->funcs[SMART_CHG_LONG_CHG_PROTECT]);

	/* clean up all recorders */
	smart_chg->plugin_soc = 0;
	smart_chg->plugin_board_temp = 0;
	smart_chg->back_flag = SCREEN_STATE_UNKONW;
	smart_chg->stop_charge = false;
	long_chg_protect->active_flag = false;

	return 0;
}

int xm_smart_chg_init(struct charger_manager *manager)
{
	struct xm_smart_chg *smart_chg = NULL;
	int i = 0;

	if (manager->smart_chg) {
		xm_err("smart charge already initialized\n");
		return -EINVAL;
	}

	smart_chg = devm_kzalloc(manager->dev, sizeof(*smart_chg), GFP_KERNEL);
	if (!smart_chg) {
		return -ENOMEM;
	}

	smart_chg->dev = manager->dev;

	/* votable initialize */
	smart_chg->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!smart_chg->total_fcc_votable) {
		xm_err("find TOTAL_FCC voltable failed\n");
	}

	smart_chg->main_fcc_votable = find_votable("MAIN_FCC");
	if (!smart_chg->main_fcc_votable) {
		xm_err("find MAIN_FCC voltable failed\n");
	}

	smart_chg->main_icl_votable = find_votable("MAIN_ICL");
	if (!smart_chg->main_icl_votable) {
		xm_err("find MAIN_ICL voltable failed\n");
	}

	smart_chg->fv_votable = find_votable("MAIN_FV");
	if (!smart_chg->fv_votable) {
		xm_err("find MAIN_FV voltable failed\n");
	}

	/* power supply/class initialize */
	smart_chg->batt_psy = power_supply_get_by_name("battery");
	if (!smart_chg->batt_psy) {
		xm_err("get battery power supply failed\n");
	}

	/* smart charge work initialize */
	INIT_DELAYED_WORK(&smart_chg->smart_chg_work, handle_smart_chg_work);

	/* smart charge mutex lock initialize */
	mutex_init(&smart_chg->smart_chg_work_lock);

	/* default dynamic function on/off switch */
	for (i = SMART_CHG_FUNC_MIN; i < SMART_CHG_FUNC_MAX; i++) {
		smart_chg->funcs[i].func_on = false;
	}

	/* default flags */
	smart_chg->status = SMART_CHG_SUCCESS;
	smart_chg->stop_charge = 0;

	manager->smart_chg = smart_chg;

	xm_info("smart charge %s initialize success\n", XM_SMART_CHG_VERSION);

	return 0;
}

int xm_smart_chg_deinit(struct charger_manager *manager)
{
	if (!manager->smart_chg) {
		return 0;
	}

	cancel_delayed_work_sync(&manager->smart_chg->smart_chg_work);

	devm_kfree(manager->dev, manager->smart_chg);
	manager->smart_chg = NULL;

	xm_info("smart charge %s deinitialize success\n", XM_SMART_CHG_VERSION);

	return 0;
}

int xm_smart_chg_run(struct charger_manager *manager)
{
	struct xm_smart_chg *smart_chg = manager->smart_chg;

	/* NOTE: Don't add any code before this */
	smart_chg_on_plugin_routines(smart_chg);

	schedule_delayed_work(&smart_chg->smart_chg_work, msecs_to_jiffies(3000));

	return 0;
}

int xm_smart_chg_stop(struct charger_manager *manager)
{
	struct xm_smart_chg *smart_chg = manager->smart_chg;

	cancel_delayed_work_sync(&smart_chg->smart_chg_work);

	/* NOTE: Don't add any code after this */
	smart_chg_on_plugout_routines(smart_chg);

	return 0;
}
