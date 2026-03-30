// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched/prio.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>
#include <linux/printk.h>

#include "subpmic_sy_charger.h"
#include "sy6976.h"
#include "hq_charger_class.h"
#include "hq_led_class.h"
#include "soft_bc12.h"
#include "tcpci_typec.h"
#include "tcpci.h"
#include "hq_notify.h"

#define SUBPMIC_CHARGER_VERSION         "1.1.1"

static const u32 adc_step[] = {
	[SY6976_ADC_IBUS]        = 2500,
	[SY6976_ADC_VBUS]        = 3750,
	[SY6976_ADC_VAC]         = 5000,
	[SY6976_ADC_VBATSNS]     = 1250,
	[SY6976_ADC_VBAT]        = 1250,
	[SY6976_ADC_IBAT]        = 1220,
	[SY6976_ADC_VSYS]        = 1250,
	[SY6976_ADC_TSBUS]       = 9766,
	[SY6976_ADC_TSBAT]       = 9766,
	[SY6976_ADC_TDIE]        = 5,
	[SY6976_ADC_BATID]       = 156,
};

static int subpmic_chg_field_read(struct subpmic_chg_device *sy, enum subpmic_chg_fields field_id)
{
	int ret;
	int val;

	ret = regmap_field_read(sy->rmap_fields[field_id], &val);
	if (ret < 0) {
		dev_err(sy->dev, "i2c field read failed\n");
		return ret;
	}

	return val;
}

static int subpmic_chg_field_write(struct subpmic_chg_device *sy, enum subpmic_chg_fields field_id, u8 val)
{
	int ret = 0;

	ret = regmap_field_write(sy->rmap_fields[field_id], val);
	if (ret < 0) {
		dev_err(sy->dev, "i2c field write failed\n");
	}

	return ret;
}

static int subpmic_chg_bulk_read(struct subpmic_chg_device *sy, u8 reg,
				u8 *val, size_t count)
{
	int ret = 0;

	ret = regmap_bulk_read(sy->rmap, reg, val, count);
	if (ret < 0) {
		dev_err(sy->dev, "i2c bulk read failed\n");
	}

	return ret;
}

static int subpmic_chg_bulk_write(struct subpmic_chg_device *sy, u8 reg,
				u8 *val, size_t count)
{
	int ret = 0;

	ret = regmap_bulk_write(sy->rmap, reg, val, count);
	if (ret < 0) {
		dev_err(sy->dev, "i2c bulk write failed\n");
	}

	return ret;
}

static int subpmic_chg_write_byte(struct subpmic_chg_device *sy, u8 reg, u8 val)
{
	u8 temp = val;

	return subpmic_chg_bulk_write(sy, reg, &temp, 1);
}

static int subpmic_chg_read_byte(struct subpmic_chg_device *sy, u8 reg, u8 *val)
{
	return subpmic_chg_bulk_read(sy, reg, val, 1);
}

static const struct reg_field subpmic_chg_reg_fields[] = {
	[F_VAC_OVP]         = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP, 4, 7),
	[F_VBUS_OVP]        = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP, 0, 2),
	[F_TSBUS_FLT]       = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP, 0, 7),
	[F_TSBAT_FLT]       = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP, 0, 7),
	[F_ACDRV_MANUAL_PRE] = REG_FIELD(SUBPMIC_REG_HK_CTRL, 7, 7),
	[F_ACDRV_EN]        = REG_FIELD(SUBPMIC_REG_HK_CTRL, 5, 5),
	[F_ACDRV_MANUAL_EN] = REG_FIELD(SUBPMIC_REG_HK_CTRL, 4, 4),
	[F_WD_TIME_RST]     = REG_FIELD(SUBPMIC_REG_HK_CTRL, 3, 3),
	[F_WD_TIMER]        = REG_FIELD(SUBPMIC_REG_HK_CTRL, 0, 2),
	[F_REG_RST]         = REG_FIELD(SUBPMIC_REG_HK_CTRL + 1, 7, 7),
	[F_VBUS_PD]         = REG_FIELD(SUBPMIC_REG_HK_CTRL + 1, 6, 6),
	[F_VAC_PD]          = REG_FIELD(SUBPMIC_REG_HK_CTRL + 1, 5, 5),
	[F_CID_EN]          = REG_FIELD(SUBPMIC_REG_HK_GEN_STATE, 5, 5),
	//Cid
	[F_CID_SEL]         = REG_FIELD(SUBPMIC_REG_HK_GEN_STATE, 6, 6),
	[F_ADC_EN]          = REG_FIELD(SUBPMIC_REG_HK_ADC_CTRL, 7, 7),
	[F_ADC_FREEZE]      = REG_FIELD(SUBPMIC_REG_HK_ADC_CTRL, 5, 5),
	[F_BATID_ADC_EN]    = REG_FIELD(SUBPMIC_REG_HK_ADC_CTRL, 3, 3),
	[F_EDL_ACTIVE_LEVEL] = REG_FIELD(0x27, 1, 1),
	/* Charger */
	/* REG30 */
	[F_VSYS_MIN]        = REG_FIELD(SUBPMIC_REG_VSYS_MIN, 0, 2),
	/* REG31 */
	[F_BATSNS_EN]       = REG_FIELD(SUBPMIC_REG_VBAT, 7, 7),
	[F_VBAT]            = REG_FIELD(SUBPMIC_REG_VBAT, 0, 6),
	/* REG32 */
	[F_ICHG_CC]         = REG_FIELD(SUBPMIC_REG_ICHG_CC, 0, 6),
	/* REG33 */
	[F_VINDPM_VBAT]     = REG_FIELD(SUBPMIC_REG_VINDPM, 5, 6),
	[F_VINDPM_DIS]      = REG_FIELD(SUBPMIC_REG_VINDPM, 4, 4),
	[F_VINDPM]          = REG_FIELD(SUBPMIC_REG_VINDPM, 0, 3),
	/* REG34 */
	[F_IINDPM_DIS]      = REG_FIELD(SUBPMIC_REG_IINDPM, 7, 7),
	[F_IINDPM]          = REG_FIELD(SUBPMIC_REG_IINDPM, 0, 5),
	/* REG35 */
	[F_FORCE_ICO]       = REG_FIELD(SUBPMIC_REG_ICO_CTRL, 7, 7),
	[F_ICO_EN]          = REG_FIELD(SUBPMIC_REG_ICO_CTRL, 6, 6),
	[F_IINDPM_ICO]      = REG_FIELD(SUBPMIC_REG_ICO_CTRL, 0, 5),
	/* REG36 */
	[F_VBAT_PRECHG]     = REG_FIELD(SUBPMIC_REG_PRECHARGE_CTRL, 6, 7),
	[F_IPRECHG]         = REG_FIELD(SUBPMIC_REG_PRECHARGE_CTRL, 0, 3),
	/* REG37 */
	[F_TERM_EN]         = REG_FIELD(SUBPMIC_REG_TERMINATION_CTRL, 7, 7),
	[F_ITERM]           = REG_FIELD(SUBPMIC_REG_TERMINATION_CTRL, 0, 4),
	/* REG38 */
	[F_RECHG_DIS]       = REG_FIELD(SUBPMIC_REG_RECHARGE_CTRL, 4, 4),
	[F_RECHG_DG]        = REG_FIELD(SUBPMIC_REG_RECHARGE_CTRL, 2, 3),
	[F_VRECHG]          = REG_FIELD(SUBPMIC_REG_RECHARGE_CTRL, 0, 1),
	/* REG39 */
	[F_VBOOST]          = REG_FIELD(SUBPMIC_REG_VBOOST_CTRL, 3, 7),
	[F_IBOOST]          = REG_FIELD(SUBPMIC_REG_VBOOST_CTRL, 0, 2),
	/* REG3A */
	[F_CONV_OCP_DIS]    = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 4, 4),
	[F_TSBAT_JEITA_DIS] = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 3, 3),
	[F_IBAT_OCP_DIS]    = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 2, 2),
	[F_VPMID_OVP_OTG_DIS] = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 1, 1),
	[F_VBAT_OVP_BUCK_DIS] = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 0, 0),
	/* REG3B */
	[F_T_BATFET_RST]    = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 5, 5),
	[F_BATFET_RST_EN]   = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 3, 3),
	[F_BATFET_DLY]      = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 2, 2),
	[F_BATFET_DIS]      = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 1, 1),
	/* REG3C */
	[F_HIZ_EN]          = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 7, 7),
	[F_PERFORMANCE_EN]  = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 6, 6),
	[F_DIS_BUCKCHG_PATH] = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 5, 5),
	[F_DIS_SLEEP_FOR_OTG] = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 4, 4),
	[F_QB_EN]           = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 2, 2),
	[F_BOOST_EN]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 1, 1),
	[F_CHG_EN]          = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 0, 0),
	/* REG3D */
	[F_VBAT_TRACK]      = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 5, 5),
	[F_IBATOCP]         = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 3, 4),
	[F_VSYSOVP_DIS]     = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 2, 2),
	[F_VSYSOVP_TH]      = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 0, 1),
	/* REG3E */
	[F_BAT_COMP]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 5, 7),
	[F_VCLAMP]          = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 2, 4),
	[F_JEITA_ISET_COOL] = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 1, 1),
	[F_JEITA_VSET_WARM] = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 0, 0),
	/* REG3F */
	[F_TMR2X_EN]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 7, 7),
	[F_CHG_TIMER_EN]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 6, 6),
	[F_CHG_TIMER]       = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 4, 5),
	[F_TDIE_REG_DIS]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 3, 3),
	[F_TDIE_REG]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 1, 2),
	[F_PFM_DIS]         = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 0, 0),
	/* REG40 */
	[F_BAT_COMP_OFF]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 6, 7),
	[F_VBAT_LOW_OTG]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 5, 5),
	[F_BOOST_FREQ]      = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 3, 4),
	[F_BUCK_FREQ]       = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 1, 2),
	[F_BAT_LOAD_EN]     = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 0, 0),
	/* REG49 */
	[F_IINDPM_MASK]     = REG_FIELD(SUBPMIC_REG_CHG_INT_MASK + 2, 1, 1),
	/* REG56 */
	[F_JEITA_COOL_TEMP] = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 6, 7),
	[F_JEITA_WARM_TEMP] = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 4, 5),
	[F_BOOST_NTC_HOT_TEMP]  = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 2, 3),
	[F_BOOST_NTC_COLD_TEMP] = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 0, 0),
	/* REG5D */
	[F_TESTM_EN]            = REG_FIELD(SUBPMIC_REG_Internal, 0, 0),
	/* REG5E */
	[F_KEY_EN_OWN]          = REG_FIELD(SUBPMIC_REG_Internal + 1, 0, 0),
	/*LED*/
	/*REG80*/
	[F_TRPT]         = REG_FIELD(SUBPMIC_REG_LED_CTRL, 0, 2),
	[F_FL_TX_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL, 3, 3),
	[F_TLED2_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL, 4, 4),
	[F_TLED1_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL, 5, 5),
	[F_FLED2_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL, 6, 6),
	[F_FLED1_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL, 7, 7),
	[F_FLED1_BR]     = REG_FIELD(SUBPMIC_REG_FLED1_BR_CTR, 0, 6),
	[F_FLED2_BR]     = REG_FIELD(SUBPMIC_REG_FLED2_BR_CTR, 0, 6),
	[F_FTIMEOUT]     = REG_FIELD(SUBPMIC_REG_FLED_TIMER, 0, 3),
	[F_FRPT]         = REG_FIELD(SUBPMIC_REG_FLED_TIMER, 4, 6),
	[F_FTIMEOUT_EN]  = REG_FIELD(SUBPMIC_REG_FLED_TIMER, 7, 7),
	[F_TLED1_BR]     = REG_FIELD(SUBPMIC_REG_TLED1_BR_CTR, 0, 6),
	[F_TLED2_BR]     = REG_FIELD(SUBPMIC_REG_TLED2_BR_CTR, 0, 6),
	[F_PMID_FLED_OVP_DEG] = REG_FIELD(SUBPMIC_REG_LED_PRO, 0, 1),
	[F_VBAT_MIN_FLED]     = REG_FIELD(SUBPMIC_REG_LED_PRO, 2, 4),
	[F_VBAT_MIN_FLED_DEG] = REG_FIELD(SUBPMIC_REG_LED_PRO, 5, 6),
	[F_LED_POWER]         = REG_FIELD(SUBPMIC_REG_LED_PRO, 7, 7),
	/* DPDM */
	/* REG90 */
	[F_FORCE_INDET]     = REG_FIELD(SUBPMIC_REG_DPDM_EN, 7, 7),
	[F_AUTO_INDET_EN]   = REG_FIELD(SUBPMIC_REG_DPDM_EN, 6, 6),
	[F_HVDCP_EN]        = REG_FIELD(SUBPMIC_REG_DPDM_EN, 5, 5),
	[F_QC_EN]           = REG_FIELD(SUBPMIC_REG_DPDM_EN, 0, 0),
	/* REG91 */
	[F_DP_DRIV]         = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 5, 7),
	[F_DM_DRIV]         = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 2, 4),
	[F_BC1_2_VDAT_REF_SET] = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 1, 1),
	[F_BC1_2_DP_DM_SINK_CAP] = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 0, 0),
	/* REG92 */
	[F_QC2_V_MAX]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 0, 1),
	[F_QC3_PULS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 2, 2),
	[F_QC3_MINUS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 3, 3),
	[F_QC3_5_16_PLUS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 4, 4),
	[F_QC3_5_16_MINUS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 5, 5),
	[F_QC3_5_3_SEQ]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 6, 6),
	[F_QC3_5_2_SEQ]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 7, 7),
	/* REG94 */
	[F_VBUS_STAT]       = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 5, 7),
	[F_BC1_2_DONE]      = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 2, 2),
	[F_DP_OVP]          = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 1, 1),
	[F_DM_OVP]          = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 0, 0),
	/* REG9D */
	[F_DM_500K_PD_EN]   = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 7, 7),
	[F_DP_500K_PD_EN]   = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 6, 6),
	[F_DM_SINK_EN]      = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 5, 5),
	[F_DP_SINK_EN]      = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 4, 4),
	[F_DP_SRC_10UA]     = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 3, 3),
	/* REG08 */
	[F_TSBUS_PROTECT_DIS]   = REG_FIELD(SUBPMIC_REG_REG08_CTRL, 0, 0),
};

__maybe_unused
static int subpmic_chg_dump_regs(struct subpmic_chg_device *sy, char *buf)
{
	int ret = 0, reg = 0;
	u8 val = 0;
	int count = 0;

	for (reg = SUBPMIC_REG_DEVICE_ID; reg < SUBPMIC_REG_MAX; reg++) {
		ret = subpmic_chg_read_byte(sy, reg, &val);
		if (ret < 0)
			return ret;
		if (buf)
			count += snprintf(buf + count, PAGE_SIZE - count,
					"[0x%x] -> 0x%x\n", reg, val);
		dev_info(sy->dev, "[0x%x] -> 0x%x\n", reg, val);
	}

	return count;
}

/**
 * DPDM Module
 */
__maybe_unused
static int subpmic_chg_set_dp_drive(struct subpmic_chg_device *sy, enum DPDM_DRIVE state)
{
	switch (state) {
	case DPDM_500K_DOWN:
		subpmic_chg_field_write(sy, F_DP_DRIV, DPDM_HIZ);
		subpmic_chg_field_write(sy, F_DP_500K_PD_EN, true);
		break;
	default:
		subpmic_chg_field_write(sy, F_DP_DRIV, state);
		break;
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_set_dm_drive(struct subpmic_chg_device *sy, enum DPDM_DRIVE state)
{
	subpmic_chg_write_byte(sy, SUBPMIC_REG_DPDM_INTERNAL + 1, 0x00);

	switch (state) {
	case DPDM_500K_DOWN:
		subpmic_chg_field_write(sy, F_DM_DRIV, 0);
		subpmic_chg_field_write(sy, F_DM_500K_PD_EN, 1);
		break;
	case DPDM_V1_8:
		subpmic_chg_write_byte(sy, SUBPMIC_REG_DPDM_INTERNAL + 2, 0x2a);
		subpmic_chg_write_byte(sy, SUBPMIC_REG_DPDM_INTERNAL + 1, 0x0a);
		break;
	default:
		subpmic_chg_field_write(sy, F_DM_DRIV, state);
		break;
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_set_dp_cap(struct subpmic_chg_device *sy, enum DPDM_CAP cap)
{
	switch (cap) {
	case DPDM_CAP_SNK_50UA:
		subpmic_chg_field_write(sy, F_DP_SINK_EN, true);
		subpmic_chg_field_write(sy, F_BC1_2_DP_DM_SINK_CAP, false);
		break;
	case DPDM_CAP_SNK_100UA:
		subpmic_chg_field_write(sy, F_DP_SINK_EN, true);
		subpmic_chg_field_write(sy, F_BC1_2_DP_DM_SINK_CAP, true);
		break;
	case DPDM_CAP_SRC_10UA:
		subpmic_chg_field_write(sy, F_DP_SINK_EN, false);
		subpmic_chg_field_write(sy, F_DP_SRC_10UA, true);
		break;
	case DPDM_CAP_SRC_250UA:
		subpmic_chg_field_write(sy, F_DP_SINK_EN, false);
		subpmic_chg_field_write(sy, F_DP_SRC_10UA, false);
		break;
	default:
		subpmic_chg_field_write(sy, F_DP_SINK_EN, false);
		break;
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_set_dm_cap(struct subpmic_chg_device *sy, enum DPDM_CAP cap)
{
	switch (cap) {
	case DPDM_CAP_SNK_50UA:
		subpmic_chg_field_write(sy, F_DM_SINK_EN, true);
		subpmic_chg_field_write(sy, F_BC1_2_DP_DM_SINK_CAP, false);
		break;
	case DPDM_CAP_SNK_100UA:
		subpmic_chg_field_write(sy, F_DM_SINK_EN, true);
		subpmic_chg_field_write(sy, F_BC1_2_DP_DM_SINK_CAP, true);
		break;
	default:
		subpmic_chg_field_write(sy, F_DM_SINK_EN, false);
		break;
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_auto_dpdm_enable(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_AUTO_INDET_EN, en);
}
/***************************************************************************/
/**
 * Soft BC1.2 ops
 */
__maybe_unused
static int subpmic_chg_bc12_init(struct soft_bc12 *bc)
{
	struct subpmic_chg_device *sy = bc->private;
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_AUTO_INDET_EN);
	if (ret > 0) {
		subpmic_chg_field_write(sy, F_AUTO_INDET_EN, false);
		msleep(500);
	}
	subpmic_chg_set_dm_drive(sy, DPDM_HIZ);
	subpmic_chg_set_dp_drive(sy, DPDM_HIZ);

	return 0;
}

__maybe_unused
static int subpmic_chg_bc12_deinit(struct soft_bc12 *bc)
{
	struct subpmic_chg_device *sy = bc->private;
	// set DPDM out to 3.3v , vooc/ufcs need
	return subpmic_chg_write_byte(sy, SUBPMIC_REG_DPDM_INTERNAL + 2, 0x6a);
}

__maybe_unused
static int subpmic_chg_update_bc12_state(struct soft_bc12 *bc)
{
	struct subpmic_chg_device *sy = bc->private;
	int ret;
	uint8_t dp, dm;

	// must set REG_DPDM_INTERNAL -> 0xa0
	ret = subpmic_chg_write_byte(sy, SUBPMIC_REG_DPDM_INTERNAL, 0xa0);
	if (ret < 0)
		return ret;

	udelay(1000);

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_DP_STAT, &dp);
	switch (dp) {
	case 0x00:
		bc->dp_state = DPDM_V0_TO_V0_325;
		break;
	case 0x01:
		bc->dp_state = DPDM_V0_325_TO_V1;
		break;
	case 0x03:
		bc->dp_state = DPDM_V1_TO_V1_35;
		break;
	case 0x07:
		bc->dp_state = DPDM_V1_35_TO_V22;
		break;
	case 0x0F:
		bc->dp_state = DPDM_V2_2_TO_V3;
		break;
	case 0x1F:
		bc->dp_state = DPDM_V3;
		break;
	default:
		break;
	}

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_DM_STAT, &dm);
	switch (dm) {
	case 0x00:
		bc->dm_state = DPDM_V0_TO_V0_325;
		break;
	case 0x01:
		bc->dm_state = DPDM_V0_325_TO_V1;
		break;
	case 0x03:
		bc->dm_state = DPDM_V1_TO_V1_35;
		break;
	case 0x07:
		bc->dm_state = DPDM_V1_35_TO_V22;
		break;
	case 0x0F:
		bc->dm_state = DPDM_V2_2_TO_V3;
		break;
	case 0x1F:
		bc->dm_state = DPDM_V3;
		break;
	default:
		break;
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_set_bc12_state(struct soft_bc12 *bc, enum DPDM_SET_STATE state)
{
	struct subpmic_chg_device *sy = bc->private;

	switch (state) {
	case DPDM_HIZ_STATE:
		subpmic_chg_set_dm_drive(sy, DPDM_HIZ);
		subpmic_chg_set_dp_drive(sy, DPDM_HIZ);
		subpmic_chg_set_dp_cap(sy, DPDM_CAP_DISABLE);
		subpmic_chg_set_dm_cap(sy, DPDM_CAP_DISABLE);
		break;
	case DPDM_FLOATING_STATE:
		subpmic_chg_set_dp_drive(sy, DPDM_V2_7);
		subpmic_chg_set_dm_drive(sy, DPDM_20K_DOWN);
		subpmic_chg_set_dp_cap(sy, DPDM_CAP_SRC_10UA);
		break;
	case DPDM_PRIMARY_STATE:
		subpmic_chg_set_dp_drive(sy, DPDM_V0_6);
		subpmic_chg_set_dp_cap(sy, DPDM_CAP_SRC_250UA);
		subpmic_chg_set_dm_drive(sy, DPDM_HIZ);
		subpmic_chg_set_dm_cap(sy, DPDM_CAP_SNK_100UA);
		break;
	case DPDM_SECONDARY_STATE:
		subpmic_chg_set_dp_cap(sy, DPDM_CAP_SNK_100UA);
		subpmic_chg_set_dm_drive(sy, DPDM_V1_8);
		break;
	case DPDM_HVDCP_STATE:
		subpmic_chg_set_dm_cap(sy, DPDM_CAP_SNK_100UA);
		subpmic_chg_set_dm_drive(sy, DPDM_HIZ);
		subpmic_chg_set_dp_drive(sy, DPDM_V0_6);
		break;
	default:
		break;
	}

	return 0;
}

static int subpmic_chg_get_online(struct subpmic_chg_device *sy, bool *en);

__maybe_unused
static int subpmic_chg_bc12_get_vbus_online(struct soft_bc12 *bc)
{
	struct subpmic_chg_device *sy = bc->private;
	bool en = 0;
	int ret = subpmic_chg_get_online(sy, &en);

	if (ret < 0)
		return 0;

	return en;
}
/***************************************************************************/
/**
 * BUCK Module
 */
__maybe_unused
static int subpmic_chg_set_ico_enable(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_ICO_EN, en);
}

__maybe_unused
static int __subpmic_chg_get_chg_status(struct subpmic_chg_device *sy)
{
	int ret;
	u8 val;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_CHG_INT_STAT + 1, &val);
	if (ret < 0)
		return ret;
	val >>= 5;
	val &= 0x7;

	return val;
}

__maybe_unused
static int subpmic_chg_mask_buck_irq(struct subpmic_chg_device *sy, int irq_channel)
{
	int ret;
	u8 val[3] = {0};

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_INT_MASK, val, 3);
	if (ret < 0) {
		return ret;
	}
	val[0] |= irq_channel;
	val[1] |= irq_channel >> 8;
	val[2] |= irq_channel >> 16;

	return subpmic_chg_bulk_write(sy, SUBPMIC_REG_CHG_INT_MASK, val, 3);
}

__maybe_unused
int subpmic_chg_unmask_buck_irq(struct subpmic_chg_device *sy, int irq_channel)
{
	int ret;
	u8 val[3] = {0};

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_INT_MASK, val, 3);
	if (ret < 0) {
		return ret;
	}
	val[0] &= ~(irq_channel);
	val[1] &= ~(irq_channel >> 8);
	val[2] &= ~(irq_channel >> 16);

	return subpmic_chg_bulk_write(sy, SUBPMIC_REG_CHG_INT_MASK, val, 3);
}
//EXPORT_SYMBOL(subpmic_chg_unmask_buck_irq);

__maybe_unused
static int subpmic_chg_set_sys_volt(struct subpmic_chg_device *sy, int mv)
{
	int i = 0;

	if (mv < vsys_min[0])
		mv = vsys_min[0];

	if (mv > vsys_min[ARRAY_SIZE(vsys_min) - 1])
		mv = vsys_min[ARRAY_SIZE(vsys_min) - 1];

	for (i = 0; i < ARRAY_SIZE(vsys_min); i++) {
		if (mv <= vsys_min[i])
		break;
	}

	return subpmic_chg_field_write(sy, F_VSYS_MIN, i);
}
/***************************************************************************/
/**
 * Hourse Keeping Module
 */
__maybe_unused
int subpmic_chg_set_adc_func(struct subpmic_chg_device *sy, int channel, bool en)
{
	int ret;
	u8 val[2] = {0};

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_HK_ADC_CTRL, val, 2);
	if (ret < 0) {
		return ret;
	}
	val[0] = en ? val[0] | (channel >> 8) : val[0] & ~(channel >> 8);
	val[1] = en ? val[1] | channel : val[1] & ~channel;

	return subpmic_chg_bulk_write(sy, SUBPMIC_REG_HK_ADC_CTRL, val, 2);
}
//EXPORT_SYMBOL(subpmic_chg_set_adc_func);

__maybe_unused
static int subpmic_chg_get_adc(struct subpmic_chg_device *sy, enum sy6976_adc_ch id)
{
	u32 reg = SUBPMIC_REG_HK_IBUS_ADC + id * 2;
	u8 val[2] = {0};
	u32 ret = 0;

	ret = subpmic_chg_field_read(sy, F_ADC_EN);
	if (ret <= 0)
		return ret;
	if (id == SY6976_ADC_BATID) {
		subpmic_chg_field_write(sy, F_BATID_ADC_EN, true);
		mdelay(100);
	}
	mutex_lock(&sy->adc_read_lock);
	subpmic_chg_field_write(sy, F_ADC_FREEZE, true);//SY reg0F bit5 is null
	ret = subpmic_chg_bulk_read(sy, reg, val, sizeof(val));
	if (ret < 0) {
		return ret;
	}
	ret = val[1] + (val[0] << 8);
	if (id == SY6976_ADC_TDIE) {
		ret = ret / 2;
	} else if (id == SY6976_ADC_BATID) {
		ret = 156 * ret / 1000;
	} else if (id == SY6976_ADC_TSBUS || id == SY6976_ADC_TSBAT) {
		// get percentage
		ret = ret * adc_step[id] / 100000;
		// get res
		ret = 100 * ret / (100 - ret) * 1000;
	} else {
		ret *= adc_step[id];
	}

	subpmic_chg_field_write(sy, F_ADC_FREEZE, false);//SY reg0F bit5 is null
	mutex_unlock(&sy->adc_read_lock);

	return ret;
}

__maybe_unused
static int subpmic_chg_chip_reset(struct subpmic_chg_device *sy)
{
	return subpmic_chg_field_write(sy, F_REG_RST, true);
}

__maybe_unused
static int subpmic_chg_set_acdrv(struct subpmic_chg_device *sy, bool en)
{
	int ret;
	int cnt = 0;
	int from_ic;

	from_ic = subpmic_chg_field_read(sy, F_ACDRV_EN);
	do {
		if (cnt++ > 3) {
			dev_err(sy->dev, "[ERR]set acdrv failed\n");
			return -EIO;
		}

		ret = subpmic_chg_field_write(sy, F_ACDRV_EN, en);
		if (ret < 0)
			continue;

		from_ic = subpmic_chg_field_read(sy, F_ACDRV_EN);
		if (from_ic < 0)
			continue;
	} while (en != from_ic);
	dev_info(sy->dev, "acdrv set %d success\n", (int)en);

	return 0;
}

__maybe_unused
static int subpmic_chg_set_vac_ovp(struct subpmic_chg_device *sy, int mv)
{
	if (mv <= 6500)
		mv = 0;
	else if (mv <= 11000)
		mv = 1;
	else if (mv <= 12000)
		mv = 2;
	else if (mv <= 13000)
		mv = 3;
	else if (mv <= 14000)
		mv = 4;
	else if (mv <= 15000)
		mv = 5;
	else if (mv <= 16000)
		mv = 6;
	else
		mv = 7;

	return subpmic_chg_field_write(sy, F_VAC_OVP, mv);
}

__maybe_unused
static int subpmic_chg_set_vbus_ovp(struct subpmic_chg_device *sy, int mv)
{
	if (mv <= 6000)
		mv = 0;
	else if (mv <= 10000)
		mv = 1;
	else if (mv <= 12000)
		mv = 2;
	else
		mv = 3;

	return subpmic_chg_field_write(sy, F_VBUS_OVP, mv);
}

__maybe_unused
static int subpmic_chg_mask_hk_irq(struct subpmic_chg_device *sy, int irq_channel)
{
	u8 val = 0;

	subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_INT_MASK, &val);
	val |= irq_channel;

	return subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_INT_MASK, val);
}

__maybe_unused
int subpmic_chg_unmask_hk_irq(struct subpmic_chg_device *sy, int irq_channel)
{
	u8 val = 0;

	subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_INT_MASK, &val);
	val &= ~irq_channel;

	return subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_INT_MASK, val);
}
//EXPORT_SYMBOL(subpmic_chg_unmask_hk_irq);

/***************************************************************************/

/**
 * QC Module
 */
enum QC_STATUS {
	QC_NONE = 0,
	QC2_MODE,
	QC3_MODE,
	QC3_5_18W_MODE,
	QC3_5_27W_MODE,
	QC3_5_45W_MODE,
};

enum QC3_MODE {
	NO_SUPPORT = 0,
	SUPPORT_QC3,
	SUPPORT_QC3_5,
};

static int subpmic_chg_cid_enable(struct subpmic_chg_device *sy, bool en)
{
	int ret;
	u8 reg01, reg0B_mask;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_GEN_STATE, &reg01);
	if (ret < 0)
		return ret;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_INT_MASK, &reg0B_mask);
	if (ret < 0)
		return ret;

	if (en) {
		ret = subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_GEN_STATE, reg01|0x60);
		if (ret < 0)
			return ret;

		ret = subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_INT_MASK, reg0B_mask&(~0x80));
		if (ret < 0)
			return ret;

		dev_info(sy->dev, "cid enabled\n");
	} else {
		ret = subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_GEN_STATE, reg01&(~0x60));
		if (ret < 0)
			return ret;

		ret = subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_INT_MASK, reg0B_mask|0x80);
		if (ret < 0)
			return ret;

		dev_info(sy->dev, "cid disabled\n");
	}

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_GEN_STATE, &reg01);
	if (ret < 0)
		return ret;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_INT_MASK, &reg0B_mask);
	if (ret < 0)
		return ret;

	dev_info(sy->dev, "CID Reg01 = 0x%x, CID mask(0B) = 0x%x\n", reg01, reg0B_mask);

	return ret;
}

static int subpmic_chg_hvdcp_detect(struct subpmic_chg_device *sy)//[Silergy01]
{
	int ret = 0;
	u8 val1;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_DPDM_INT_FLAG, &val1);
	if (ret < 0) {
		return ret;
	}

	val1 = (val1 & 0xe0) >> 5;
	pr_info("hvdcp read reg94 usb type=%d", (val1));
	if (val1 == 4)//HVDCP
		return 0;
	else
		return -1;
}


static int subpmic_chg_get_online(struct subpmic_chg_device *sy, bool *en);

static void qc_detect_workfunc(struct work_struct *work)
{
	struct subpmic_chg_device *sy = container_of(work,
			struct subpmic_chg_device, qc_detect_work);
	int vbus = 0;
	int cnt = 0;
	int qc_retry_count = 0;
	bool vbus_online = false;
	u8 read_val = 0;

	dev_info(sy->dev, "%s start\n", __func__);
	sy->qc_result = QC_NONE;
	cnt = 0;
	subpmic_chg_field_write(sy, F_QC2_V_MAX, 3);
	subpmic_chg_field_write(sy, F_QC_EN, true);
	//force BC1.2 with HVDCP_EN=1
	pr_info("Second force BC");//[Silergy01]
	subpmic_chg_field_write(sy, F_HVDCP_EN, true);//[Silergy01]
	subpmic_chg_field_write(sy, F_FORCE_INDET, true);//[Silergy01]

	do {
		msleep(200);
		subpmic_chg_get_online(sy, &vbus_online);
		if (!vbus_online)
			break;
		if (subpmic_chg_hvdcp_detect(sy) == 0)
			break;
	} while (cnt++ < 12);//[Silergy01] cahnge run time to 12 from 10

	if (!vbus_online) {
		subpmic_chg_field_write(sy, F_QC_EN, false);
		subpmic_chg_field_write(sy, F_HVDCP_EN, false);//[Silergy01]
		goto out;
	}

	if (subpmic_chg_hvdcp_detect(sy) < 0) {
		subpmic_chg_field_write(sy, F_QC_EN, false);
		subpmic_chg_field_write(sy, F_HVDCP_EN, false);//[Silergy01]
		goto out;
	}

	if (sy->qc3_support == NO_SUPPORT) {
		do {
			subpmic_chg_field_write(sy, F_QC2_V_MAX, 0);
			msleep(130);
			//sy6976
			subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_ADC_CTRL, &read_val);//uu677
			pr_info("sy6976 reg0f1 = %d", read_val);//uu679

			read_val |= BIT(6);//uu677 write 1
			subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_ADC_CTRL, read_val);//uu677

			read_val &= ~BIT(6);//uu683 write 0
			subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_ADC_CTRL, read_val);//uu683

			read_val |= BIT(6);//uu683 write 1
			subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_ADC_CTRL, read_val);//uu683

			msleep(30);//uu677//uu668 10->20

			vbus = subpmic_chg_get_adc(sy, SY6976_ADC_VBUS) / 1000;

			subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_ADC_CTRL, &read_val);//uu683
			pr_info("sy6976 reg0f2= %d", read_val);//uu679

			read_val &= ~BIT(6);//uu682 write 0
			subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_ADC_CTRL, read_val);//uu682
			pr_info("sy6976 reg0f3 = %d, try Qc9V ADC: %d\n", read_val, vbus);//uu682

			if (vbus > 7200) {
				sy->qc_result = QC2_MODE;
				goto out;
			}
		} while (qc_retry_count++ < 3);
	} else {
		subpmic_chg_field_write(sy, F_QC2_V_MAX, 2);
		// qc3 detect need delay 40-60ms
		msleep(120);
		// send 16 sequence
		subpmic_chg_field_write(sy, F_QC3_5_16_PLUS, true);
		msleep(130);
		vbus = subpmic_chg_get_adc(sy, SY6976_ADC_VBUS) / 1000;
		if (vbus < 7500) {
			sy->qc_result = QC2_MODE;
			goto out;
		}
		msleep(50);

		// send 16 sequence
		subpmic_chg_field_write(sy, F_QC3_5_16_MINUS, true);
		msleep(280);
		vbus = subpmic_chg_get_adc(sy, SY6976_ADC_VBUS) / 1000;
		if (vbus < 5500) {
			sy->qc_result = QC3_MODE;
			goto out;
		}
		// send 3 sequence
		if (sy->qc3_support == SUPPORT_QC3_5)
			subpmic_chg_field_write(sy, F_QC3_5_3_SEQ, true);

		msleep(100);

		vbus = subpmic_chg_get_adc(sy, SY6976_ADC_VBUS) / 1000;
		if (vbus < 7500) {
			sy->qc_result = QC3_5_18W_MODE;
		} else if (vbus < 8500) {
			sy->qc_result = QC3_5_27W_MODE;
		} else if (vbus < 9500) {
			sy->qc_result = QC3_5_45W_MODE;
		}

		// send 2 sequence
		subpmic_chg_field_write(sy, F_QC3_5_2_SEQ, true);
		msleep(60);
	}
out:
	// set qc volt to qc2 5v , clear plus state
	//subpmic_chg_write_byte(sy, SUBPMIC_REG_DPDM_QC_CTRL, 0x03);
	msleep(100);
	pr_info("qc-result:%d", sy->qc_result);//[Silergy01]
	switch (sy->qc_result) {
	case QC2_MODE:
		sy->state.vbus_type = VBUS_TYPE_HVDCP;
		break;
	case QC3_MODE:
		sy->state.vbus_type = VBUS_TYPE_HVDCP_3;
		break;
	case QC3_5_18W_MODE:
	case QC3_5_27W_MODE:
	case QC3_5_45W_MODE:
		sy->state.vbus_type = VBUS_TYPE_HVDCP_3P5;
		break;
	default:
		sy->state.vbus_type = VBUS_TYPE_DCP;
		break;
	}

	if (sy->qc_result == QC2_MODE) {
		subpmic_chg_field_write(sy, F_QC2_V_MAX, 0);
		sy->qc_vbus = 9000;
	} else if (sy->qc_result == QC3_MODE)
		subpmic_chg_field_write(sy, F_QC2_V_MAX, 2);

	hq_charger_notifier_call_chain(CHARGER_EVENT_HVDCP_DONE, NULL);

	charger_changed(sy->sy_charger);
	dev_info(sy->dev, "%s end\n", __func__);
}

__maybe_unused
static int subpmic_chg_qc_identify(struct subpmic_chg_device *sy)
{
	mdelay(200);

	if (sy->sy_charger->m_pd_active != 0 && sy->sy_charger->m_pd_active != 10) {
		dev_info(sy->dev, "%s m_pd_active is %d\n", __func__, sy->sy_charger->m_pd_active);
		return 0;
	}

	if (work_busy(&sy->qc_detect_work)) {
		dev_err(sy->dev, "qc_detect work running\n");
		return -EBUSY;
	}

	sy->qc_vbus = 5000;
	schedule_work(&sy->qc_detect_work);

	return 0;
}

__maybe_unused
static int subpmic_chg_qc3_vbus_puls(struct subpmic_chg_device *sy, bool state, int count)
{
	int ret  = 0;
	int i = 0;

	for (i = 0; i < count; i++) {
		if (state)
			ret |= subpmic_chg_field_write(sy, F_QC3_PULS, true);
		else
			ret |= subpmic_chg_field_write(sy, F_QC3_MINUS, true);
		mdelay(8);
	}
	return ret;
}

__maybe_unused
static int subpmic_chg_request_vbus(struct subpmic_chg_device *sy, int mv, int step)
{
	int count = 0, i;
	int ret = 0;

	count = (mv - sy->qc_vbus) / step;
	for (i = 0; i < abs(count); i++) {
		if (count > 0)
			ret |= subpmic_chg_field_write(sy, F_QC3_PULS, true);
		else
			ret |= subpmic_chg_field_write(sy, F_QC3_MINUS, true);
		mdelay(8);
	}

	if (ret >= 0)
		sy->qc_vbus = mv;

	return ret;
}

__maybe_unused
static int subpmic_chg_get_online(struct subpmic_chg_device *sy, bool *en)
{
	*en = sy->state.online;

	return 0;
}

__maybe_unused
static int subpmic_chg_get_vbus_type(struct subpmic_chg_device *sy, enum vbus_type *vbus_type)
{
	*vbus_type = sy->state.vbus_type;

	return 0;
}

__maybe_unused
static int subpmic_chg_is_charge_done(struct subpmic_chg_device *sy, bool *en)
{
	*en = __subpmic_chg_get_chg_status(sy) == SUBPMIC_CHG_STATE_TERM;

	return 0;
}

__maybe_unused
static int subpmic_chg_set_qc_term_vbus(struct subpmic_chg_device *sy)
{
	if (sy->qc_result == QC2_MODE && (sy->qc_vbus != 5000)) {
		subpmic_chg_write_byte(sy, SUBPMIC_REG_DPDM_QC_CTRL, 0x03);
		sy->qc_vbus = 5000;
		dev_info(sy->dev, "QC charge done, drop qc_vbus = %d\n", sy->qc_vbus);
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_get_hiz_status(struct subpmic_chg_device *sy, bool *en)
{
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_HIZ_EN);
	if (ret < 0)
		return ret;

	*en = ret;

	return 0;
}

__maybe_unused
static int subpmic_chg_get_input_volt_lmt(struct subpmic_chg_device *sy, uint32_t *mv)
{
	int ret;

	ret = subpmic_chg_field_read(sy, F_PERFORMANCE_EN);
	if (ret < 0)
		return ret;

	if (ret) {
		*mv = 0;
		return 0;
	}

	ret = subpmic_chg_field_read(sy, F_VINDPM);
	if (ret < 0)
		return ret;

	if (ret <= (ARRAY_SIZE(vindpm) - 1)) {
		*mv = vindpm[ret];
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_get_input_curr_lmt(struct subpmic_chg_device *sy, uint32_t *ma)
{
	int ret;

	ret = subpmic_chg_field_read(sy, F_DIS_BUCKCHG_PATH);
	if (ret < 0)
		return ret;

	if (ret) {
		*ma = 0;
		return 0;
	}

	ret = subpmic_chg_field_read(sy, F_IINDPM);
	if (ret < 0)
		return ret;

	*ma = SUBPMIC_BUCK_IINDPM_STEP * ret + SUBPMIC_BUCK_IINDPM_OFFSET;

	return 0;
}

__maybe_unused
static int subpmic_chg_get_chg_status(struct subpmic_chg_device *sy,
					uint32_t *chg_state, uint32_t *chg_status)
{
	int state = 0;
	*chg_state = 0;
	*chg_status = 0;

	if (!sy->state.online) {
		*chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	}

	state = __subpmic_chg_get_chg_status(sy);
	switch (state) {
	case SUBPMIC_CHG_STATE_CC:
	case SUBPMIC_CHG_STATE_CV:
	case SUBPMIC_CHG_STATE_NO_CHG:
		*chg_state = POWER_SUPPLY_CHARGE_TYPE_FAST;
		*chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case SUBPMIC_CHG_STATE_PRECHG:
	case SUBPMIC_CHG_STATE_TRICK:
		*chg_state = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		*chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case SUBPMIC_CHG_STATE_TERM:
		*chg_status = POWER_SUPPLY_STATUS_FULL;
		subpmic_chg_set_qc_term_vbus(sy);
		break;
	default:
		break;
	}

	return 0;
}

__maybe_unused
static int subpmic_chg_get_chip_chg_status(struct subpmic_chg_device *sy,
					uint32_t *chg_status)
{
	int state = 0;

	state = __subpmic_chg_get_chg_status(sy);
	if (state < 0) {
		dev_err(sy->dev, "failed to get charge status, ret = %d\n", state);
		return state;
	}

	*chg_status = state;

	return 0;
}

__maybe_unused
static int subpmic_chg_get_otg_status(struct subpmic_chg_device *sy, bool *en)
{
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_BOOST_EN);
	if (ret < 0)
		return ret;
	*en = ret;

	return 0;
}

__maybe_unused
static int subpmic_chg_get_term_curr(struct subpmic_chg_device *sy, uint32_t *ma)
{
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_ITERM);
	if (ret < 0)
		return ret;
	*ma = ret * SUBPMIC_BUCK_ITERM_STEP + SUBPMIC_BUCK_ITERM_OFFSET;

	return ret;
}

__maybe_unused
static int subpmic_chg_get_term_volt(struct subpmic_chg_device *sy, uint32_t *mv)
{
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_VBAT);
	if (ret < 0)
		return ret;

	*mv = ret * SUBPMIC_BUCK_VBAT_STEP + SUBPMIC_BUCK_VBAT_OFFSET;

	return 0;
}

__maybe_unused
static int subpmic_chg_set_hiz(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_HIZ_EN, en);
}

__maybe_unused
static int subpmic_chg_set_input_curr_lmt(struct subpmic_chg_device *sy, int ma)
{
	int ret = 0;

	dev_info(sy->dev, "%s  %d", __func__, ma);
	if (ma < 100) {
		return subpmic_chg_field_write(sy, F_DIS_BUCKCHG_PATH, true);
	}

	ret = subpmic_chg_field_write(sy, F_DIS_BUCKCHG_PATH, false);
	if (ret < 0)
		return ret;

	if (ma < SUBPMIC_BUCK_IINDPM_MIN)
		ma = SUBPMIC_BUCK_IINDPM_MIN;

	if (ma > SUBPMIC_BUCK_IINDPM_MAX)
		ma = SUBPMIC_BUCK_IINDPM_MAX;

	ma = (ma - SUBPMIC_BUCK_IINDPM_OFFSET) / SUBPMIC_BUCK_IINDPM_STEP;

	return subpmic_chg_field_write(sy, F_IINDPM, ma);
}

static int subpmic_chg_disable_power_path(struct subpmic_chg_device *sy, bool ma)
{
	int ret = 0;
	uint8_t val = 0;

	ret = subpmic_chg_read_byte(sy, F_DIS_BUCKCHG_PATH, &val);
	if (ret < 0) {
		dev_err(sy->dev, "read byte error\n");
		return ret;
	}
	dev_info(sy->dev, "sy6976 common read F_DIS_BUCKCHG_PATH =%d, enable =%d",
			(val & 0x20), ma);

	return subpmic_chg_field_write(sy, F_DIS_BUCKCHG_PATH, ma);
}

__maybe_unused
static int subpmic_chg_set_input_volt_lmt(struct subpmic_chg_device *sy, int mv)
{
	int i = 0, ret;

	if (mv < vindpm[0])
		mv = vindpm[0];

	if (mv > vindpm[ARRAY_SIZE(vindpm) - 1])
		mv = vindpm[ARRAY_SIZE(vindpm) - 1];

	for (i = 0; i < ARRAY_SIZE(vindpm); i++) {
		if (mv <= vindpm[i])
			break;
	}
	ret = subpmic_chg_field_write(sy, F_VINDPM, i);
	if (ret < 0)
		return ret;

	return 0;
}

__maybe_unused
static int subpmic_chg_set_ichg(struct subpmic_chg_device *sy, int ma)
{
	if (ma <= SUBPMIC_BUCK_ICHG_MIN) {
		ma = SUBPMIC_BUCK_ICHG_MIN;
	} else if (ma >= SUBPMIC_BUCK_ICHG_MAX) {
		ma = SUBPMIC_BUCK_ICHG_MAX;
	}

	ma = (ma - SUBPMIC_BUCK_ICHG_OFFSET) / SUBPMIC_BUCK_ICHG_STEP;

	return subpmic_chg_field_write(sy, F_ICHG_CC, ma);
}

__maybe_unused
static int subpmic_chg_get_ichg(struct subpmic_chg_device *sy, int *ma)
{
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_ICHG_CC);
	if (ret < 0)
		return ret;

	*ma = ret * SUBPMIC_BUCK_ICHG_STEP + SUBPMIC_BUCK_ICHG_OFFSET;

	return ret;
}

__maybe_unused
static int subpmic_chg_set_chg(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_CHG_EN, en);
}

__maybe_unused
static int subpmic_chg_get_chg(struct subpmic_chg_device *sy)
{
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_CHG_EN);

	return ret;
}

__maybe_unused
static int subpmic_chg_set_otg(struct subpmic_chg_device *sy, bool en)
{
	int ret;
	int cnt = 0;
	u8 boost_state;

	ret = subpmic_chg_set_chg(sy, !en);
	if (ret < 0) {
		return ret;
	}

	do {
		boost_state = 0;

		ret = subpmic_chg_field_write(sy, F_BOOST_EN, en ? true : false);
		if (ret < 0) {
			subpmic_chg_set_chg(sy, true);
			return ret;
		}

		mdelay(30);
		ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_CHG_INT_STAT + 1, &boost_state);
		if (cnt++ > 3) {
			subpmic_chg_set_chg(sy, true);
			return -EIO;
		}
	} while (en != (!!(boost_state & BIT(4))));
	dev_info(sy->dev, "otg set success");

	return 0;
}

enum {
	SUBPMIC_NORMAL_USE_OTG = 0,
	SUBPMIC_LED_USE_OTG,
};

static int subpmic_chg_request_otg(struct subpmic_chg_device *sy, int index, bool en)
{
	int ret = 0;
	u8 val = 0;
	u8 val1 = 0;
	u8 val2 = 0;

	dev_info(sy->dev, "%s: index = %d\n", __func__, index);
	//sy6976 for debug
	if (en)
		set_bit(index, &sy->request_otg);
	else
		clear_bit(index, &sy->request_otg);

	dev_info(sy->dev, "now request_otg = %lx , val index = %d\n", sy->request_otg, index);

	if (!en && sy->request_otg) {
		return 0;
	}

	ret = subpmic_chg_set_otg(sy, en);
	if (ret < 0) {
		if (en)
			clear_bit(index, &sy->request_otg);
		else
			set_bit(index, &sy->request_otg);
		return ret;
	}

	if (index != SUBPMIC_LED_USE_OTG && en) {
		dev_info(sy->dev, "sy6976 in QB_EN true ,index:%d\n", index);
		ret = subpmic_chg_field_write(sy, F_QB_EN, true);
	} else if (index != SUBPMIC_LED_USE_OTG && !en) {
		ret = subpmic_chg_field_write(sy, F_QB_EN, false);
	}

	//uu314+  check en to change 0x07
	if (en)
		subpmic_chg_write_byte(sy, 0x07, 0x30);//uu314
	else
		subpmic_chg_write_byte(sy, 0x07, 0x00);//uu314

	ret = subpmic_chg_read_byte(sy, 0x07, &val);
	ret = subpmic_chg_read_byte(sy, 0x09, &val1);
	ret = subpmic_chg_read_byte(sy, 0x3c, &val2);
	dev_info(sy->dev, "sy6976 debug for 0x07:%d 0x09:%d 0x3c:%d\n", val, val1, val2);

	return 0;
}

__maybe_unused
static int subpmic_chg_normal_request_otg(struct subpmic_chg_device *sy, bool en)
{
	dev_err(sy->dev, "sy6976 in subpmic_chg_normal_request_otg\n");
	return subpmic_chg_request_otg(sy, SUBPMIC_NORMAL_USE_OTG, en);
}

__maybe_unused
static int subpmic_chg_set_otg_curr(struct subpmic_chg_device *sy, int ma)
{
	int i = 0;

	if (ma < boost_curr[0])
		ma = boost_curr[0];

	if (ma > boost_curr[ARRAY_SIZE(boost_curr) - 1])
		ma = boost_curr[ARRAY_SIZE(boost_curr) - 1];

	for (i = 0; i <= ARRAY_SIZE(boost_curr) - 1; i++) {
		if (ma < boost_curr[i])
			break;
	}

	return subpmic_chg_field_write(sy, F_IBOOST, i);
}

__maybe_unused
static int subpmic_chg_set_otg_volt(struct subpmic_chg_device *sy, int mv)
{
	if (mv < SUBPMIC_BUCK_OTG_VOLT_MIN)
		mv = SUBPMIC_BUCK_OTG_VOLT_MIN;

	if (mv > SUBPMIC_BUCK_OTG_VOLT_MAX)
		mv = SUBPMIC_BUCK_OTG_VOLT_MAX;

	mv = (mv - SUBPMIC_BUCK_OTG_VOLT_OFFSET) / SUBPMIC_BUCK_OTG_VOLT_STEP;

	return subpmic_chg_field_write(sy, F_VBOOST, mv);
}

__maybe_unused
static int subpmic_chg_set_term(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_TERM_EN, en);
}

__maybe_unused
static int subpmic_chg_set_term_curr(struct subpmic_chg_device *sy, int ma)
{
	if (ma < SUBPMIC_BUCK_ITERM_MIN)
		ma = SUBPMIC_BUCK_ITERM_MIN;

	if (ma > SUBPMIC_BUCK_ITERM_MAX)
		ma = SUBPMIC_BUCK_ITERM_MAX;

	ma = (ma - SUBPMIC_BUCK_ITERM_OFFSET) / SUBPMIC_BUCK_ITERM_STEP;

	return subpmic_chg_field_write(sy, F_ITERM, ma);
}

__maybe_unused
static int subpmic_chg_set_term_volt(struct subpmic_chg_device *sy, int mv)
{
	if (mv == 4530)
		mv = 4530 - SUBPMIC_BUCK_VBAT_STEP;
	if (mv < SUBPMIC_BUCK_VBAT_MIN)
		mv = SUBPMIC_BUCK_VBAT_MIN;

	if (mv > SUBPMIC_BUCK_VBAT_MAX)
		mv = SUBPMIC_BUCK_VBAT_MAX;

	mv = (mv - SUBPMIC_BUCK_VBAT_OFFSET) / SUBPMIC_BUCK_VBAT_STEP;

	return subpmic_chg_field_write(sy, F_VBAT, mv);
}

__maybe_unused
static int subpmic_chg_adc_enable(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_ADC_EN, en);
}

__maybe_unused
static int subpmic_chg_set_prechg_volt(struct subpmic_chg_device *sy, int mv)
{
	int i = 0, ret;

	if (mv < prechg_volt[0])
		mv = prechg_volt[0];

	if (mv > prechg_volt[ARRAY_SIZE(prechg_volt) - 1])
		mv = prechg_volt[ARRAY_SIZE(prechg_volt) - 1];

	for (i = 0; i < ARRAY_SIZE(prechg_volt); i++) {
		if (mv <= prechg_volt[i])
			break;
	}

	ret = subpmic_chg_field_write(sy, F_VBAT_PRECHG, i);
	if (ret < 0)
		return ret;

	return 0;
}

__maybe_unused
static int subpmic_chg_set_prechg_curr(struct subpmic_chg_device *sy, int ma)
{
	if (ma < SUBPMIC_BUCK_PRE_CURR_MIN)
		ma = SUBPMIC_BUCK_PRE_CURR_MIN;

	if (ma > SUBPMIC_BUCK_PRE_CURR_MAX)
		ma = SUBPMIC_BUCK_PRE_CURR_MAX;

	ma = (ma - SUBPMIC_BUCK_PRE_CURR_OFFSET) / SUBPMIC_BUCK_PRE_CURR_STEP;

	return subpmic_chg_field_write(sy, F_IPRECHG, ma);
}

__maybe_unused
static int subpmic_chg_set_rst(struct subpmic_chg_device *sy, bool en)
{
	int ret = 0;

	if (en) {
		ret |= subpmic_chg_field_write(sy, F_BATFET_RST_EN, true);
	} else {
		ret |= subpmic_chg_field_write(sy, F_BATFET_RST_EN, false);
	}

	return ret;
}

__maybe_unused
static int subpmic_chg_set_shipmode(struct subpmic_chg_device *sy, bool en)
{
	int ret = 0;

	if (en) {
		ret = subpmic_chg_field_write(sy, F_BATFET_DLY, false);
		ret |= subpmic_chg_field_write(sy, F_BATFET_DIS, true);
	} else {
		ret |= subpmic_chg_field_write(sy, F_BATFET_DIS, false);
	}

	return ret;
}

__maybe_unused
static int subpmic_chg_set_rechg_vol(struct subpmic_chg_device *sy, int val)
{
	int i = 0;

	if (val < rechg_volt[0])
		val = rechg_volt[0];

	if (val > rechg_volt[ARRAY_SIZE(rechg_volt)-1])
		val = rechg_volt[ARRAY_SIZE(rechg_volt)-1];

	for (i = 0; i < ARRAY_SIZE(rechg_volt); i++) {
		if (val <= rechg_volt[i])
			break;
	}

	return subpmic_chg_field_write(sy, F_VRECHG, i);
}

static void bc12_timeout_func(struct timer_list *timer)
{
	struct subpmic_chg_device *sy = container_of(timer,
			struct subpmic_chg_device, bc12_timeout);

	dev_info(sy->dev, "BC1.2 timeout\n");
	mutex_unlock(&sy->bc_detect_lock);

	charger_changed(sy->sy_charger);
}

__maybe_unused
static int bc12_timeout_start(struct subpmic_chg_device *sy)
{
	del_timer(&sy->bc12_timeout);
	sy->bc12_timeout.expires = jiffies + msecs_to_jiffies(400);
	sy->bc12_timeout.function = bc12_timeout_func;
	add_timer(&sy->bc12_timeout);

	return 0;
}

__maybe_unused
static int bc12_timeout_cancel(struct subpmic_chg_device *sy)
{
	del_timer(&sy->bc12_timeout);

	return 0;
}

__maybe_unused
static int subpmic_chg_force_dpdm(struct subpmic_chg_device *sy)
{
	if (mutex_trylock(&sy->bc_detect_lock) == 0) {
		return -EBUSY;
	}

	bc12_timeout_start(sy);

	#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	Charger_Detect_Init();
	#endif
	subpmic_chg_field_write(sy, F_HVDCP_EN, false);
	dev_info(sy->dev, "sy6976 in subpmic_chg_force_dpdm\n");

	return subpmic_chg_field_write(sy, F_FORCE_INDET, true);
}

__maybe_unused
static int subpmic_chg_request_dpdm(struct subpmic_chg_device *sy, bool en)
{
	// struct subpmic_chg_device *sy = charger_get_private(charger);
	// todo
	return 0;
}

__maybe_unused
static int subpmic_chg_set_wd_timeout(struct subpmic_chg_device *sy, int ms)
{
	int i = 0;

	if (ms < subpmic_chg_wd_time[0])
		ms = subpmic_chg_wd_time[0];

	if (ms > subpmic_chg_wd_time[ARRAY_SIZE(subpmic_chg_wd_time) - 1])
		ms = subpmic_chg_wd_time[ARRAY_SIZE(subpmic_chg_wd_time) - 1];

	for (i = 0; i < ARRAY_SIZE(subpmic_chg_wd_time); i++) {
		if (ms <= subpmic_chg_wd_time[i])
			break;
	}

	return subpmic_chg_field_write(sy, F_WD_TIMER, i);
}

__maybe_unused
static int subpmic_chg_kick_wd(struct subpmic_chg_device *sy)
{
	return subpmic_chg_field_write(sy, F_WD_TIME_RST, true);
}

__maybe_unused
static int subpmic_chg_request_qc20(struct subpmic_chg_device *sy, int mv)
{
	int val = 0;

	if (sy->qc_result != QC2_MODE) {
		return -EIO;
	}

	subpmic_chg_field_write(sy, F_QC_EN, true);
	if (mv == 5000)
		val = 3;
	else if (mv == 9000)
		val = 0;
	else if (mv == 12000)
		val = 1;

	return subpmic_chg_field_write(sy, F_QC2_V_MAX, val);
}

__maybe_unused
static int subpmic_chg_request_qc30(struct subpmic_chg_device *sy, int mv)
{
	if (sy->qc_result != QC3_MODE) {
		return -EIO;
	}

	return subpmic_chg_request_vbus(sy, mv, 200);
}

__maybe_unused
static int subpmic_chg_request_qc35(struct subpmic_chg_device *sy, int mv)
{
	if (sy->qc_result != QC3_5_18W_MODE &&
		sy->qc_result != QC3_5_27W_MODE &&
		sy->qc_result != QC3_5_45W_MODE) {
		return -EIO;
	}

	return subpmic_chg_request_vbus(sy, mv, 20);
}

__maybe_unused
static int subpmic_chg_get_vindpm_state(struct subpmic_chg_device *sy, int *vindpm_state)
{
	u8 val[3] = {0};
	int ret = 0;
	int int_flg = 0;
	int state = 0;
	int vindpm_flag;

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_INT_STAT, val, 3);
	if (ret < 0) {
		return ret;
	}
	state = val[0] + (val[1] << 8) + (val[2] << 16);

	*vindpm_state = !!(state & SUBPMIC_BUCK_STATE_VINDPM);
	dev_info(sy->dev, "chip vindpm_state = 0x%x\n", *vindpm_state);

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_INT_FLG, val, 3);
	if (ret < 0) {
		return ret;
	}
	int_flg = val[0] + (val[1] << 8) + (val[2] << 16);
	dev_info(sy->dev, "Buck INT FLG : 0x%x\n", int_flg);

	vindpm_flag = !!(int_flg & SUBPMIC_BUCK_FLAG_VINDPM);
	dev_info(sy->dev, "chip vindpm_flag = 0x%x\n", vindpm_flag);

	return 0;
}

__maybe_unused
static int subpmic_chg_set_safety_time(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_CHG_TIMER_EN, !!en);
}

__maybe_unused
static int subpmic_chg_is_safety_enable(struct subpmic_chg_device *sy, bool *en)
{
	int ret = 0;

	ret = subpmic_chg_field_read(sy, F_CHG_TIMER_EN);
	if (ret < 0)
		return ret;
	*en = !!ret;

	return 0;
}

static irqreturn_t subpmic_chg_led_alert_handler(int irq, void *data)
{
	struct subpmic_chg_device *sy = data;
	int ret = 0;
	u8 val = 0;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_LED_FLAG, &val);
	if (ret < 0) {
		return ret;
	}
	dev_info(sy->dev, "LED Flag -> %x\n", val);

	if (val & SUBPMIC_LED_FLAG_FLASH_DONE) {
		dev_info(sy->dev, "led flash done\n");
		complete(&sy->flash_end);
	}

	return IRQ_HANDLED;
}

static irqreturn_t subpmic_chg_dpdm_alert_handler(int irq, void *data)
{
	struct subpmic_chg_device *sy = data;
	int ret = 0;
	u8 val = 0, result = 0;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_DPDM_INT_FLAG, &val);
	if (ret < 0) {
		return ret;
	}
	pr_info("sy6976_dpdm_alert_handler!!In this handle reg94=%x\n", val);

	if (val & SUBPMIC_DPDM_BC12_DETECT_DONE) {
		bc12_timeout_cancel(sy);
		result = (val >> 5) & 0x7;
		sy->state.vbus_type = result;
		mutex_unlock(&sy->bc_detect_lock);
		#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
		Charger_Detect_Release();
		#endif
		charger_changed(sy->sy_charger);
	}

	return IRQ_HANDLED;
}

static int subpmic_chg_bc12_notify_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct soft_bc12 *bc = data;
	struct subpmic_chg_device *sy = bc->private;

	if (sy->use_soft_bc12) {
		dev_info(sy->dev, "BC1.2 COMPLETE : -> %ld\n", event);
		sy->state.vbus_type = event;
		charger_changed(sy->sy_charger);
	}

	return NOTIFY_OK;
}

static irqreturn_t subpmic_chg_buck_alert_handler(int irq, void *data)
{
	struct subpmic_chg_device *sy = data;
	int ret;
	u8 val[3];
	u32 flt, flt_stat, state, int_flg;

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_FLT_FLG, val, 2);
	if (ret < 0) {
		return ret;
	}

	flt = val[0] + (val[1] << 8);
	if (flt != 0) {
		dev_err(sy->dev, "Buck FAULT : 0x%x\n", flt);
	}

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_FLT_STAT, val, 2);
	if (ret < 0) {
		return ret;
	}

	flt_stat = val[0] + (val[1] << 8);
	if (flt_stat != 0) {
		dev_info(sy->dev, "Buck_flt state : 0x%x\n", flt_stat);
		if ((val[1] & 0x04) == 0x04) {
			hq_charger_notifier_call_chain(CHARGER_EVENT_CHG_TIMEOUT, NULL);
			dev_err(sy->dev, "charge timeout!\n");
		}
	}

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_INT_STAT, val, 3);
	if (ret < 0) {
		return ret;
	}

	state = val[0] + (val[1] << 8) + (val[2] << 16);
	dev_info(sy->dev, "Buck State : 0x%x\n", state);

	ret = subpmic_chg_bulk_read(sy, SUBPMIC_REG_CHG_INT_FLG, val, 3);
	if (ret < 0) {
		return ret;
	}
	int_flg = val[0] + (val[1] << 8) + (val[2] << 16);
	dev_info(sy->dev, "Buck INT FLG : 0x%x\n", int_flg);

	sy->state.boost_good = !!(state & SUBPMIC_BUCK_FLAG_BOOST_GOOD);

	sy->state.chg_state = SUBPMIC_BUCK_GET_CHG_STATE(state);

	//charger_changed(sy->sy_charger);

	return IRQ_HANDLED;
}

static irqreturn_t subpmic_chg_hk_alert_handler(int irq, void *data)
{
	struct subpmic_chg_device *sy = data;
	static int last_vbus_present;
	static int last_online;
	static int last_cid;
	int ret = 0;
	uint8_t val = 0;
	uint8_t val1 = 0;
	uint8_t val2 = 0;
	uint8_t cidval = 0;
	uint8_t cur_cid = 0;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_FLT_FLG, &val);
	if (ret < 0)
		goto out;
	if (val != 0) {
		dev_err(sy->dev, "Hourse Keeping FAULT : 0x%x\n", val);
	}

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_INT_STAT, &val2);
	if (ret < 0)
		goto out;

	if (((val2 & 0x02) >> 1) == 0 && last_vbus_present == 1) {
		//tcpm_inquire_vbus_level(struct tcpc_device *tcpc, bool from_ic);
		//发起一个work;work 内容：tcpci_alert_fake_power_status_changed
		//tcpm_inquire_true_vbus_level(sy->tcpc, true);
		dev_err(sy->dev, "buck vbus0");
		if (sy->tcpc)
			tcpci_alert_fake_power_status_changed(sy->tcpc);
	}
	dev_info(sy->dev, "Hourse Keeping State : 0x%x\n", val2);
	last_vbus_present = (val2 & 0x02) >> 1;

	/* [Silergy00]: start*/
	//[Silergy00]:onlinie juegement
	//Use Reg41 BIT0 VBUS_GOOD_STAT(BIT0)==1 to judge adapter in;
	//Not Use VACOK or VBUS_PRESENT(VACOK or VBUS_PRESENT are both 1 in normal OTG process)
	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_CHG_INT_STAT , &val);
	if (ret < 0)
		goto out;

	dev_info(sy->dev, "SUBPMIC_REG_CHG_INT_STAT : 0x%x\n", val);

	if (sy->buck_init.cid_en == 1) {
		ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_INT_FLG, &cidval);
		if (ret < 0)
			goto out;
		dev_info(sy->dev, "sy6976 SUBPMIC_REG_HK_INT_FLG: 0x%x\n", cidval);

		cur_cid = val2 >> 7;
		dev_info(sy->dev, "sy6976 cur_cid is %d,last_cid is %d\n", cur_cid, last_cid);
		if (!last_cid && cur_cid) {
			dev_info(sy->dev, "cid detect plug-in\n");
			tcpm_typec_change_role_postpone(sy->tcpc, TYPEC_ROLE_TRY_SNK, true);
		} else if (last_cid && !cur_cid) {
			dev_info(sy->dev, "cid detect plug-out\n");
			tcpm_typec_change_role_postpone(sy->tcpc, TYPEC_ROLE_SNK, true);
		}
		last_cid = cur_cid;
	}

	subpmic_chg_buck_alert_handler(0, sy);

	sy->state.online = (val & BIT(0));//[Silergy00]:
	/* [Silergy00]: end*/
	if (last_online && !sy->state.online) {
		dev_info(sy->dev, "!!! plug out\n");

		// wait qc_work end
		cancel_work_sync(&sy->qc_detect_work);
		// relese qc
		subpmic_chg_field_write(sy, F_QC_EN, false);
		mutex_unlock(&sy->bc_detect_lock);
		sy->state.vbus_type = NONE;
	}

	//sy6976 for plug in
	if (!last_online && sy->state.online) {//uu563+
		//subpmic_chg_field_write(sy, F_HVDCP_EN, false);//[Silergy01]
		subpmic_chg_field_write(sy, F_AUTO_INDET_EN, false);
		dev_info(sy->dev, "!!!buck plug in or soft reset\n");

		subpmic_chg_set_vac_ovp(sy, 14000); //uu720

		//subpmic_chg_field_write(sy, F_VBAT, 80);
		subpmic_chg_field_write(sy, F_BATSNS_EN, 1);
		subpmic_chg_field_write(sy, F_ADC_EN, 1);
		subpmic_chg_field_write(sy, F_WD_TIMER, 0);
	}

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_DPDM_INT_FLAG, &val1);
	if (ret < 0) {
		return ret;
	}

	dev_info(sy->dev, "sy6976 common read 0x94 =%d", val1);//for debug
	if ((val1 & 0x04) == 0x04) { //input_detect_done_flag==1
		dev_info(sy->dev, "sy6976 interrupt reg94 usb type=%d", ((val1&0xe0)>>5));
		sy->state.vbus_type = (((val1 & 0xe0) >> 5) & 0x7);//update vbus type
		mutex_unlock(&sy->bc_detect_lock);
		#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
		Charger_Detect_Release();
		#endif
		hq_charger_notifier_call_chain(CHARGER_EVENT_BC12_DONE, NULL);
	}

	/*
	 * WORKROUND: fix xiaomi usb power strip(XMCXB01QM) HVDCP issue
	 * SY6976 detected HVDCP vbus type upexpectedly, but unable to raise vbus to 9v.
	 * We force report DCP vbus type even detect HVDCP and will report HVDCP after vbus raised.
	 */
	// if ((sy->state.vbus_type == VBUS_TYPE_HVDCP) ||
	// 		(sy->state.vbus_type == VBUS_TYPE_HVDCP_3) ||
	// 			(sy->state.vbus_type == VBUS_TYPE_HVDCP_3P5))
	// 	sy->state.vbus_type = VBUS_TYPE_DCP;

	if (!last_online && sy->state.online) {
		sy->state.vbus_type = NONE;
	}

	last_online = sy->state.online;

	charger_changed(sy->sy_charger);

	//subpmic_chg_dump_regs(sy, buf);
out:
	return IRQ_HANDLED;
}

__maybe_unused
static int subpmic_chg_led1_flash_enable(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_FLED1_EN, en);
}

__maybe_unused
static int subpmic_chg_led2_flash_enable(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_FLED2_EN, en);
}

__maybe_unused
static int subpmic_chg_led1_torch_enable(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_TLED1_EN, en);
}

__maybe_unused
static int subpmic_chg_led2_torch_enable(struct subpmic_chg_device *sy, bool en)
{
	return subpmic_chg_field_write(sy, F_TLED2_EN, en);
}

__maybe_unused
static int subpmic_chg_set_led1_flash_curr(struct subpmic_chg_device *sy, int curr)
{
	if (curr < SUBPMIC_LED_FLASH_CURR_MIN)
		curr = SUBPMIC_LED_FLASH_CURR_MIN;

	if (curr > SUBPMIC_LED_FLASH_CURR_MAX)
		curr = SUBPMIC_LED_FLASH_CURR_MAX;

	curr = (curr * 1000 - SUBPMIC_LED_FLASH_CURR_OFFSET) / SUBPMIC_LED_FLASH_CURR_STEP;

	return subpmic_chg_field_write(sy, F_FLED1_BR, curr);
}

__maybe_unused
static int subpmic_chg_set_led2_flash_curr(struct subpmic_chg_device *sy, int curr)
{
	if (curr < SUBPMIC_LED_FLASH_CURR_MIN)
		curr = SUBPMIC_LED_FLASH_CURR_MIN;

	if (curr > SUBPMIC_LED_FLASH_CURR_MAX)
		curr = SUBPMIC_LED_FLASH_CURR_MAX;

	curr = (curr * 1000 - SUBPMIC_LED_FLASH_CURR_OFFSET) / SUBPMIC_LED_FLASH_CURR_STEP;

	return subpmic_chg_field_write(sy, F_FLED2_BR, curr);
}

__maybe_unused
static int subpmic_chg_set_led1_torch_curr(struct subpmic_chg_device *sy, int curr)
{
	if (curr < SUBPMIC_LED_TORCH_CURR_MIN)
		curr = SUBPMIC_LED_TORCH_CURR_MIN;

	if (curr > SUBPMIC_LED_TORCH_CURR_MAX)
		curr = SUBPMIC_LED_TORCH_CURR_MAX;

	curr = (curr * 1000 - SUBPMIC_LED_TORCH_CURR_OFFSET) / SUBPMIC_LED_TORCH_CURR_STEP;

	return subpmic_chg_field_write(sy, F_TLED1_BR, curr);
}

__maybe_unused
static int subpmic_chg_set_led2_torch_curr(struct subpmic_chg_device *sy, int curr)
{
	if (curr < SUBPMIC_LED_TORCH_CURR_MIN)
		curr = SUBPMIC_LED_TORCH_CURR_MIN;

	if (curr > SUBPMIC_LED_TORCH_CURR_MAX)
		curr = SUBPMIC_LED_TORCH_CURR_MAX;

	curr = (curr * 1000 - SUBPMIC_LED_TORCH_CURR_OFFSET) / SUBPMIC_LED_TORCH_CURR_STEP;

	return subpmic_chg_field_write(sy, F_TLED2_BR, curr);
}

__maybe_unused
static int subpmic_chg_set_led_flash_timer(struct subpmic_chg_device *sy, int ms)
{
	int i = 0;

	if (ms < 0) {
		return subpmic_chg_field_write(sy, F_FTIMEOUT_EN, false);
	}

	subpmic_chg_field_write(sy, F_FTIMEOUT_EN, true);

	if (ms < led_time[0])
		ms = led_time[0];

	if (ms > led_time[ARRAY_SIZE(led_time) - 1])
		ms = led_time[ARRAY_SIZE(led_time) - 1];

	for (i = 0; i < ARRAY_SIZE(led_time); i++) {
		if (ms <= led_time[i])
			break;
	}

	return subpmic_chg_field_write(sy, F_FTIMEOUT, i);
}

__maybe_unused
static int subpmic_chg_set_led_flag_mask(struct subpmic_chg_device *sy, int mask)
{
	uint8_t val = 0;
	int ret;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_LED_MASK, &val);
	if (ret < 0)
		return ret;

	val |= mask;

	return subpmic_chg_write_byte(sy, SUBPMIC_REG_LED_MASK, val);
}

__maybe_unused
static int subpmic_chg_set_led_flag_unmask(struct subpmic_chg_device *sy, int mask)
{
	uint8_t val = 0;
	int ret;

	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_LED_MASK, &val);
	if (ret < 0)
		return ret;

	val &= ~mask;

	return subpmic_chg_write_byte(sy, SUBPMIC_REG_LED_MASK, val);
}

__maybe_unused
static int subpmic_chg_set_led_vbat_min(struct subpmic_chg_device *sy, int mv)
{
	if (mv <= SUBPMIC_LED_VBAT_MIN_MIN)
		mv = SUBPMIC_LED_VBAT_MIN_MIN;

	if (mv >= SUBPMIC_LED_VBAT_MIN_MAX)
		mv = SUBPMIC_LED_VBAT_MIN_MAX;

	mv = (mv - SUBPMIC_LED_VBAT_MIN_OFFSET) / SUBPMIC_LED_VBAT_MIN_STEP;

	return subpmic_chg_field_write(sy, F_VBAT_MIN_FLED, mv);
}

__maybe_unused
static int subpmic_chg_set_led_flash_curr(struct subpmic_chg_device *sy, int index, int ma)
{
	int ret = 0;

	switch (index) {
	case LED1_FLASH:
		ret = subpmic_chg_set_led1_flash_curr(sy, ma);
		break;
	case LED2_FLASH:
		ret = subpmic_chg_set_led2_flash_curr(sy, ma);
		break;
	case LED_ALL_FLASH:
		ret = subpmic_chg_set_led1_flash_curr(sy, ma);
		ret |= subpmic_chg_set_led2_flash_curr(sy, ma);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret < 0)
		return ret;

	if (atomic_read(&sy->led_work_running) == 0) {
		atomic_set(&sy->led_work_running, 1);
		reinit_completion(&sy->flash_run);
		reinit_completion(&sy->flash_end);
		schedule_delayed_work(&sy->led_work, msecs_to_jiffies(0));
	}
	dev_info(sy->dev, "[%s] index : %d, curr : %d", __func__, index, ma);

	return 0;
}
__maybe_unused
static int subpmic_chg_set_led_flash_enable(struct subpmic_chg_device *sy, int index, bool en)
{
	subpmic_chg_set_led_flag_unmask(sy, SUBPMIC_LED_OVP_MASK);
	sy->led_state = en;
	sy->led_index = index;
	complete(&sy->flash_run);
	dev_info(sy->dev, "[%s] index : %d, en : %d", __func__, index, en);

	return 0;
}

__maybe_unused
int subpmic_chg_set_led_torch(struct subpmic_chg_device *sy, int index, int ma, bool en)
{
	int ret = 0;

	if (index == 1) {
		ret = subpmic_chg_set_led1_torch_curr(sy, ma);
		ret |= subpmic_chg_led1_torch_enable(sy, en);
	} else if (index == 2) {
		ret = subpmic_chg_set_led2_torch_curr(sy, ma);
		ret |= subpmic_chg_led2_torch_enable(sy, en);
	} else if (index == 3) {
		ret = subpmic_chg_set_led1_torch_curr(sy, ma);
		ret |= subpmic_chg_led1_torch_enable(sy, en);
		ret |= subpmic_chg_set_led2_torch_curr(sy, ma);
		ret |= subpmic_chg_led2_torch_enable(sy, en);
	}

	return ret;
}

int subpmic_chg_set_led_torch_curr(struct subpmic_chg_device *sy, int index, int ma)
{
	int ret = 0;

	if (index == 1) {
		ret = subpmic_chg_set_led1_torch_curr(sy, ma);
	} else if (index == 2) {
		ret = subpmic_chg_set_led2_torch_curr(sy, ma);
	} else if (index == 3) {
		ret = subpmic_chg_set_led1_torch_curr(sy, ma);
		ret |= subpmic_chg_set_led2_torch_curr(sy, ma);
	}

	return ret;
}

int subpmic_chg_set_led_torch_enable(struct subpmic_chg_device *sy, int index, bool en)
{
	int ret = 0;

	if (index == 1) {
		ret = subpmic_chg_led1_torch_enable(sy, en);
	} else if (index == 2) {
		ret = subpmic_chg_led2_torch_enable(sy, en);
	} else if (index == 3) {
		ret = subpmic_chg_led1_torch_enable(sy, en);
		ret |= subpmic_chg_led2_torch_enable(sy, en);
	}

	return ret;
}

__maybe_unused
static int subpmic_set_led_flash_curr(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, int ma)
{
	struct subpmic_chg_device *sy = subpmic_led_get_private(led_dev);

	return subpmic_chg_set_led_flash_curr(sy, id, ma);
}

__maybe_unused
static int subpmic_set_led_flash_timer(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, int ms)
{
	struct subpmic_chg_device *sy = subpmic_led_get_private(led_dev);

	return subpmic_chg_set_led_flash_timer(sy, ms);
}

__maybe_unused
static int subpmic_set_led_flash_enable(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, bool en)
{
	struct subpmic_chg_device *sy = subpmic_led_get_private(led_dev);

	return subpmic_chg_set_led_flash_enable(sy, id, en);
}

__maybe_unused
static int subpmic_set_led_torch_curr(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, int ma)
{
	struct subpmic_chg_device *sy = subpmic_led_get_private(led_dev);

	return subpmic_chg_set_led_torch_curr(sy, id, ma);
}

__maybe_unused
static int subpmic_set_led_torch_enable(struct subpmic_led_dev *led_dev, enum SUBPMIC_LED_ID id, bool en)
{
	struct subpmic_chg_device *sy = subpmic_led_get_private(led_dev);

	return subpmic_chg_set_led_torch_enable(sy, id, en);
}

static void subpmic_chg_led_flash_done_workfunc(struct work_struct *work)
{
	struct subpmic_chg_device *sy = container_of(work,
			struct subpmic_chg_device, led_work.work);
	int ret = 0, now_vbus = 0;
	bool in_otg = false;

	dev_err(sy->dev, "%s: start led flash done workfunc\n", __func__);

	ret = subpmic_chg_get_otg_status(sy, &in_otg);
	if (ret < 0)
		return;

	if (in_otg)
		goto en_led_flash;

	// mask irq
	disable_irq(sy->irq[IRQ_HK]);
	disable_irq(sy->irq[IRQ_BUCK]);

	// must todo
	ret = subpmic_chg_set_chg(sy, false);
	ret |= subpmic_chg_field_write(sy, F_DIS_BUCKCHG_PATH, true);
	ret |= subpmic_chg_field_write(sy, F_DIS_SLEEP_FOR_OTG, true);
	// set otg volt curr
	now_vbus = subpmic_chg_get_adc(sy, SY6976_ADC_VBUS) / 1000;
	dev_info(sy->dev, "now vbus = %d\n", now_vbus);
	if (now_vbus < 0)
		goto err_set_acdrv;

	if (now_vbus == 0)
		ret |= subpmic_chg_set_otg_volt(sy, 5000);
	else
		ret |= subpmic_chg_set_otg_volt(sy, now_vbus);

	if (ret < 0) {
		goto err_set_acdrv;
	}

en_led_flash:
	ret = subpmic_chg_request_otg(sy, 1, true);
	if (ret < 0) {
		goto err_set_otg;
	}

	// wait flash en cmd
	// todo
	if (wait_for_completion_timeout(&sy->flash_run,
				msecs_to_jiffies(60000)) < 0) {
		goto err_en_flash;
	}

	if (!sy->led_state) {
		goto err_en_flash;
	}

	// open flash led
	switch (sy->led_index) {
	case LED1_FLASH:
		ret = subpmic_chg_led1_flash_enable(sy, true);
		break;
	case LED2_FLASH:
		ret = subpmic_chg_led2_flash_enable(sy, true);
		break;
	case LED_ALL_FLASH:
		ret = subpmic_chg_led1_flash_enable(sy, true);
		ret |= subpmic_chg_led2_flash_enable(sy, true);
		break;
	}

	if (ret < 0) {
		// Must close otg after otg set success
		goto err_en_flash;
	}

	wait_for_completion_timeout(&sy->flash_end, msecs_to_jiffies(1000));

	switch (sy->led_index) {
	case LED1_FLASH:
		subpmic_chg_led1_flash_enable(sy, false);
		break;
	case LED2_FLASH:
		subpmic_chg_led2_flash_enable(sy, false);
		break;
	case LED_ALL_FLASH:
		subpmic_chg_led1_flash_enable(sy, false);
		subpmic_chg_led2_flash_enable(sy, false);
		break;
	}

err_en_flash:
	dev_err(sy->dev, "sy6976 err_en_flash\n");
	subpmic_chg_request_otg(sy, 1, false);
	if (in_otg)
		goto out;

err_set_otg:
	subpmic_chg_set_otg_volt(sy, 5000);

err_set_acdrv:
	// unmask irq
	subpmic_chg_field_write(sy, F_DIS_BUCKCHG_PATH, false);
	subpmic_chg_field_write(sy, F_DIS_SLEEP_FOR_OTG, false);
	subpmic_chg_set_led_flag_mask(sy, SUBPMIC_LED_OVP_MASK);
	mdelay(300);
	enable_irq(sy->irq[IRQ_HK]);
	enable_irq(sy->irq[IRQ_BUCK]);
	subpmic_chg_hk_alert_handler(0, sy);
	subpmic_chg_buck_alert_handler(0, sy);

out:
	atomic_set(&sy->led_work_running, 0);
	return;
}

static void subpmic_cid_det_workfunc(struct work_struct *work)
{
	struct subpmic_chg_device *sy = container_of(work,
		struct subpmic_chg_device, cid_det_work.work);
	int ret = 0;
	uint8_t cid_value = 0;

	dev_info(sy->dev, "%s\n", __func__);
	ret = subpmic_chg_read_byte(sy, SUBPMIC_REG_HK_INT_STAT, &cid_value);
	if (ret < 0)
		return;

	cid_value = (cid_value >> 7);
	if (cid_value) {
		dev_info(sy->dev, "cid detect plug-in\n");
		tcpm_typec_change_role_postpone(sy->tcpc, TYPEC_ROLE_TRY_SNK, true);
	} else {
		dev_info(sy->dev, "cid detect plug-out\n");
		tcpm_typec_change_role_postpone(sy->tcpc, TYPEC_ROLE_SNK, true);
		sy->tcpc->adapt_pid = 0;
		sy->tcpc->adapt_vid = 0;
	}
}

/***************************************************************************/
static int subpmic_chg_request_irq_thread(struct subpmic_chg_device *sy)
{
	int i = 0, ret = 0;
	const struct {
		char *name;
		irq_handler_t hdlr;
	} subpmic_chg_chg_irqs[] = {
		{"Hourse Keeping", subpmic_chg_hk_alert_handler},
		{"Buck Charger", subpmic_chg_buck_alert_handler},
		{"DPDM", subpmic_chg_dpdm_alert_handler},
		{"LED", subpmic_chg_led_alert_handler},
	};

	for (i = 0; i < ARRAY_SIZE(subpmic_chg_chg_irqs); i++) {
		ret = platform_get_irq_byname(to_platform_device(sy->dev),
					subpmic_chg_chg_irqs[i].name);
		if (ret < 0) {
			dev_err(sy->dev, "failed to get irq %s\n", subpmic_chg_chg_irqs[i].name);
			return ret;
		}

		sy->irq[i] = ret;

		dev_info(sy->dev, "%s irq = %d\n", subpmic_chg_chg_irqs[i].name, ret);
		ret = devm_request_threaded_irq(sy->dev, ret, NULL,
				subpmic_chg_chg_irqs[i].hdlr, IRQF_ONESHOT,
				dev_name(sy->dev), sy);
		if (ret < 0) {
			dev_err(sy->dev, "failed to request irq %s\n", subpmic_chg_chg_irqs[i].name);
			return ret;
		}

	}

	return 0;
}


static int subpmic_chg_led_hw_init(struct subpmic_chg_device *sy)
{
	int ret;

	// select external source
	ret = subpmic_chg_field_write(sy, F_LED_POWER, 1);
	ret |= subpmic_chg_set_led_vbat_min(sy, 2800);
	// set flash timeout disable
	ret |= subpmic_chg_set_led_flash_timer(sy, SUBPMIC_DEFAULT_FLASH_TIMEOUT);
	ret |= subpmic_chg_set_led_flag_mask(sy, SUBPMIC_LED_OVP_MASK);

	return ret;
}

static int subpmic_chg_hw_init(struct subpmic_chg_device *sy)
{
	int ret, i = 0;
	//u8 val = 0;
	const struct {
		enum subpmic_chg_fields field;
		u8 val;
	} buck_init[] = {
		{F_BATSNS_EN,       sy->buck_init.batsns_en},
		{F_VBAT,            (sy->buck_init.vbat - SUBPMIC_BUCK_VBAT_OFFSET) / SUBPMIC_BUCK_VBAT_STEP},
		{F_ICHG_CC,         (sy->buck_init.ichg - SUBPMIC_BUCK_ICHG_OFFSET) / SUBPMIC_BUCK_ICHG_STEP},
		{F_IINDPM_DIS,      sy->buck_init.iindpm_dis},
		{F_VBAT_PRECHG,     sy->buck_init.vprechg},
		{F_IPRECHG,         (sy->buck_init.iprechg - SUBPMIC_BUCK_IPRECHG_OFFSET) / SUBPMIC_BUCK_IPRECHG_STEP},
		{F_TERM_EN,         sy->buck_init.iterm_en},
		{F_ITERM,           (sy->buck_init.iterm - SUBPMIC_BUCK_ITERM_OFFSET) / SUBPMIC_BUCK_ITERM_STEP},
		{F_RECHG_DIS,       sy->buck_init.rechg_dis},
		{F_RECHG_DG,        sy->buck_init.rechg_dg},
		{F_VRECHG,          sy->buck_init.rechg_volt},
		{F_CONV_OCP_DIS,    sy->buck_init.conv_ocp_dis},
		{F_TSBAT_JEITA_DIS, 0},
		{F_IBAT_OCP_DIS,    sy->buck_init.ibat_ocp_dis},
		{F_VPMID_OVP_OTG_DIS, sy->buck_init.vpmid_ovp_otg_dis},
		{F_VBAT_OVP_BUCK_DIS, sy->buck_init.vbat_ovp_buck_dis},
		{F_IBATOCP,         sy->buck_init.ibat_ocp},
		{F_QB_EN,           false},
		{F_ACDRV_MANUAL_EN, true},
		{F_ICO_EN,          false},
		{F_EDL_ACTIVE_LEVEL, true},
		{F_ACDRV_MANUAL_PRE, true},
		{F_HVDCP_EN,        false},
		{F_TSBUS_PROTECT_DIS, true},
		{F_CHG_TIMER,       sy->buck_init.safety_timer},
		{F_TMR2X_EN,        false},
		{F_IINDPM_MASK,     true},
		{F_ADC_EN,          false},
	};

	// reset all registers without flag registers
	ret = subpmic_chg_chip_reset(sy);
	// this need watchdog, if i2c operate failed, watchdog reset
	ret |= subpmic_chg_set_chg(sy, false);
	// set buck freq = 1M , boost freq = 1M
	ret |= subpmic_chg_write_byte(sy, SUBPMIC_REG_CHG_CTRL + 4, 0x8a);
	ret |= subpmic_chg_set_chg(sy, true);

	ret |= subpmic_chg_set_wd_timeout(sy, 0);

	//ret |= subpmic_chg_enter_test_mode(sy, true);
	//ret |= subpmic_chg_read_byte(sy, 0xFC, &val);
	//val |= BIT(5);
	//ret |= subpmic_chg_write_byte(sy, 0xFC, val);
	//ret |= subpmic_chg_enter_test_mode(sy, false);

	ret |= subpmic_chg_set_vac_ovp(sy, 14000);

	ret |= subpmic_chg_set_vbus_ovp(sy, 14000);

	ret |= subpmic_chg_set_ico_enable(sy, false);

	ret |= subpmic_chg_set_otg_curr(sy, 1500);

	//ret |= subpmic_chg_set_acdrv(sy, true);

	ret |= subpmic_chg_set_rst(sy, false);

	ret |= subpmic_chg_set_sys_volt(sy, 3500);

	ret |= subpmic_chg_mask_hk_irq(sy, SUBPMIC_HK_RESET_MASK |
			SUBPMIC_HK_ADC_DONE_MASK | SUBPMIC_HK_REGN_OK_MASK |
			SUBPMIC_HK_VAC_PRESENT_MASK);

	// ret |= subpmic_chg_mask_buck_irq(sy, SUBPMIC_BUCK_ICO_MASK |
	// 		SUBPMIC_BUCK_IINDPM_MASK | SUBPMIC_BUCK_VINDPM_MASK |
	// 		SUBPMIC_BUCK_CHG_MASK | SUBPMIC_BUCK_QB_ON_MASK |
	// 		SUBPMIC_BUCK_VSYSMIN_MASK);

	ret |= subpmic_chg_mask_buck_irq(sy, SUBPMIC_BUCK_ICO_MASK |
			SUBPMIC_BUCK_CHG_MASK | SUBPMIC_BUCK_QB_ON_MASK |
			SUBPMIC_BUCK_VSYSMIN_MASK);

	ret |= subpmic_chg_auto_dpdm_enable(sy, false);

	// disable TSBUS and TSBAT
	ret |= subpmic_chg_write_byte(sy, SUBPMIC_REG_HK_ADC_CTRL + 1, 0x06);
	//ret |= subpmic_chg_field_write(sy, F_CID_SEL, true);
	//ret |= subpmic_chg_field_write(sy, F_CID_EN, true);
	ret |= subpmic_chg_field_write(sy, F_CID_SEL, 0);
	ret |= subpmic_chg_field_write(sy, F_CID_EN, 0);

	// mask qc int
	ret |= subpmic_chg_write_byte(sy, SUBPMIC_REG_QC3_INT_MASK, 0xFF);

	for (i = 0; i < ARRAY_SIZE(buck_init); i++) {
		ret |= subpmic_chg_field_write(sy,
			buck_init[i].field, buck_init[i].val);
	}

	ret |= subpmic_chg_led_hw_init(sy);

	ret |= subpmic_chg_adc_enable(sy, true);

	/* set jeita, cool -> 5, warm -> 54.5 */
	ret |= subpmic_chg_field_write(sy, F_JEITA_COOL_TEMP, 0);
	ret |= subpmic_chg_field_write(sy, F_JEITA_WARM_TEMP, 3);

	ret |= subpmic_chg_field_write(sy, F_HVDCP_EN, 0);

	if (ret < 0)
		return ret;

	if (sy->buck_init.cid_en == 1)
		subpmic_chg_cid_enable(sy, true);
	else
		subpmic_chg_cid_enable(sy, false);

	return 0;
}

static struct soft_bc12_ops bc12_ops = {
	.init = subpmic_chg_bc12_init,
	.deinit = subpmic_chg_bc12_deinit,
	.update_bc12_state = subpmic_chg_update_bc12_state,
	.set_bc12_state = subpmic_chg_set_bc12_state,
	.get_vbus_online = subpmic_chg_bc12_get_vbus_online,
};
/**
 * SOUTHCHIP CHARGER MANAGER
 */

static inline enum sy6976_adc_ch to_sy6976_adc(enum chg_adc_channel channel)
{
	enum sy6976_adc_ch ch = SY6976_ADC_INVALID;

	switch (channel) {
	case CHG_ADC_VBUS:
		ch = SY6976_ADC_VBUS;
		break;
	case CHG_ADC_VSYS:
		ch = SY6976_ADC_VSYS;
		break;
	case CHG_ADC_VBAT:
		ch = SY6976_ADC_VBATSNS;
		break;
	case CHG_ADC_VAC:
		ch = SY6976_ADC_VAC;
		break;
	case CHG_ADC_IBUS:
		ch = SY6976_ADC_IBUS;
		break;
	case CHG_ADC_IBAT:
		ch = SY6976_ADC_IBAT;
		break;
	case CHG_ADC_TSBUS:
		ch = SY6976_ADC_TSBUS;
		break;
	case CHG_ADC_TSBAT:
		ch = SY6976_ADC_TSBAT;
		break;
	case CHG_ADC_TDIE:
		ch = SY6976_ADC_TDIE;
		break;
	default:
		//hq_err("charger adc channel %d not support\n");
		ch = SY6976_ADC_INVALID;
		break;
	}

	return ch;
}

static int sy_chg_get_adc(struct charger_dev *charger,
			enum chg_adc_channel channel, uint32_t *value)
{
	int ret = 0;
	int adc_val = 0;
	struct subpmic_chg_device *sy = charger_get_private(charger);

	adc_val = subpmic_chg_get_adc(sy, to_sy6976_adc(channel));

	/* convert to charger class unified units */
	switch (channel) {
	case CHG_ADC_VBUS:
	case CHG_ADC_VSYS:
	case CHG_ADC_VBAT:
	case CHG_ADC_VAC:
	case CHG_ADC_IBUS:
	case CHG_ADC_IBAT:
		*value = adc_val / 1000;
		break;
	case CHG_ADC_TSBUS:
	case CHG_ADC_TSBAT:
	case CHG_ADC_TDIE:
		*value = adc_val;
		break;
	default:
		//hq_err("charger adc channel %d not support\n");
		ret = -1;
		break;
	}

	return ret;
}

static int sy_chg_get_online(struct charger_dev *charger, bool *online)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_online(sy, online);
}

static int sy_chg_get_vbus_type(struct charger_dev *charger, enum vbus_type *vbus_type)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_vbus_type(sy, vbus_type);
}

static int sy_chg_is_charge_done(struct charger_dev *charger, bool *done)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_is_charge_done(sy, done);
}

static int sy_chg_get_hiz_status(struct charger_dev *charger, bool *state)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_hiz_status(sy, state);
}

static int sy_chg_get_ichg(struct charger_dev *charger, uint32_t *ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_ichg(sy, ma);
}

static int sy_chg_get_input_curr_lmt(struct charger_dev *charger, uint32_t *ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_input_curr_lmt(sy, ma);
}

static int sy_chg_get_input_volt_lmt(struct charger_dev *charger, uint32_t *mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_input_volt_lmt(sy, mv);
}

static int sy_chg_get_chg_status(struct charger_dev *charger, uint32_t *state, uint32_t *status)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_chg_status(sy, state, status);
}

static int sy_chg_get_chip_chg_status(struct charger_dev *charger, uint32_t *status)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_chip_chg_status(sy, status);
}

static int sy_chg_get_otg_status(struct charger_dev *charger, bool *state)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_otg_status(sy, state);
}

static int sy_chg_get_term_curr(struct charger_dev *charger, uint32_t *ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_term_curr(sy, ma);
}

static int sy_chg_get_term_volt(struct charger_dev *charger, uint32_t *mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_term_volt(sy, mv);
}

static int sy_chg_set_hiz(struct charger_dev *charger, bool en)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_hiz(sy, en);
}

static int sy_chg_set_input_curr_lmt(struct charger_dev *charger, int ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_input_curr_lmt(sy, ma);
}

static int sy_chg_disable_power_path(struct charger_dev *charger, bool ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_disable_power_path(sy, ma);
}

static int sy_chg_set_input_volt_lmt(struct charger_dev *charger, int mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_input_volt_lmt(sy, mv);
}

static int sy_chg_set_ichg(struct charger_dev *charger, int ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_ichg(sy, ma);
}

static int sy_chg_set_chg(struct charger_dev *charger, bool en)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_chg(sy, en);
}

static int sy_chg_get_chg(struct charger_dev *charger)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_chg(sy);
}

static int sy_chg_set_otg(struct charger_dev *charger, bool en)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_normal_request_otg(sy, en);
}

static int sy_chg_set_otg_curr(struct charger_dev *charger, int ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_otg_curr(sy, ma);
}

static int sy_chg_set_otg_volt(struct charger_dev *charger, int mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_otg_volt(sy, mv);
}

static int sy_chg_set_term(struct charger_dev *charger, bool en)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_term(sy, en);
}

static int sy_chg_set_term_curr(struct charger_dev *charger, int ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_term_curr(sy, ma);
}

static int sy_chg_set_term_volt(struct charger_dev *charger, int mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_term_volt(sy, mv);
}

static int sy_chg_set_qc_term_vbus(struct charger_dev *charger)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_qc_term_vbus(sy);
}

static int sy_chg_enable_adc(struct charger_dev *charger, bool en)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_adc_enable(sy, en);
}

static int sy_chg_set_prechg_curr(struct charger_dev *charger, int ma)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_prechg_curr(sy, ma);
}

static int sy_chg_set_prechg_volt(struct charger_dev *charger, int mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_prechg_volt(sy, mv);
}

static int sy_chg_force_dpdm(struct charger_dev *charger)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_force_dpdm(sy);
}

static int sy_chg_reset(struct charger_dev *charger)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_chip_reset(sy);
}

static int sy_chg_set_wd_timeout(struct charger_dev *charger, int ms)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_wd_timeout(sy, ms);
}

static int sy_chg_kick_wd(struct charger_dev *charger)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_kick_wd(sy);
}

static int sy_chg_set_shipmode(struct charger_dev *charger, bool en)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	subpmic_chg_set_shipmode(sy, en);

	return 0;
}

static int sy_chg_set_rechg_vol(struct charger_dev *charger, int mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_set_rechg_vol(sy, mv);
}

static int sy_chg_qc_identify(struct charger_dev *charger, int qc3_enable)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	sy->qc3_support = qc3_enable;

	return subpmic_chg_qc_identify(sy);
}

static int sy_chg_qc3_vbus_puls(struct charger_dev *charger, bool state, int count)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_qc3_vbus_puls(sy, state, count);
}

static int sy_chg_request_qc20(struct charger_dev *charger, int mv)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_request_qc20(sy, mv);
}

static int sy_chg_get_vindpm_state(struct charger_dev *charger, int *vindpm_state)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_get_vindpm_state(sy, vindpm_state);
}

static int sy_chg_dump_registers(struct charger_dev *charger)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);

	return subpmic_chg_dump_regs(sy, NULL);
}

static int sy_disable_dpdm_block(struct charger_dev *charger)
{
	struct subpmic_chg_device *sy = charger_get_private(charger);
	int ret = 0;

	ret = subpmic_chg_field_write(sy, F_QC_EN, false);
	if (ret < 0) {
		dev_err(sy->dev, "dpdm block disable fail \n");
	}

	return ret;
}

static struct charger_ops charger_ops = {
	.get_adc = sy_chg_get_adc,
	.get_vbus_type = sy_chg_get_vbus_type,
	.get_online = sy_chg_get_online,
	.is_charge_done = sy_chg_is_charge_done,
	.get_hiz_status = sy_chg_get_hiz_status,
	.get_ichg = sy_chg_get_ichg,
	.get_input_curr_lmt = sy_chg_get_input_curr_lmt,
	.get_input_volt_lmt = sy_chg_get_input_volt_lmt,
	.get_chg_status = sy_chg_get_chg_status,
	.get_chip_chg_status = sy_chg_get_chip_chg_status,
	.get_otg_status = sy_chg_get_otg_status,
	.get_term_curr = sy_chg_get_term_curr,
	.get_term_volt = sy_chg_get_term_volt,
	.set_hiz = sy_chg_set_hiz,
	.set_input_curr_lmt = sy_chg_set_input_curr_lmt,
	.disable_power_path = sy_chg_disable_power_path,
	.set_input_volt_lmt = sy_chg_set_input_volt_lmt,
	.set_ichg = sy_chg_set_ichg,
	.set_chg = sy_chg_set_chg,
	.get_chg = sy_chg_get_chg,
	.set_otg = sy_chg_set_otg,
	.set_otg_curr = sy_chg_set_otg_curr,
	.set_otg_volt = sy_chg_set_otg_volt,
	.set_term = sy_chg_set_term,
	.set_term_curr = sy_chg_set_term_curr,
	.set_term_volt = sy_chg_set_term_volt,
	.set_qc_term_vbus = sy_chg_set_qc_term_vbus,
	.adc_enable = sy_chg_enable_adc,
	.set_prechg_curr = sy_chg_set_prechg_curr,
	.set_prechg_volt = sy_chg_set_prechg_volt,
	.force_dpdm = sy_chg_force_dpdm,
	.reset = sy_chg_reset,
	.set_wd_timeout = sy_chg_set_wd_timeout,
	.kick_wd = sy_chg_kick_wd,
	.set_shipmode = sy_chg_set_shipmode,
	.set_rechg_vol = sy_chg_set_rechg_vol,
	.qc_identify = sy_chg_qc_identify,
	.qc3_vbus_puls = sy_chg_qc3_vbus_puls,
	.qc2_vbus_mode = sy_chg_request_qc20,
	.get_vindpm_state = sy_chg_get_vindpm_state,
	.dump_registers = sy_chg_dump_registers,
	.disable_dpdm_block = sy_disable_dpdm_block,
};

static struct subpmic_led_ops sy6976_led_ops = {
	.set_led_flash_curr = subpmic_set_led_flash_curr,
	.set_led_flash_time = subpmic_set_led_flash_timer,
	.set_led_flash_enable = subpmic_set_led_flash_enable,
	.set_led_torch_curr = subpmic_set_led_torch_curr,
	.set_led_torch_enable = subpmic_set_led_torch_enable,
};

static int subpmic_chg_parse_dtb(struct subpmic_chg_device *sy, struct device_node *np)
{
	int ret, i;
	const struct {
		const char *name;
		u32 *val;
	} buck_data[] = {
		{"sy,vsys-limit",       &(sy->buck_init.vsyslim)},
		{"sy,batsnc-enable",    &(sy->buck_init.batsns_en)},
		{"sy,vbat",             &(sy->buck_init.vbat)},
		{"sy,charge-curr",      &(sy->buck_init.ichg)},
		{"sy,iindpm-disable",   &(sy->buck_init.iindpm_dis)},
		{"sy,input-curr-limit", &(sy->buck_init.iindpm)},
		{"sy,ico-enable",       &(sy->buck_init.ico_enable)},
		{"sy,iindpm-ico",       &(sy->buck_init.iindpm_ico)},
		{"sy,precharge-volt",   &(sy->buck_init.vprechg)},
		{"sy,precharge-curr",   &(sy->buck_init.iprechg)},
		{"sy,term-en",          &(sy->buck_init.iterm_en)},
		{"sy,term-curr",        &(sy->buck_init.iterm)},
		{"sy,rechg-dis",        &(sy->buck_init.rechg_dis)},
		{"sy,rechg-dg",         &(sy->buck_init.rechg_dg)},
		{"sy,rechg-volt",       &(sy->buck_init.rechg_volt)},
		{"sy,boost-voltage",    &(sy->buck_init.vboost)},
		{"sy,boost-max-current", &(sy->buck_init.iboost)},
		{"sy,conv-ocp-dis",     &(sy->buck_init.conv_ocp_dis)},
		{"sy,tsbat-jeita-dis",  &(sy->buck_init.tsbat_jeita_dis)},
		{"sy,ibat-ocp-dis",     &(sy->buck_init.ibat_ocp_dis)},
		{"sy,vpmid-ovp-otg-dis", &(sy->buck_init.vpmid_ovp_otg_dis)},
		{"sy,vbat-ovp-buck-dis", &(sy->buck_init.vbat_ovp_buck_dis)},
		{"sy,ibat-ocp",         &(sy->buck_init.ibat_ocp)},
		{"sy,cid-en",           &(sy->buck_init.cid_en)},
		{"sy,fast-chg-safety-timer", &(sy->buck_init.safety_timer)},
	};

	for (i = 0; i < ARRAY_SIZE(buck_data); i++) {
		ret = of_property_read_u32(np, buck_data[i].name, buck_data[i].val);
		if (ret < 0) {
			dev_err(sy->dev, "not find property %s\n", buck_data[i].name);
			return ret;
		} else {
			dev_info(sy->dev, "%s: %d\n", buck_data[i].name,
					(int)*buck_data[i].val);
		}
	}

	return 0;
}

#ifdef CONFIG_ENABLE_SYSFS_DEBUG
static ssize_t subpmic_chg_show_regs(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct subpmic_chg_device *sy = dev_get_drvdata(dev);

	return subpmic_chg_dump_regs(sy, buf);
}

static int get_parameters(char *buf, unsigned long *param, int num_of_par)
{
	int cnt = 0;
	char *token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if (kstrtoul(token, 0, &param[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else {
			return -EINVAL;
		}
	}

	return 0;
}

static ssize_t subpmic_chg_test_store_property(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct subpmic_chg_device *sy = dev_get_drvdata(dev);
	int ret/*, i*/;
	long int val;

	ret = get_parameters((char *)buf, &val, 1);
	if (ret < 0) {
		dev_err(dev, "get parameters fail\n");
		return -EINVAL;
	}

	switch (val) {
	case 1: /* enable otg */
		subpmic_chg_request_otg(sy, 0, true);
		break;
	case 2: /* disenable otg */
		subpmic_chg_request_otg(sy, 0, false);
		break;
	case 3: /* open led1 flash mode */
		subpmic_chg_set_led_flash_curr(sy, LED1_FLASH, 300);
		break;
	case 4:
		subpmic_chg_set_led_flash_curr(sy, LED2_FLASH, 300);
		break;
	case 5:
		subpmic_chg_set_led_flash_curr(sy, LED_ALL_FLASH, 300);
		break;
	case 6:
		subpmic_chg_set_led_flash_enable(sy, LED1_FLASH, true);
		break;
	case 7:
		subpmic_chg_set_led_flash_enable(sy, LED2_FLASH, true);
		break;
	case 8:
		subpmic_chg_set_led_flash_enable(sy, LED_ALL_FLASH, true);
		break;
	case 9:
		subpmic_chg_set_led_flash_enable(sy, LED_ALL_FLASH, false);
		break;
	case 10:
		switch (subpmic_chg_qc_identify(sy)) {
		case QC2_MODE:
			subpmic_chg_request_qc20(sy, 9000);
			break;
		case QC3_MODE:
			subpmic_chg_request_qc30(sy, 9000);
			mdelay(2000);
			subpmic_chg_request_qc30(sy, 5000);
			break;
		case QC3_5_18W_MODE:
		case QC3_5_27W_MODE:
		case QC3_5_45W_MODE:
			subpmic_chg_request_qc35(sy, 9000);
			mdelay(2000);
			subpmic_chg_request_qc35(sy, 5000);
			break;
		default:
			break;
		}

		break;
	case 11:
		subpmic_chg_request_qc35(sy, 9000);
		break;
	case 12:
		subpmic_chg_field_write(sy, F_QC3_PULS, true);
		break;
	case 13:
		subpmic_chg_field_write(sy, F_QC3_MINUS, true);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t subpmic_chg_test_show_property(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int ret;

	ret = snprintf(buf, 256, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		"1: otg enable", "2: otg disable",
		"3: en led1 flash", "4: dis led1 flash",
		"5: en led1 torch", "6: dis led2 torch", "7: enter shipmode");

	return ret;
}

static DEVICE_ATTR(showregs, 0440, subpmic_chg_show_regs, NULL);
static DEVICE_ATTR(test, 0660, subpmic_chg_test_show_property,
				subpmic_chg_test_store_property);

static void subpmic_chg_sysfs_file_init(struct device *dev)
{
	device_create_file(dev, &dev_attr_showregs);
	device_create_file(dev, &dev_attr_test);
}
#endif /* CONFIG_ENABLE_SYSFS_DEBUG */

static int subpmic_chg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct subpmic_chg_device *sy;
	int i, ret;
	int retry_count = 0;

	dev_info(dev, "%s (%s)\n", __func__, SUBPMIC_CHARGER_VERSION);

	sy = devm_kzalloc(dev, sizeof(*sy), GFP_KERNEL);
	if (!sy)
		return -ENOMEM;

	sy->rmap = dev_get_regmap(dev->parent, NULL);
	if (!sy->rmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}
	sy->dev = dev;
	platform_set_drvdata(pdev, sy);

	for (i = 0; i < ARRAY_SIZE(subpmic_chg_reg_fields); i++) {
		sy->rmap_fields[i] = devm_regmap_field_alloc(dev,
				sy->rmap, subpmic_chg_reg_fields[i]);
		if (IS_ERR(sy->rmap_fields[i])) {
			dev_err(dev, "cannot allocate regmap field\n");
			return PTR_ERR(sy->rmap_fields[i]);
		}
	}

	ret = subpmic_chg_parse_dtb(sy, dev->of_node);
	if (ret < 0) {
		dev_err(dev, "dtb parse failed\n");
		goto err_1;
	}

	while (sy->tcpc == NULL && retry_count < 10) {
		sy->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (sy->tcpc) {
			break;
		}
		msleep(500);
		dev_info(dev, "sy6976 tcpc_dev_get_by_name retry\n");
		retry_count++;
	}

	if (sy->tcpc == NULL) {
		dev_err(dev, "tcpc_dev_get_by_name failed\n");
		goto err_1;
	}

	ret = subpmic_chg_hw_init(sy);
	if (ret < 0) {
		dev_err(dev, "hw init failed\n");
		goto err_1;
	}

	sy->sy_charger = charger_register("primary_chg", sy->dev, &charger_ops, sy);
	if (!sy->sy_charger) {
		ret = PTR_ERR(sy->sy_charger);
		goto err_1;
	}

	sy->led_dev = subpmic_led_register("subpmic_led", sy->dev, &sy6976_led_ops, sy);
	if (IS_ERR_OR_NULL(sy->led_dev)) {
		ret = PTR_ERR(sy->led_dev);
		dev_err(dev, "led_dev register failed\n");
	}

	ret = subpmic_chg_request_irq_thread(sy);
	if (ret < 0) {
		dev_err(dev, "irq request failed\n");
		goto err_1;
	}

	sy->use_soft_bc12 = false;
	if (sy->use_soft_bc12) {
		sy->bc = bc12_register(sy, &bc12_ops, false);
		if (!sy->bc) {
			ret = PTR_ERR(sy->bc);
			goto err_1;
		}
		sy->bc12_result_nb.notifier_call = subpmic_chg_bc12_notify_cb;
		bc12_register_notifier(sy->bc, &sy->bc12_result_nb);
	}

	mutex_init(&sy->bc_detect_lock);
	mutex_init(&sy->adc_read_lock);
	INIT_WORK(&sy->qc_detect_work, qc_detect_workfunc);
	INIT_DELAYED_WORK(&sy->led_work, subpmic_chg_led_flash_done_workfunc);
	init_completion(&sy->flash_end);
	init_completion(&sy->flash_run);
	sy->led_state = false;
	sy->request_otg = 0;

	#ifdef CONFIG_ENABLE_SYSFS_DEBUG
	subpmic_chg_sysfs_file_init(sy->dev);
	#endif /* CONFIG_ENABLE_SYSFS_DEBUG */

	subpmic_chg_hk_alert_handler(0, sy);
	//subpmic_chg_buck_alert_handler(0, sy);

	INIT_DELAYED_WORK(&sy->cid_det_work, subpmic_cid_det_workfunc);
	schedule_delayed_work(&sy->cid_det_work, msecs_to_jiffies(1500));

	dev_info(dev, "probe success\n");

	return 0;

err_1:
	dev_info(dev, "probe failed\n");

	return ret;
}

static int subpmic_chg_remove(struct platform_device *pdev)
{
	struct subpmic_chg_device *sy = platform_get_drvdata(pdev);
	int i = 0;

	for (i = 0; i < IRQ_MAX; i++) {
		disable_irq(sy->irq[i]);
	}

	if (sy->use_soft_bc12) {
		bc12_unregister_notifier(sy->bc, &sy->bc12_result_nb);
		bc12_unregister(sy->bc);
	}

	charger_unregister(sy->sy_charger);

	return 0;
}

static void subpmic_chg_shutdown(struct platform_device *pdev)
{
	struct subpmic_chg_device *sy = platform_get_drvdata(pdev);
	int ret = 0;

	ret = subpmic_chg_normal_request_otg(sy, false);
	ret = subpmic_chg_field_write(sy, F_ACDRV_MANUAL_EN, false);
	if (ret < 0)
		dev_info(sy->dev, "%s: set ACDRV mode fail\n", __func__);
	else
		dev_info(sy->dev, "%s: set ACDRV auto-mode success\n", __func__);

	subpmic_chg_set_chg(sy, false);
}

static const struct of_device_id subpmic_chg_of_match[] = {
	{.compatible = "silergy,subpmic_sy_charger",},
	{},
};
MODULE_DEVICE_TABLE(of, subpmic_chg_of_match);

static struct platform_driver subpmic_chg_driver = {
	.driver = {
		.name = "subpmic_sy_charger",
		.of_match_table = of_match_ptr(subpmic_chg_of_match),
	},
	.probe = subpmic_chg_probe,
	.remove = subpmic_chg_remove,
	.shutdown = subpmic_chg_shutdown,
};

module_platform_driver(subpmic_chg_driver);

MODULE_DESCRIPTION("silergy subpmic platform driver");
MODULE_LICENSE("GPL v2");
