// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 LiXun Technology(Shanghai) Co., Ltd.
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/phy/phy.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include "lxchg_printk.h"
#include "lxchg_manager.h"

#ifdef TAG
#undef TAG
#define  TAG "[LX_PD_ADAPTER]"
#endif

/* PD */
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"
#include "lx_adapter_class.h"

struct pd_adapter_policy {
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct adapter_dev *adapter_dev;
	struct device *dev;
	char *adapter_dev_name;
	struct adapter_cap pd_cap;
};

extern void * adapter_get_private(struct adapter_dev *adapter);
static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
	if (tcpm_inquire_typec_attach_state(tcpc) != TYPEC_ATTACHED_SNK)
		return -EINVAL;
	return 0;
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct pd_adapter_policy *pinfo;
	struct adapter_dev *adapter;

	pinfo = container_of(pnb, struct pd_adapter_policy, pd_nb);
	adapter = pinfo->adapter_dev;

	lx_err("PD charger event:%d %d\n", (int)event, (int)noti->pd_state.connected);

	switch(event) {
		case TCP_NOTIFY_SOFT_RESET:
			lx_info("tcpc received soft reset.\n");
			adapter->soft_reset = true;
			break;
		default:
			break;
	}

	return 0;
}

static int pd_handshake(struct adapter_dev *dev)
{
	struct pd_adapter_policy *policy;
	int ret;

	policy = (struct pd_adapter_policy *)adapter_get_private(dev);

	if (policy == NULL || policy->tcpc == NULL) {
		lx_err("policy null\n");
		return -EINVAL;
	}
	lx_info("---->\n");
	ret = tcpm_inquire_pd_pe_ready(policy->tcpc);

	if(!ret) {
		ret = check_typec_attached_snk(policy->tcpc);
		if (ret) {
			msleep(1000);
			ret = tcpm_inquire_pd_pe_ready(policy->tcpc);
		}
	}
	return ret;
}

static int pd_get_cap(struct adapter_dev *dev, struct adapter_cap *cap)
{
	struct pd_adapter_policy *policy;
	struct tcpm_remote_power_cap pd_cap;
	int ret, i;

	policy = (struct pd_adapter_policy *)adapter_get_private(dev);

	if (policy == NULL || policy->tcpc == NULL) {
		lx_err("policy null\n");
		return -EINVAL;
	}

	ret = tcpm_get_remote_power_cap(policy->tcpc, &pd_cap);
	if(ret < 0)
		return ret;

	cap->cnt = pd_cap.nr;
	for (i = 0; i < pd_cap.nr; i++) {
		cap->volt_max[i] = pd_cap.max_mv[i];
		cap->volt_min[i] = pd_cap.min_mv[i];
		cap->curr_max[i] = pd_cap.ma[i];
		cap->curr_min[i] = 100;
		cap->type[i] = pd_cap.type[i];
		lx_info("V[%d-%d] I[100-%d]\n", cap->volt_min[i], cap->volt_max[i], cap->curr_max[i]);
	}

	policy->pd_cap = *cap;

	return ret;
}

static int pd_set_cap(struct adapter_dev *dev, uint8_t nr,
											uint32_t mv, uint32_t ma)
{
	struct pd_adapter_policy *policy;
	struct tcpm_power_cap_val curr_cap;
	int ret;

	policy = (struct pd_adapter_policy *)adapter_get_private(dev);

	if (policy == NULL || policy->tcpc == NULL) {
		lx_err("policy null\n");
		return -EINVAL;
	}
	lx_info("nr:%d mV:%d, mA:%d\n", nr, mv, ma);

	ret = tcpm_inquire_select_source_cap(policy->tcpc, &curr_cap);
	if(ret < 0)
		return ret;

	if(policy->pd_cap.type[nr] == TCPM_POWER_CAP_VAL_TYPE_AUGMENT
		&& curr_cap.type == TCPM_POWER_CAP_VAL_TYPE_FIXED) {
		ret = tcpm_set_apdo_charging_policy(policy->tcpc,
			DPM_CHARGING_POLICY_PPS|DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR, mv, ma, NULL);
	} else if(policy->pd_cap.type[nr] == TCPM_POWER_CAP_VAL_TYPE_FIXED
		&& curr_cap.type == TCPM_POWER_CAP_VAL_TYPE_AUGMENT) {
		ret = tcpm_set_pd_charging_policy(policy->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);
	} else {
		ret = tcpm_dpm_pd_request(policy->tcpc, mv, ma, NULL);
	}

	return ret;
}

static int pd_set_wdt(struct adapter_dev *dev, uint32_t ms)
{
	lx_info("---->\n");
	return 0;
}

static int pd_reset(struct adapter_dev *dev)
{
	struct pd_adapter_policy *policy;
	int ret;

	policy = (struct pd_adapter_policy *)adapter_get_private(dev);

	if (policy == NULL || policy->tcpc == NULL) {
		lx_err("policy null\n");
		return -EINVAL;
	}
	lx_info("---->\n");
	ret = check_typec_attached_snk(policy->tcpc);

	ret = tcpm_set_pd_charging_policy(policy->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);

	return ret;
}

static int set_soft_reset_state(struct adapter_dev *dev, bool val)
{
	dev->soft_reset = !!val;
	lx_info("pd_soft_reset = %d\n", dev->soft_reset);

	return true;
}

static int get_soft_reset_state(struct adapter_dev *dev)
{
	return dev->soft_reset;
}

static struct adapter_ops adapter_ops = {
	.handshake = pd_handshake,
	.get_cap = pd_get_cap,
	.set_cap = pd_set_cap,
	.set_wdt = pd_set_wdt,
	.reset = pd_reset,
	.get_softreset = get_soft_reset_state,
	.set_softreset = set_soft_reset_state,
};

static int pd_adapter_parse_dt(struct charger_manager *manager, struct pd_adapter_policy *policy)
{
	struct device_node *np = of_find_node_by_name(manager->dev->of_node, "lx_pd_adapter");
	lx_info("--->\n");

	if (of_property_read_string(np, "pd_adapter_name", (char const **)&policy->adapter_dev_name) < 0) {
		lx_err("no charger name\n");
		return -ENOMEM;
	}
	return 0;
}

int pd_adapter_init(struct charger_manager *manager)
{
	struct pd_adapter_policy *policy = NULL;
	int ret = 0;

	lx_info("start!\n");

	policy = devm_kzalloc(manager->dev, sizeof(*policy), GFP_KERNEL);
	if (!policy)
		return -ENOMEM;

	ret = pd_adapter_parse_dt(manager, policy);
	if (ret) {
		lx_err("pd adapter disable!\n");
		goto err_out;
	}

	policy->adapter_dev = adapter_register(policy->adapter_dev_name,
		manager->dev, &adapter_ops, policy);
	if (IS_ERR_OR_NULL(policy->adapter_dev)) {
		ret = PTR_ERR(policy->adapter_dev);
		goto err_out;
	}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
		policy->tcpc = manager->tcpc;
#endif
	policy->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(policy->tcpc, &policy->pd_nb,
				TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC |
				TCP_NOTIFY_TYPE_VBUS);
	if (ret < 0) {
		lx_err("register tcpc notifer fail\n");
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}
	lx_info("successfully!\n");
	return 0;

err_get_tcpc_dev:
	adapter_unregister(policy->adapter_dev);

err_out:
	devm_kfree(manager->dev, policy);

	return ret;
}
