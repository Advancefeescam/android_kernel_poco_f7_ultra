// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023-2023 XRing Technologies Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/cpu_pm.h>
#include <linux/arm-smccc.h>
#include <linux/version.h>
#include <linux/syscore_ops.h>
#include <linux/irqchip.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/suspend.h>
#include <dt-bindings/xring/platform-specific/fcm_acpu_address_map.h>
#include <dt-bindings/xring/platform-specific/dfx_memory_layout.h>
#include <dt-bindings/xring/common/pm/include/sys_pm_comm.h>
#include <dt-bindings/xring/platform-specific/pm/include/sys_pm_plat.h>
#include <dt-bindings/xring/platform-specific/common/mdr/include/mdr_public_if.h>
#include <soc/xring/trace_hook_set.h>
#include "xring/power/x_sys_pm.h"
#include "x_sys_pm_private.h"
#include "soc/xring/x_sys_pm.h"

#define SYS_PM_MARK(tick, keypoint) \
	do { \
		sys_pm_sctrl_tickmark(tick); \
		set_sr_keypoint(keypoint); \
	} while (0)

static bool g_is_in_suspend_resume;
u32 g_wksrc_stat;
static void __iomem *g_sctrl_base;
static void __iomem *g_sr_dfx_base;
static struct kobject *g_sleep_stats_kobject;

static const char * const g_subsys_lp_label[] = {
#define X(ENUM, STR) STR,
	SUBSYS_ID
#undef X
};

static ssize_t sys_pm_subsys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf, enum subsys_id id)
{
	struct subsys_low_power *subsys_data = (struct subsys_low_power *)SR_DFX_LOW_PWR;
	struct subsys_low_power subsys;
	ssize_t ret = 0;

	if (id >= SUBSYS_MAX || !g_subsys_lp_label[id])
		return -EINVAL;

	subsys = subsys_data[id];

	ret = snprintf(buf, PAGE_SIZE, "%s:\nCount = %llu\nLast Entered At = %llu\nLast Exited At = %llu\nAccumulated Duration = %llu\n",
			g_subsys_lp_label[id],
			subsys.count,
			subsys.last_entry_time,
			subsys.last_exit_time,
			subsys.total_duration);
	return ret;
}

#define CREATE_READONLY_SYSFS_FILE(name, id) \
static ssize_t sys_pm_##name##_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) \
{ \
	return sys_pm_subsys_show(kobj, attr, buf, id); \
} \
static struct kobj_attribute name##_attr = __ATTR(name, 0644, sys_pm_##name##_show, NULL)

CREATE_READONLY_SYSFS_FILE(xctrl_cpu, SUBSYS_XCTRL_CPU);
CREATE_READONLY_SYSFS_FILE(shub, SUBSYS_SHUB);
CREATE_READONLY_SYSFS_FILE(adsp, SUBSYS_ADSP);
CREATE_READONLY_SYSFS_FILE(lpcore, SUBSYS_LCORE);
CREATE_READONLY_SYSFS_FILE(xrse, SUBSYS_XRSE);

static struct attribute *xring_sleep_stats_attrs[] = {
	&xctrl_cpu_attr.attr,
	&shub_attr.attr,
	&adsp_attr.attr,
	&lpcore_attr.attr,
	&xrse_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = xring_sleep_stats_attrs,
};

void __iomem *sys_pm_get_dfx_base(void)
{
	return g_sr_dfx_base;
}

u32 sys_pm_wksrc_stat_get(void)
{
	return g_wksrc_stat;
}

void sys_pm_sctrl_tickmark(enum sys_pm_sctrl_tick tick)
{
	if (!IS_ERR_OR_NULL(g_sctrl_base))
		writel(SCTRL_TICK(tick), g_sctrl_base + SCTRL_TICKMARK_REG);
}

bool is_system_in_suspend_resume(void)
{
	return g_is_in_suspend_resume;
}
EXPORT_SYMBOL(is_system_in_suspend_resume);

static inline bool is_x_sys_pm_suspend(void)
{
	return system_state == SYSTEM_SUSPEND;
}

static int cpu_pm_notifier_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	switch (action) {
	case CPU_CLUSTER_PM_ENTER:
		if (is_x_sys_pm_suspend())
			x_sys_pm_print(NULL, "%s: CPU_CLUSTER_PM_ENTER", __func__);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block g_cpu_pm_notifier_nb = {
	.notifier_call = cpu_pm_notifier_cb,
	.priority = PRIO_NOTIFIER_SYS_PM
};

static int pm_notifier_cb(struct notifier_block *nb,
					unsigned long mode, void *_unused)
{
	int ret = 0;

	switch (mode) {
	case PM_SUSPEND_PREPARE:
		g_is_in_suspend_resume = true;
		set_sr_keypoint(SR_KEYPOINT_SUSPEND_START);
		sys_pm_sctrl_tickmark(SYS_TICK_ACPU_SUSPEND_FINISH);

		ret = cpu_pm_register_notifier(&g_cpu_pm_notifier_nb);
		if (ret != 0) {
			x_sys_pm_print(NULL, "%s:%d Failed to register PM notifier, ret:%d!\n",
				__func__, __LINE__, ret);
		}
		break;

	case PM_POST_SUSPEND:
		sys_pm_sctrl_tickmark(SYS_TICK_ACPU_RESUME_BEGIN);

		cpu_pm_unregister_notifier(&g_cpu_pm_notifier_nb);

		g_is_in_suspend_resume = false;
		set_sr_keypoint(SR_KEYPOINT_RESUME_END);
		break;

	default:
		break;
	}

	return ret;
}

static struct notifier_block g_pm_notifier_nb = {
	.notifier_call = pm_notifier_cb,
};

static int x_sys_pm_create_sysfs(struct platform_device *dev)
{
	g_sleep_stats_kobject = kobject_create_and_add("xring_stats", &dev->dev.kobj);

	if (IS_ERR_OR_NULL(g_sleep_stats_kobject))
		return -ENOMEM;

	if (sysfs_create_group(g_sleep_stats_kobject, &attr_group)) {
		kobject_put(g_sleep_stats_kobject);
		return -ENOMEM;
	}

	return 0;
}

static int x_sys_pm_ioremap(void)
{
	g_sctrl_base = ioremap(ACPU_LMS_SYS_CTRL, ACPU_LMS_SYS_CTRL_SIZE);

	if (IS_ERR_OR_NULL(g_sctrl_base))
		return -ENOMEM;

	g_sr_dfx_base = ioremap(SR_DFX_PHY_BASE, SR_DFX_PHY_SIZE);
	if (IS_ERR_OR_NULL(g_sr_dfx_base)) {
		iounmap(g_sctrl_base);
		return -ENOMEM;
	}

	return 0;
}

static void x_sys_pm_iounmap(void)
{
	if (!IS_ERR_OR_NULL(g_sctrl_base)) {
		iounmap(g_sctrl_base);
		g_sctrl_base = NULL;
	}

	if (!IS_ERR_OR_NULL(g_sr_dfx_base)) {
		iounmap(g_sr_dfx_base);
		g_sr_dfx_base = NULL;
	}
}

static int x_sys_pm_prepare(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_SUSPEND_BEGIN, SR_KEYPOINT_SUSPEND_PREPARE);
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}

static int x_sys_pm_suspend(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_SUSPEND, SR_KEYPOINT_SUSPEND);
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}

static int x_sys_pm_suspend_late(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_SUSPEND_LATE, SR_KEYPOINT_SUSPEND_LATE);
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}

static int x_sys_pm_suspend_noirq(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_SUSPEND_NOIRQ, SR_KEYPOINT_SUSPEND_NOIRQ);
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}

static int x_sys_pm_resume_noirq(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_RESUME_NOIRQ, SR_KEYPOINT_RESUME_NOIRQ);
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}

static int x_sys_pm_resume_early(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_RESUME_EARLY, SR_KEYPOINT_RESUME_EARLY);
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}

static int x_sys_pm_resume(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_RESUME, SR_KEYPOINT_RESUME);
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}

static void x_sys_pm_complete(struct device *dev)
{
	SYS_PM_MARK(SYS_TICK_ACPU_RESUME_FINISH, SR_KEYPOINT_RESUME_COMPLETE);

	pr_debug("%s %d\n", __func__, __LINE__);
}

static const struct dev_pm_ops x_sys_pm_ops = {
	.prepare = x_sys_pm_prepare,
	.complete = x_sys_pm_complete,
	.suspend = x_sys_pm_suspend,
	.suspend_late = x_sys_pm_suspend_late,
	.resume = x_sys_pm_resume,
	.resume_early = x_sys_pm_resume_early,
	.suspend_noirq = x_sys_pm_suspend_noirq,
	.resume_noirq = x_sys_pm_resume_noirq,
};

static int x_sys_pm_probe(struct platform_device *dev)
{
	int ret;

	ret = x_sys_pm_create_sysfs(dev);
	if (ret) {
		pr_err("create sysfs failed:%d\n", ret);
		return ret;
	}

	ret = x_sys_pm_ioremap();
	if (ret) {
		pr_err("ioremap failed:%d\n", ret);
		goto err_sysfs;
	}

	ret = of_property_read_u32(dev->dev.of_node, "xring,wksrc", &g_wksrc_stat);
	if (ret)
		goto err_unmap;

	ret = register_pm_notifier(&g_pm_notifier_nb);
	if (ret)
		goto err_unmap;

	return 0;
err_unmap:
	x_sys_pm_iounmap();
err_sysfs:
	kobject_put(g_sleep_stats_kobject);
	return ret;
}

static int x_sys_pm_remove(struct platform_device *dev)
{
	unregister_pm_notifier(&g_pm_notifier_nb);

	x_sys_pm_iounmap();

	kobject_put(g_sleep_stats_kobject);

	return 0;
}

static const struct of_device_id x_sys_pm_match_table[] = {
	{ .compatible = "xring,x_sys_pm" },
	{},
};

static struct platform_driver x_sys_pm_driver = {
	.driver = {
		.name = "x_sys_pm",
		.of_match_table = x_sys_pm_match_table,
		.pm = &x_sys_pm_ops,
	},
	.probe = x_sys_pm_probe,
	.remove = x_sys_pm_remove,
};

static int __init x_sys_pm_init(void)
{
	int ret;

	ret = platform_driver_register(&x_sys_pm_driver);
	if (ret) {
		x_sys_pm_print(NULL, "platdrv register failed, %d\n", ret);
		return ret;
	}

	pr_info("init success\n");
	return 0;
}

static void __exit x_sys_pm_exit(void)
{
	platform_driver_unregister(&x_sys_pm_driver);
}

module_init(x_sys_pm_init);
module_exit(x_sys_pm_exit);

MODULE_AUTHOR("Xu Peng <xupeng9@xiaomi.com>");
MODULE_DESCRIPTION("xring sys pm driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: xr_doze");
