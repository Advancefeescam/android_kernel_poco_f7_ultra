// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */
#include "hq_charger_manager.h"
#include "xm_batt_health.h"
#include "hq_printk.h"

#define xm_err     hq_err
#define xm_warn    hq_warn
#define xm_notice  hq_notice
#define xm_info    hq_info
#define xm_debug   hq_debug

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_BATT_HEALTH]"
#endif

static int batt_health_over_fv_protect_func(struct xm_batt_health *batt_health)
{
	static int over_fv_cnt;

	if (!batt_health->over_fv_protect_on) {
		return 0;
	}

	if ((batt_health->vbat > batt_health->effective_fv) &&
		(!batt_health->over_fv_flag) &&
		(batt_health->pd_active)) {
		over_fv_cnt++;
		if (over_fv_cnt > 1000)
			over_fv_cnt = 3;
	}

	if (!batt_health->over_fv_flag && (over_fv_cnt >= 3) && (batt_health->effective_fcc >= 3000)) {
		xm_debug("over fv protect function triggered\n");
		batt_health->over_fv_flag = true;
		over_fv_cnt = 0;
		batt_health->effective_fcc -= 1000;
		vote(batt_health->total_fcc_votable, XM_BATT_HEALTH_VOTER, true, batt_health->effective_fcc);
	}

	if (batt_health->over_fv_flag && (batt_health->pd_active != CHARGE_PD_ACTIVE)) {
		xm_debug("over fv protect function deactivated\n");
		over_fv_cnt = 0;
		batt_health->over_fv_flag = false;
		vote(batt_health->total_fcc_votable, XM_BATT_HEALTH_VOTER, false, 0);
	}

	return 0;
}

static int batt_health_night_smart_charge(struct xm_batt_health *batt_health)
{
	struct charger_manager *manager = NULL;

	manager = dev_get_drvdata(batt_health->dev);
	if (IS_ERR_OR_NULL(manager)) {
		xm_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	if (batt_health->night_smart_charge_on && (batt_health->soc >= 80)) {
		xm_info("night smart charge function triggered\n");
		batt_health->night_charging_flag = true;

		/* TODO: disable buck/cp use votable */
		vote(batt_health->total_fcc_votable, NIGHT_CHARGING_VOTER, true, 0);
	}

	if (batt_health->night_charging_flag) {
		if (!batt_health->night_smart_charge_on || (batt_health->soc <= 75)) {
			xm_info("night smart charge function deactivated");
			batt_health->night_charging_flag = false;
			/* TODO: enable buck/cp use votable */
			vote(batt_health->total_fcc_votable, NIGHT_CHARGING_VOTER, false, 0);
		}
	}

	xm_info("night_smart_charge_on: %d, night_charging_flag: %d\n",
		batt_health->night_smart_charge_on, batt_health->night_charging_flag);

	return 0;
}

static int monitor_smart_batt_fv(struct xm_batt_health *batt_health)
{
	int jeita_fv = 0;
	int diff_fv = 0;

	jeita_fv = get_client_vote_locked(batt_health->fv_votable, JEITA_VOTER);
	if (jeita_fv == -EINVAL) {
		xm_err("jeita client is not enabled or not found\n");
		return -EINVAL;
	}

	diff_fv = max(batt_health->smart_batt, batt_health->smart_fv);

	if ((diff_fv % 8) > 0)
		diff_fv -= 8;

	if (diff_fv < 0)
		diff_fv = 0;

	vote(batt_health->fv_votable, XM_BATT_HEALTH_VOTER, true, (jeita_fv - diff_fv));

	xm_info("jeita_fv: %dmv smart_batt: %dmv smart_fv: %dmv diff_fv: %dmv\n",
		jeita_fv, batt_health->smart_batt, batt_health->smart_fv, diff_fv);

	return 0;
}

static int batt_health_battery_manager(struct xm_batt_health *batt_health)
{
	monitor_smart_batt_fv(batt_health);

	return 0;
}

// static int batt_health_smart_time_location_capacity(struct xm_batt_health *batt_health)
// {
// 	monitor_smart_batt_fv(batt_health);

// 	return 0;
// }

static int batt_health_update_state(struct xm_batt_health *batt_health)
{
	union power_supply_propval pval = {0};
	int ret = 0;
	struct charger_manager *manager = NULL;

	manager = dev_get_drvdata(batt_health->dev);
	if (IS_ERR_OR_NULL(manager)) {
		xm_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	batt_health->pd_active = manager->pd_active;

	batt_health->effective_fcc = get_effective_result(batt_health->total_fcc_votable);

	batt_health->effective_fv = get_effective_result(batt_health->fv_votable);

	ret = power_supply_get_property(batt_health->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		xm_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}
	batt_health->soc = pval.intval;

	ret = power_supply_get_property(batt_health->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		xm_err("get battery temperature failed, ret = %d\n", ret);
		return ret;
	}
	batt_health->tbat = pval.intval / 10;

	ret = power_supply_get_property(batt_health->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		xm_err("get battery voltage failed, ret = %d\n", ret);
		return ret;
	}
	batt_health->vbat = pval.intval / 1000;

	xm_info("tbat: %d, vbat: %dmv, soc: %d, effctive_fcc: %d, effective_fv: %d, pd_active: %d\n",
		batt_health->tbat, batt_health->vbat, batt_health->soc,
		batt_health->effective_fcc, batt_health->effective_fv,
		batt_health->pd_active);

	return 0;
}

static void handle_batt_health_work(struct work_struct *work)
{
	struct xm_batt_health *batt_health = container_of(work, struct xm_batt_health, batt_health_work.work);

	mutex_lock(&batt_health->batt_health_work_lock);

	batt_health_update_state(batt_health);

	batt_health_over_fv_protect_func(batt_health);

	batt_health_night_smart_charge(batt_health);

	batt_health_battery_manager(batt_health);

	/* NOTE: reuse battery manager function */
	//batt_health_smart_time_location_capacity(batt_health);

	mutex_unlock(&batt_health->batt_health_work_lock);

	schedule_delayed_work(&batt_health->batt_health_work, msecs_to_jiffies(1000));
}

int xm_batt_health_init(struct charger_manager *manager)
{
	struct xm_batt_health *batt_health = NULL;

	if (manager->batt_health) {
		xm_err("battery health already initialized\n");
		return -EINVAL;
	}

	batt_health = devm_kzalloc(manager->dev, sizeof(*batt_health), GFP_KERNEL);
	if (!batt_health) {
		return -ENOMEM;
	}

	batt_health->dev = manager->dev;

	/* votable initialize */
	batt_health->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!batt_health->total_fcc_votable) {
		xm_err("find TOTAL_FCC voltable failed\n");
	}

	batt_health->fv_votable = find_votable("MAIN_FV");
	if (!batt_health->fv_votable) {
		xm_err("find MAIN_FV voltable failed\n");
	}

	/* power supply/class initialize */
	batt_health->batt_psy = power_supply_get_by_name("battery");
	if (!batt_health->batt_psy) {
		xm_err("get battery power supply failed\n");
	}

	/* battery health work initialize */
	INIT_DELAYED_WORK(&batt_health->batt_health_work, handle_batt_health_work);

	/* battery health mutex lock initialize  */
	mutex_init(&batt_health->batt_health_work_lock);

	/* default dynamic function on/off switch */
	batt_health->over_fv_protect_on = true;
	batt_health->night_smart_charge_on = false;

	/* default flags */
	batt_health->over_fv_flag = false;
	batt_health->night_charging_flag = false;

	manager->batt_health = batt_health;

	xm_info("battery health %s initialize success\n", XM_BATT_HEALTH_VERSION);

	return 0;
}

int xm_batt_health_deinit(struct charger_manager *manager)
{
	if (!manager->batt_health) {
		return 0;
	}

	cancel_delayed_work_sync(&manager->batt_health->batt_health_work);

	devm_kfree(manager->dev, manager->batt_health);
	manager->batt_health = NULL;

	xm_info("battery health %s deinitialize success\n", XM_BATT_HEALTH_VERSION);

	return 0;
}

int xm_batt_health_run(struct charger_manager *manager)
{
	struct xm_batt_health *batt_health = manager->batt_health;

	schedule_delayed_work(&batt_health->batt_health_work, msecs_to_jiffies(3000));

	return 0;
}

int xm_batt_health_stop(struct charger_manager *manager)
{
	struct xm_batt_health *batt_health = manager->batt_health;

	cancel_delayed_work_sync(&batt_health->batt_health_work);

	return 0;
}
