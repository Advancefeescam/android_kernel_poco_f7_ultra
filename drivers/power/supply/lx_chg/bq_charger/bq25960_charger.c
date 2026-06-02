// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Texas Instruments Incorporated.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/hardware_info.h>
#include "bq25960.h"
#include "bq25960_reg.h"
#include "../charger_class/lx_cp_class.h"
#if IS_ENABLED(CONFIG_MIEV)
#include "xm_chg_dfs.h"
#endif
#include "lx_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[LX_CP_BQ25960]"
#endif

/************************************************************************/
static int __bq25960_read_byte(struct bq25960 *sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		lx_err("i2c read fail: can't read from reg 0x%02X\n", reg);
#if IS_ENABLED(CONFIG_MIEV)
		xmdfs_notifier_call_chain(CHG_DFX_CP_I2C_ERR, NULL);
#endif
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __bq25960_write_byte(struct bq25960 *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		lx_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

int bq25960_read_byte(struct bq25960 *sc, u8 reg, u8 *data)
{
	int ret;

	if (sc->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&sc->i2c_rw_lock);
	ret = __bq25960_read_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int bq25960_write_byte(struct bq25960 *sc, u8 reg, u8 data)
{
	int ret;

	if (sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __bq25960_write_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int bq25960_update_bits(struct bq25960 *sc, u8 reg,
				u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (sc->skip_reads || sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __bq25960_read_byte(sc, reg, &tmp);
	if (ret) {
		lx_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq25960_write_byte(sc, reg, tmp);
	if (ret)
		lx_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

/*********************************************************************/

int bq25960_enable_charge(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_CHG_ENABLE;
	else
		val = BQ25960_CHG_DISABLE;

	val <<= BQ25960_CHG_EN_SHIFT;

	lx_err("bq25960 charger %s\n", enable == false ? "disable" : "enable");
	ret = bq25960_update_bits(sc, BQ25960_REG_0F,
				BQ25960_CHG_EN_MASK, val);

	return ret;
}

int bq25960_check_charge_enabled(struct bq25960 *sc, bool *enabled)
{
	int ret;
	u8 val;
	ret = bq25960_read_byte(sc, BQ25960_REG_0F, &val);
	lx_info(">>>reg [0x0f] = 0x%02x\n", val);
	if (!ret)
		*enabled = !!(val & BQ25960_CHG_EN_MASK);

	return ret;
}

int bq25960_enable_wdt(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_WATCHDOG_ENABLE;
	else
		val = BQ25960_WATCHDOG_DISABLE;

	val <<= BQ25960_WATCHDOG_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_10,
				BQ25960_WATCHDOG_DIS_SHIFT, val);

	return ret;
}

/*
static int bq25960_set_wdt(struct bq25960 *sc, int ms)
{
	int ret;
	u8 val;

	if (ms == 500)
		val = BQ25960_WATCHDOG_0P5S;
	else if (ms == 1000)
		val = BQ25960_WATCHDOG_1S;
	else if (ms == 5000)
		val = BQ25960_WATCHDOG_5S;
	else if (ms == 30000)
		val = BQ25960_WATCHDOG_30S;
	else
		val = BQ25960_WATCHDOG_30S;

	val <<= BQ25960_WATCHDOG_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_0B,
				BQ25960_WATCHDOG_MASK, val);
	return ret;
}
*/

int bq25960_set_reg_reset(struct bq25960 *sc)
{
	int ret;
	u8 val = 1;

	val = BQ25960_REG_RESET_ENABLE;

	val <<= BQ25960_REG_RESET_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_0F,
				BQ25960_REG_RESET_MASK, val);

	return ret;
}

int bq25960_enable_batovp(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_BAT_OVP_ENABLE;
	else
		val = BQ25960_BAT_OVP_DISABLE;

	val <<= BQ25960_BAT_OVP_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_00,
				BQ25960_BAT_OVP_DIS_MASK, val);
	return ret;
}

int bq25960_set_batovp_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BAT_OVP_BASE)
		threshold = BQ25960_BAT_OVP_BASE;

	val = (threshold - BQ25960_BAT_OVP_BASE) * 1000 / BQ25960_BAT_OVP_LSB;

	val <<= BQ25960_BAT_OVP_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_00,
				BQ25960_BAT_OVP_MASK, val);
	return ret;
}

int bq25960_enable_batovp_alarm(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_BAT_OVP_ALM_ENABLE;
	else
		val = BQ25960_BAT_OVP_ALM_DISABLE;

	val <<= BQ25960_BAT_OVP_ALM_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_01,
				BQ25960_BAT_OVP_ALM_DIS_MASK, val);
	return ret;
}

int bq25960_set_batovp_alarm_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BAT_OVP_ALM_BASE)
		threshold = BQ25960_BAT_OVP_ALM_BASE;

	val = (threshold - BQ25960_BAT_OVP_ALM_BASE) / BQ25960_BAT_OVP_ALM_LSB;

	val <<= BQ25960_BAT_OVP_ALM_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_01,
				BQ25960_BAT_OVP_ALM_MASK, val);
	return ret;
}

int bq25960_enable_batocp(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_BAT_OCP_ENABLE;
	else
		val = BQ25960_BAT_OCP_DISABLE;

	val <<= BQ25960_BAT_OCP_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_02,
				BQ25960_BAT_OCP_DIS_MASK, val);
	return ret;
}

int bq25960_set_batocp_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BAT_OCP_BASE)
		threshold = BQ25960_BAT_OCP_BASE;

	val = (threshold - BQ25960_BAT_OCP_BASE)*10 / BQ25960_BAT_OCP_LSB;

	val <<= BQ25960_BAT_OCP_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_02,
				BQ25960_BAT_OCP_MASK, val);
	return ret;
}

int bq25960_enable_batocp_alarm(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_BAT_OCP_ALM_ENABLE;
	else
		val = BQ25960_BAT_OCP_ALM_DISABLE;

	val <<= BQ25960_BAT_OCP_ALM_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_03,
				BQ25960_BAT_OCP_ALM_DIS_MASK, val);
	return ret;
}

int bq25960_set_batocp_alarm_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BAT_OCP_ALM_BASE)
		threshold = BQ25960_BAT_OCP_ALM_BASE;

	val = (threshold - BQ25960_BAT_OCP_ALM_BASE) / BQ25960_BAT_OCP_ALM_LSB;

	val <<= BQ25960_BAT_OCP_ALM_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_03,
				BQ25960_BAT_OCP_ALM_MASK, val);
	return ret;
}

int bq25960_set_busovp_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BUS_OVP_BASE)
		threshold = BQ25960_BUS_OVP_BASE;

	val = (threshold - BQ25960_BUS_OVP_BASE) / BQ25960_BUS_OVP_LSB;

	val <<= BQ25960_BUS_OVP_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_06,
				BQ25960_BUS_OVP_MASK, val);
	return ret;
}

int bq25960_enable_busovp_alarm(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_BUS_OVP_ALM_ENABLE;
	else
		val = BQ25960_BUS_OVP_ALM_DISABLE;

	val <<= BQ25960_BUS_OVP_ALM_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_07,
				BQ25960_BUS_OVP_ALM_DIS_MASK, val);
	return ret;
}

int bq25960_set_busovp_alarm_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BUS_OVP_ALM_BASE)
		threshold = BQ25960_BUS_OVP_ALM_BASE;

	val = (threshold - BQ25960_BUS_OVP_ALM_BASE) / BQ25960_BUS_OVP_ALM_LSB;

	val <<= BQ25960_BUS_OVP_ALM_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_07,
				BQ25960_BUS_OVP_ALM_MASK, val);
	return ret;
}

/*BQ25960 no enable_busocp reg*/
# if 0
int bq25960_enable_busocp(struct bq25960 *sc, bool enable)
{
	int ret = 0;
	u8 val;

	if (enable)
		val = BQ25960_BUS_OCP_ENABLE;
	else
		val = BQ25960_BUS_OCP_DISABLE;

	val <<= BQ25960_BUS_OCP_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_08,
				BQ25960_BUS_OCP_DIS_MASK, val);

	return ret;
}
#endif

int bq25960_set_busocp_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold * 10 < BQ25960_BUS_OCP_BASE)
		threshold = BQ25960_BUS_OCP_BASE / 10;

	val = (threshold*10 - BQ25960_BUS_OCP_BASE) / (BQ25960_BUS_OCP_LSB*10);

	val <<= BQ25960_BUS_OCP_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_08,
				BQ25960_BUS_OCP_MASK, val);
	return ret;
}

int bq25960_enable_busocp_alarm(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_BUS_OCP_ALM_ENABLE;
	else
		val = BQ25960_BUS_OCP_ALM_DISABLE;

	val <<= BQ25960_BUS_OCP_ALM_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_09,
				BQ25960_BUS_OCP_ALM_DIS_MASK, val);
	return ret;
}

int bq25960_set_busocp_alarm_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BUS_OCP_ALM_BASE)
		threshold = BQ25960_BUS_OCP_ALM_BASE;

	val = (threshold - BQ25960_BUS_OCP_ALM_BASE) / BQ25960_BUS_OCP_ALM_LSB;

	val <<= BQ25960_BUS_OCP_ALM_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_09,
				BQ25960_BUS_OCP_ALM_MASK, val);
	return ret;
}

int bq25960_enable_batucp_alarm(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_BAT_UCP_ALM_ENABLE;
	else
		val = BQ25960_BAT_UCP_ALM_DISABLE;

	val <<= BQ25960_BAT_UCP_ALM_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_04,
				BQ25960_BAT_UCP_ALM_DIS_MASK, val);
	return ret;
}

int bq25960_set_batucp_alarm_th(struct bq25960 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < BQ25960_BAT_UCP_ALM_BASE)
		threshold = BQ25960_BAT_UCP_ALM_BASE;

	val = (threshold - BQ25960_BAT_UCP_ALM_BASE) / BQ25960_BAT_UCP_ALM_LSB;

	val <<= BQ25960_BAT_UCP_ALM_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_04,
				BQ25960_BAT_UCP_ALM_MASK, val);
	return ret;
}
//////////////////////////////////////////////////////////////////////////////////////////
#if 0
int bq25960_set_acovp_th(struct bq25960 *sc, int threshold)
{
	int ret = 0;

	u8 val;

	if (threshold < BQ25960_AC_OVP_BASE)
		threshold = BQ25960_AC_OVP_BASE;

	if (threshold == BQ25960_AC_OVP_6P5V)
		val = 0x07;
	else
		val = (threshold - BQ25960_AC_OVP_BASE) /  BQ25960_AC_OVP_LSB;

	val <<= BQ25960_AC_OVP_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_05,
				BQ25960_AC_OVP_MASK, val);

	return ret;

}


int bq25960_set_vdrop_th(struct bq25960 *sc, int threshold)
{
	int ret = 0;
	u8 val;

	if (threshold == 300)
		val = BQ25960_VDROP_THRESHOLD_300MV;
	else
		val = BQ25960_VDROP_THRESHOLD_400MV;

	val <<= BQ25960_VDROP_THRESHOLD_SET_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_05,
				BQ25960_VDROP_THRESHOLD_SET_MASK,
				val);
	return ret;
}

int bq25960_set_vdrop_deglitch(struct bq25960 *sc, int us)
{
	int ret = 0;
	u8 val;

	if (us == 8)
		val = BQ25960_VDROP_DEGLITCH_8US;
	else
		val = BQ25960_VDROP_DEGLITCH_5MS;

	val <<= BQ25960_VDROP_DEGLITCH_SET_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_05,
				BQ25960_VDROP_DEGLITCH_SET_MASK,
				val);
	return ret;
}
#endif

int bq25960_enable_bat_therm(struct bq25960 *sc, bool enable)
{
	int ret = 0;
	u8 val;

	if (enable)
		val = BQ25960_TSBAT_FLT_ENABLE;
	else
		val = BQ25960_TSBAT_FLT_DISABLE;

	val <<= BQ25960_TSBAT_FLT_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_0A,
				BQ25960_TSBAT_FLT_DIS_MASK, val);
	return ret;
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
 #if 0
int bq25960_set_bat_therm_th(struct bq25960 *sc, u8 threshold)
{
	int ret;

	ret = bq25960_write_byte(sc, BQ25960_REG_29, threshold);
	return ret;
}
#endif

int bq25960_enable_bus_therm(struct bq25960 *sc, bool enable)
{
	int ret = 0;
	u8 val;

	if (enable)
		val = BQ25960_TSBUS_FLT_ENABLE;
	else
		val = BQ25960_TSBUS_FLT_DISABLE;

	val <<= BQ25960_TSBUS_FLT_DIS_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_0A,
				BQ25960_TSBUS_FLT_DIS_MASK, val);
	return ret;
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
#if 0
int bq25960_set_bus_therm_th(struct bq25960 *sc, u8 threshold)
{
	int ret = 0;
#if 0
	ret = bq25960_write_byte(sc, BQ25960_REG_28, threshold);
#endif
	return ret;
}
#endif
/*
 * please be noted that the unit here is degC
 */
 #if 0
int bq25960_set_die_therm_th(struct bq25960 *sc, u8 threshold)
{
	int ret = 0;
	u8 val;

	/*BE careful, LSB is here is 1/LSB, so we use multiply here*/
	val = (threshold - BQ25960_TDIE_ALM_BASE) * 10/BQ25960_TDIE_ALM_LSB;
	val <<= BQ25960_TDIE_ALM_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_2A,
				BQ25960_TDIE_ALM_MASK, val);

	return ret;
}
#endif
int bq25960_enable_adc(struct bq25960 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = BQ25960_ADC_ENABLE;
	else
		val = BQ25960_ADC_DISABLE;

	val <<= BQ25960_ADC_EN_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_23,
				BQ25960_ADC_EN_MASK, val);
	return ret;
}

int bq25960_set_adc_scanrate(struct bq25960 *sc, bool oneshot)
{
	int ret;
	u8 val;

	if (oneshot)
		val = BQ25960_ADC_RATE_ONESHOT;
	else
		val = BQ25960_ADC_RATE_CONTINOUS;

	val <<= BQ25960_ADC_RATE_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_23,
				BQ25960_ADC_EN_MASK, val);
	return ret;
}

int bq25960_get_adc_data(struct bq25960 *sc, int channel,  int *result)
{
	int ret;
	u8 work_mode = 0x08;
	u8 val_l, val_h;
	u16 val;

	if (channel >= BQ25960_ADC_MAX_NUM)
		return 0;

	ret = bq25960_read_byte(sc, ADC_REG_BASE + (channel << 1), &val_h);
	ret = bq25960_read_byte(sc, ADC_REG_BASE + (channel << 1) + 1, &val_l);

	if (ret < 0)
		return ret;
	val = (val_h << 8) | val_l;
	ret = bq25960_read_byte(sc, BQ25960_REG_0F, &work_mode);
	work_mode = work_mode & BQ25960_EN_BYPASS_MASK;

	if (sc->is_bq25960) {
		if (channel == BQ25960_ADC_IBUS)
			val = (work_mode == 0 ? (66 + val * 9972 / 10000) : (64 + val * 10279 / 10000));
		else if (channel == BQ25960_ADC_VBUS)
			val = val * 1002 / 1000;
		else if (channel == BQ25960_ADC_VAC1)
			val = 3 + val * 10008 / 10000;
		else if (channel == BQ25960_ADC_VOUT)
			val = 5 + val * 10006 / 10000;
		else if (channel == BQ25960_ADC_VBAT)
			val = val * 125 / 100;
		else if (channel == BQ25960_ADC_IBAT)
			val = val * 3125 / 1000;
		else if (channel == BQ25960_ADC_TSBUS)
			val = val * 9766 / 100000;
		else if (channel == BQ25960_ADC_TSBAT)
			val = val * 9766 / 100000;
		else if (channel == BQ25960_ADC_TDIE)
			val = val * 5 / 10;
	}

	*result = val;

	return 0;
}

int bq25960_set_adc_scan(struct bq25960 *sc, int channel, bool enable)
{
	int ret;
	u8 reg;
	u8 mask;
	u8 shift;
	u8 val;

	if (channel > BQ25960_ADC_MAX_NUM)
		return -EINVAL;

	if (channel == BQ25960_ADC_IBUS) {
		reg = BQ25960_REG_23;
		shift = BQ25960_IBUS_ADC_DIS_SHIFT;
		mask = BQ25960_IBUS_ADC_DIS_MASK;
	} else if (channel == BQ25960_ADC_VBUS) {
		reg = BQ25960_REG_23;
		shift = BQ25960_VBUS_ADC_DIS_SHIFT;
		mask = BQ25960_VBUS_ADC_DIS_MASK;
	} else {
		reg = BQ25960_REG_24;
		shift = 9 - channel;
		mask = 1 << shift;
	}

	if (enable)
		val = 0 << shift;
	else
		val = 1 << shift;

	ret = bq25960_update_bits(sc, reg, mask, val);

	return ret;
}

#if 0
/*init mask*/
int bq25960_set_alarm_int_mask(struct bq25960 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = bq25960_read_byte(sc, BQ25960_REG_0F, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = bq25960_write_byte(sc, BQ25960_REG_0F, val);

	return ret;
}
#endif

/*
static int bq25960_clear_alarm_int_mask(struct bq25960 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = bq25960_read_byte(sc, BQ25960_REG_0F, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = bq25960_write_byte(sc, BQ25960_REG_0F, val);

	return ret;
}
*/

/*
static int bq25960_set_fault_int_mask(struct bq25960 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = bq25960_read_byte(sc, BQ25960_REG_12, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = bq25960_write_byte(sc, BQ25960_REG_12, val);

	return ret;
}
*/

/*
static int bq25960_clear_fault_int_mask(struct bq25960 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = bq25960_read_byte(sc, BQ25960_REG_12, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = bq25960_write_byte(sc, BQ25960_REG_12, val);

	return ret;
}
*/

#if 0
int bq25960_set_sense_resistor(struct bq25960 *sc, int r_mohm)
{
	int ret = 0;

	u8 val;

	if (r_mohm == 2)
		val = BQ25960_SET_IBAT_SNS_RES_2MHM;
	else if (r_mohm == 5)
		val = BQ25960_SET_IBAT_SNS_RES_5MHM;
	else
		return -EINVAL;

	val <<= BQ25960_SET_IBAT_SNS_RES_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_2B,
				BQ25960_SET_IBAT_SNS_RES_MASK,
				val);

	return ret;
}
#endif

#if 0
int bq25960_enable_regulation(struct bq25960 *sc, bool enable)
{
	int ret = 0;



	u8 val;

	if (enable)
		val = BQ25960_EN_REGULATION_ENABLE;
	else
		val = BQ25960_EN_REGULATION_DISABLE;

	val <<= BQ25960_EN_REGULATION_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_2B,
				BQ25960_EN_REGULATION_MASK,
				val);
	return ret;
}
#endif

int bq25960_set_ss_timeout(struct bq25960 *sc, int timeout)
{
	int ret = 0;
	u8 val;

	switch (timeout) {
	case 0:
		val = BQ25960_SS_TIMEOUT_DISABLE;
		break;
	case 6:
		val = BQ25960_SS_TIMEOUT_6P25MS;
		break;
	case 12:
		val = BQ25960_SS_TIMEOUT_12P5MS;
		break;
	case 25:
		val = BQ25960_SS_TIMEOUT_25MS;
		break;
	case 50:
		val = BQ25960_SS_TIMEOUT_50MS;
		break;
	case 100:
		val = BQ25960_SS_TIMEOUT_100MS;
		break;
	case 400:
		val = BQ25960_SS_TIMEOUT_400MS;
		break;
	case 1500:
		val = BQ25960_SS_TIMEOUT_1500MS;
		break;
	case 100000:
		val = BQ25960_SS_TIMEOUT_10000MS;
		break;
	default:
		val = BQ25960_SS_TIMEOUT_DISABLE;
		break;
	}

	val <<= BQ25960_SS_TIMEOUT_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_11,
				BQ25960_SS_TIMEOUT_MASK,
				val);

	return ret;
}

/*there is no set_ibat reg*/
#if 0
int bq25960_set_ibat_reg_th(struct bq25960 *sc, int th_ma)
{
	int ret = 0;
	u8 val;

	if (th_ma == 200)
		val = BQ25960_IBAT_REG_200MA;
	else if (th_ma == 300)
		val = BQ25960_IBAT_REG_300MA;
	else if (th_ma == 400)
		val = BQ25960_IBAT_REG_400MA;
	else if (th_ma == 500)
		val = BQ25960_IBAT_REG_500MA;
	else
		val = BQ25960_IBAT_REG_500MA;

	val <<= BQ25960_IBAT_REG_SHIFT;
	ret = bq25960_update_bits(sc, BQ25960_REG_2C,
				BQ25960_IBAT_REG_MASK,
				val);

	return ret;
}
#endif

/*there is no set_vbat reg*/
#if 0
int bq25960_set_vbat_reg_th(struct bq25960 *sc, int th_mv)
{
	int ret = 0;
	u8 val;

	if (th_mv == 50)
		val = BQ25960_VBAT_REG_50MV;
	else if (th_mv == 100)
		val = BQ25960_VBAT_REG_100MV;
	else if (th_mv == 150)
		val = BQ25960_VBAT_REG_150MV;
	else
		val = BQ25960_VBAT_REG_200MV;

	val <<= BQ25960_VBAT_REG_SHIFT;

	ret = bq25960_update_bits(sc, BQ25960_REG_2C,
				BQ25960_VBAT_REG_MASK,
				val);

	return ret;
}
#endif

#if 0
static int bq25960_get_work_mode(struct bq25960 *sc, int *mode)
{
	int ret;
	u8 val;

	ret = bq25960_read_byte(sc, bq25960_REG_0C, &val);

	if (ret) {
		lx_err("Failed to read operation mode register\n");
		return ret;
	}

	val = (val & BQ25960_MS_MASK) >> BQ25960_MS_SHIFT;
	if (val == BQ25960_MS_MASTER)
		*mode = BQ25960_ROLE_MASTER;
	else if (val == BQ25960_MS_SLAVE)
		*mode = BQ25960_ROLE_SLAVE;
	else
		*mode = BQ25960_ROLE_STDALONE;

	lx_info("work mode:%s\n", *mode == BQ25960_ROLE_STDALONE ? "Standalone" :
			(*mode == BQ25960_ROLE_SLAVE ? "Slave" : "Master"));
	return ret;
}
#endif

int bq25960_check_vbus_error_status(struct bq25960 *sc, unsigned int *status)
{
	int ret = 0;
	u8 data = 0;

	ret = bq25960_read_byte(sc, BQ25960_REG_0A, &data);
	if (ret == 0)
		lx_err("vbus error >>>>%02x\n", data);
	*status = 0;

	return ret;
}

int bq25960_detect_device(struct bq25960 *sc)
{
	int ret;
	u8 data;

	ret = bq25960_read_byte(sc, BQ25960_REG_22, &data);
	if (ret == 0) {
		sc->part_no = (data & BQ25960_DEVICE_ID_MASK);
		sc->part_no >>= BQ25960_DEVICE_ID_SHIFT;
		lx_info("part_no = 0x%x\n", sc->part_no);
	}
	return ret;
}

void bq25960_check_alarm_status(struct bq25960 *sc)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&sc->data_lock);
#if 0
	ret = bq25960_read_byte(sc, BQ25960_REG_08, &flag);
	if (!ret && (flag & BQ25960_IBUS_UCP_FALL_FLAG_MASK))
		sc_dbg("UCP_FLAG =0x%02X\n",
			!!(flag & BQ25960_IBUS_UCP_FALL_FLAG_MASK));

	ret = bq25960_read_byte(sc, BQ25960_REG_2D, &flag);
	if (!ret && (flag & BQ25960_VDROP_OVP_FLAG_MASK))
		sc_dbg("VDROP_OVP_FLAG =0x%02X\n",
			!!(flag & BQ25960_VDROP_OVP_FLAG_MASK));
#endif
	/*read to clear alarm flag*/
	ret = bq25960_read_byte(sc, BQ25960_REG_0E, &flag);
	if (!ret && flag)
		sc_dbg("INT_FLAG =0x%02X\n", flag);

	ret = bq25960_read_byte(sc, BQ25960_REG_0D, &stat);
	if (!ret && stat != sc->prev_alarm) {
		sc_dbg("INT_STAT = 0X%02x\n", stat);
		sc->prev_alarm = stat;
		sc->bat_ovp_alarm = !!(stat & BAT_OVP_ALARM);
		sc->bat_ocp_alarm = !!(stat & BAT_OCP_ALARM);
		sc->bus_ovp_alarm = !!(stat & BUS_OVP_ALARM);
		sc->bus_ocp_alarm = !!(stat & BUS_OCP_ALARM);
		sc->batt_present  = !!(stat & VBAT_INSERT);
		sc->vbus_present  = !!(stat & VBUS_INSERT);
		sc->bat_ucp_alarm = !!(stat & BAT_UCP_ALARM);
	}


	ret = bq25960_read_byte(sc, BQ25960_REG_08, &stat);
	if (!ret && (stat & 0x50))
		lx_err("Reg[05]BUS_UCPOVP = 0x%02X\n", stat);

	ret = bq25960_read_byte(sc, BQ25960_REG_0A, &stat);
	if (!ret && (stat & 0x02))
		lx_err("Reg[0A]CONV_OCP = 0x%02X\n", stat);

	mutex_unlock(&sc->data_lock);
}


static struct bq25960_intr_flag cp_intr_flag[] = {
	{ .reg = 0x18, .len = 7, .bit = {
		{.mask = BIT(0), .name = "vbus ovp alm flag", .notify = BQ25960_NOTIFY_VBUSOVPALM},
		{.mask = BIT(1), .name = "vbus ovp flag", .notify = BQ25960_NOTIFY_VBUSOVP},
		{.mask = BIT(3), .name = "ibat ocp alm flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "ibat ocp flag", .notify = BQ25960_NOTIFY_IBATOCP},
		{.mask = BIT(5), .name = "vout ovp flag", .notify = BQ25960_NOTIFY_VOUTOVP},
		{.mask = BIT(6), .name = "vbat ovp alm flag", .notify = BQ25960_NOTIFY_VBATOVPALM},
		{.mask = BIT(7), .name = "vbat ovp flag", .notify = BQ25960_NOTIFY_VBATOVP},
		},
	},
	{ .reg = 0x19, .len = 3, .bit = {
		{.mask = BIT(2), .name = "pin diag fall flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(5), .name = "ibus ucp fall flag", .notify = BQ25960_NOTIFY_IBUSUCPF},
		{.mask = BIT(7), .name = "ibus ocp flag", .notify = BQ25960_NOTIFY_IBUSOCP},
		},
	},
	{ .reg = 0x1a, .len = 8, .bit = {
		{.mask = BIT(0), .name = "acrb2 config flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(1), .name = "acrb1 config flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(2), .name = "vbus present flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(3), .name = "vac2 insert flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "vac1 insert flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(5), .name = "vat insert flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(6), .name = "vac2 ovp flag", .notify = BQ25960_NOTIFY_VACOVP},
		{.mask = BIT(7), .name = "vac1 ovp flag", .notify = BQ25960_NOTIFY_VACOVP},
		},
	},
	{ .reg = 0x1b, .len = 8, .bit = {
		{.mask = BIT(0), .name = "wd timeout flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(1), .name = "tdie alm flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(2), .name = "tshut flag", .notify = BQ25960_NOTIFY_TDIEFLT},
		{.mask = BIT(3), .name = "tsbat flt flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "tsbus flt flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(5), .name = "tsbus tsbat alm flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(6), .name = "ss timeout flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(7), .name = "adc done flag", .notify = BQ25960_NOTIFY_OTHER},
		},
	},
	{ .reg = 0x1c, .len = 2, .bit = {
		//{.mask = BIT(3), .name = "vbus errorlo flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "vbus errorhi flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(6), .name = "cp switching flag", .notify = BQ25960_NOTIFY_OTHER},
		},
	},
};

static void bq25960_notify_fault_status(struct bq25960 *bq, int notify)
{
#if IS_ENABLED(CONFIG_MIEV)
	struct dfs_info info;

	switch (notify) {
	case BQ25960_NOTIFY_IBATOCP:
		xmdfs_notifier_call_chain(CHG_DFX_CP_IBAT_OCP, NULL);
		break;

	case BQ25960_NOTIFY_IBUSOCP:
		xmdfs_notifier_call_chain(CHG_DFX_CP_IBUS_OCP, NULL);
		break;

	case BQ25960_NOTIFY_VBATOVP:
		bq25960_get_adc_data(bq, BQ25960_ADC_VBAT, &info.data[0]);
		mdelay(100);
		bq25960_get_adc_data(bq, BQ25960_ADC_VBAT, &info.data[1]);
		xmdfs_notifier_call_chain(CHG_DFX_CP_VBAT_OVP, &info);
		break;

	case BQ25960_NOTIFY_VBUSOVP:
		xmdfs_notifier_call_chain(CHG_DFX_CP_VBUS_OVP, NULL);
		break;

	case BQ25960_NOTIFY_VACOVP:
		xmdfs_notifier_call_chain(CHG_DFX_CP_VAC_OVP, NULL);
		break;

	case BQ25960_NOTIFY_TDIEFLT:
		xmdfs_notifier_call_chain(CHG_DFX_CP_TDIE_HOT, NULL);
		break;

	default:
		break;
	}
#endif
}

static void bq25960_check_fault_status(struct bq25960 *bq)
{
    int ret;
    u8 flag = 0;
    int i,j;
    for (i=0;i < ARRAY_SIZE(cp_intr_flag);i++) {
        ret = bq25960_read_byte(bq, cp_intr_flag[i].reg, &flag);
        for (j=0; j <  cp_intr_flag[i].len; j++) {
            if (flag & cp_intr_flag[i].bit[j].mask) {
                bq25960_notify_fault_status(bq, cp_intr_flag[i].bit[j].notify);
                lx_err("trigger :%s\n",cp_intr_flag[i].bit[j].name);
            }
        }
    }
}

int bq25960_dump_reg(struct bq25960 *sc)
{
	int ret;
	int i;
	u8 val;

	for (i = 0; i <= 0x37; i++) {
		ret = bq25960_read_byte(sc, i, &val);
		lx_err("%s reg[0x%02x] = 0x%02x\n", __func__, i, val);
	}

	return ret;
}

/*
static int bq25960_check_reg_status(struct bq25960 *sc)
{
	int ret;
	u8 val;

	ret = bq25960_read_byte(sc, bq25960_REG_2C, &val);
	if (!ret) {
		sc->vbat_reg = !!(val & BQ25960_VBAT_REG_ACTIVE_STAT_MASK);
		sc->ibat_reg = !!(val & BQ25960_IBAT_REG_ACTIVE_STAT_MASK);
	}


	return ret;
}
*/

static ssize_t bq25960_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct bq25960 *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq25960");
	for (addr = 0x0; addr <= 0x37; addr++) {
		ret = bq25960_read_byte(sc, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t bq25960_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bq25960 *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x37)
		bq25960_write_byte(sc, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, 0660, bq25960_show_registers, bq25960_store_register);

static void bq25960_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}

static int bq25960_init_adc(struct bq25960 *sc)
{
	bq25960_set_adc_scanrate(sc, false);
	bq25960_set_adc_scan(sc, BQ25960_ADC_IBUS, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_VBUS, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_VOUT, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_VBAT, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_IBAT, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_TSBUS, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_TSBAT, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_TDIE, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_VAC1, true);
	bq25960_set_adc_scan(sc, BQ25960_ADC_VAC2, true);

	bq25960_enable_adc(sc, true);

	return 0;
}

static int bq25960_init_int_src(struct bq25960 *sc)
{
	int ret = 0;
	/*TODO:be careful ts bus and ts bat alarm bit mask is in
	 *	fault mask register, so you need call
	 *	bq25960_set_fault_int_mask for tsbus and tsbat alarm
	 */
#if 0
	ret = bq25960_set_alarm_int_mask(sc, ADC_DONE
		/*			| BAT_UCP_ALARM */
					| BAT_OVP_ALARM);
	if (ret) {
		lx_err("failed to set alarm mask:%d\n", ret);
		return ret;
	}
#endif
#if 0
	ret = bq25960_set_fault_int_mask(sc, TS_BUS_FAULT);
	if (ret) {
		lx_err("failed to set fault mask:%d\n", ret);
		return ret;
	}
#endif
	return ret;
}

static int bq25960_init_protection(struct bq25960 *sc)
{
	int ret;

	ret = bq25960_enable_batovp(sc, !sc->cfg->bat_ovp_disable);
	lx_info("%s bat ovp %s\n",
		sc->cfg->bat_ovp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_enable_batocp(sc, !sc->cfg->bat_ocp_disable);
	lx_info("%s bat ocp %s\n",
		sc->cfg->bat_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_enable_batovp_alarm(sc, !sc->cfg->bat_ovp_alm_disable);
	lx_info("%s bat ovp alarm %s\n",
		sc->cfg->bat_ovp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_enable_batocp_alarm(sc, !sc->cfg->bat_ocp_alm_disable);
	lx_info("%s bat ocp alarm %s\n",
		sc->cfg->bat_ocp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_enable_batucp_alarm(sc, !sc->cfg->bat_ucp_alm_disable);
	lx_info("%s bat ocp alarm %s\n",
		sc->cfg->bat_ucp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_enable_busovp_alarm(sc, !sc->cfg->bus_ovp_alm_disable);
	lx_info("%s bus ovp alarm %s\n",
		sc->cfg->bus_ovp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	/*
	ret = bq25960_enable_busocp(sc, !sc->cfg->bus_ocp_disable);
	lx_info("%s bus ocp %s\n",
		sc->cfg->bus_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");
	*/

	ret = bq25960_enable_busocp_alarm(sc, !sc->cfg->bus_ocp_alm_disable);
	lx_info("%s bus ocp alarm %s\n",
		sc->cfg->bus_ocp_alm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_enable_bat_therm(sc, !sc->cfg->bat_therm_disable);
	lx_info("%s bat therm %s\n",
		sc->cfg->bat_therm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_enable_bus_therm(sc, !sc->cfg->bus_therm_disable);
	lx_info("%s bus therm %s\n",
		sc->cfg->bus_therm_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = bq25960_set_batovp_th(sc, sc->cfg->bat_ovp_th);
	lx_info("set bat ovp th %d %s\n", sc->cfg->bat_ovp_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_batovp_alarm_th(sc, sc->cfg->bat_ovp_alm_th);
	lx_info("set bat ovp alarm threshold %d %s\n", sc->cfg->bat_ovp_alm_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_batocp_th(sc, sc->cfg->bat_ocp_th);
	lx_info("set bat ocp threshold %d %s\n", sc->cfg->bat_ocp_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_batocp_alarm_th(sc, sc->cfg->bat_ocp_alm_th);
	lx_info("set bat ocp alarm threshold %d %s\n", sc->cfg->bat_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_busovp_th(sc, sc->cfg->bus_ovp_th);
	lx_info("set bus ovp threshold %d %s\n", sc->cfg->bus_ovp_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_busovp_alarm_th(sc, sc->cfg->bus_ovp_alm_th);
	lx_info("set bus ovp alarm threshold %d %s\n", sc->cfg->bus_ovp_alm_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_busocp_th(sc, sc->cfg->bus_ocp_th);
	lx_info("set bus ocp threshold %d %s\n", sc->cfg->bus_ocp_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_busocp_alarm_th(sc, sc->cfg->bus_ocp_alm_th);
	lx_info("set bus ocp alarm th %d %s\n", sc->cfg->bus_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_batucp_alarm_th(sc, sc->cfg->bat_ucp_alm_th);
	lx_info("set bat ucp threshold %d %s\n", sc->cfg->bat_ucp_alm_th,
		!ret ? "successfully" : "failed");

	/*
	ret = bq25960_set_bat_therm_th(sc, sc->cfg->bat_therm_th);
	lx_info("set die therm threshold %d %s\n", sc->cfg->bat_therm_th,
		!ret ? "successfully" : "failed");
	ret = bq25960_set_bus_therm_th(sc, sc->cfg->bus_therm_th);
	lx_info("set bus therm threshold %d %s\n", sc->cfg->bus_therm_th,
		!ret ? "successfully" : "failed");
	ret = bq25960_set_die_therm_th(sc, sc->cfg->die_therm_th);
	lx_info("set die therm threshold %d %s\n", sc->cfg->die_therm_th,
		!ret ? "successfully" : "failed");

	ret = bq25960_set_acovp_th(sc, sc->cfg->ac_ovp_th);
	lx_info("set ac ovp threshold %d %s\n", sc->cfg->ac_ovp_th,
		!ret ? "successfully" : "failed");
	*/

	return 0;
}

static int bq25960_init_regulation(struct bq25960 *sc)
{
	//bq25960_set_ibat_reg_th(sc, 300);
	//bq25960_set_vbat_reg_th(sc, 100);

	//bq25960_set_vdrop_deglitch(sc, 5000);
	//bq25960_set_vdrop_th(sc, 400);

	//bq25960_enable_regulation(sc, false);
#if 0
	if (sc->is_bq25960) {
		bq25960_write_byte(sc, BQ25960_REG_2E, 0x08);
		bq25960_write_byte(sc, BQ25960_REG_34, 0x01);
	}
#endif
	return 0;
}

static int bq25960_init_device(struct bq25960 *sc)
{
	bq25960_set_reg_reset(sc);
	bq25960_enable_wdt(sc, false);

	bq25960_set_ss_timeout(sc, 10000);
	//bq25960_set_sense_resistor(sc, sc->cfg->sense_r_mohm);

	bq25960_init_protection(sc);
	bq25960_init_adc(sc);
	bq25960_init_int_src(sc);

	bq25960_init_regulation(sc);

	return 0;
}

int bq25960_set_present(struct bq25960 *sc, bool present)
{
	sc->usb_present = present;

	if (present)
		bq25960_init_device(sc);
	return 0;
}

/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t bq25960_charger_interrupt(int irq, void *dev_id)
{
	struct bq25960 *sc = dev_id;
	int i;
	u8 stat = 0;

	sc_dbg("INT OCCURRED\n");
	for (i = 0; i < 0x16; i++) {
		bq25960_read_byte(sc, i, &stat);
		lx_err("FAULT_STAT REG[%x] = 0x%02X\n", i, stat);
	}

	/* TODO */
	bq25960_check_alarm_status(sc);
	bq25960_check_fault_status(sc);

	return IRQ_HANDLED;
}

static int bq25960_parse_dt(struct bq25960 *sc, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	sc->cfg = devm_kzalloc(dev, sizeof(struct bq25960_cfg),
					GFP_KERNEL);

	if (!sc->cfg)
		return -ENOMEM;

	sc->cfg->bat_ovp_disable = of_property_read_bool(np,
			"ti,bq25960,bat-ovp-disable");
	sc->cfg->bat_ocp_disable = of_property_read_bool(np,
			"ti,bq25960,bat-ocp-disable");
	sc->cfg->bat_ovp_alm_disable = of_property_read_bool(np,
			"ti,bq25960,bat-ovp-alarm-disable");
	sc->cfg->bat_ocp_alm_disable = of_property_read_bool(np,
			"ti,bq25960,bat-ocp-alarm-disable");
	sc->cfg->bus_ocp_disable = of_property_read_bool(np,
			"ti,bq25960,bus-ocp-disable");
	sc->cfg->bus_ovp_alm_disable = of_property_read_bool(np,
			"ti,bq25960,bus-ovp-alarm-disable");
	sc->cfg->bus_ocp_alm_disable = of_property_read_bool(np,
			"ti,bq25960,bus-ocp-alarm-disable");
	sc->cfg->bat_ucp_alm_disable = of_property_read_bool(np,
			"ti,bq25960,bat-ucp-alarm-disable");
	sc->cfg->bat_therm_disable = of_property_read_bool(np,
			"ti,bq25960,bat-therm-disable");
	sc->cfg->bus_therm_disable = of_property_read_bool(np,
			"ti,bq25960,bus-therm-disable");

	ret = of_property_read_u32(np, "ti,bq25960,bat-ovp-threshold",
			&sc->cfg->bat_ovp_th);
	if (ret) {
		lx_err("failed to read bat-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,bat-ovp-alarm-threshold",
			&sc->cfg->bat_ovp_alm_th);
	if (ret) {
		lx_err("failed to read bat-ovp-alarm-threshold\n");
		return ret;
	}
#if 0
	ret = of_property_read_u32(np, "ti,bq25960,bat-ocp-threshold",
			&sc->cfg->bat_ocp_th);
	if (ret) {
		lx_err("failed to read bat-ocp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,bat-ocp-alarm-threshold",
			&sc->cfg->bat_ocp_alm_th);
	if (ret) {
		lx_err("failed to read bat-ocp-alarm-threshold\n");
		return ret;
	}
#endif
	ret = of_property_read_u32(np, "ti,bq25960,bus-ovp-threshold",
			&sc->cfg->bus_ovp_th);
	if (ret) {
		lx_err("failed to read bus-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,bus-ovp-alarm-threshold",
			&sc->cfg->bus_ovp_alm_th);
	if (ret) {
		lx_err("failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,bus-ocp-threshold",
			&sc->cfg->bus_ocp_th);
	if (ret) {
		lx_err("failed to read bus-ocp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,bus-ocp-alarm-threshold",
			&sc->cfg->bus_ocp_alm_th);
	if (ret) {
		lx_err("failed to read bus-ocp-alarm-threshold\n");
		return ret;
	}
#if 0
	ret = of_property_read_u32(np, "ti,bq25960,bat-ucp-alarm-threshold",
			&sc->cfg->bat_ucp_alm_th);
	if (ret) {
		lx_err("failed to read bat-ucp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti,bq25960,bat-therm-threshold",
			&sc->cfg->bat_therm_th);
	if (ret) {
		lx_err("failed to read bat-therm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,bus-therm-threshold",
			&sc->cfg->bus_therm_th);
	if (ret) {
		lx_err("failed to read bus-therm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,die-therm-threshold",
			&sc->cfg->die_therm_th);
	if (ret) {
		lx_err("failed to read die-therm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "ti,bq25960,ac-ovp-threshold",
			&sc->cfg->ac_ovp_th);
	if (ret) {
		lx_err("failed to read ac-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ti,bq25960,sense-resistor-mohm",
			&sc->cfg->sense_r_mohm);
	if (ret) {
		lx_err("failed to read sense-resistor-mohm\n");
		return ret;
	}
#endif
	return 0;
}

static void determine_initial_status(struct bq25960 *sc)
{
	if (sc->client->irq)
		bq25960_charger_interrupt(sc->client->irq, sc);
}

static const struct of_device_id bq25960_charger_match_table[] = {
	{
		.compatible = "ti,bq25960-standalone",
		.data = &bq25960_mode_data[BQ25960_STDALONE],
	},
	{
		.compatible = "ti,bq25960-master",
		.data = &bq25960_mode_data[BQ25960_MASTER],
	},

	{
		.compatible = "ti,bq25960-slave",
		.data = &bq25960_mode_data[BQ25960_SLAVE],
	},
	{},
};
MODULE_DEVICE_TABLE(of, bq25960_charger_match_table);

static inline enum bq25960_adc_ch to_bq25960_adc(enum cp_adc_channel channel)
{
	enum bq25960_adc_ch ch = BQ25960_ADC_MAX_NUM;

	switch (channel) {
	case CP_ADC_VBUS:
		ch = BQ25960_ADC_VBUS;
		break;
	case CP_ADC_VBAT:
		ch = BQ25960_ADC_VBAT;
		break;
	case CP_ADC_IBUS:
		ch = BQ25960_ADC_IBUS;
		break;
	case CP_ADC_IBAT:
		ch = BQ25960_ADC_IBAT;
		break;
	case CP_ADC_TDIE:
		ch = BQ25960_ADC_TDIE;
		break;
	default:
		ch = BQ25960_ADC_MAX_NUM;
		break;
	}

	return ch;
}

static int bq25960_set_charge(struct chargerpump_dev *charger_pump, bool enable)
{
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	return bq25960_enable_charge(chip, enable);
}

static int bq25960_get_status(struct chargerpump_dev *charger_pump, unsigned int *status)
{
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	return bq25960_check_vbus_error_status(chip, status);
}

static int bq259605_get_is_enable(struct chargerpump_dev *charger_pump, bool *enable)
{
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	return bq25960_check_charge_enabled(chip, enable);
}

static int bq25960_get_adc_value(struct chargerpump_dev *charger_pump, enum cp_adc_channel ch, int *value)
{
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	return bq25960_get_adc_data(chip, to_bq25960_adc(ch), value);
}

static int bq25960_set_enable_adc(struct chargerpump_dev *charger_pump, bool en)
{
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	return bq25960_enable_adc(chip, en);
}

static int bq25960_get_chip_id(struct chargerpump_dev *charger_pump, int *value)
{
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	*value = chip->part_no;
	return 0;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int bq25960_set_cp_workmode(struct chargerpump_dev *charger_pump, int workmode)
{
	int ret = 0;
	u8 val;
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	if (workmode)
		val = BQ_1_1_MODE;
	else
		val = BQ_2_1_MODE;

	val <<= BQ25960_EN_BYPASS_SHIFT;

	lx_info("enter bq_set_cp_workmode = %d, val = %d\n", workmode, val);

	ret = bq25960_update_bits(chip, BQ25960_REG_0F, BQ25960_EN_BYPASS_MASK, val);
	return ret;
}

static int bq25960_get_cp_workmode(struct chargerpump_dev *charger_pump, int *workmode)
{
	int ret;
	u8 val;
	struct bq25960 *chip = chargerpump_get_private(charger_pump);

	ret = bq25960_read_byte(chip, BQ25960_REG_0F, &val);

	val = val & BQ25960_EN_BYPASS_MASK;
	lx_info("bq25960 in get_cp_workmode val = %d, ret = %d", val, ret);

	if (!ret)
		*workmode = (val == 0x08 ? BQ_1_1_MODE : BQ_2_1_MODE);

	lx_info("bq25960_get_cp_workmode = %d", *workmode);
	return ret;
}
#endif

static int bq25960_dump_register(struct chargerpump_dev *charger_pump)
{
	struct bq25960 *chip = chargerpump_get_private(charger_pump);
	int ret = 0;

	ret = bq25960_dump_reg(chip);

	return ret;
}

static void bq25960h_set_rcp_workfunc(struct work_struct *work)
{
	int ret = 0;
	u8 val = 0;
	struct bq25960 *chip = container_of(work,
					struct bq25960, set_rcp_work.work);

	ret = bq25960_write_byte(chip, BQ25960_REG_05, 0x9E);
	if (ret != 0) {
		lx_info("[BQ25960H]:  regmap_write 0x05 fail\n");
	}

	//[REVCHG] For debug
	ret = bq25960_read_byte(chip, BQ25960_REG_0F, &val);
	if (ret)
		return;
	lx_info("[REVCHG]: reg[0x0f] = 0x%02x\n", val);

	ret = bq25960_read_byte(chip, BQ25960_REG_FA, &val);
	if (ret)
		return;
	lx_info("[REVCHG]: in 9V reg[0xFA] = 0x%02x\n", val);

	ret = bq25960_read_byte(chip, BQ25960_REG_9A, &val);
	if (ret)
		return;
	lx_info("[REVCHG]: in 9V reg[0x9A] = 0x%02x\n", val);

	ret = bq25960_read_byte(chip, BQ25960_REG_9B, &val);
	if (ret)
		return;
	lx_info("[REVCHG]: in 9V reg[0x9B] = 0x%02x\n", val);

	bq25960_dump_reg(chip);

	lx_info("[BQ25960H] the end\n");
}


// reverse output 9V
static int bq25960_set_otg_enable(struct chargerpump_dev *charger_pump, bool enable)
{
	int ret = 0;
	u8 val = 0; //add for log
	int i = 5;
	struct bq25960 *chip = chargerpump_get_private(charger_pump);
	lx_info("[BQ25960] is %d\n", __func__, enable);

	if (enable) {
		ret = bq25960_write_byte(chip, BQ25960_REG_10, 0x87);	//disable watchdog
		ret |= bq25960_write_byte(chip, BQ25960_REG_0E, 0x6C);	//set vac1_ovp as 14v
		ret |= bq25960_write_byte(chip, BQ25960_REG_06, 0x6C);	//set vbus_ovp as 12.4v
		//ret |= bq25960_write_byte(bq, BQ25960_REG_0A, 0xEC);	//disable TSBAT+TSBUS
		ret |= bq25960_write_byte(chip, BQ25960_REG_0A, 0x6C);	//disable TSBAT+TSBUS
		ret |= bq25960_write_byte(chip, BQ25960_REG_9A, 0x34);	//Lock 0x3F I2C Address
		ret |= bq25960_write_byte(chip, BQ25960_REG_9B, 0x5B);	//Lock 0x3F I2C Address
		ret |= bq25960_write_byte(chip, BQ25960_REG_A0, 0x80);	//Enter 0x3F I2C Address
		mdelay(10);
		ret = bq25960_write_byte(chip, BQ25960_REG_A8, 0x00);//reset A8

		//disable the busucp and busrcp
		ret |= bq25960_write_byte(chip, BQ25960_REG_05, 0xAE);
		//enable cp reverse and output 9V on vac1_ovpfet
		ret |= bq25960_write_byte(chip, BQ25960_REG_0F, 0x12);

		mdelay(10);

		while (i--) {
			ret = bq25960_write_byte(chip, BQ25960_REG_0F, 0x12);
			if (ret != 0) {
				pr_err("%s regmap_write 0x0F fail \n", __func__);
			}

			ret = bq25960_read_byte(chip, BQ25960_REG_0F, &val);
			if (ret == 0) {
				lx_info("[BQ25960H] regmap_read 0x0F = 0x%02x\n", __func__, val);
			} else {
				lx_info("[BQ25960H] regmap_read REG 0x0F error\n", __func__);
			}

            if (0x12 == val)
                break;

            mdelay(10);
        }

		//recovery to default value for 0xfa register
		ret |= bq25960_write_byte(chip, BQ25960_REG_FA, 0x21);
		//set 3A rcp as cp reverse current limit
		ret |= bq25960_write_byte(chip, BQ25960_REG_05, 0xBE);

		schedule_delayed_work(&chip->set_rcp_work, msecs_to_jiffies(500));
	} else {
		lx_info("[BQ25960H] in bq25960_exit_reverse_output\n", __func__);
		//disable cp reverse
		ret = bq25960_write_byte(chip, BQ25960_REG_0F, 0x00);
		//configure the busucp and busrcp back to the desired setting
		ret |= bq25960_write_byte(chip, BQ25960_REG_05, 0x0E);
		//recover vac1ovp to default 6.5v
		ret |= bq25960_write_byte(chip, BQ25960_REG_FA, 0x20);	//exit cp reverse
		ret |= bq25960_write_byte(chip, BQ25960_REG_F9, 0x00);	//exit cp reverse
		mdelay(10);
		//exits 0x3f and return to 0x65 i2c 7bit address
		ret |= bq25960_write_byte(chip, BQ25960_REG_A0, 0x00);
	}

	pr_err("[BQ25960H] in end of bq25960h_set_otg\n");
	return ret;
}

static struct chargerpump_ops bq25960_ops = {
	.set_enable = bq25960_set_charge,
	.get_status = bq25960_get_status,
	.get_is_enable = bq259605_get_is_enable,
	.get_adc_value = bq25960_get_adc_value,
	.set_enable_adc = bq25960_set_enable_adc,
	.get_chip_id = bq25960_get_chip_id,
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	.set_cp_workmode = bq25960_set_cp_workmode,
	.get_cp_workmode = bq25960_get_cp_workmode,
#endif
	.dump_cp_register = bq25960_dump_register,
	.set_otg_enable = bq25960_set_otg_enable,
};

/************************psy start**************************************/
static enum power_supply_property bq25960_charger_props[] = {
		POWER_SUPPLY_PROP_ONLINE,
		POWER_SUPPLY_PROP_VOLTAGE_NOW,
		POWER_SUPPLY_PROP_CURRENT_NOW,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
		POWER_SUPPLY_PROP_TEMP,
};

static int bq25960_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct bq25960 *chip = power_supply_get_drvdata(psy);
	int result = 0;
	bool online = false;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		bq25960_check_charge_enabled(chip, &online);
		val->intval = online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq25960_get_adc_data(chip, BQ25960_ADC_VBUS, &result);
		val->intval = result;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq25960_get_adc_data(chip, BQ25960_ADC_IBUS, &result);
		val->intval = result;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq25960_get_adc_data(chip, BQ25960_ADC_VBAT, &result);
		val->intval = result;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq25960_get_adc_data(chip, BQ25960_ADC_IBAT, &result);
		val->intval = result;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq25960_get_adc_data(chip, BQ25960_ADC_TDIE, &result);
		val->intval = result;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq25960_charger_set_property(struct power_supply *psy,
			enum power_supply_property prop,
			const union power_supply_propval *val)
{
	struct bq25960 *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		bq25960_enable_charge(chip, val->intval);
		lx_info("POWER_SUPPLY_PROP_ONLINE: %s\n",
				val->intval ? "enable" : "disable");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq25960_charger_is_writeable(struct power_supply *psy,
				enum power_supply_property prop)
{
	return 0;
}

static int bq25960_psy_register(struct bq25960 *chip)
{
	chip->psy_cfg.drv_data = chip;
	chip->psy_cfg.of_node = chip->dev->of_node;

	chip->psy_desc.name = "sc-cp-master";

	chip->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	chip->psy_desc.properties = bq25960_charger_props;
	chip->psy_desc.num_properties = ARRAY_SIZE(bq25960_charger_props);
	chip->psy_desc.get_property = bq25960_charger_get_property;
	chip->psy_desc.set_property = bq25960_charger_set_property;
	chip->psy_desc.property_is_writeable = bq25960_charger_is_writeable;


	chip->psy = devm_power_supply_register(chip->dev,
			&chip->psy_desc, &chip->psy_cfg);
	if (IS_ERR(chip->psy)) {
		lx_err("%s failed to register psy\n", __func__);
		return PTR_ERR(chip->psy);
	}

	lx_info("%s power supply register successfully\n", chip->psy_desc.name);

	return 0;
}
/************************psy end**************************************/


static int bq25960_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct bq25960 *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret;

	lx_info("---->\n");
	sc =  devm_kzalloc(&client->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc) {
		ret = -ENOMEM;
		goto err_1;
	}
	sc->dev = &client->dev;
	sc->client = client;

	mutex_init(&sc->i2c_rw_lock);
	mutex_init(&sc->data_lock);
	mutex_init(&sc->charging_disable_lock);

	sc->is_bq25960 = true;

	ret = bq25960_detect_device(sc);

	if (ret) {
		lx_err("No bq25960 device found!\n");
		return -ENODEV;
	}

	i2c_set_clientdata(client, sc);
	bq25960_create_device_node(&(client->dev));

	match = of_match_node(bq25960_charger_match_table, node);
	if (match == NULL) {
		lx_err("device tree match not found!\n");
		return -ENODEV;
	}
/*
	bq25960_get_work_mode(sc, &sc->mode);

	if (sc->mode !=  *(int *)match->data) {
		lx_err("device operation mode mismatch with dts configuration\n");
		return -EINVAL;
	}
*/

	sc->mode =  *(int *)match->data;
	ret = bq25960_parse_dt(sc, &client->dev);
	if (ret)
		return -EIO;

	ret = bq25960_init_device(sc);
	if (ret) {
		lx_err("Failed to init device\n");
		return ret;
	}

	if (ret)
		return ret;

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, bq25960_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"bq25960 charger irq", sc);
		if (ret < 0) {
			lx_err("request irq for irq=%d failed, ret =%d\n",
							client->irq, ret);
			goto err_1;
		}
		enable_irq_wake(client->irq);
	}

	device_init_wakeup(sc->dev, 1);
	determine_initial_status(sc);

	if (sc->mode == BQ25960_ROLE_MASTER) {
		sc->master_cp_chg = chargerpump_register("master_cp_chg",
								sc->dev, &bq25960_ops, sc);
		if (IS_ERR_OR_NULL(sc->master_cp_chg)) {
			ret = PTR_ERR(sc->master_cp_chg);
			dev_err(sc->dev, "Fail to register master_cp_chg!\n");
			goto err_1;
		}
	} else {
		sc->slave_cp_chg = chargerpump_register("slave_cp_chg",
								sc->dev, &bq25960_ops, sc);
		if (IS_ERR_OR_NULL(sc->slave_cp_chg)) {
			ret = PTR_ERR(sc->slave_cp_chg);
			dev_err(sc->dev, "Fail to register slave_cp_chg!\n");
			goto err_1;
		}
	}
	bq25960_write_byte(sc, BQ25960_REG_0E, 0x6c);
	bq25960_write_byte(sc, BQ25960_REG_0A, 0x6c);
	bq25960_update_bits(sc, BQ25960_REG_10, 0x04, 0x04);

	INIT_DELAYED_WORK(&sc->set_rcp_work, bq25960h_set_rcp_workfunc);
	bq25960_set_otg_enable(sc->master_cp_chg,false);//init


	ret = bq25960_psy_register(sc);
	if (ret < 0) {
		dev_err(sc->dev, "%s psy register failed(%d)\n", __func__, ret);
		goto err_1;
	}
	hardwareinfo_set_prop(HARDWARE_SUB_CHARGER_MASTER, "bq25960_charger_pump");
	lx_err("%s:HARDWARE_SUB_CHARGER_MASTER is set!\n", __func__);

	lx_err("bq25960 probe successfully, Part Num:%d\n!", sc->part_no);

	return 0;

err_1:
	return ret;
}

static int bq25960_charger_remove(struct i2c_client *client)
{
	struct bq25960 *sc = i2c_get_clientdata(client);

	bq25960_enable_adc(sc, false);
	mutex_destroy(&sc->charging_disable_lock);
	mutex_destroy(&sc->data_lock);
	mutex_destroy(&sc->i2c_rw_lock);

	return 0;
}

static void bq25960_charger_shutdown(struct i2c_client *client)
{
	struct bq25960 *sc = i2c_get_clientdata(client);

	bq25960_enable_adc(sc, false);
}

static int bq25960_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	lx_info("Suspend successfully!");
	if (device_may_wakeup(dev))
		enable_irq_wake(client->irq);
	disable_irq(client->irq);

	return 0;
}
static int bq25960_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	lx_info("Resume successfully!");
	if (device_may_wakeup(dev))
		disable_irq_wake(client->irq);
	enable_irq(client->irq);

	return 0;
}

static const struct dev_pm_ops bq25960_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bq25960_suspend, bq25960_resume)
};

static const struct i2c_device_id bq25960_charger_id[] = {
	{"bq25960-standalone", BQ25960_STDALONE},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq25960_charger_id);

static struct i2c_driver bq25960_charger_driver = {
	.driver		= {
		.name	= "bq25960-charger",
		.owner	= THIS_MODULE,
		.of_match_table = bq25960_charger_match_table,
		.pm	= &bq25960_pm_ops,
	},
	.id_table	= bq25960_charger_id,

	.probe		= bq25960_charger_probe,
	.remove		= bq25960_charger_remove,
	.shutdown	= bq25960_charger_shutdown,
};

module_i2c_driver(bq25960_charger_driver);

MODULE_DESCRIPTION("TI BQ25960 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("tianye9@xiaomi.com");
