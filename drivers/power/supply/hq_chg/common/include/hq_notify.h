/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */
#ifndef __HQ_NOTIFY_H__
#define __HQ_NOTIFY_H__

#include <linux/notifier.h>

enum charger_notifier_events {
	CHARGER_EVENT_DEFAULT = 0,
	CHARGER_EVENT_BC12_DONE = 1,
	CHARGER_EVENT_HVDCP_DONE = 2,
	CHARGER_EVENT_CHG_TIMEOUT = 3,
	CHARGER_EVENT_CID_DETECT = 4,
};

struct cm_notify {
	bool cid_state;
};

int hq_charger_notifier_register(struct notifier_block *nb);
int hq_charger_notifier_unregister(struct notifier_block *nb);
int hq_charger_notifier_call_chain(unsigned long val, void *v);
int hq_chargermanager_notifier_register(struct notifier_block *nb);
int hq_chargermanager_notifier_unregister(struct notifier_block *nb);
int hq_chargermanager_notifier_call_chain(unsigned long val, void *v);
int hq_chargerpump_notifier_register(struct notifier_block *nb);
int hq_chargerpump_notifier_unregister(struct notifier_block *nb);
int hq_chargerpump_notifier_call_chain(unsigned long val, void *v);
int hq_fg_notifier_register(struct notifier_block *nb);
int hq_fg_notifier_unregister(struct notifier_block *nb);
int hq_fg_notifier_call_chain(unsigned long val, void *v);

#endif /* __HQ_NOTIFY_H__ */
