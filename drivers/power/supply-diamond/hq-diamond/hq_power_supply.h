/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */
#ifndef __HQ_POWER_SUPPLY_H
#define __HQ_POWER_SUPPLY_H
enum power_supply_quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,		/* Charging Power <= 10W */
	QUICK_CHARGE_FAST,			/* 10W < Charging Power <= 20W */
	QUICK_CHARGE_FLASH,			/* 20W < Charging Power <= 30W */
	QUICK_CHARGE_TURBE,			/* 30W < Charging Power <= 50W */
	QUICK_CHARGE_SUPER,			/* Charging Power > 50W */
	QUICK_CHARGE_MAX,
};
enum power_supply_tx_adapter_type {
	ADAPTER_NONE = 0,			/* Nothing Attached */
	ADAPTER_SDP,				/* Standard Downstream Port */
	ADAPTER_CDP,				/* Charging Downstream Port */
	ADAPTER_DCP,				/* Dedicated Charging Port */
	ADAPTER_OCP,				/* Other Charging Port */
	ADAPTER_QC2,				/* Qualcomm Charge 2.0 */
	ADAPTER_QC3,				/* Qualcomm Charge 3.0 */
	ADAPTER_PD,				/* Power Delivery Port */
	ADAPTER_AUTH_FAILED,			/* Authenticated Failed Adapter */
	ADAPTER_PRIVATE_QC3,			/* Qualcomm Charge 3.0 with Private Protocol */
	ADAPTER_PRIVATE_PD,			/* PD Adapter with Private Protocol */
	ADAPTER_CAR_POWER,			/* Wireless Car Charger */
	ADAPTER_PRIVATE_PD_40W,			/* 40W PD Adapter with Private Protocol */
	ADAPTER_VOICE_BOX,			/* Voice Box which Support Wireless Charger */
	ADAPTER_PRIVATE_PD_50W,			/* 50W PD Adapter with Private Protocol */
};
enum bq2589x_charge_type {
	POWER_SUPPLY_TYPE_USB_HVDCP = 13,
	POWER_SUPPLY_TYPE_USB_HVDCP3,
	POWER_SUPPLY_TYPE_USB_HVDCP3P5,
	POWER_SUPPLY_TYPE_USB_FLOAT,
};
#endif
