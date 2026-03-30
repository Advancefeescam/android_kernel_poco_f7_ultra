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
#include "hq_thermal_policy.h"

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_THERMAL]"
#endif

static int thermal_policy_parse_dts(struct hq_thermal_policy *policy)
{
	int ret = 0;
	int byte_len = 0;
	struct device_node *node = policy->dev->of_node;

	policy->thermal_enable = of_property_read_bool(node, "hq,thermal-enable");

	if (of_find_property(node, "hq,pd-thermal-mitigation", &byte_len)) {
		policy->pd_thermal_mitigation = devm_kzalloc(policy->dev, byte_len, GFP_KERNEL);
		policy->pd_thermal_levels = byte_len / sizeof(u32);
		ret = of_property_read_u32_array(node, "hq,pd-thermal-mitigation",
			policy->pd_thermal_mitigation, policy->pd_thermal_levels);
		if (ret < 0) {
			hq_err("pd_thermal_mitigation parse error\n");
		}
	}

	if (of_find_property(node, "hq,qc2-thermal-mitigation", &byte_len)) {
		policy->qc2_thermal_mitigation = devm_kzalloc(policy->dev, byte_len, GFP_KERNEL);
		policy->qc2_thermal_levels = byte_len / sizeof(u32);
		ret = of_property_read_u32_array(node, "hq,qc2-thermal-mitigation",
			policy->qc2_thermal_mitigation, policy->qc2_thermal_levels);
		if (ret < 0) {
			hq_err("qc2_thermal_mitigation parse error\n");
		}
	}

	policy->thermal_level_max = max(policy->pd_thermal_levels, policy->qc2_thermal_levels);

	return ret;
}

static int thermal_policy_set_thermal_level(struct hq_thermal_policy *policy)
{
	int rc;
	int thermal_level = 0;
	char *voter_name = NULL;
	struct charger_manager *manager = NULL;

#ifdef THERMAL_POLICY_SHILED_ON_BOOT
	int tbat = 250;
	union power_supply_propval pval = {0,};
	struct timespec64 ts_boot;
	struct timespec64 ts_shiled;
#endif

	manager = dev_get_drvdata(policy->dev);
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	/* keep thermal_level as last thermal level */
	thermal_level = policy->thermal_level;
	if (thermal_level < 0)
		goto err;

	if (policy->last_thermal_level == policy->thermal_level) {
		hq_debug("thermal level no change\n");
		return 0;
	}

#ifdef KERNEL_FACTORY_HQ_CHG
	/* force disable thermal policy in factory build */
	policy->thermal_enable = false;
#endif //KERNEL_FACTORY_HQ_CHG

	if (policy->thermal_enable == false) {
		hq_err("thermal ibat limit is disable\n");
		return 0;
	}

	policy->pd_active = manager->pd_active;

	policy->board_temp = manager->board_temp;

	if (policy->thermal_type == TEMP_THERMAL_TYPE) {
		voter_name = TEMP_THERMAL_DAEMON_VOTER;
	} else if (policy->thermal_type == CALL_THERMAL_TYPE) {
		voter_name = CALL_THERMAL_DAEMON_VOTER;
	} else {
		hq_err("unknow thermal type = %d\n", policy->thermal_type);
		return 0;
	}

#ifdef THERMAL_POLICY_SHILED_ON_BOOT
	/*
	 * force thermal level to zero within 60s of kernel bootup.
	 * battery temperature limitation to avoid trigger
	 * hot temperature shutdown when bootup.
	 */
	rc = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0)
		hq_err("get battery temperature error.\n");
	else
		tbat = pval.intval;

	ktime_get_boottime_ts64(&ts_boot);
	ts_shiled.tv_sec = 60;
	ts_shiled.tv_nsec = 0;

	if ((timespec64_compare(&ts_boot, &ts_shiled) == -1) && (tbat < 450)) {
		hq_info("thermal_level = %d, tbat = %d, force thermal level to zero.\n",
			policy->thermal_level, tbat);
		thermal_level = 0;
	}
#endif

	if (policy->pd_active == CHARGE_PD_PPS_ACTIVE
		|| policy->thermal_type == CALL_THERMAL_TYPE) {
		if (thermal_level >= policy->pd_thermal_levels) {
			hq_err("thermal level is invalid\n");
			goto err;
		}

		vote(policy->total_fcc_votable, voter_name, true,
			policy->pd_thermal_mitigation[thermal_level]);
	} else {
		if (thermal_level >= policy->qc2_thermal_levels) {
			hq_err("thermal level is invalid\n");
			goto err;
		}

		vote(policy->total_fcc_votable, voter_name, true,
			policy->qc2_thermal_mitigation[thermal_level]);
	}

	policy->last_thermal_level = thermal_level;

	rc = get_client_vote_locked(policy->total_fcc_votable, voter_name);
	hq_info("%s: thermal vote susessful val = %d, current = %d\n", voter_name, thermal_level, rc);

	return 0;
err:
	vote(policy->total_fcc_votable, voter_name, false, 0);

	return 0;
}

#ifdef THERMAL_POLICY_SHILED_ON_BOOT
static void thermal_restore_work(struct work_struct *work)
{
	struct hq_thermal_policy *policy = 
				container_of(work, struct hq_thermal_policy, thermal_restore_work.work);

	if (policy == NULL)
		return;

	/*
	 * always use TEMP_THERMAL_DAEMON_VOTER within 60s of kernel bootup
	 * so use TEMP_THERMAL_DAEMON_VOTER here to restore thermal level
	 */
	hq_info("restore thermal level after kernel bootup 60s\n");
	thermal_policy_set_thermal_level(policy);
}
#endif


static void handle_thermal_policy_update(struct work_struct *work)
{
	struct hq_thermal_policy *policy = container_of(work, struct hq_thermal_policy, policy_update_work.work);

	mutex_lock(&policy->thermal_update_lock);

	thermal_policy_set_thermal_level(policy);

	mutex_unlock(&policy->thermal_update_lock);
}

int hq_thermal_policy_init(struct charger_manager *manager)
{
	struct hq_thermal_policy *policy = NULL;
	int ret = 0;
#ifdef THERMAL_POLICY_SHILED_ON_BOOT
	struct timespec64 ts_boot;
	struct timespec64 ts_delay;
	struct timespec64 ts_shiled;
#endif
	if (manager->thermal_policy) {
		hq_err("thermal policy already initialized\n");
		return -EINVAL;
	}

	policy = devm_kzalloc(manager->dev, sizeof(*policy), GFP_KERNEL);
	if (!policy) {
		return -ENOMEM;
	}

	policy->dev = manager->dev;

	ret = thermal_policy_parse_dts(policy);
	if (ret < 0) {
		hq_err("parse thermal policy dts failed\n");
		return ret;
	}

	policy->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!policy->total_fcc_votable) {
		hq_err("find TOTAL_FCC voltable failed\n");
		return ret;
	}

	policy->batt_psy = power_supply_get_by_name("battery");
	if (!policy->batt_psy) {
		hq_err("get battery power supply failed\n");
		return ret;
	}

	INIT_DELAYED_WORK(&policy->policy_update_work, handle_thermal_policy_update);

	/* thermal update mutex lock initialize  */
	mutex_init(&policy->thermal_update_lock);

#ifdef THERMAL_POLICY_SHILED_ON_BOOT
	INIT_DELAYED_WORK(&policy->thermal_restore_work, thermal_restore_work);
	ktime_get_boottime_ts64(&ts_boot);
	ts_shiled.tv_sec = 60;
	ts_shiled.tv_nsec = 0;
	ts_delay = timespec64_sub(ts_shiled, ts_boot);
	hq_info("ts_delay: tv_sec = %llds, tv_nsec = %ldns, jiffies = %lu\n",
		ts_delay.tv_sec, ts_delay.tv_nsec, timespec64_to_jiffies(&ts_delay));
	schedule_delayed_work(&policy->thermal_restore_work, timespec64_to_jiffies(&ts_delay));
#endif

	manager->thermal_policy = policy;

	hq_info("thermal policy initialize success\n");

	return 0;
}

int hq_thermal_policy_deinit(struct charger_manager *manager)
{
	if (!manager->thermal_policy) {
		return 0;
	}

	cancel_delayed_work_sync(&manager->thermal_policy->policy_update_work);

#ifdef THERMAL_POLICY_SHILED_ON_BOOT
	cancel_delayed_work_sync(&manager->thermal_policy->thermal_restore_work);
#endif

	devm_kfree(manager->dev, manager->thermal_policy);
	manager->thermal_policy = NULL;

	hq_info("thermal policy deinitialize success\n");

	return 0;
}

int hq_thermal_policy_run(struct charger_manager *manager)
{
	struct hq_thermal_policy *policy = manager->thermal_policy;

	schedule_delayed_work(&policy->policy_update_work, msecs_to_jiffies(50));

	return 0;
}
