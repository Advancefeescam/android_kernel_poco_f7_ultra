/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include "hq_printk.h"
#include "hq_utils.h"
#include "hq_voter.h"
#include "hq_charger_manager.h"
#include "hq_jeita_policy.h"

#if IS_ENABLED(CONFIG_BUILD_TARGET_TAIKO)
#include "hq_jeita_para_taiko.h"
#elif IS_ENABLED(CONFIG_BUILD_TARGET_KOTO)
#include "hq_jeita_para_koto.h"
#else
#include "hq_jeita_charoite.h"
#endif

//#define JEITA_POLICY_DEBUG

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_JEITA]"
#endif

#ifdef JEITA_POLICY_DEBUG
static inline void dump_jeita_parameter(struct jeita_parameter *para)
{
	int i = 0;
	struct chg_parameter *chg_para = NULL;

	if (!para || !para->chg_para)
		return;

	chg_para = para->chg_para;

	printk(KERN_CONT "[HQ_CHG_JEITA][%s] cycle_range: %d~%d, charge_mode: %d, ",
			__func__, para->cycle_range.low, para->cycle_range.high, para->charge_mode);
	printk(KERN_CONT "ichg: ");
	for (i = 0; i < chg_para->ichg_para_size; i++) {
		printk(KERN_CONT "{v:%d~%dmv, i:%dma} ",
				chg_para->ichg_para[i].vbat_range.low,
				chg_para->ichg_para[i].vbat_range.high,
				chg_para->ichg_para[i].ichg);
	}
	printk(KERN_CONT ", fv: %dmv, iterm: { ", chg_para->fv);
	for (i = 0; i < 5; i++) {
		printk(KERN_CONT "%dma ", chg_para->iterm[i]);
	}
	printk(KERN_CONT "}\n");
}
#endif

static inline bool get_jeita_cfg(struct jeita_parameter *para_tbl, int tbl_size, struct jeita_condition *cond, struct jeita_config *cfg)
{
	int i = 0;
	int j = 0;
	struct jeita_parameter *jeita_para;
	struct chg_parameter *chg_para;
	bool found = false;
	int low = 0;
	int high = 0;

	for (i = 0; i < tbl_size; i++) {
		jeita_para = &para_tbl[i];

		low = jeita_para->cycle_range.low;
		high = jeita_para->cycle_range.high;
		if (is_between(low, high, cond->cycle_cnt)) {
			chg_para = jeita_para->chg_para;

			for (j = 0; j < chg_para->ichg_para_size; j++) {

				low = chg_para->ichg_para[j].vbat_range.low;
				high = chg_para->ichg_para[j].vbat_range.high;
				if (is_between(low, high, cond->vbat)) {
					if ((jeita_para->charge_mode == ALL_CHARGE_MODE)
						|| jeita_para->charge_mode == cond->charge_mode) {
						found = true;
						break;
					}
				}
			}
		}

		if (found) {
			break;
		}
	}

	if (found) {
		cfg->fv = para_tbl[i].chg_para->fv;
		cfg->iterm = para_tbl[i].chg_para->iterm[cond->batt_id];
		cfg->ichg = para_tbl[i].chg_para->ichg_para[j].ichg;
	}

	return found;
}

static int jeita_policy_select_config(struct hq_jeita_policy *policy)
{
	int found = false;
	int i = 0;
	int temp_range_idx = -1;
	int tbl_size = 0;
	struct jeita_condition *condition;
	struct jeita_config *config;
	int low = 0;
	int high = 0;

	if (!policy) {
		return -ENOMEM;
	}

	condition = &policy->jeita_cond;
	config = &policy->jeita_cfg;

	for (i = 0; i < JEITA_TEMP_RANGE_NUM; i++) {
		low = jeita_temp_range[i].low;
		high = jeita_temp_range[i].high;
		if (is_between(low, high, condition->tbat)) {
			temp_range_idx = i;
			break;
		}
	}

	switch (temp_range_idx) {
	case JEITA_T0:
		hq_info("select jeita t0 configuration\n");
		tbl_size = sizeof(jeita_para_t0) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t0, tbl_size, condition, config);
		break;
	case JEITA_T1:
		hq_info("select jeita t1 configuration\n");
		tbl_size = sizeof(jeita_para_t1) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t1, tbl_size, condition, config);
		break;
	case JEITA_T2:
		hq_info("select jeita t2 configuration\n");
		tbl_size = sizeof(jeita_para_t2) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t2, tbl_size, condition, config);
		break;
	case JEITA_T3:
		hq_info("select jeita t3 configuration\n");
		tbl_size = sizeof(jeita_para_t3) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t3, tbl_size, condition, config);
		break;
	case JEITA_T4:
		hq_info("select jeita t4 configuration\n");
		tbl_size = sizeof(jeita_para_t4) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t4, tbl_size, condition, config);
		break;
	case JEITA_T5:
		hq_info("select jeita t5 configuration\n");
		tbl_size = sizeof(jeita_para_t5) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t5, tbl_size, condition, config);
		break;
	case JEITA_T6:
		hq_info("select jeita t6 configuration\n");
		tbl_size = sizeof(jeita_para_t6) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t6, tbl_size, condition, config);
		break;
	case JEITA_T7:
		hq_info("select jeita t7 configuration\n");
		tbl_size = sizeof(jeita_para_t7) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t7, tbl_size, condition, config);
		break;
	case JEITA_T8:
		hq_info("select jeita t8 configuration\n");
		tbl_size = sizeof(jeita_para_t8) / sizeof(struct jeita_parameter);
		found = get_jeita_cfg(jeita_para_t8, tbl_size, condition, config);
		break;
	default:
		hq_err("failed to select jeita temperature configuration, tbat = %d, temp_range_idx = %d\n",
			condition->tbat, temp_range_idx);
		break;
	}

	if (!found) {
		hq_err("failed to get jeita configuration\n");
		config->fv = DEFAULT_FV;
		config->iterm = DEFAULT_ITERM;
		config->ichg = DEFAULT_ICHG;

		return 0;
	}
#if IS_ENABLED(CONFIG_ISC_PROTECT)
	if (condition->isc_status == 3) {
		config->ichg = config->ichg * 8 / 10;
		config->fv = config->fv - 15;
		hq_err("isc status is active isc_ichg:%d isc_fv:%d\n", config->ichg, config->fv);
	}
#endif

	return 0;
}

static int jeita_policy_set_config(struct hq_jeita_policy *policy)
{
	struct jeita_config *config = NULL;
	struct charger_manager *manager = NULL;

	if (!policy) {
		return -ENOMEM;
	}

	manager = dev_get_drvdata(policy->dev);

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	config = &policy->jeita_cfg;

	vote(policy->total_fcc_votable, JEITA_VOTER, true, config->ichg);
	vote(policy->iterm_votable, JEITA_VOTER, true, config->iterm);

	if (manager->vbus_type == VBUS_TYPE_SDP) {
		vote(policy->fv_votable, JEITA_VOTER, true, config->fv - 8);
		hq_info("SDP ichg: %dma, iterm: %dma, fv: %dmv\n",
		config->ichg, config->iterm, config->fv - 8);
	} else {
		if (manager->tbat > 450 && !manager->warm_stop_charge && manager->vbat <= 4100)
			vote(policy->fv_votable, JEITA_VOTER, true, 4096);
		else
			vote(policy->fv_votable, JEITA_VOTER, true, config->fv);

		hq_info("ichg: %dma, iterm: %dma, fv: %dmv, tbat:%d\n",
		config->ichg, config->iterm, config->fv, manager->tbat);
	}

	return 0;
}

static int jeita_policy_update_condition(struct hq_jeita_policy *policy)
{
	int ret = 0;
	struct jeita_condition *condition = NULL;
	union power_supply_propval pval = {0};
	struct charger_manager *manager = NULL;

	if (!policy) {
		return -ENOMEM;
	}

	manager = dev_get_drvdata(policy->dev);
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	condition = &policy->jeita_cond;

#if IS_ENABLED(CONFIG_ISC_PROTECT)
	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
		}
	}
	condition->isc_status = fuel_gauge_get_isc_status(manager->fuel_gauge);
#endif

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("get battery temperature failed, ret = %d\n", ret);
		return ret;
	}
	condition->tbat = pval.intval;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		hq_err("get battery voltage failed, ret = %d\n", ret);
		return ret;
	}
	condition->vbat = pval.intval / 1000;

	ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		hq_err("get battery cycle count failed, ret = %d\n", ret);
		return ret;
	}
	condition->cycle_cnt = pval.intval;

	condition->batt_id = manager->batt_id;
	condition->charge_mode = manager->charge_mode;

	hq_info("tbat: %d, vbat: %dmv, cycle_cnt: %d, batt_id: %d, charge_mode: %d, isc_status: %d\n",
		condition->tbat, condition->vbat, condition->cycle_cnt,
		condition->batt_id, condition->charge_mode, condition->isc_status);

	return ret;
}

static void handle_jeita_policy_update(struct work_struct *work)
{
	int ret = 0;
	struct jeita_condition *condition = NULL;
	struct jeita_config *config = NULL;
	struct hq_jeita_policy *policy = container_of(work, struct hq_jeita_policy, policy_update_work.work);

	mutex_lock(&policy->jeita_update_lock);

	condition = &policy->jeita_cond;
	config = &policy->jeita_cfg;

	ret = jeita_policy_update_condition(policy);
	if (ret < 0) {
		hq_err("failed to update jeita condition\n");
		goto exit_work;
	}

	ret = jeita_policy_select_config(policy);
	if (ret < 0) {
		hq_err("failed to select jeita configuration\n");
		goto exit_work;
	}

	ret = jeita_policy_set_config(policy);
	if (ret < 0) {
		hq_err("failed to set jeita configuration\n");
		goto exit_work;
	}

exit_work:
	mutex_unlock(&policy->jeita_update_lock);

	//__pm_relax(policy->jeita_ws);
}

int hq_jeita_policy_init(struct charger_manager *manager)
{
	struct hq_jeita_policy *policy = NULL;

	if (manager->jeita_policy) {
		hq_err("jeita policy already initialized\n");
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

	policy->fv_votable = find_votable("MAIN_FV");
	if (!policy->fv_votable) {
		hq_err("find MAIN_FV voltable failed\n");
	}

	policy->iterm_votable = find_votable("MAIN_ITERM");
	if (!policy->iterm_votable) {
		hq_err("find MAIN_FV voltable failed\n");
	}

	/* power supply/class initialize */
	policy->batt_psy = power_supply_get_by_name("battery");
	if (!policy->batt_psy) {
		hq_err("get battery power supply failed\n");
	}

	policy->bms_psy = power_supply_get_by_name("bms");
	if (!policy->bms_psy) {
		hq_err("get bms power supply failed\n");
	}

	/* jeita update work initialize */
	INIT_DELAYED_WORK(&policy->policy_update_work, handle_jeita_policy_update);

	/* jeita update mutex lock initialize  */
	mutex_init(&policy->jeita_update_lock);

	/* jeita wakeup source initialize  */
	policy->jeita_ws = wakeup_source_register(manager->dev, "jeita_ws");
	if (!policy->jeita_ws)
		return -EINVAL;

	manager->jeita_policy = policy;

	hq_info("jeita policy initialize success\n");

	return 0;
}

int hq_jeita_policy_deinit(struct charger_manager *manager)
{
	if (!manager->jeita_policy) {
		return 0;
	}

	cancel_delayed_work_sync(&manager->jeita_policy->policy_update_work);
	wakeup_source_unregister(manager->jeita_policy->jeita_ws);

	devm_kfree(manager->dev, manager->jeita_policy);
	manager->jeita_policy = NULL;

	hq_info("jeita policy deinitialize success\n");

	return 0;
}

int hq_jeita_policy_run(struct charger_manager *manager)
{
	/* keep system wakeup, will relax when work done */
	//__pm_stay_awake(policy->jeita_ws);
	struct hq_jeita_policy *policy = manager->jeita_policy;

	schedule_delayed_work(&policy->policy_update_work, msecs_to_jiffies(100));

	return 0;
}

/*
 * Huaqin Charge Framework Jeita Policy Release Note
 *
 * v1.0.0 - 2024.11.14
 * 1. first release of jeita policy
 */
