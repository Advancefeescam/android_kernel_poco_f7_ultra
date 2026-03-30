/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __MI_THERMAL_NOTIFY_H__
#define __MI_THERMAL_NOTIFY_H__

#include <linux/notifier.h>

enum thermal_notifier_events {
	EVENT_BOARDTEMP_CHANGE = 0,
	/* TODO: more mi_thermal_message events if needed */
};

int mi_thermal_reg_notifier(struct notifier_block *nb);
int mi_thermal_unreg_notifier(struct notifier_block *nb);
int mi_thermal_notifier_call_chain(unsigned long event, int val);

#endif /* __MI_THERMAL_NOTIFY_H__ */
