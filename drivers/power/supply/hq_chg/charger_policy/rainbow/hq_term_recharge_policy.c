/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "hq_printk.h"
#include "hq_charger_manager.h"
#include "hq_term_recharge_policy.h"

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_TERM_RECHARGE]"
#endif

static int term_recharge_policy_update_state(struct hq_term_recharge_policy *policy)
{
	union power_supply_propval pval = {0};
	int ret = 0;
	struct charger_manager *manager = NULL;

	manager = dev_get_drvdata(policy->dev);
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("failed to get charger manager form device\n");
		return -EFAULT;
	}
	policy->pd_active = manager->pd_active;
	policy->vbus_type = manager->vbus_type;
	policy->board_version = manager->board_version;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0) {
		hq_err("get battery charge status failed, ret = %d\n", ret);
		return ret;
	}
	policy->charge_status = pval.intval;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0) {
		hq_err("get battery current now failed, ret = %d\n", ret);
		return ret;
	}
	policy->ibat = -(pval.intval / 1000);

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		hq_err("get fg capacity failed, ret = %d\n", ret);
		return ret;
	}
	policy->fg_soc = pval.intval;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		hq_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}
	policy->ui_soc = pval.intval;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		hq_err("get battery voltage now failed, ret = %d\n", ret);
		return ret;
	}
	policy->vbat = pval.intval / 1000;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("get battery temperature failed, ret = %d\n", ret);
		return ret;
	}
	policy->tbat = pval.intval;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		hq_err("get battery cycle count failed, ret = %d\n", ret);
		return ret;
	}
	policy->cycle_cnt = pval.intval;

	ret = charger_is_charge_done(policy->charger, &policy->is_charge_done);
	if (ret < 0) {
		hq_err("get is charge done fail, ret = %d\n", ret);
		return ret;
	}

	ret = charger_get_rechg_volt(policy->charger, &policy->rchg_val);
	if (ret < 0) {
		hq_err("get recharge volt fail, ret = %d\n", ret);
		return ret;
	}

	policy->iterm = get_effective_result_locked(policy->iterm_votable);

	policy->fv = get_effective_result_locked(policy->fv_votable);

	policy->raw_soc = fuel_gauge_get_raw_soc(manager->fuel_gauge);

	policy->charge_mode = manager->charge_mode;

	hq_info("iterm: %d, fv: %d, tbat: %d, ibat: %dma, vbat: %dmv, fg_soc: %d, ui_soc: %d, raw_soc: %d, cycle_cnt: %d, charge_status: %d, is_charge_done: %d, rchg_val: %d, pd_active: %d, board_version: %d\n",
		policy->iterm, policy->fv, policy->tbat, policy->ibat, policy->vbat, policy->fg_soc, policy->ui_soc, policy->raw_soc, policy->cycle_cnt,
		policy->charge_status, policy->is_charge_done, policy->rchg_val, policy->pd_active, policy->board_version);

	return ret;
}

static int handle_terminated(struct hq_term_recharge_policy *policy)
{
	hq_info("start ++++\n");

	vote(policy->total_fcc_votable, TERM_RECHARGE_VOTER, true, 0);
	policy->charge_status = POWER_SUPPLY_STATUS_FULL;
	policy->term_check_cnt = 0;
	policy->terminated = true;

	return 0;
}

static int term_recharge_policy_check_terminate(struct hq_term_recharge_policy *policy)
{
	if ((policy->fg_soc >= TERM_LOWER_LIMIT_FGSOC) &&
		(policy->charge_status == TERM_CHARGE_STATUS) &&
		(policy->vbat >= TERM_LOWER_LIMIT_VBAT(policy->fv)) &&
		(policy->ibat <= TERM_UPPER_LIMIT_IBAT(policy->iterm))) {
		if (policy->term_check_cnt >= TERM_CHECK_COUNT) {
			handle_terminated(policy);
		} else {
			policy->term_check_cnt++;
		}
	}

	hq_info("TERM_LOWER_LIMIT_VBAT(policy->fv) = %d TERM_UPPER_LIMIT_IBAT(policy->iterm) = %d\n",
			TERM_LOWER_LIMIT_VBAT(policy->fv), TERM_UPPER_LIMIT_IBAT(policy->iterm));

	return 0;

}

static void charger_full_handle_report_rsoc100(struct hq_term_recharge_policy *policy)
{
	#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
	struct fuel_gauge_dev *gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	int rsoc = fuel_gauge_get_rsoc(gauge);
	int ret = 0;

	hq_info("term_check_cnt = %d\n", policy->term_check_cnt);

	if((policy->term_check_cnt >= TERM_CHECK_COUNT -1 || policy->is_charge_done) && rsoc != 100) {

		ret = fuel_gauge_set_fg_rsoc100(gauge);
		if (ret < 0) {
			hq_info("report fg soc to 100 err\n");
		}
		hq_info("report fg soc to 100\n");
	}
	#endif

}

static void charger_full_handle_adapter_vbus(struct hq_term_recharge_policy *policy)
{
	int vbus_volt = 0;

	if (policy->charge_status == POWER_SUPPLY_STATUS_FULL) {
		charger_get_adc(policy->charger, CHG_ADC_VBUS, &vbus_volt);
		if (vbus_volt < CHARGE_FULL_DROP_VBUS_MV)
			return;

		if(policy->pd_active) {
			adapter_set_cap(policy->adapter, 0, 5000, 2000);
			hq_info("pd adapter charger full!, VBUS drop 5V\n");
		} else if(policy->vbus_type == VBUS_TYPE_HVDCP) {
			charger_set_qc_term_vbus(policy->charger);
			hq_info("QC adapter charger full!, VBUS drop 5V\n");
		}

	}
}


static int term_recharge_policy_check_recharge_eea(struct hq_term_recharge_policy *policy)
{
	int ret = 0;
	static bool is_charge_done;
	static bool terminated;
	static bool uisoc_100_adapter_plugin;

	if ((!policy->is_charge_again) &&
		(policy->cycle_cnt > 300) &&
		(policy->charge_mode == FFC_CHARGE_MODE) &&
		(policy->tbat > CHARGE_AGAIN_TEMP_LOWER_LIMIT && policy->tbat <= CHARGE_AGAIN_TEMP_UPPER_LIMIT) &&
		((!is_charge_done && policy->is_charge_done) || (!terminated && policy->terminated))) {
		schedule_delayed_work(&policy->battery_charge_again_work, msecs_to_jiffies(60000 * 5));
		policy->is_charge_again = true;
	}

	if ((!is_charge_done && policy->is_charge_done) ||
		(!terminated && policy->terminated) ||
		(!uisoc_100_adapter_plugin && policy->uisoc_100_adapter_plugin)) {
		ret = charger_set_rechg_volt(policy->charger, EEA_BUCK_RECHARGE_VOLTAGE_400MV);
		if (ret)
			hq_err("set rechg volt fail, %d\n", ret);
	}

	if (policy->ui_soc < EEA_RECHARGE_SOC && (policy->is_charge_done || policy->terminated || policy->uisoc_100_adapter_plugin)) {
		vote(policy->iterm_votable, TERM_RECHARGE_VOTER, false, 0);
		vote(policy->fv_votable, TERM_RECHARGE_VOTER, false, 0);
		vote(policy->total_fcc_votable, TERM_RECHARGE_VOTER, false, 0);

		ret = charger_set_rechg_volt(policy->charger, EEA_BUCK_RECHARGE_VOLTAGE_100MV);
		if (ret)
			hq_err("set rechg volt fail, %d\n", ret);

		if (policy->uisoc_100_adapter_plugin) {
			vote(policy->total_fcc_votable, ITER_VOTER, false, 0);
			policy->soft_charge_status = POWER_SUPPLY_STATUS_CHARGING;
			policy->uisoc_100_adapter_plugin = false;
		}

		policy->terminated = false;
		g_policy->cp_charge_done = false;

		hq_info("recharge !!!\n");
	}

	hq_info("soft_charge_status: %d, uisoc_100_adapter_plugin: %d, is_charge_done = %d, policy->is_charge_done = %d, terminated = %d, policy->terminated = %d\n",
			policy->soft_charge_status, policy->uisoc_100_adapter_plugin, is_charge_done, policy->is_charge_done, terminated, policy->terminated);

	is_charge_done = policy->is_charge_done;
	terminated = policy->terminated;
	uisoc_100_adapter_plugin = policy->uisoc_100_adapter_plugin;

	return 0;
}

static int term_recharge_policy_check_recharge(struct hq_term_recharge_policy *policy)
{
	int ret = 0;
	static bool is_charge_done;
	static bool terminated;

	hq_info("is_charge_done = %d, policy->is_charge_done = %d, terminated = %d, policy->terminated = %d\n",
			is_charge_done, policy->is_charge_done, terminated, policy->terminated);

	if ((!policy->is_charge_again) &&
		(policy->charge_mode == FFC_CHARGE_MODE) &&
		(policy->tbat > CHARGE_AGAIN_TEMP_LOWER_LIMIT && policy->tbat <= CHARGE_AGAIN_TEMP_UPPER_LIMIT) &&
		((!is_charge_done && policy->is_charge_done) || (!terminated && policy->terminated))) {
		schedule_delayed_work(&policy->battery_charge_again_work, msecs_to_jiffies(60000 * 5));
		policy->is_charge_again = true;
	}

	if ((!is_charge_done && policy->is_charge_done) || (!terminated && policy->terminated)) {
		ret = charger_set_rechg_volt(policy->charger, BUCK_RECHARGE_VOLTAGE_400MV);
		if (ret)
			hq_err("set rechg volt fail, %d\n", ret);
	}

	if (policy->raw_soc < RECHARGE_RSOC && (policy->is_charge_done || policy->terminated)) {
		vote(policy->iterm_votable, TERM_RECHARGE_VOTER, false, 0);
		vote(policy->fv_votable, TERM_RECHARGE_VOTER, false, 0);
		vote(policy->total_fcc_votable, TERM_RECHARGE_VOTER, false, 0);

		if (policy->is_charge_done) {
			ret = charger_set_chg(policy->charger, false);
			if (ret)
				hq_err("set chg disable fail, %d\n", ret);

			hq_info("close charge\n");

			msleep(500);

			ret = charger_set_chg(policy->charger, true);
			if (ret)
				hq_err("set chg en fail, %d\n", ret);

			hq_info("open charge\n");
		}

		ret = charger_set_rechg_volt(policy->charger, BUCK_RECHARGE_VOLTAGE_100MV);
		if (ret)
			hq_err("set rechg volt fail, %d\n", ret);

		policy->terminated = false;
		g_policy->cp_charge_done = false;

		hq_info("recharge !!!\n");
	}

	is_charge_done = policy->is_charge_done;
	terminated = policy->terminated;

	return 0;
}

static void battery_charge_again(struct work_struct *work)
{
	int i = 0;
	int ret = 0;
	bool charge_again = false;
	struct hq_term_recharge_policy *policy =
					container_of(work, struct hq_term_recharge_policy, battery_charge_again_work.work);

	hq_info("enter\n");

	for (i = 0; i < CHARGE_AGAIN_RANGE_NUM; i++) {
		if (policy->cycle_cnt >= charge_again_table[i].l_cycle && policy->cycle_cnt <= charge_again_table[i].u_cycle) {
			if (policy->vbat < charge_again_table[i].fv) {
				charge_again = true;
				break;
			}
		}
	}

	if (charge_again) {
		vote(policy->iterm_votable, TERM_RECHARGE_VOTER, true, charge_again_table[i].iterm);
		vote(policy->fv_votable, TERM_RECHARGE_VOTER, true, charge_again_table[i].fv);
		vote(policy->total_fcc_votable, TERM_RECHARGE_VOTER, true, charge_again_table[i].ichg);

		if (policy->terminated) {
			policy->terminated = false;
		}

		if (policy->is_charge_done) {
			ret = charger_set_chg(policy->charger, false);
			if (ret)
				hq_err("set chg disable fail, %d\n", ret);

			ret = charger_set_chg(policy->charger, true);
			if (ret)
				hq_err("set chg en fail, %d\n", ret);
		}

		hq_info("charge again !!!\n");
	}
}

static void handle_term_rechg_policy_update(struct work_struct *work)
{
	struct hq_term_recharge_policy *policy =
					container_of(work, struct hq_term_recharge_policy, policy_update_work.work);

	mutex_lock(&policy->term_recharge_update_lock);

	term_recharge_policy_update_state(policy);

	term_recharge_policy_check_terminate(policy);

	charger_full_handle_report_rsoc100(policy);

	charger_full_handle_adapter_vbus(policy);

	if (policy->board_version == EEA_VERSION)
		term_recharge_policy_check_recharge_eea(policy);
	else
		term_recharge_policy_check_recharge(policy);

	mutex_unlock(&policy->term_recharge_update_lock);

	schedule_delayed_work(&policy->policy_update_work, msecs_to_jiffies(5000));
}

int hq_term_recharge_policy_init(struct charger_manager *manager)
{
	struct hq_term_recharge_policy *policy = NULL;

	if (manager->term_recharge_policy) {
		hq_err("terminate and recharge policy already initialized\n");
		return -EINVAL;
	}

	policy = devm_kzalloc(manager->dev, sizeof(*policy), GFP_KERNEL);
	if (!policy) {
		return -ENOMEM;
	}

	policy->dev = manager->dev;

	/* votable initialize */
	policy->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!policy->total_fcc_votable) {
		hq_err("find TOTAL_FCC voltable failed\n");
	}

	policy->iterm_votable = find_votable("MAIN_ITERM");
	if (!policy->iterm_votable) {
		hq_err("find MAIN_ITERM voltable failed\n");
	}

	policy->fv_votable = find_votable("MAIN_FV");
	if (!policy->fv_votable) {
		hq_err("find MAIN_FV voltable failed\n");
	}

	policy->batt_psy = power_supply_get_by_name("battery");
	if (!policy->batt_psy) {
		hq_err("get battery power supply failed\n");
	}

	policy->charger = charger_find_dev_by_name("primary_chg");
	if (!manager->charger) {
		hq_err("failed to find primary_chg device\n");
	}

	policy->adapter = adapter_find_dev_by_name("pd_adapter1");
	if (!policy->adapter) {
		hq_err("failed to find pd adapter\n");
	}

	policy->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!manager->fuel_gauge) {
		hq_err("failed to find fuel_gauge device\n");
	}

	INIT_DELAYED_WORK(&policy->policy_update_work, handle_term_rechg_policy_update);

	INIT_DELAYED_WORK(&policy->battery_charge_again_work, battery_charge_again);

	/* terminate and recharge update mutex lock initialize  */
	mutex_init(&policy->term_recharge_update_lock);

	manager->term_recharge_policy = policy;

	policy->board_version = manager->board_version;

	policy->is_charge_again = false;

	policy->terminated = false;

	hq_info("board_version: %d,terminate and recharge policy initialize success\n", policy->board_version);

	return 0;
}

int hq_term_recharge_policy_deinit(struct charger_manager *manager)
{
	if (!manager->term_recharge_policy) {
		return 0;
	}

	cancel_delayed_work_sync(&manager->term_recharge_policy->policy_update_work);

	devm_kfree(manager->dev, manager->term_recharge_policy);
	manager->term_recharge_policy = NULL;

	hq_info("terminate and recharge policy deinitialize success\n");

	return 0;
}

static int term_recharge_on_plugin_routines(struct hq_term_recharge_policy *policy)
{
	int ret = 0;
	union power_supply_propval pval = {0};

	if (policy->batt_psy == NULL)
		return 0;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		hq_err("get battery charge status failed, ret = %d\n", ret);
		return ret;
	}
	policy->ui_soc = pval.intval;

	if (policy->ui_soc == TERM_LOWER_LIMIT_UISOC) {
		policy->uisoc_100_adapter_plugin = true;
		vote(policy->total_fcc_votable, ITER_VOTER, true, 0);
		policy->soft_charge_status = POWER_SUPPLY_STATUS_FULL;
	} else {
		policy->soft_charge_status = POWER_SUPPLY_STATUS_CHARGING;
		vote(policy->total_fcc_votable, ITER_VOTER, false, 0);
		policy->uisoc_100_adapter_plugin = false;
	}

	ret = charger_set_rechg_volt(policy->charger, EEA_BUCK_RECHARGE_VOLTAGE_100MV);
	if (ret)
		hq_err("set rechg volt fail, %d\n", ret);

	hq_info("eea:uisoc=%d, adapter plug in\n", policy->ui_soc);

	return 0;
}

static int term_recharge_on_plugout_routines(struct hq_term_recharge_policy *policy)
{

	hq_info("eea:adapter plug out\n");
	policy->uisoc_100_adapter_plugin = false;
	vote(policy->total_fcc_votable, ITER_VOTER, false, 0);
	policy->soft_charge_status = POWER_SUPPLY_STATUS_DISCHARGING;

	return 0;
}

int hq_term_recharge_policy_run(struct charger_manager *manager)
{
	struct hq_term_recharge_policy *policy = manager->term_recharge_policy;

	if (policy->board_version == EEA_VERSION)
		term_recharge_on_plugin_routines(policy);
	charger_set_rechg_volt(policy->charger, 100);
	schedule_delayed_work(&policy->policy_update_work, msecs_to_jiffies(150));

	return 0;
}

int hq_term_recharge_policy_stop(struct charger_manager *manager)
{
	struct hq_term_recharge_policy *policy = manager->term_recharge_policy;

	if (policy->board_version == EEA_VERSION)
		term_recharge_on_plugout_routines(policy);

	policy->terminated = false;
	policy->is_charge_again = false;

	cancel_delayed_work_sync(&policy->policy_update_work);
	cancel_delayed_work_sync(&policy->battery_charge_again_work);

	return 0;
}
