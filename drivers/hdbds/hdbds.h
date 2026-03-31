
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023-2028 Xiaomi Technologies Co., Ltd.
 */
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/version.h>
#include <linux/iio/consumer.h>

#define CLASS_NAME			"huada_gnss"
#define DTS_HDBDS_RN_VDD_STR	"hdbds_rn_vdd"
#define DTS_HDBDS_PRRSTX_STR	"hdbds_prrstx"
#define DTS_HDBDS_BOOT_STR	"hdbds_boot"
#define DTS_HDBDS_L1_STR	"hdbds_l1"
#define DTS_HDBDS_L5_STR	"hdbds_l5"
#define DTS_HDSMS_TCXO_STR	"hdsms_tcxo"
#define DTS_HDSMS_VDD_LDO_STR	"hdsms_vdd_ldo"
#define DTS_HDSMS_RD_PRRSTX_STR	"hdsms_rd_prrstx"
#define DTS_HDSMS_RD_BOOT_STR	"hdsms_rd_boot"
#define DTS_HDSMS_RD_INIT_STR	"hdsms_rd_int"

enum gpio_values {
	GPIO_INPUT = 0x0,
	GPIO_OUTPUT = 0x1,
	GPIO_HIGH = 0x2,
	GPIO_OUTPUT_HIGH = 0x3,
	GPIO_IRQ = 0x4,
};

struct platform_gpio {
	unsigned int hdbds_rn_vdd;
	unsigned int hdbds_prrstx;
	unsigned int hdbds_boot;
	unsigned int hdbds_l1;
	unsigned int hdbds_l5;
	unsigned int hdsms_tcxo;
	unsigned int hdsms_vdd_ldo;
	unsigned int hdsms_rd_prrstx;
	unsigned int hdsms_rd_boot;
	unsigned int hdsms_rd_int;
	struct iio_channel *hdsms_vdet_chan;
};

struct platform_configs {
	struct platform_gpio gpio;
};

struct hd_bds_sysfs {
	struct device *dev;
	struct class hd_bds_class;
	struct platform_gpio gpio;
	struct platform_configs configs;

	struct pinctrl              *hdbds_pinctrl;
	struct pinctrl_state        *hd_boot;
	struct pinctrl_state        *hdsms_tcxo_disable;
	struct pinctrl_state        *hdsms_tcxo_enable;
	struct pinctrl_state        *hdsms_vdd_ldo_default;
	struct pinctrl_state        *hdsms_vdd_ldo_disable;
	struct pinctrl_state        *hdsms_vdd_ldo_en;
	struct pinctrl_state        *hdsms_rd_prrstx_default;
	struct pinctrl_state        *hdsms_rd_reset_suspend;
	struct pinctrl_state        *hdsms_rd_boot_default;
	struct pinctrl_state        *hdsms_rd_boot_en;
	struct pinctrl_state        *hdsms_rd_int_active;
	struct pinctrl_state        *hdsms_rd_int_suspend;
	struct pinctrl_state        *hdbds_rn_vdd_default;
	struct pinctrl_state        *hdbds_rn_vdd_disable;
	struct pinctrl_state        *hdbds_rn_vdd_enable;
	struct pinctrl_state        *hdbds_prrstx_default;
	struct pinctrl_state        *hdbds_prrstx_reset_suspend;
	struct pinctrl_state        *hdbds_boot_default;
	struct pinctrl_state        *hdbds_boot_suspend;
	struct pinctrl_state        *hdbds_boot_enable;
	struct pinctrl_state        *hdbds_l1_default;
	struct pinctrl_state        *hdbds_l1_disable;
	struct pinctrl_state        *hdbds_l1_enable;
	struct pinctrl_state        *hdbds_l5_default;
	struct pinctrl_state        *hdbds_l5_disable;
	struct pinctrl_state        *hdbds_l5_enable;
};