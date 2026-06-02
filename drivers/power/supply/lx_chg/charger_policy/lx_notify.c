/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 LiXun Technology(Shanghai) Co., Ltd.
 */

#include <linux/module.h>
#include "lx_notify.h"

// notifer intertface to other module
static BLOCKING_NOTIFIER_HEAD(lxchg_notifier);
static BLOCKING_NOTIFIER_HEAD(xmdfs_notifier);

/* lxchg(common) -> other module */
int lxchg_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lxchg_notifier, nb);
}
EXPORT_SYMBOL_GPL(lxchg_notifier_register);

int lxchg_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lxchg_notifier, nb);
}
EXPORT_SYMBOL_GPL(lxchg_notifier_unregister);

int lxchg_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&lxchg_notifier, val, v);
}
EXPORT_SYMBOL_GPL(lxchg_notifier_call_chain);

void lxchg_psy_updata(enum lxchg_notifier_events evt)
{
	lxchg_notifier_call_chain(evt, NULL);
}
EXPORT_SYMBOL(lxchg_psy_updata);

/* xiaomi dfs -> other module */
int xmdfs_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&xmdfs_notifier, nb);
}
EXPORT_SYMBOL_GPL(xmdfs_notifier_register);

int xmdfs_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&xmdfs_notifier, nb);
}
EXPORT_SYMBOL_GPL(xmdfs_notifier_unregister);

int xmdfs_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&xmdfs_notifier, val, v);
}
EXPORT_SYMBOL_GPL(xmdfs_notifier_call_chain);


MODULE_AUTHOR("Yanjuh.Guo@luxshare.com");
MODULE_DESCRIPTION("lx notify common driver");
MODULE_LICENSE("GPL v2");
