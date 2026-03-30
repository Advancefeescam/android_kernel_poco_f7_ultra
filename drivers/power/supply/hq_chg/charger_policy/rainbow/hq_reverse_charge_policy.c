// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include "hq_charger_manager.h"
#include "hq_reverse_charge_policy.h"
#include "xm_chg_uevent.h"
#include "tcpm.h"
#include "hq_printk.h"

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_REVCHG]"
#endif

static void get_reverse_svid(struct charger_manager *manager)
{
	struct reverse_charge_policy *policy = NULL;
	uint32_t pd_vdos[8];

	if (manager == NULL || manager->tcpc == NULL)
		hq_err("manager or manager->charger is NULL\n");

	policy = manager->reverse_charge_policy;

	if (policy->reverse_adapter_svid != 0 && policy->reverse_adapter_svid != 0xff00) {
		hq_err("alredy get svid:%x\n", policy->reverse_adapter_svid);
		return;
	}

	tcpm_inquire_pd_partner_inform(manager->tcpc, pd_vdos);
	hq_info("VDO[0] : %08x\n", pd_vdos[0]);

	policy->reverse_adapter_svid = pd_vdos[0] & 0x0000FFFF;
}

__maybe_unused
static void is_need_limit_curr(struct charger_manager *manager)
{
	union power_supply_propval pval = {0};
	struct pd_port *pd_port = NULL;
	struct reverse_charge_policy *policy = NULL;
	int soc;
	int tbat = 250;
	int limit_state = 0;
	static int last_limit_state = 0;
	int ret = 0;

	if (manager == NULL || manager->tcpc == NULL) {
		hq_err("manager or manager->charger is NULL\n");
		ret = true;
	}

	policy = manager->reverse_charge_policy;
	if ((policy != NULL) && (policy->batt_psy != NULL)) {
		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret < 0) {
			hq_err("get battery capacity failed, ret = %d\n", ret);
			return;
		}
		soc = pval.intval;

		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret < 0) {
			hq_err("get battery temperature failed, ret = %d\n", ret);
			return;
		}
		tbat = pval.intval;
	} else
		ret = -1;

	pd_port = &manager->tcpc->pd_port;

	if (tbat > 0 && soc <= 15)
		limit_state = 1;
	else if (tbat <= 0 && soc <= 15)
		limit_state = 2;
	else
		limit_state = 0;

	if (limit_state != last_limit_state) {
		tcpm_dpm_pd_soft_reset(manager->tcpc, NULL);
		last_limit_state = limit_state;
	}

	hq_info("limit_state = %d, last_limit_state = %d\n", limit_state, last_limit_state);
}

static bool is_enable_revchg_9W(struct charger_manager *manager)
{
	int ret = 0;
	union power_supply_propval pval = {0};
	struct reverse_charge_policy *policy = manager->reverse_charge_policy;
	int batt_soc;
	int tbat;
	int vbus_val = 0;

	if (policy->batt_psy) {
		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret < 0) {
			hq_err("get battery capacity failed, ret = %d\n", ret);
			return ret;
		}
		batt_soc = pval.intval;

		ret = power_supply_get_property(policy->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret < 0) {
			hq_err("get battery temperature failed, ret = %d\n", ret);
			return ret;
		}
		tbat = pval.intval;
	} else
		ret = -1;

	if (manager->master_cp_chg) {
		chargerpump_set_enable_adc(manager->master_cp_chg, true);
		mdelay(100);
		chargerpump_get_adc_value(manager->master_cp_chg, CP_ADC_VBUS, &vbus_val);
		mdelay(10);
		chargerpump_set_enable_adc(manager->master_cp_chg, false);
	}
	get_reverse_svid(manager);

	hq_err("vbus = %d, svid = %d\n", vbus_val, policy->reverse_adapter_svid);

	if (policy->reverse_adapter_svid == 0x5ac) { //apple
		if (!policy->in_otg_mode || tbat < 0 || manager->board_temp > 40000 ||
				(vbus_val > 6500 && vbus_val < 7300)) {
			if (!policy->reverse_quick_charge_flag) {
				hq_err("need report rechg normal\n");
				//xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, REVCHG_NORMAL);
			}
			ret = 0;
		} else if (batt_soc >= 80 || vbus_val >= 7300){
			if (!policy->reverse_quick_charge_flag) {
				hq_err("need report rechg quick\n");
				xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, REVCHG_QUICK);
			} else
				ret = 1;
		} else
			ret = 0;
	} else { // other phone
		if (!policy->in_otg_mode ||
			tbat < 0 || batt_soc < 30 || manager->board_temp > 40000) {
			if (!policy->reverse_quick_charge_flag) {
				hq_err("need report rechg normal\n");
				//xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, REVCHG_NORMAL);
			}
			ret = 0;
		} else {
			if (!policy->reverse_quick_charge_flag) {
				hq_err("need report rechg quick\n");
				xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, REVCHG_QUICK);
			} else
				ret = 1;
		}
	}

	hq_info("state = %d\n", ret);

	return ret;
}

static bool is_enable_revchg_18W(struct charger_manager *manager)
{
	int ret = 0;
	int ibat = 0;
	union power_supply_propval pval = {0};
	struct reverse_charge_policy *policy = manager->reverse_charge_policy;

	if((manager->screen_state == SCREEN_STATE_BRIGHT) || !policy->bms_psy) {
		hq_err("manager->screen_state = %d\n", manager->screen_state);
		return FALSE;
	}

	ret = power_supply_get_property(policy->bms_psy, POWER_SUPPLY_PROP_CURRENT_AVG, &pval);
	if (ret < 0) {
		hq_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}

	ibat = pval.intval;
	if (ibat > 3000000) {
		hq_info("battery discharging curr high, cant enable quick revchg\n");
		return FALSE;
	} else {
		if (policy->revchg_bcl_flag)
			return TRUE;
		else {
			hq_info("revchg_bcl_flag not set\n");
			xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, REVCHG_ENABLE_BCL);
			return FALSE;
		}
	}
}

void reverse_charge_func(struct charger_manager *manager)
{
	struct reverse_charge_policy *policy = manager->reverse_charge_policy;

	switch (policy->otg_vbus_level) {
		case VBUS_0V:
				charger_set_otg(manager->charger, FALSE);
				chargerpump_set_reverse_charge(manager->master_cp_chg, FALSE);
				xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, FALSE);
				policy->in_otg_mode = FALSE;
				policy->pd30_source = FALSE;
				charger_set_chg(manager->charger, TRUE);
			break;
		case VBUS_5V:
			policy->in_otg_mode = TRUE;
			charger_set_otg(manager->charger, TRUE);
		break;
		case VBUS_9V:
			chargerpump_set_reverse_charge(manager->master_cp_chg, TRUE);
			msleep(200);
			tcpm_notify_vbus_stable(manager->tcpc);
			charger_set_otg(manager->charger, FALSE);
			charger_set_chg(manager->charger, FALSE);
		break;
		default:
		break;
    }

	if (manager->reverse_charge_policy->pd30_source) {
		cancel_delayed_work_sync(&manager->reverse_charge_policy->reverse_charge_check_work);
		schedule_delayed_work(&policy->reverse_charge_check_work, msecs_to_jiffies(3000));
	}
}

void update_pdo_caps(struct charger_manager *manager, enum reverse_charge_watt mode)
{
	struct pd_port *pd_port = &manager->tcpc->pd_port;

	hq_info("revchg mode = %d\n", mode);
	switch(mode) {
		case REVCHG_1_5W:
			pd_port->local_src_cap_default.pdos[0] =  0x2601901e; //5V0.3A
			break;
		case REVCHG_7_5W:
			pd_port->local_src_cap_default.pdos[0] =  0x26019096; //5V1.5A
			break;
		case REVCHG_QUICK_9W:
			pd_port->local_src_cap_default.pdos[0] =  0x2602D064; //9V1A
			break;
		case REVCHG_QUICK_18W:
			pd_port->local_src_cap_default.pdos[0] =  0x2602D0C8; //9V2A
			break;
		default:
			break;
	}

	pd_port->local_src_cap_default.nr = 1;
	tcpm_dpm_pd_soft_reset(manager->tcpc, NULL);
}

static void reverse_charge_check_work(struct work_struct *work)
{
	struct reverse_charge_policy *policy =
		container_of(work, struct reverse_charge_policy, reverse_charge_check_work.work);
	struct charger_manager *manager = dev_get_drvdata(policy->dev);

	int ret = 0;

	if (!policy->reverse_charge_wakelock->active)
		__pm_stay_awake(policy->reverse_charge_wakelock);

	switch(policy->otg_vbus_level) {
		case VBUS_5V:
			ret = is_enable_revchg_9W(manager);
			if (!ret) {
				hq_info("cant switch to 9V, stay at 5V");
				is_need_limit_curr(manager);
			} else {
				hq_info("switch to 9V1A");
				update_pdo_caps(manager, REVCHG_QUICK_9W);
				policy->reverse_power_mode = REVCHG_QUICK_9W;
			}
		break;
		case VBUS_9V:
			if (policy->reverse_power_mode == REVCHG_QUICK_9W) {
				ret = is_enable_revchg_9W(manager);
				if (!ret) {
					update_pdo_caps(manager, REVCHG_7_5W);
					policy->reverse_power_mode = REVCHG_7_5W;
					hq_info("cant stay at 9V, switch to 5V");
				}

				ret = is_enable_revchg_18W(manager);
				if (!ret) {
					hq_info("cant switch to 9V2A, stay at 9V1A");
				} else {
					hq_info("switch to 9V2A");
					update_pdo_caps(manager, REVCHG_QUICK_18W);
					policy->reverse_power_mode = REVCHG_QUICK_18W;
				}
			} else if (policy->reverse_power_mode == REVCHG_QUICK_18W) {
				ret = is_enable_revchg_9W(manager);
				if (!ret) {
					update_pdo_caps(manager, REVCHG_7_5W);
					policy->reverse_power_mode = REVCHG_7_5W;
					hq_info("cant stay at 9V, switch to 5V");
				} else {
					if (manager->screen_state == SCREEN_STATE_BRIGHT) {
						hq_info("cant stay at 9V2A, switch to 9V1A");
						update_pdo_caps(manager, REVCHG_QUICK_9W);
						policy->reverse_power_mode = REVCHG_QUICK_9W;
						mdelay(50);
						xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, REVCHG_DISABLE_BCL);
					}
				}
			}
		break;
		default:break;
	}

	schedule_delayed_work(&policy->reverse_charge_check_work, msecs_to_jiffies(3000));
}

int reverse_charge_policy_init(struct charger_manager *manager)
{
	struct reverse_charge_policy *policy = NULL;
	char *name = NULL;

	if (manager->reverse_charge_policy) {
		hq_err("reverse charge already initialized\n");
		return -EINVAL;
	}

	policy = devm_kzalloc(manager->dev, sizeof(*policy), GFP_KERNEL);
	if (IS_ERR_OR_NULL(policy))  {
		hq_err("failed to get shutdown policy\n");
		return -ENOMEM;
	}

	policy->dev = manager->dev;

	name = devm_kasprintf(policy->dev, GFP_KERNEL, "%s",
		"reverse_charge suspend wakelock");
	policy->reverse_charge_wakelock =
		wakeup_source_register(NULL, name);

	policy->batt_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(policy->batt_psy)) {
		hq_err("get battery power supply failed\n");
		return -EINVAL;
	}

	policy->bms_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(policy->bms_psy)) {
		hq_err("get bms power supply failed\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&policy->reverse_charge_check_work, reverse_charge_check_work);

	manager->reverse_charge_policy = policy;

	hq_info("reverse charge policy initialize success\n");

	return 0;
}

int reverse_charge_policy_deinit(struct charger_manager *manager)
{
	if (!manager->reverse_charge_policy) {
		return 0;
	}

	cancel_delayed_work_sync(&manager->reverse_charge_policy->reverse_charge_check_work);
	devm_kfree(manager->dev, manager->reverse_charge_policy);
	manager->reverse_charge_policy = NULL;

	hq_info("reverse charge policy deinitialize success\n");

	return 0;
}

int reverse_charge_policy_stop(struct charger_manager *manager)
{
	struct reverse_charge_policy *policy = manager->reverse_charge_policy;

	hq_info("stop reverse charge policy\n");
	update_pdo_caps(manager, REVCHG_7_5W);
	xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 0);
	policy->reverse_power_mode = 0;
	policy->otg_vbus_level = 0;
	policy->in_otg_mode = 0;
	policy->pd30_source = 0;
	policy->otg_curr_lmt_flag = 0;
	policy->reverse_quick_charge_flag = 0;
	policy->reverse_adapter_svid = 0;
	charger_set_chg(manager->charger, true);
	cancel_delayed_work_sync(&policy->reverse_charge_check_work);
	__pm_relax(policy->reverse_charge_wakelock);

	return 0;
}

int reverse_charge_policy_run(struct charger_manager *manager)
{
	struct reverse_charge_policy *policy = manager->reverse_charge_policy;

	cancel_delayed_work_sync(&manager->reverse_charge_policy->reverse_charge_check_work);
	schedule_delayed_work(&policy->reverse_charge_check_work, msecs_to_jiffies(50));

	return 0;
}
