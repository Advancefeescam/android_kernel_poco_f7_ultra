/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include "mi_thermal_notify.h"

/* TODO: why use srcu notifier? */
SRCU_NOTIFIER_HEAD_STATIC(mi_thermal_notifier);

int mi_thermal_reg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&mi_thermal_notifier, nb);
}
EXPORT_SYMBOL_GPL(mi_thermal_reg_notifier);

int mi_thermal_unreg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&mi_thermal_notifier, nb);
}
EXPORT_SYMBOL_GPL(mi_thermal_unreg_notifier);

int mi_thermal_notifier_call_chain(unsigned long event, int val)
{
	return srcu_notifier_call_chain(&mi_thermal_notifier, event, &val);
}
EXPORT_SYMBOL_GPL(mi_thermal_notifier_call_chain);

MODULE_AUTHOR("pengyuzhe@huaqin.com");
MODULE_DESCRIPTION("mi thermal notify");
MODULE_LICENSE("GPL v2");