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
#include "hq_charger_manager.h"
#include "hq_shutdown_policy.h"
#include "hq_fg_class.h"
#include "xm_chg_uevent.h"

static int battery_info_update(struct hq_shutdown_policy *policy)
{
	int ret = 0;
	union power_supply_propval pval = {0};

	if (IS_ERR_OR_NULL(policy))  {
		hq_err("failed to get shutdown policy\n");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(policy->batt_psy) || IS_ERR_OR_NULL(policy->bms_psy))  {
		hq_err("failed to get power supply\n");
		return -EINVAL;
	}

	if (policy->batt_psy) {
		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret < 0) {
			hq_err("get battery voltage failed, ret = %d\n", ret);
			return ret;
		}
		policy->vbat = pval.intval / 1000;

		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
		if (ret < 0) {
			hq_err("get battery status failed, ret = %d\n", ret);
			return ret;
		}
		policy->batt_status = pval.intval;

		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret < 0) {
			hq_err("get battery capacity failed, ret = %d\n", ret);
			return ret;
		}
		policy->batt_soc = pval.intval;

		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret < 0) {
			hq_err("get battery temperature failed, ret = %d\n", ret);
			return ret;
		}
		policy->tbat = pval.intval;
	} else {
		return -1;
	}

	if (policy->bms_psy) {
		ret = power_supply_get_property(policy->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret < 0) {
			hq_err("get bms capacity failed, ret = %d\n", ret);
			return ret;
		}
		policy->fg_soc = pval.intval;

		ret = power_supply_get_property(policy->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret < 0) {
			hq_err("get bms cycle count failed, ret = %d\n", ret);
			return ret;
		}
		policy->cycle_count = pval.intval;

		ret = power_supply_get_property(policy->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0) {
			hq_err("get bms ibat failed, ret = %d\n", ret);
			return ret;
		}
		policy->ibat = -pval.intval;

	} else {
		return -1;
	}

	return ret;
}

static int hq_get_temp_state(int tbat, int *temp_state_val)
{
	static int temp_state = 0;

	if (tbat < -60) {
		if (temp_state > BAT_COLD && tbat >= -80)
			temp_state = BAT_LITTLE_COLD;
		else
			temp_state = BAT_COLD;
	} else if (tbat < 0) {
		if (temp_state <= BAT_COLD && tbat < -40)
			temp_state = BAT_COLD;
		else if (temp_state > BAT_LITTLE_COLD && tbat >= -20)
			temp_state = BAT_COOL;
		else
			temp_state = BAT_LITTLE_COLD;
	} else if (tbat < 100) {
		if (temp_state <= BAT_LITTLE_COLD && tbat < 20)
			temp_state = BAT_LITTLE_COLD;
		else if (temp_state > BAT_COOL && tbat >= 80)
			temp_state = BAT_NORMAL;
		else
			temp_state = BAT_COOL;
	} else {
		if (temp_state <= BAT_COOL && tbat < 120)
			temp_state = BAT_COOL;
		else
			temp_state = BAT_NORMAL;
	}

	*temp_state_val = temp_state;

	hq_info("read temp_state = %d\n", *temp_state_val);

	return 0;
}

static void hq_get_count_shutdown_vol(int temp_state, int dod_count, int *count_vol)
{
	switch(temp_state) {
	case BAT_COLD:
		if (dod_count <= 80) {
			count_vol[0] = 2800;
			count_vol[1] = 2850;
			count_vol[2] = 2900;
		} else if (dod_count > 80 && dod_count <= 199) {
			count_vol[0] = 2850;
			count_vol[1] = 2900;
			count_vol[2] = 2950;
		} else if (dod_count > 199 && dod_count <= 559) {
			count_vol[0] = 2900;
			count_vol[1] = 2950;
			count_vol[2] = 3000;
		} else {
			count_vol[0] = 2950;
			count_vol[1] = 3000;
			count_vol[2] = 3050;
			}
		break;

	case BAT_LITTLE_COLD:
		if (dod_count <= 80) {
			count_vol[0] = 2800;
			count_vol[1] = 2850;
			count_vol[2] = 2900;
		} else if (dod_count > 80 && dod_count <= 199) {
			count_vol[0] = 2950;
			count_vol[1] = 3000;
			count_vol[2] = 3050;
		} else if (dod_count > 199 && dod_count <= 559) {
			count_vol[0] = 3000;
			count_vol[1] = 3050;
			count_vol[2] = 3100;
		} else {
			count_vol[0] = 3050;
			count_vol[1] = 3100;
			count_vol[2] = 3150;
		}
		break;

	case BAT_COOL:
		if (dod_count <= 80) {
			count_vol[0] = 2900;
			count_vol[1] = 2950;
			count_vol[2] = 2950;
		} else if (dod_count > 80 && dod_count <= 199) {
			count_vol[0] = 3050;
			count_vol[1] = 3100;
			count_vol[2] = 3100;
		} else if (dod_count > 199 && dod_count <= 559) {
			count_vol[0] = 3100;
			count_vol[1] = 3150;
			count_vol[2] = 3150;
		} else {
			count_vol[0] = 3200;
			count_vol[1] = 3250;
			count_vol[2] = 3250;
		}
		break;

	case BAT_NORMAL:
		if (dod_count <= 80) {
			count_vol[0] = 3000;
			count_vol[1] = 3050;
			count_vol[2] = 3050;
		} else if (dod_count > 80 && dod_count <= 199) {
			count_vol[0] = 3100;
			count_vol[1] = 3150;
			count_vol[2] = 3150;
		} else if (dod_count > 199 && dod_count <= 559) {
			count_vol[0] = 3200;
			count_vol[1] = 3250;
			count_vol[2] = 3250;
		} else {
			count_vol[0] = 3300;
			count_vol[1] = 3340;
			count_vol[2] = 3340;
		}
		break;

	default:
		count_vol[0] = 3400;
		count_vol[1] = 3400;
		count_vol[2] = 3400;
		break;
	}

	hq_info("count_vol0 = %d, count_vol1 = %d, count_vol2 = %d\n",
		count_vol[0], count_vol[1], count_vol[2]);
}

static void hq_get_cycle_shutdown_vol(int temp_state, int cycle_count, int *cycle_vol)
{
	switch(temp_state) {
	case BAT_COLD:
	case BAT_LITTLE_COLD:
		if (cycle_count <= 600) {
			cycle_vol[0] = 2800;
			cycle_vol[1] = 2850;
			cycle_vol[2] = 2900;
		} else if (cycle_count > 600 && cycle_count <= 1200) {
			cycle_vol[0] = 3000;
			cycle_vol[1] = 3050;
			cycle_vol[2] = 3100;
		} else {
			cycle_vol[0] = 3050;
			cycle_vol[1] = 3100;
			cycle_vol[2] = 3150;
		}
		break;

	case BAT_COOL:
	case BAT_NORMAL:
		if (cycle_count <= 600) {
			cycle_vol[0] = 3000;
			cycle_vol[1] = 3050;
			cycle_vol[2] = 3050;
		} else if (cycle_count > 600 && cycle_count <= 1200) {
			cycle_vol[0] = 3200;
			cycle_vol[1] = 3250;
			cycle_vol[2] = 3250;
		} else {
			cycle_vol[0] = 3300;
			cycle_vol[1] = 3340;
			cycle_vol[2] = 3340;
		}
		break;

	default:
		cycle_vol[0] = 3400;
		cycle_vol[1] = 3400;
		cycle_vol[2] = 3400;
		break;
	}

	hq_info("cycle_vol0 = %d, cycle_vol1 = %d, cycle_vol2 = %d\n",
		cycle_vol[0], cycle_vol[1], cycle_vol[2]);
}

static void hq_update_delta_soh(struct hq_shutdown_policy *policy, int shutdown_fg_vol)
{
	u8 delta_termV_index = 0;
	int i = 0;

	if (shutdown_fg_vol < DEFAULT_FG_TERM_VOL) {
		policy->fuel_gauge->delta_soh = 0;
		return;
	}

	delta_termV_index = (shutdown_fg_vol - DEFAULT_FG_TERM_VOL) / 50;

	if (delta_termV_index <= 0)
		policy->fuel_gauge->delta_soh = 0;
	else if (delta_termV_index >= MAX_TERMV_SOH_TABLE_CNT)
		policy->fuel_gauge->delta_soh = 11;
	else {
		for (i = 0; i < MAX_TERMV_SOH_TABLE_CNT; i++) {
			if(delta_termV_index == delta_termV_SOH_table[i].delta_termV_index) {
				policy->fuel_gauge->delta_soh = delta_termV_SOH_table[i].delta_soh;
				break;
			}
		}
	}
	hq_info("delta_termV_index = %d, delta_soh = %d",
		delta_termV_index, policy->fuel_gauge->delta_soh);
}

static void hq_update_shutdown_voltage(struct hq_shutdown_policy *policy)
{
	int ret = 0;
	int count_vol[3] = {0};
	int cycle_vol[3] = {0};
	int temp_state = 0;
	int shutdown_fg_vol;

	if (IS_ERR_OR_NULL(policy))  {
		hq_err("failed to get shutdown policy\n");
		return;
	}

	if (IS_ERR_OR_NULL(policy->fuel_gauge))  {
		hq_err("failed to get fuel gauge\n");
		return;
	}

	ret = hq_get_temp_state(policy->tbat, &temp_state);
	if (ret < 0) {
		hq_err("failed to get temp state\n");
		return;
	}

	ret = fuel_gauge_get_dod_count(policy->fuel_gauge);
	if (ret < 0) {
		hq_err("failed to get dod count\n");
		return;
	}else{
		policy->dod_count = ret;
	}

	hq_get_count_shutdown_vol(temp_state, policy->dod_count, count_vol);
	hq_get_cycle_shutdown_vol(temp_state, policy->cycle_count, cycle_vol);

	policy->shutdown_vol = max(count_vol[0], cycle_vol[0]);
	policy->shutdown_delay_vol = max(count_vol[1], cycle_vol[1]);
	shutdown_fg_vol = max(count_vol[2], cycle_vol[2]);

	if (shutdown_fg_vol != policy->shutdown_fg_vol) {
		ret = fuel_gauge_set_term_voltage(policy->fuel_gauge, shutdown_fg_vol);
		if (ret < 0) {
			hq_err("failed to set term voltage\n");
		}else{
			policy->shutdown_fg_vol = shutdown_fg_vol;
		}
		hq_update_delta_soh(policy, shutdown_fg_vol);
	}

	hq_info("shutdown voltage=%d,%d,%d state=%d,%d cycle=%d count=%d\n",
				policy->shutdown_vol, policy->shutdown_delay_vol, policy->shutdown_fg_vol,
				policy->tbat, temp_state, policy->cycle_count, policy->dod_count);
}

static int low_vbat_power_off(struct hq_shutdown_policy *policy)
{
	static int count = 0;
	if (IS_ERR_OR_NULL(policy))  {
		hq_err("failed to get shutdown policy\n");
		return -EINVAL;
	}

	if ((policy->fg_soc == 0) &&
		(policy->vbat < policy->shutdown_vol) && !policy->shutdown_delay) {
		if (policy->batt_status == POWER_SUPPLY_STATUS_CHARGING) {
			if (policy->ibat > 0) {
				count++;
				hq_info("status is chg count: %d\n", count);
			} else
				count = 0;
		} else {
			policy->SOC0_shutdown = true;
			hq_info("status is not chg force power off trigger: vbat: %d\n", policy->vbat);
			return true;
		}

		if (count > 3) {
			policy->SOC0_shutdown = true;
			hq_err("status is chg force power off trigger: vbat: %d\n", policy->vbat);
			return true;
		}
	}

	hq_info("fg_soc = %d, shutdown count = %d, SOC0_shutdown = %d\n",
		policy->fg_soc, count, policy->SOC0_shutdown);

	return false;
}

static void power_off_check_work(struct hq_shutdown_policy *policy)
{
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int rc = 0;
#endif
	if (IS_ERR_OR_NULL(policy))  {
		hq_err("failed to get shutdown policy\n");
		return;
	}

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (IS_ERR_OR_NULL(policy->fuel_gauge))  {
		hq_err("failed to get fuel gauge\n");
		return;
	}

	rc = fuel_gauge_check_fg_status(policy->fuel_gauge);
	if (policy->batt_soc == 1 || (rc && policy->vbat)) {
#else
	if (policy->batt_soc == 1) {
#endif
		if ((policy->vbat < policy->shutdown_delay_vol) &&
			(policy->batt_status != POWER_SUPPLY_STATUS_CHARGING) && (policy->vbat >= policy->shutdown_vol)) {
				policy->shutdown_delay = true;
		} else if (policy->batt_status == POWER_SUPPLY_STATUS_CHARGING
						&& policy->shutdown_delay) {
				policy->shutdown_delay = false;
		}
	} else
		policy->shutdown_delay = false;

	if (policy->last_shutdown_delay != policy->shutdown_delay) {
		policy->last_shutdown_delay = policy->shutdown_delay;
		power_supply_changed(policy->usb_psy);
		power_supply_changed(policy->batt_psy);
		xm_charge_uevent_report(CHG_UEVENT_SHUTDOWN_DELAY, policy->shutdown_delay);
		hq_info("power off countdown trigger: vbat: %d\n", policy->vbat);
	}
	hq_info("shutdown_delay = %d, last_shutdown_delay = %d\n",
		policy->shutdown_delay, policy->last_shutdown_delay);
}

static void shutdown_policy_check_work(struct work_struct *work)
{
	int ret = 0;
	struct hq_shutdown_policy *policy =
		container_of(work, struct hq_shutdown_policy, policy_check_work.work);
	struct charger_manager *manager = power_supply_get_drvdata(policy->batt_psy);
	static bool first_flag = true;

	mutex_lock(&policy->shutdown_check_lock);
	ret = battery_info_update(policy);
	mutex_unlock(&policy->shutdown_check_lock);
	if (ret < 0)
		goto retry;

	mutex_lock(&policy->shutdown_check_lock);
	hq_update_shutdown_voltage(policy);
	if (manager) {
		manager->fuel_gauge ->shutdown_vol = policy->shutdown_vol;
		manager->fuel_gauge ->shutdown_delay_vol = policy->shutdown_delay_vol;
	} else {
		hq_err("manager is NULL\n");
		mutex_unlock(&policy->shutdown_check_lock);
		goto retry;
	}
	mutex_unlock(&policy->shutdown_check_lock);

	ret = low_vbat_power_off(policy);
	if (ret == true)
		return;

	power_off_check_work(policy);

retry:
	if(first_flag) {
		schedule_delayed_work(&policy->policy_check_work, msecs_to_jiffies(5000));
		first_flag = false;
	} else if (policy->vbat < INCREASE_FREQUENCY_VOL || policy->batt_soc <= 2) {
		schedule_delayed_work(&policy->policy_check_work, msecs_to_jiffies(5000));
	} else
		schedule_delayed_work(&policy->policy_check_work, msecs_to_jiffies(20000));
}

int hq_shutdown_policy_init(struct charger_manager *manager)
{
	struct hq_shutdown_policy *policy = NULL;

	if (manager->shutdown_policy) {
		hq_err("shutdown policy already initialized\n");
		return -EINVAL;
	}

	policy = devm_kzalloc(manager->dev, sizeof(*policy), GFP_KERNEL);
	if (IS_ERR_OR_NULL(policy))  {
		hq_err("failed to get shutdown policy\n");
		return -ENOMEM;
	}

	policy->dev = manager->dev;

	/* power supply/class initialize */
	policy->batt_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(policy->batt_psy)) {
		hq_err("get battery power supply failed\n");
		return -EINVAL;
	}

	policy->usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(policy->usb_psy)) {
		hq_err("get usb power supply failed\n");
		return -EINVAL;
	}

	policy->bms_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(policy->bms_psy)) {
		hq_err("get bms power supply failed\n");
		return -EINVAL;
	}

	if (policy->fuel_gauge == NULL) {
		policy->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(policy->fuel_gauge)) {
			hq_err("can not find fuel_gauge device\n");
			return -EINVAL;
		}
	}

	INIT_DELAYED_WORK(&policy->policy_check_work, shutdown_policy_check_work);
	mutex_init(&policy->shutdown_check_lock);

	policy->SOC0_shutdown = false;
	policy->shutdown_delay = false;
	policy->last_shutdown_delay = false;
	policy->shutdown_vol = DEFAULT_SHUTDOWN_VOL;
	policy->shutdown_delay_vol = DEFAULT_SHUTDOWN_DELAY_VOL;
	manager->shutdown_policy = policy;

	hq_info("shutdown policy initialize success\n");

	return 0;
}

int hq_shutdown_policy_deinit(struct charger_manager *manager)
{
	if (!manager->shutdown_policy) {
		return 0;
	}

	cancel_delayed_work_sync(&manager->shutdown_policy->policy_check_work);
	mutex_destroy(&manager->shutdown_policy->shutdown_check_lock);
	devm_kfree(manager->dev, manager->shutdown_policy);
	manager->shutdown_policy = NULL;

	hq_info("shutdown policy deinitialize success\n");

	return 0;
}

int hq_shutdown_policy_run(struct charger_manager *manager)
{
	struct hq_shutdown_policy *policy = manager->shutdown_policy;

	schedule_delayed_work(&policy->policy_check_work, msecs_to_jiffies(500));

	return 0;
}
