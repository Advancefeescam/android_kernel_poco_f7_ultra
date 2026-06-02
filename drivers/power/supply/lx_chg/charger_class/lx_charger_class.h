/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2023 LiXun Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_LIXUN_CHARGER_CLASS_H__
#define __LINUX_LIXUN_CHARGER_CLASS_H__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <generated/autoconf.h>
#include <linux/device/class.h>
#include <linux/reboot.h>
#include <linux/hrtimer.h>


enum vendor_supply_id {
	FIRST_SUPPLY = 1,
	SECOND_SUPPLY,
	THIRD_SUPPLY,
};

enum chg_adc_channel {
	CHG_ADC_VBUS      = 0,
	CHG_ADC_VSYS      = 1,
	CHG_ADC_VBAT      = 2,
	CHG_ADC_VAC       = 3,
	CHG_ADC_IBUS      = 4,
	CHG_ADC_IBAT      = 5,
	CHG_ADC_TSBUS     = 6,
	CHG_ADC_TSBAT     = 7,
	CHG_ADC_TDIE      = 8,
	CHG_ADC_MAX       = 9,
};

enum chg_type {
	VBUS_TYPE_NONE = 0,
	/* bc1.2 */
	VBUS_TYPE_SDP,
	VBUS_TYPE_CDP,
	VBUS_TYPE_DCP,
	VBUS_TYPE_FLOAT,
	VBUS_TYPE_NON_STAND,
	/* qc */
	VBUS_TYPE_HVDCP,
	VBUS_TYPE_HVDCP_3,
	VBUS_TYPE_HVDCP_3P5,
	/* pd */
	VBUS_TYPE_PD,
	VBUS_TYPE_PD_PPS,
};

struct charger_dev {
	struct device dev;
	char *name;
	void *private;
	struct charger_ops *ops;
	enum chg_type real_type;
	int m_pd_active;
	int vendor_supply_id;
};

struct charger_ops {
	int (*get_adc)(struct charger_dev *charger, enum chg_adc_channel channel, uint32_t *value);
	int (*get_vbus_type)(struct charger_dev *charger, enum chg_type *type);
	int (*get_online)(struct charger_dev *charger, bool *en);
	int (*is_charge_done)(struct charger_dev *charger, bool *en);
	int (*get_hiz_status)(struct charger_dev *charger, bool *en);
	int (*get_ichg)(struct charger_dev *charger, uint32_t *ma);
	int (*get_input_volt_lmt)(struct charger_dev *charger, uint32_t *mv);
	int (*get_input_curr_lmt)(struct charger_dev *charger, uint32_t *ma);
	int (*get_chg_status)(struct charger_dev *charger, uint32_t *chg_state,
							uint32_t *chg_status);
	int (*get_chip_chg_status)(struct charger_dev *charger, uint32_t *chg_status);
	int (*get_otg_status)(struct charger_dev *charger, bool *en);
	int (*get_term_curr)(struct charger_dev *charger, uint32_t *ma);
	int (*get_term_volt)(struct charger_dev *charger, uint32_t *mv);
	int (*set_hiz)(struct charger_dev *charger, bool en);
	int (*set_input_curr_lmt)(struct charger_dev *charger, int ma);
	int (*disable_power_path)(struct charger_dev *charger, bool ma);
	int (*set_input_volt_lmt)(struct charger_dev *charger, int mv);
	int (*set_ichg)(struct charger_dev *charger, int ma);
	int (*enable_chg)(struct charger_dev *charger, bool en);
	int (*get_chg_enabled)(struct charger_dev *charger, bool *en);
	int (*set_otg)(struct charger_dev *charger, bool en);
	int (*set_otg_curr)(struct charger_dev *charger, int ma);
	int (*set_otg_volt)(struct charger_dev *charger, int mv);
	int (*set_term)(struct charger_dev *charger, bool en);
	int (*set_term_curr)(struct charger_dev *charger, int ma);
	int (*set_term_volt)(struct charger_dev *charger, int mv);
	int (*set_qc_term_vbus)(struct charger_dev *charger);
	int (*adc_enable)(struct charger_dev *charger, bool en);
	int (*set_prechg_volt)(struct charger_dev *charger, int mv);
	int (*set_prechg_curr)(struct charger_dev *charger, int ma);
	int (*force_dpdm)(struct charger_dev *charger);
	int (*reset)(struct charger_dev *charger);
	int (*request_dpdm)(struct charger_dev *charger, bool en);
	int (*set_wd_timeout)(struct charger_dev *charger, int ms);
	int (*kick_wd)(struct charger_dev *charger);
	int (*set_shipmode)(struct charger_dev *charger, bool en);
	int (*set_rechg_vol)(struct charger_dev *charger, int mv);
	int (*qc_identify)(struct charger_dev *charger, int qc3_enable);
	int (*qc3_vbus_puls)(struct charger_dev *charger, bool state, int count);
	int (*qc2_vbus_mode)(struct charger_dev *charger, int mv);
	int (*mtbf_regs_show)(struct charger_dev *charger);
};


int charger_unregister(struct charger_dev *charger);
struct charger_dev *charger_find_dev_by_name(const char *name);
struct charger_dev *charger_register(char *name, struct device *parent,
							struct charger_ops *ops, void *private);
void *charger_get_private(struct charger_dev *charger);
int charger_get_adc(struct charger_dev *charger, enum chg_adc_channel channel, uint32_t *value);
int charger_get_vbus_type(struct charger_dev *charger, enum chg_type *type);
int charger_get_online(struct charger_dev *charger, bool *en);
int charger_is_charge_done(struct charger_dev *charger, bool *en);
int charger_get_hiz_status(struct charger_dev *charger, bool *en);
int charger_get_ichg(struct charger_dev *charger, uint32_t *ma);
int charger_get_input_volt_lmt(struct charger_dev *charger, uint32_t *mv);
int charger_get_input_curr_lmt(struct charger_dev *charger, uint32_t *ma);
int charger_get_chg_status(struct charger_dev *charger, uint32_t *chg_state, uint32_t *chg_status);
int charger_get_chip_chg_status(struct charger_dev *charger, uint32_t *chg_status);
int charger_get_otg_status(struct charger_dev *charger, bool *en);
int charger_get_term_curr(struct charger_dev *charger, uint32_t *ma);
int charger_get_term_volt(struct charger_dev *charger, uint32_t *mv);
int charger_set_hiz(struct charger_dev *charger, bool en);
int charger_set_input_curr_lmt(struct charger_dev *charger, int ma);
int charger_disable_power_path(struct charger_dev *charger, bool ma);
int charger_set_input_volt_lmt(struct charger_dev *charger, int mv);
int charger_set_ichg(struct charger_dev *charger, int ma);
int charger_enable_chg(struct charger_dev *charger, bool en);
int charger_get_chg_enabled(struct charger_dev *charger, bool *en);
int charger_set_otg(struct charger_dev *charger, bool en);
int charger_set_otg_curr(struct charger_dev *charger, int ma);
int charger_set_otg_volt(struct charger_dev *charger, int mv);
int charger_set_term(struct charger_dev *charger, bool en);
int charger_set_term_curr(struct charger_dev *charger, int ma);
int charger_set_term_volt(struct charger_dev *charger, int mv);
int charger_set_qc_term_vbus(struct charger_dev *charger);
int charger_set_rechg_volt(struct charger_dev *charger, int mv);
int charger_adc_enable(struct charger_dev *charger, bool en);
int charger_set_prechg_volt(struct charger_dev *charger, int mv);
int charger_set_prechg_curr(struct charger_dev *charger, int ma);
int charger_force_dpdm(struct charger_dev *charger);
int charger_reset(struct charger_dev *charger);
int charger_set_wd_timeout(struct charger_dev *charger, int ms);
int charger_kick_wd(struct charger_dev *charger);
int charger_request_qc20(struct charger_dev *charger, int mv);
int charger_qc_identify(struct charger_dev *charger, int qc3_enable);
int charger_qc3_vbus_puls(struct charger_dev *charger, bool state, int count);
int charger_qc2_vbus_mode(struct charger_dev *charger, int mv);
int charger_set_shipmode(struct charger_dev *charger, bool en);
int charger_mtbf_regs_show(struct charger_dev *charger);
#endif /* __LINUX_LIXUN_CHARGER_CLASS_H__ */
