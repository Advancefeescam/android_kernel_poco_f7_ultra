// SPDX-License-Identifier: GPL-2.0
/*
 *xm_battmngr_init.c
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
#ifndef __XM_BATTMNGR_INIT_H
#define __XM_BATTMNGR_INIT_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/debugfs.h>
#include <linux/device.h>

#include <mca/battmngr/battmngr_voter.h>
#include <mca/battmngr/battmngr_notifier.h>
#include <mca/battmngr/xm_charger_core.h>
//#include "../../extSOC/inc/virtual_fg.h"

struct xm_battmngr {
	struct device *dev;
	struct xm_charger charger;
	//struct vir_bq_fg_chip fg;
	struct notifier_block battmngr_nb;
	struct battmngr_notify battmngr_noti;
};

#endif /* __XM_BATTMNGR_INIT_H */

