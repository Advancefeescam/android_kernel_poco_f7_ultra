// SPDX-License-Identifier: GPL-2.0
/*
 *battmngr_notifier.c
 *
 * pogo driver
 *
 * Copyright (c) 2024-2024 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <mca/battmngr/battmngr_notifier.h>

static BLOCKING_NOTIFIER_HEAD(battmngr_notifier);

struct battmngr_notify *g_battmngr_noti;
EXPORT_SYMBOL(g_battmngr_noti);

int battmngr_notifier_register(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&battmngr_notifier, n);
}
EXPORT_SYMBOL(battmngr_notifier_register);

int battmngr_notifier_unregister(struct notifier_block *n)
{
	return blocking_notifier_chain_unregister(&battmngr_notifier, n);
}
EXPORT_SYMBOL(battmngr_notifier_unregister);

int battmngr_notifier_call_chain(unsigned long event,
		struct battmngr_notify *data)
{
	return blocking_notifier_call_chain(&battmngr_notifier, event, (void *)data);
}
EXPORT_SYMBOL(battmngr_notifier_call_chain);

MODULE_DESCRIPTION("Battery Manager notifier");
MODULE_AUTHOR("getian@xiaomi.com");
MODULE_LICENSE("GPL v2");

