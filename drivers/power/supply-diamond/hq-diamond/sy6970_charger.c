/*
 * BQ2589x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include "sy6970_reg.h"
#include "sy6970_iio.h"
#include "hq_power_supply.h"
//#include <linux/hardware_info.h>
#include <../../../misc/mediatek/typec/diamond/tcpc/inc/tcpm.h>
#define USB_PD_MI_SVID			0x2717
#define USB_PD_HW_SVID			0x29CF
#define PD_WAIT_TIMEOUT (2 * HZ)


enum bq2589x_vbus_type {
	BQ2589X_VBUS_NONE,
	BQ2589X_VBUS_USB_SDP,
	BQ2589X_VBUS_USB_CDP, /*CDP for bq25890, Adapter for bq25892*/
	BQ2589X_VBUS_USB_DCP,
	BQ2589X_VBUS_MAXC,
	BQ2589X_VBUS_UNKNOWN,
	BQ2589X_VBUS_NONSTAND,
	BQ2589X_VBUS_OTG,
	BQ2589X_VBUS_TYPE_NUM,
};

enum bq2589x_part_no {
	SYV690 = 0x01,
	BQ25890 = 0x03,
	BQ25892 = 0x00,
	BQ25895 = 0x07,
	SC89890H = 0x04,
};


#define BQ2589X_STATUS_PLUGIN		0x0001
#define BQ2589X_STATUS_PG			0x0002
#define	BQ2589X_STATUS_CHARGE_ENABLE 0x0004
#define BQ2589X_STATUS_FAULT		0x0008

#define BQ2589X_STATUS_EXIST		0x0100

struct bq2589x_config {
	bool	enable_auto_dpdm;
/*	bool	enable_12v;*/

	int		battery_voltage_term;
	int		charge_current;
	int		input_current;

	bool	enable_term;
	int		term_current;
	int		charge_voltage;

	bool 	enable_ico;
	bool	use_absolute_vindpm;
	bool	otg_status;
	int 	otg_vol;
	int 	otg_current;
};

struct bq2589x {
	struct device *dev;
	struct i2c_client *client;
	enum   bq2589x_part_no part_no;
	int    revision;

	unsigned int    status;
	int		vbus_type;
	int		charge_type;

	bool	enabled;
	bool	is_sc89890h;

	int		vbus_volt;
	int		vbat_volt;

	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;

	int		rsoc;
	struct	bq2589x_config	cfg;
	struct delayed_work irq_work;
	struct work_struct adapter_in_work;
	struct work_struct adapter_out_work;
	struct delayed_work monitor_work;
	struct delayed_work ico_work;
	struct delayed_work force_work;

	struct wakeup_source *wt_ws;
	int wakeup_flag;

	struct power_supply_desc usb;
	struct power_supply_desc wall;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *wall_psy;
	struct power_supply_config usb_cfg;
	struct power_supply_config wall_cfg;

	int old_type;
	int otg_gpio;
	int irq_gpio;
	int usb_switch1;
	int usb_switch2;
	int usb_switch_flag;
	int usb_swtich_status;
	int hz_flag;
	int stop_rerun;
	int curr_flag;
	bool force_exit_flag;
	struct delayed_work tcp_work;
	struct notifier_block tcp_nb;
	struct tcpc_device *tcpc;
	int pd_status;
	wait_queue_head_t pd_wait;
	bool pd_flag;
	bool can_bc12;
	bool bc12_done;
	bool usb_flag;
	bool resume_completed;
	bool poweroffchg_flag;
	bool enterBc12Flg;
	bool isEnableCpPd;
	bool isHw22p5Flg;
	int old_charge_type;
	bool isFloatFlg;
	bool isMiSvidFlg;
	bool shutdown_flag;
};

static struct bq2589x *g_bq;

static DEFINE_MUTEX(bq2589x_i2c_lock);

static int bq2589x_read_byte(struct bq2589x *bq, u8 *data, u8 reg)
{
	int ret;
	int count = 3;

	if(!bq->resume_completed)
		return 0;

	while(1) {
		mutex_lock(&bq2589x_i2c_lock);
		ret = i2c_smbus_read_byte_data(bq->client, reg);
		if (ret < 0 && count > 1) {
			dev_err(bq->dev, "failed to read 0x%.2x\n", reg);
			mutex_unlock(&bq2589x_i2c_lock);
			count--;
		} else {
			*data = (u8)ret;
			mutex_unlock(&bq2589x_i2c_lock);
			break;
		}
		udelay(200);
	}
	return 0;
}

static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data)
{
	int ret;

	if(!bq->resume_completed)
		return 0;

	mutex_lock(&bq2589x_i2c_lock);
	ret = i2c_smbus_write_byte_data(bq->client, reg, data);
	mutex_unlock(&bq2589x_i2c_lock);
	return ret;
}

static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = bq2589x_read_byte(bq, &tmp, reg);

	if (ret)
		return ret;

	tmp &= ~mask;
	tmp |= data & mask;

	return bq2589x_write_byte(bq, reg, tmp);
}


static enum bq2589x_vbus_type bq2589x_get_vbus_type(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0)
		return 0;
	val &= BQ2589X_VBUS_STAT_MASK;
	val >>= BQ2589X_VBUS_STAT_SHIFT;

	return val;
}

static int  bq2589x_get_chg_type(struct bq2589x *bq)
{
	u8 val;
	int type, real_type;

	val = bq2589x_get_vbus_type(bq);
	type = (int)val;

	if(!bq->is_sc89890h && bq->old_type == BQ2589X_VBUS_UNKNOWN) {
		type = BQ2589X_VBUS_NONSTAND;
	}

	if (type == BQ2589X_VBUS_USB_SDP)
		real_type = POWER_SUPPLY_TYPE_USB;
	else if (type == BQ2589X_VBUS_USB_CDP)
		real_type = POWER_SUPPLY_TYPE_USB_CDP;
	else if (bq->pd_flag == true)
		real_type = POWER_SUPPLY_TYPE_USB_DCP;
	else if (type == BQ2589X_VBUS_USB_DCP)
		real_type = POWER_SUPPLY_TYPE_USB_DCP;
	else if (type == BQ2589X_VBUS_NONSTAND || type == BQ2589X_VBUS_UNKNOWN)
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	else if (type == BQ2589X_VBUS_MAXC) {
		real_type = POWER_SUPPLY_TYPE_USB_HVDCP;
	} else
		real_type = POWER_SUPPLY_TYPE_UNKNOWN;

	if((!bq->is_sc89890h) && (bq->old_charge_type == POWER_SUPPLY_TYPE_USB_DCP)
		&& (real_type == POWER_SUPPLY_TYPE_UNKNOWN) && (bq->enterBc12Flg)) {
		real_type = POWER_SUPPLY_TYPE_USB_DCP;
		bq->isHw22p5Flg = true;
	}
	if((bq->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT)
		&& (real_type == POWER_SUPPLY_TYPE_UNKNOWN) && (bq->enterBc12Flg)) {
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		bq->isFloatFlg = true;
	}

	bq->charge_type = real_type;

	return real_type;
}


static int bq2589x_enable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_ENABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);

}

static int bq2589x_disable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_DISABLE << BQ2589X_OTG_CONFIG_SHIFT;

	if (!bq)
		return 0;

	if(bq->shutdown_flag)
		return 0;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
							   BQ2589X_OTG_CONFIG_MASK, val);

}

static int bq2589x_set_otg_volt(struct bq2589x *bq, int volt)
{
	u8 val = 0;
/*m6 charge add code start*/	
	if (bq->is_sc89890h) {
		if (volt < SC89890H_BOOSTV_BASE)
			volt = SC89890H_BOOSTV_BASE;
		if (volt > SC89890H_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * SC89890H_BOOSTV_LSB)
			volt = SC89890H_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * SC89890H_BOOSTV_LSB;

		val = ((volt - SC89890H_BOOSTV_BASE) / SC89890H_BOOSTV_LSB) << BQ2589X_BOOSTV_SHIFT;
	} else {
		if (volt < BQ2589X_BOOSTV_BASE)
			volt = BQ2589X_BOOSTV_BASE;
		if (volt > BQ2589X_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * BQ2589X_BOOSTV_LSB)
			volt = BQ2589X_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * BQ2589X_BOOSTV_LSB;

		val = ((volt - BQ2589X_BOOSTV_BASE) / BQ2589X_BOOSTV_LSB) << BQ2589X_BOOSTV_SHIFT;
	}
/*m6 charge add code end*/
	return bq2589x_update_bits(bq, BQ2589X_REG_0A, BQ2589X_BOOSTV_MASK, val);

}

static int bq2589x_set_otg_current(struct bq2589x *bq, int curr)
{
	u8 temp;

	if (curr <= 500)
		temp = BQ2589X_BOOST_LIM_500MA;
	else if (curr > 500 && curr <= 800)
		temp = BQ2589X_BOOST_LIM_700MA;
	else if (curr > 800 && curr <= 1200)
		temp = BQ2589X_BOOST_LIM_1100MA;
	else if (curr > 1200 && curr <= 1400)
		temp = BQ2589X_BOOST_LIM_1300MA;
	else if (curr > 1400 && curr <= 1700)
		temp = BQ2589X_BOOST_LIM_1600MA;
	else if (curr > 1700 && curr <= 1900)
		temp = BQ2589X_BOOST_LIM_1800MA;
	else if (curr > 1900 && curr <= 2200)
		temp = BQ2589X_BOOST_LIM_2100MA;
	else if (curr > 2200 && curr <= 2300)
		temp = BQ2589X_BOOST_LIM_2400MA;
	else
		temp = BQ2589X_BOOST_LIM_2400MA;

	pr_err("bq2589x_set_otg_current cur = %d", curr);
	return bq2589x_update_bits(bq, BQ2589X_REG_0A, BQ2589X_BOOST_LIM_MASK, temp << BQ2589X_BOOST_LIM_SHIFT);
}

static int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;

	if (!bq)
		return 0;

	if(bq->shutdown_flag)
		return 0;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status |= BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}

static int bq2589x_disable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_DISABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if (ret == 0)
		bq->status &= ~BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}

/* interfaces that can be called by other module */
int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	if (!bq)
		return 0;

	if(bq->shutdown_flag)
		return 0;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_02);
	if (ret < 0) {
		dev_err(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT) == BQ2589X_ADC_CONTINUE_ENABLE)
		return 0; /*is doing continuous scan*/
	if (oneshot) {
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_START_MASK, BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	} else
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,  BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}

int bq2589x_adc_stop(struct bq2589x *bq)
{
	if (!bq)
		return 0;

	if(bq->shutdown_flag)
		return 0;

	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK, BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
}

int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0E);
	if (ret < 0) {
		dev_err(bq->dev, "read battery voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_BATV_BASE + ((val & BQ2589X_BATV_MASK) >> BQ2589X_BATV_SHIFT) * BQ2589X_BATV_LSB ;
		return volt;
	}
}

#if 0
int bq2589x_adc_read_sys_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0F);
	if (ret < 0) {
		dev_err(bq->dev, "read system voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_SYSV_BASE + ((val & BQ2589X_SYSV_MASK) >> BQ2589X_SYSV_SHIFT) * BQ2589X_SYSV_LSB ;
		return volt;
	}
}
#endif

int bq2589x_adc_read_vbus_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_11);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		return volt;
	}
}

int bq2589x_read_vindpm_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0D);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		return volt;
	}
}

#if 0
int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_10);
	if (ret < 0) {
		dev_err(bq->dev, "read temperature failed :%d\n", ret);
		return ret;
	} else{
		temp = BQ2589X_TSPCT_BASE + ((val & BQ2589X_TSPCT_MASK) >> BQ2589X_TSPCT_SHIFT) * BQ2589X_TSPCT_LSB ;
		return temp;
	}
}
#endif

int bq2589x_adc_read_charge_current(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_12);
	if (ret < 0) {
		dev_err(bq->dev, "read charge current failed :%d\n", ret);
		return ret;
	} else{
		volt = (int)(BQ2589X_ICHGR_BASE + ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT) * BQ2589X_ICHGR_LSB) ;
		return volt;
	}
}

int bq2589x_set_charge_current(struct bq2589x *bq, int curr)
{
	u8 ichg;
/*m6 charge add code start*/	
	if (bq->is_sc89890h)
		ichg = (curr - SC89890H_ICHG_BASE)/SC89890H_ICHG_LSB;
	else
		ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
/*m6 charge add code end*/
	return bq2589x_update_bits(bq, BQ2589X_REG_04, BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);

}

int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;

/*m6 charge add code start*/	
	if (bq->is_sc89890h)
		iterm = (curr - SC89890H_ITERM_BASE) / SC89890H_ITERM_LSB;
	else
		iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;
/*m6 charge add code end*/
	return bq2589x_update_bits(bq, BQ2589X_REG_05, BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);
}

int bq2589x_set_prechg_current(struct bq2589x *bq, int curr)
{
	u8 iprechg;

/*m6 charge add code start*/	
	if (bq->is_sc89890h)
		iprechg = (curr - SC89890H_IPRECHG_BASE) / SC89890H_IPRECHG_LSB;
	else
		iprechg = (curr - BQ2589X_IPRECHG_BASE) / BQ2589X_IPRECHG_LSB;
/*m6 charge add code end*/
	return bq2589x_update_bits(bq, BQ2589X_REG_05, BQ2589X_IPRECHG_MASK, iprechg << BQ2589X_IPRECHG_SHIFT);
}

int bq2589x_set_chargevoltage(struct bq2589x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2589X_VREG_BASE)/BQ2589X_VREG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_06, BQ2589X_VREG_MASK, val << BQ2589X_VREG_SHIFT);
}

int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt)
{
	u8 val;
	val = (volt - BQ2589X_VINDPM_BASE) / BQ2589X_VINDPM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_0D, BQ2589X_VINDPM_MASK, val << BQ2589X_VINDPM_SHIFT);
}

int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr)
{
	u8 val;

	val = (curr - BQ2589X_IINLIM_BASE) / BQ2589X_IINLIM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_IINLIM_MASK, val << BQ2589X_IINLIM_SHIFT);
}

int bq2589x_set_vindpm_offset(struct bq2589x *bq, int offset)
{
	u8 val;

	val = (offset - BQ2589X_VINDPMOS_BASE)/BQ2589X_VINDPMOS_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_01, BQ2589X_VINDPMOS_MASK, val << BQ2589X_VINDPMOS_SHIFT);
}

int bq2589x_get_charging_status(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		dev_err(bq->dev, "%s Failed to read register 0x0b:%d\n", __func__, ret);
		return ret;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	return val;
}

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, val);
}

int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout)
{
	u8 val = timeout << BQ2589X_WDT_SHIFT;
	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, val);
}

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_WDT_RESET_MASK, val);
}


static  int bq2589x_is_dpdm_done(struct bq2589x *bq,int *done)
{
	int ret = 0;
	u8 data=0;
	ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_02);
	//pr_err("%s data(0x%x)\n",  __func__, data);
	data &= (BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT);
	*done = (data >> BQ2589X_FORCE_DPDM_SHIFT);
	 return ret;
}

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_FORCE_DPDM_MASK, val);
}

void bq2589x_force_dpdm_done(struct bq2589x *bq)
{
	int retry = 0;
	int bc_count = 200;
	int done = 1;

	bq2589x_force_dpdm(bq);
	while(retry++ < bc_count){
		bq2589x_is_dpdm_done(bq,&done);
		msleep(20);
		if(!done) //already known charger type
			break;
	}
}

int bq2589x_disable_hvdcp(struct bq2589x *bq)
{
	u8 val = BQ2589X_HVDCP_DISABLE << BQ2589X_HVDCPEN_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_HVDCPEN_MASK, val);
}

int bq2589x_enable_hvdcp(struct bq2589x *bq)
{
	u8 val = BQ2589X_HVDCP_ENABLE << BQ2589X_HVDCPEN_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_HVDCPEN_MASK, val);
}

int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14, BQ2589X_RESET_MASK, val);
	return ret;
}
#if 0
int bq2589x_enter_ship_mode(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_BATFET_OFF << BQ2589X_BATFET_DIS_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_BATFET_DIS_MASK, val);
	return ret;

}
#endif

int bq2589x_enter_hiz_mode(struct bq2589x *bq)
{
	u8 val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	if (!bq)
		return 0;

	if(bq->shutdown_flag)
		return 0;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}

int bq2589x_get_hiz_mode(struct bq2589x *bq, u8 *state)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_00);
	if (ret)
		return ret;
	*state = (val & BQ2589X_ENHIZ_MASK) >> BQ2589X_ENHIZ_SHIFT;

	return 0;
}

#if 0
int bq2589x_pumpx_enable(struct bq2589x *bq, int enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_PUMPX_ENABLE << BQ2589X_EN_PUMPX_SHIFT;
	else
		val = BQ2589X_PUMPX_DISABLE << BQ2589X_EN_PUMPX_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_04, BQ2589X_EN_PUMPX_MASK, val);

	return ret;
}

int bq2589x_pumpx_increase_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_UP << BQ2589X_PUMPX_UP_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_UP_MASK, val);

	return ret;

}

int bq2589x_pumpx_increase_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if (ret)
		return ret;

	if (val & BQ2589X_PUMPX_UP_MASK)
		return 1;   /* not finished*/
	else
		return 0;   /* pumpx up finished*/

}

int bq2589x_pumpx_decrease_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_DOWN << BQ2589X_PUMPX_DOWN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_DOWN_MASK, val);

	return ret;

}

int bq2589x_pumpx_decrease_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if (ret)
		return ret;

	if (val & BQ2589X_PUMPX_DOWN_MASK)
		return 1;   /* not finished*/
	else
		return 0;   /* pumpx down finished*/

}

static int bq2589x_read_idpm_limit(struct bq2589x *bq)
{
	uint8_t val;
	int curr;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_13);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		curr = BQ2589X_IDPM_LIM_BASE + ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB ;
		return curr;
	}
}
#endif

static int bq2589x_force_ico(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_FORCE_ICO << BQ2589X_FORCE_ICO_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_FORCE_ICO_MASK, val);

	return ret;
}

static int bq2589x_check_force_ico_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_14);
	if (ret)
		return ret;

	if (val & BQ2589X_ICO_OPTIMIZED_MASK)
		return 1;  /*finished*/
	else
		return 0;   /* in progress*/
}

static int bq2589x_enable_term(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_TERM_ENABLE << BQ2589X_EN_TERM_SHIFT;
	else
		val = BQ2589X_TERM_DISABLE << BQ2589X_EN_TERM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TERM_MASK, val);

	return ret;
}

static int bq2589x_enable_auto_dpdm(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_AUTO_DPDM_ENABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;
	else
		val = BQ2589X_AUTO_DPDM_DISABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_AUTO_DPDM_EN_MASK, val);

	return ret;

}

static int bq2589x_use_absolute_vindpm(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_FORCE_VINDPM_ENABLE << BQ2589X_FORCE_VINDPM_SHIFT;
	else
		val = BQ2589X_FORCE_VINDPM_DISABLE << BQ2589X_FORCE_VINDPM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_0D, BQ2589X_FORCE_VINDPM_MASK, val);

	return ret;

}

static int bq2589x_enable_ico(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_ICO_ENABLE << BQ2589X_ICOEN_SHIFT;
	else
		val = BQ2589X_ICO_DISABLE << BQ2589X_ICOEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_ICOEN_MASK, val);

	return ret;

}

static bool bq2589x_is_charge_done(struct bq2589x *bq)
{
	int ret;
	u8 val;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if (ret < 0) {
		dev_err(bq->dev, "%s:read REG0B failed :%d\n", __func__, ret);
		return false;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;

	return (val == BQ2589X_CHRG_STAT_CHGDONE);
}

static void bq2589x_dump_regs(struct bq2589x *bq)
{
	int addr, ret;
	u8 val[0x15];

	if(!bq->resume_completed)
		return;

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(bq, &val[addr], addr);
		if (ret != 0)
			pr_err("%s:read failed :%d\n", __func__, ret);
	}
	pr_err("bq2589x_dump_regs:0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x,0x%.2x\n",
		val[0],val[1],val[2],val[3],val[4],val[5],val[6],val[7],val[8],val[9],val[0xa],val[0xb],val[0xc],val[0xd],val[0xe],val[0xf],val[0x10],
		val[0x11],val[0x12],val[0x13],val[0x14]);
}

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;

	/*common initialization*/
	bq2589x_disable_watchdog_timer(bq);

	bq2589x_enable_auto_dpdm(bq, bq->cfg.enable_auto_dpdm);
	bq2589x_enable_term(bq, bq->cfg.enable_term);
	bq2589x_enable_ico(bq, bq->cfg.enable_ico);
	/*force use absolute vindpm if auto_dpdm not enabled*/
	if (!bq->cfg.enable_auto_dpdm)
		bq->cfg.use_absolute_vindpm = true;

	bq->cfg.use_absolute_vindpm = false;
	bq2589x_use_absolute_vindpm(bq, bq->cfg.use_absolute_vindpm);

	if (!bq->is_sc89890h) {
		ret = bq2589x_set_vindpm_offset(bq, 600);
		if (ret < 0) {
			dev_err(bq->dev, "%s:Failed to set vindpm offset:%d\n", __func__, ret);
			return ret;
		}
	}

	ret = bq2589x_set_term_current(bq, bq->cfg.term_current);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set termination current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_chargevoltage(bq, bq->cfg.charge_voltage);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge voltage:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_charge_current(bq, bq->cfg.charge_current);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_otg_volt(bq, bq->cfg.otg_vol);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge voltage:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_set_otg_current(bq, bq->cfg.otg_current);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		return ret;
	}

	ret = bq2589x_enable_charger(bq);
	if (ret < 0) {
		dev_err(bq->dev, "%s:Failed to enable charger:%d\n", __func__, ret);
		return ret;
	}

	bq->curr_flag = 0;
	//bq2589x_adc_start(bq, false);
	bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
		BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
	bq2589x_update_bits(bq, BQ2589X_REG_02, 0x8, 0 << 3);
	if (bq->is_sc89890h) {
		bq2589x_update_bits(bq, BQ2589X_REG_01, 0x2, 0 << 1);
		bq2589x_update_bits(bq, BQ2589X_REG_02, 0x4, 0 << 2);
	} else
		bq2589x_update_bits(bq, BQ2589X_REG_02, 0x4, 0 << 2);

	//bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK, 2 << 5);
	//bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_VCLAMP_MASK, 1 << 2);
	//bq2589x_set_watchdog_timer(bq, BQ2589X_WDT_160S);
	//bq2589x_reset_watchdog_timer(bq);
	//bq2589x_enable_ico(bq, false);

	return ret;
}


static int bq2589x_charge_status(struct bq2589x *bq)
{
	u8 val = 0;

	bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	switch (val) {
	case BQ2589X_CHRG_STAT_FASTCHG:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	case BQ2589X_CHRG_STAT_PRECHG:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	case BQ2589X_CHRG_STAT_CHGDONE:
	case BQ2589X_CHRG_STAT_IDLE:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	default:
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static ssize_t bq2589x_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "Charger 1");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(g_bq, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[0x%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static DEVICE_ATTR(registers, S_IRUGO, bq2589x_show_registers, NULL);

static struct attribute *bq2589x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2589x_attr_group = {
	.attrs = bq2589x_attributes,
};


static int bq2589x_parse_dt(struct device *dev, struct bq2589x *bq)
{
	int ret;
	struct device_node *np = dev->of_node;

	bq->cfg.enable_auto_dpdm = of_property_read_bool(np, "ti,bq2589x,enable-auto-dpdm");
	bq->cfg.enable_term = of_property_read_bool(np, "ti,bq2589x,enable-termination");
	bq->cfg.enable_ico = of_property_read_bool(np, "ti,bq2589x,enable-ico");
	bq->cfg.use_absolute_vindpm = of_property_read_bool(np, "ti,bq2589x,use-absolute-vindpm");

	ret = of_property_read_u32(np, "ti,bq2589x,otg_vol", &bq->cfg.otg_vol);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,otg_current", &bq->cfg.otg_current);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-voltage",&bq->cfg.charge_voltage);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,charge-current",&bq->cfg.charge_current);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2589x,term-current",&bq->cfg.term_current);
	if (ret)
		return ret;

	/*bq->otg_gpio = of_get_named_gpio(np, "otg-gpio", 0);
        if (ret < 0) {
                pr_err("%s no otg_gpio info\n", __func__);
	};*/

	bq->irq_gpio = of_get_named_gpio(np, "intr-gpio", 0);
        if (ret < 0) {
                pr_err("%s no intr_gpio info\n", __func__);
                return ret;
        } else {
                pr_err("%s intr_gpio infoi %d\n", __func__, bq->irq_gpio);
	}
	/*bq->usb_switch1 = of_get_named_gpio(np, "usb-switch1", 0);
        if (ret < 0) {
                pr_err("%s no usb-switch1 info\n", __func__);
                return ret;
	}

	bq->usb_switch2 = of_get_named_gpio(np, "usb-switch2", 0);
        if (ret < 0) {
                pr_err("%s no usb-switch2 info\n", __func__);
                return ret;
	}*/
	return 0;
}

/*static int bq2589x_usb_switch(struct bq2589x *bq, bool en)
{
	gpio_direction_output(bq->usb_switch1, en);
	gpio_direction_output(bq->usb_switch2, en);
	bq->usb_swtich_status = en;
	pr_err("bq2589x_usb_switch %d\n", en);
	return 0;
}*/


static int bq2589x_detect_device(struct bq2589x *bq)
{
	int ret;
	u8 data;

	ret = bq2589x_read_byte(bq, &data, BQ2589X_REG_14);
	if (ret == 0) {
		bq->part_no = (data & BQ2589X_PN_MASK) >> BQ2589X_PN_SHIFT;
		bq->revision = (data & BQ2589X_DEV_REV_MASK) >> BQ2589X_DEV_REV_SHIFT;
	}

	return ret;
}

static void bq2589x_adjust_absolute_vindpm(struct bq2589x *bq)
{
	u16 vbus_volt;
	u16 vindpm_volt;
	int ret;

	ret = bq2589x_disable_charger(bq);	
	if (ret < 0) {
		dev_err(bq->dev,"%s:failed to disable charger\n",__func__);
		/*return;*/
	}
	/* wait for new adc data */
	msleep(1000);
	vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	ret = bq2589x_enable_charger(bq);
	if (ret < 0) {
		dev_err(bq->dev, "%s:failed to enable charger\n",__func__);
		return;
	}
/*
	if (vbus_volt < 6000)
		vindpm_volt = vbus_volt - 600;
	else
		vindpm_volt = vbus_volt - 1200;
*/
	if (bq->vbus_type != BQ2589X_VBUS_USB_SDP)
		vindpm_volt = 4500;
	else
		vindpm_volt = 4400;
	ret = bq2589x_set_input_volt_limit(bq, vindpm_volt);
	if (ret < 0)
		dev_err(bq->dev, "%s:Set absolute vindpm threshold %d Failed:%d\n", __func__, vindpm_volt, ret);
	else
		dev_info(bq->dev, "%s:Set absolute vindpm threshold %d successfully\n", __func__, vindpm_volt);

}

static void bq2589x_adapter_in_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_in_work);
	int ret;
	union power_supply_propval val = {0,};

	bq2589x_adc_start(bq, false);
	if (bq->vbus_type == BQ2589X_VBUS_MAXC) {
		bq2589x_set_input_volt_limit(bq, 8450);
		dev_info(bq->dev, "%s:HVDCP or Maxcharge adapter plugged in\n", __func__);
		ret = bq2589x_set_input_current_limit(bq, 2000);
		if (ret < 0) 
			dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		else
			dev_info(bq->dev, "%s: Set inputcurrent to %dmA successfully\n",__func__,bq->cfg.charge_current);
		schedule_delayed_work(&bq->ico_work, msecs_to_jiffies(500));
	} else if (bq->vbus_type == BQ2589X_VBUS_USB_DCP) {/* DCP, let's check if it is PE adapter*/
		ret = bq2589x_set_input_current_limit(bq, 2000);
		dev_info(bq->dev, "%s:usb dcp adapter plugged in\n", __func__);
		ret = bq2589x_set_charge_current(bq, 1000);
		if (ret < 0) 
			dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		else
			dev_info(bq->dev, "%s: Set input current to %dmA successfully\n",__func__,bq->cfg.charge_current);
	} else if (bq->vbus_type == BQ2589X_VBUS_USB_SDP) {
		dev_info(bq->dev, "%s:host SDP plugged in\n", __func__);
		ret = bq2589x_set_input_current_limit(bq, 512);
		//bq2589x_usb_switch(bq, false);
		bq2589x_set_chargevoltage(bq, 4450);
	} else if (bq->vbus_type == BQ2589X_VBUS_USB_CDP) {
		dev_info(bq->dev, "%s:host CDP plugged in\n", __func__);
		bq2589x_set_chargevoltage(bq, 4450);
		bq2589x_set_input_volt_limit(bq, 4600);
		if(bq->batt_psy)
			power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		if(val.intval == 100)
			ret = bq2589x_set_input_current_limit(bq, 500);
		else
			ret = bq2589x_set_input_current_limit(bq, 1500);
		if (ret < 0)
			dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		else
			dev_info(bq->dev, "%s: Set input current to %dmA successfully\n",__func__,1500);
		//bq2589x_usb_switch(bq, false);
	} else if (bq->vbus_type == BQ2589X_VBUS_UNKNOWN) {
		dev_info(bq->dev, "%s:host FLOAT plugged in\n", __func__);
		ret = bq2589x_set_input_current_limit(bq, 1000);
		if (ret < 0)
			dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		else
			dev_info(bq->dev, "%s: Set input current to %dmA successfully\n",__func__,1500);
		//bq2589x_usb_switch(bq, false);
	} else {
		dev_info(bq->dev, "%s:other adapter plugged in,vbus_type is %d\n", __func__, bq->vbus_type);
		ret = bq2589x_set_input_current_limit(bq, 500);
		if (ret < 0) 
			dev_err(bq->dev, "%s:Failed to set charge current:%d\n", __func__, ret);
		else
			dev_info(bq->dev, "%s: Set input current to %dmA successfully\n",__func__,1000);
		//bq2589x_usb_switch(bq, false);
		schedule_delayed_work(&bq->ico_work, 0);
	}

	if(bq->vbus_type == BQ2589X_VBUS_USB_SDP)
		bq2589x_adjust_absolute_vindpm(bq);

	power_supply_changed(bq->usb_psy);
	cancel_delayed_work_sync(&bq->monitor_work);
	schedule_delayed_work(&bq->monitor_work, 0);
}

static void bq2589x_adapter_out_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, adapter_out_work);
	int ret;

	ret = bq2589x_set_input_current_limit(bq, 500);
	ret = bq2589x_set_input_volt_limit(bq, 4400);
	if (ret < 0)
		dev_err(bq->dev,"%s:reset vindpm threshold to 4400 failed:%d\n",__func__,ret);
	else
		dev_info(bq->dev,"%s:reset vindpm threshold to 4400 successfully\n",__func__);

//	cancel_delayed_work_sync(&bq->monitor_work);
}

static void bq2589x_ico_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, ico_work.work);
	int ret;
	int idpm;
	u8 status;
	static bool ico_issued;

	if (!ico_issued) {
		ret = bq2589x_force_ico(bq);
		if (ret < 0) {
			schedule_delayed_work(&bq->ico_work, HZ); /* retry 1 second later*/
			dev_info(bq->dev, "%s:ICO command issued failed:%d\n", __func__, ret);
		} else {
			ico_issued = true;
			schedule_delayed_work(&bq->ico_work, 3 * HZ);
			dev_info(bq->dev, "%s:ICO command issued successfully\n", __func__);
		}
	} else {
		ico_issued = false;
		ret = bq2589x_check_force_ico_done(bq);
		if (ret) {/*ico done*/
			ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
			if (ret == 0) {
				idpm = ((status & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB + BQ2589X_IDPM_LIM_BASE;
				dev_info(bq->dev, "%s:ICO done, result is:%d mA\n", __func__, idpm);
			}
		}
	}
}

static void bq2589x_force_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, force_work.work);
	u8 val = 0;

	bq->force_exit_flag = true;
	val = bq2589x_get_vbus_type(bq);
	dev_err(bq->dev, "%s:val=%d,bq->usb_switch_flag=%d force_exit_flag %d\n",
				__func__,val,bq->usb_switch_flag,bq->force_exit_flag);
	if((val == BQ2589X_VBUS_UNKNOWN) || (val == BQ2589X_VBUS_NONSTAND)) {
		bq->old_type = BQ2589X_VBUS_UNKNOWN;
		//bq2589x_usb_switch(bq, true);
//		bq->usb_switch_flag = true;
		bq2589x_force_dpdm_done(bq);
		bq->old_type = BQ2589X_VBUS_NONE;
	}
	bq->force_exit_flag = false;
	dev_err(bq->dev, "%s:val=%d\n",	__func__,val);
}

static void bq2589x_monitor_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, monitor_work.work);
	u8 status = 0;
	int ret;
	int chg_current;
	int vindpm = 0;
	union power_supply_propval val = {0,};

//	bq2589x_reset_watchdog_timer(bq);
	bq2589x_dump_regs(bq);
	bq->vbus_volt = bq2589x_adc_read_vbus_volt(bq);
	bq->vbat_volt = bq2589x_adc_read_battery_volt(bq);
	chg_current = bq2589x_adc_read_charge_current(bq);

	if (!bq->curr_flag) {
		if (bq->vbus_type == BQ2589X_VBUS_MAXC) {
			bq2589x_set_input_volt_limit(bq, 8450);
			ret = bq2589x_set_charge_current(bq, 3010);
		} else if (bq->vbus_type == BQ2589X_VBUS_USB_DCP) {
			bq2589x_set_input_volt_limit(bq, 4500);
			ret = bq2589x_set_charge_current(bq, 2000);
		} else if (bq->vbus_type == BQ2589X_VBUS_USB_SDP || bq->vbus_type == BQ2589X_VBUS_UNKNOWN) {
			ret = bq2589x_set_charge_current(bq, 500);
			//if(bq->usb_swtich_status && bq->vbus_type == BQ2589X_VBUS_USB_SDP)
				//bq2589x_usb_switch(bq, false);
		} else if (bq->vbus_type == BQ2589X_VBUS_USB_CDP) {
			ret = bq2589x_set_charge_current(bq, 1500);
			//if(bq->usb_swtich_status)
				//bq2589x_usb_switch(bq, false);
		} else {
			ret = bq2589x_set_charge_current(bq, 1000);
		}
		bq->curr_flag = 1;
	}

	if(bq->batt_psy)
		ret = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (val.intval < 100)
		bq2589x_update_bits(bq, BQ2589X_REG_06, 1, 1);
	else
		bq2589x_update_bits(bq, BQ2589X_REG_06, 1, 0);
	if (val.intval < 100)
		bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK, 2 << 5);

	if (bq->vbus_volt >= 4800) {
		vindpm = bq2589x_read_vindpm_volt(bq);
	}
	dev_info(bq->dev, "%s:vbus volt:%d,vbat volt:%d,charge current:%d, temp %d, vindpm %d\n",
			__func__, bq->vbus_volt, bq->vbat_volt, chg_current, val.intval, vindpm);

	if (val.intval > 480){
		ret = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		if(val.intval > 4150000){
			dev_info(bq->dev, "%s: high temp, voltage %d disable charge\n", __func__,  val.intval);
			ret = bq2589x_disable_charger(bq);
		}
	}

	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_13);
	if (ret == 0 && (status & BQ2589X_VDPM_STAT_MASK))
		dev_info(bq->dev, "%s:VINDPM occurred\n", __func__);
	if (ret == 0 && (status & BQ2589X_IDPM_STAT_MASK))
		dev_info(bq->dev, "%s:IINDPM occurred\n", __func__);

	/* read temperature,or any other check if need to decrease charge current*/

	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(10000));
}

static int get_chg_boot_mode(void)
{
#if 0
	char *bootmode_string= NULL;
	char bootmode_start[32] = " ";
	int rc;

	bootmode_string = strstr(saved_command_line,"androidboot.mode=");
	pr_err("bootmode_string=%s\n",bootmode_string);
	if(bootmode_string != NULL){
		strncpy(bootmode_start, bootmode_string+17, 7);
		rc = strncmp(bootmode_start, "charger", 7);
		if(rc == 0){
			pr_err("Offcharger mode!\n");
			return 1;
		}
	}
	pr_err("get_chg_boot_mode 0\n");
#endif
	return 0;
}

static void bq2589x_charger_irq_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, irq_work.work);
	u8 status = 0;
	u8 fault = 0;
	u8 charge_status = 0;
	int ret;

	//bq2589x_set_watchdog_timer(bq, BQ2589X_WDT_160S);
	//bq2589x_reset_watchdog_timer(bq);
	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_0B);
	dev_err(bq->dev, "%s: is_sc89890h=%d,reg[0x%x] = 0x%x,bq->status=%d\n", __func__,bq->is_sc89890h, BQ2589X_REG_0B, status,bq->status);
	if (ret)
	    return;
	if (bq->is_sc89890h) {
		if((bq->isFloatFlg) && (bq->enterBc12Flg)) {
			if((bq->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT) && (bq->charge_type == POWER_SUPPLY_TYPE_USB_FLOAT)) {
				dev_err(bq->dev, "%s: USB_FLOAT !!!\n", __func__);
				if(!(status & BQ2589X_PG_STAT_MASK)) {
					if(!(bq->status & BQ2589X_STATUS_PG)) {
						bq->status |= BQ2589X_STATUS_PLUGIN;
						bq->status |= BQ2589X_STATUS_PG;
					}
					bq->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
				}
			}
			if((bq->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT) && (bq->charge_type == POWER_SUPPLY_TYPE_USB_DCP)) {
				dev_err(bq->dev, "%s: USB_FLOAT->DCP !!!\n", __func__);
				if(!(status & BQ2589X_PG_STAT_MASK)) {
					if(!(bq->status & BQ2589X_STATUS_PG)) {
						bq->status |= BQ2589X_STATUS_PLUGIN;
						bq->status |= BQ2589X_STATUS_PG;
					}
				}
				bq->old_charge_type = bq->charge_type;
			}
		} else if (!(status & BQ2589X_PG_STAT_MASK) && !(bq->status & BQ2589X_STATUS_PG)) {
			dev_err(bq->dev, "%s: charge_type=%d,%d,%d,%d\n", __func__, bq->charge_type,bq->old_charge_type,bq->isFloatFlg,bq->enterBc12Flg);
			if((bq->charge_type == POWER_SUPPLY_TYPE_USB_FLOAT) && (bq->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT)) {
				if((!bq->isFloatFlg) && (!bq->enterBc12Flg))
					return;
				bq->status |= BQ2589X_STATUS_PLUGIN;
				bq->status |= BQ2589X_STATUS_PG;
				bq->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
			} else if((bq->charge_type == POWER_SUPPLY_TYPE_UNKNOWN) && (bq->old_charge_type == POWER_SUPPLY_TYPE_UNKNOWN)) {
				if((!bq->isFloatFlg) && (!bq->enterBc12Flg))
					return;
				bq->status |= BQ2589X_STATUS_PLUGIN;
				bq->status |= BQ2589X_STATUS_PG;
			} else
				return;
		}
	} else {
		if((bq->old_charge_type == POWER_SUPPLY_TYPE_USB_DCP) && (bq->isHw22p5Flg)
			&& (bq->charge_type == POWER_SUPPLY_TYPE_USB_DCP) && (bq->enterBc12Flg)) {
			dev_err(bq->dev, "%s: HW22.5W SYV!!!\n", __func__);
			if(!(status & BQ2589X_PG_STAT_MASK)) {
				if(!(bq->status & BQ2589X_STATUS_PG)) {
					bq->status |= BQ2589X_STATUS_PLUGIN;
					bq->status |= BQ2589X_STATUS_PG;
				}
				bq->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
			}
		}
		if((bq->old_charge_type == POWER_SUPPLY_TYPE_USB_DCP) && (bq->isHw22p5Flg)
			&& (bq->charge_type == POWER_SUPPLY_TYPE_USB_FLOAT) && (bq->enterBc12Flg)) {
			dev_err(bq->dev, "%s: DCP->FLOAT HW22.5W SYV!!!\n", __func__);
			if(!(status & BQ2589X_PG_STAT_MASK)) {
				if(!(bq->status & BQ2589X_STATUS_PG)) {
					bq->status |= BQ2589X_STATUS_PLUGIN;
					bq->status |= BQ2589X_STATUS_PG;
				}
			}
			bq->old_charge_type = bq->charge_type;
		}
		if((!(status & BQ2589X_PG_STAT_MASK)) && (bq->can_bc12) && (bq->bc12_done)) {
			dev_err(bq->dev, "%s: SYV690 abnormal!!!\n", __func__);
			if((!(bq->status & BQ2589X_STATUS_PLUGIN)) || (!(bq->status & BQ2589X_STATUS_PG))) {
				bq->status |= BQ2589X_STATUS_PLUGIN;
				bq->status |= BQ2589X_STATUS_PG;
			}
			bq->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
		if((bq->isFloatFlg) && (bq->enterBc12Flg)) {
			if((bq->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT) && (bq->charge_type == POWER_SUPPLY_TYPE_USB_FLOAT)) {
				dev_err(bq->dev, "%s: USB_FLOAT !!!\n", __func__);
				if(!(status & BQ2589X_PG_STAT_MASK)) {
					if(!(bq->status & BQ2589X_STATUS_PG)) {
						bq->status |= BQ2589X_STATUS_PLUGIN;
						bq->status |= BQ2589X_STATUS_PG;
					}
					bq->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
				}
			}
			if((bq->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT) && (bq->charge_type == POWER_SUPPLY_TYPE_USB_DCP)) {
				dev_err(bq->dev, "%s: USB_FLOAT->DCP !!!\n", __func__);
				if(!(status & BQ2589X_PG_STAT_MASK)) {
					if(!(bq->status & BQ2589X_STATUS_PG)) {
						bq->status |= BQ2589X_STATUS_PLUGIN;
						bq->status |= BQ2589X_STATUS_PG;
					}
				}
				bq->old_charge_type = bq->charge_type;
			}
		}
	}
    if ((status & BQ2589X_PG_STAT_MASK) && !(bq->status & BQ2589X_STATUS_PG)) {
		//bq->status |= BQ2589X_STATUS_PG;

		ret = wait_event_interruptible_timeout(bq->pd_wait, (bq->pd_flag || bq->usb_flag), (bq->poweroffchg_flag ? (PD_WAIT_TIMEOUT*4) : PD_WAIT_TIMEOUT));
		dev_err(bq->dev, "%s: ret %d, pd_status %d, pd_flag %d, usb_flag %d poweroffchg_flag %d\n",
			__func__, ret, bq->pd_status, bq->pd_flag, bq->usb_flag,bq->poweroffchg_flag);
	 	if (!ret) {
		    dev_err(bq->dev, "%s: wait pd+usb timeout!\n", __func__);
			if (bq->poweroffchg_flag) {
				bq->can_bc12 = false;
			} else {
				if (bq->pd_status == PD_CONNECT_PE_READY_SNK_PD30) {
					bq->can_bc12 = false;
					if(!bq->pd_flag)
						bq->pd_flag = true;
				} else if (bq->pd_status == PD_CONNECT_PE_READY_SNK_APDO) {
					bq->can_bc12 = false;
					if(!bq->pd_flag)
						bq->pd_flag = true;
				} else {
					bq->can_bc12 = true;
				}
			}
		} else {
			if (bq->usb_flag == true){
				dev_err(bq->dev, "Connect usb port !\n");
				bq->can_bc12 = true;
			} else {
				if (bq->pd_status == PD_CONNECT_PE_READY_SNK_PD30){
					bq->can_bc12 = false;
					if(!bq->pd_flag)
						bq->pd_flag = true;
					dev_err(bq->dev, "%s: Get Matched SVID, skip BC1.2\n", __func__);
				} else if (bq->pd_status == PD_CONNECT_PE_READY_SNK_APDO) {
					bq->can_bc12 = false;
					if(!bq->pd_flag)
						bq->pd_flag = true;
					dev_err(bq->dev, "%s: PD apdo, skip BC1.2\n", __func__);
				} else if (bq->pd_flag) {
					bq->can_bc12 = false;
					if(bq->enterBc12Flg)
						bq->enterBc12Flg = false;
					if(bq->isFloatFlg)
						bq->isFloatFlg = false;
					dev_err(bq->dev, "%s: pd_flag OK, skip BC1.2\n", __func__);
				} else {
					bq->can_bc12 = true;
					dev_err(bq->dev, "%s: Error SVID&APDO!!!\n", __func__);
				}
			}
		}

		ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_0B);
		if (ret)
			return;

		bq->vbus_type = (status & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT;
		dev_err(bq->dev, "%s: reg[0x%x] = 0x%x, vbus_type=%d,charge_type=%d\n", __func__, BQ2589X_REG_0B, status, bq->vbus_type,bq->charge_type);

		if(bq->can_bc12 && bq->isEnableCpPd) {
			bq->vbus_type = BQ2589X_VBUS_USB_DCP;
			bq->can_bc12 = false;
		}

		if(((bq->charge_type == POWER_SUPPLY_TYPE_USB_DCP) || (bq->vbus_type == BQ2589X_VBUS_UNKNOWN) || (bq->vbus_type == BQ2589X_VBUS_NONSTAND)
			|| (bq->vbus_type == BQ2589X_VBUS_NONE)) && (bq->can_bc12) && (!bq->bc12_done) && (!bq->enterBc12Flg)) {
			bq->enterBc12Flg = true;
			//if(!bq->is_sc89890h)
				bq->old_charge_type = bq->charge_type;
			bq2589x_enable_hvdcp(bq);
			dev_err(bq->dev, "%s: POWER_SUPPLY_TYPE_USB_DCP doing bc1.2!!!\n", __func__);
			//bq2589x_usb_switch(bq, false);
			msleep(10);
			//bq2589x_usb_switch(bq, true);
			msleep(20);
			bq2589x_force_dpdm_done(bq);
			bq->bc12_done = true;
		}
		if(((bq->charge_type == POWER_SUPPLY_TYPE_USB_DCP) || (bq->vbus_type == BQ2589X_VBUS_UNKNOWN) || (bq->vbus_type == BQ2589X_VBUS_NONSTAND)
			|| (bq->vbus_type == BQ2589X_VBUS_NONE)) && (bq->can_bc12) && (bq->bc12_done == false)) {
			dev_err(bq->dev, "%s: bc1.2 ongoing , break irq\n");
			return;
		}
	}

	/* Read STATUS and FAULT registers */
	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_0B);
	if (ret)
		return;

	ret = bq2589x_read_byte(bq, &fault, BQ2589X_REG_0C);
	dev_err(bq->dev, "%s: reg[0x%x] = 0x%x, reg[0x%x] = 0x%x\n", __func__, BQ2589X_REG_0B, status, BQ2589X_REG_0C, fault);
	if (ret)
		return;

	bq->vbus_type = (status & BQ2589X_VBUS_STAT_MASK) >> BQ2589X_VBUS_STAT_SHIFT;

	dev_err(bq->dev, "%s:type=%d,status=%d,charge_type=%d,can_bc12=%d,bc12_done=%d\n",
		__func__, bq->vbus_type, bq->status, bq->charge_type,bq->can_bc12,bq->bc12_done);

	if(((bq->vbus_type == BQ2589X_VBUS_UNKNOWN) || (bq->vbus_type == BQ2589X_VBUS_NONSTAND))) {

		if((bq->can_bc12) && (bq->bc12_done))

		{
			power_supply_changed(bq->usb_psy);
			if((!bq->hz_flag) && (!bq->force_exit_flag) && (!bq->stop_rerun)) {
				schedule_delayed_work(&bq->force_work, msecs_to_jiffies(4000));
				dev_err(bq->dev, "%s: force_work 4000\n", __func__);
			}
			return;
		}
	}

	if((bq->can_bc12 == false) && (status & BQ2589X_PG_STAT_MASK))
		bq->vbus_type = BQ2589X_VBUS_USB_DCP;

	if(!bq->is_sc89890h) {
		if (((bq->vbus_type == BQ2589X_VBUS_NONE) || (bq->vbus_type == BQ2589X_VBUS_OTG)) && (bq->status & BQ2589X_STATUS_PLUGIN)) {
			//bq2589x_usb_switch(bq, false);
			bq->usb_switch_flag = true;
			bq->old_type = BQ2589X_VBUS_NONE;
			bq->force_exit_flag = false;
			bq->curr_flag = 0;
			bq->stop_rerun = false;

			if (bq->can_bc12 == false)
				bq->vbus_type = BQ2589X_VBUS_NONE;
			bq->pd_flag = false;
			bq->can_bc12 = false;
			bq->bc12_done = false;
			bq->usb_flag = false;
			bq->enterBc12Flg = false;
			bq->isHw22p5Flg = false;
			bq->isFloatFlg = false;
			bq->isEnableCpPd = false;
			bq->isMiSvidFlg = false;
			bq2589x_disable_hvdcp(bq);

			dev_info(bq->dev, "%s:adapter removed\n", __func__);
			if (bq->vbus_type != BQ2589X_VBUS_OTG)
				bq2589x_adc_stop(bq);
			bq->status &= ~BQ2589X_STATUS_PLUGIN;
			cancel_delayed_work_sync(&bq->force_work);
			power_supply_changed(bq->usb_psy);
			schedule_work(&bq->adapter_out_work);
		} else if (bq->vbus_type != BQ2589X_VBUS_NONE && (bq->vbus_type != BQ2589X_VBUS_OTG) && !(bq->status & BQ2589X_STATUS_PLUGIN)) {
			dev_info(bq->dev, "%s:adapter plugged in\n", __func__);
			bq->status |= BQ2589X_STATUS_PLUGIN;

			if (bq->can_bc12 == false)
				bq->vbus_type = BQ2589X_VBUS_USB_DCP;

			bq2589x_use_absolute_vindpm(bq, true);
			bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
				BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
			power_supply_changed(bq->usb_psy);
			schedule_work(&bq->adapter_in_work);
		}
		if ((status & BQ2589X_PG_STAT_MASK) && !(bq->status & BQ2589X_STATUS_PG))
			bq->status |= BQ2589X_STATUS_PG;
		else if (!(status & BQ2589X_PG_STAT_MASK) && (bq->status & BQ2589X_STATUS_PG))
			bq->status &= ~BQ2589X_STATUS_PG;
	} else {
		if  (!(status & BQ2589X_PG_STAT_MASK) && (bq->status & BQ2589X_STATUS_PG)) {
			bq->force_exit_flag = false;
			bq->curr_flag = 0;
			bq->stop_rerun = false;

			if (bq->can_bc12 == false)
				bq->vbus_type = BQ2589X_VBUS_NONE;
			bq->pd_flag = false;
			bq->can_bc12 = false;
			bq->bc12_done = false;
			bq->usb_flag = false;
			bq->enterBc12Flg = false;
			bq->isFloatFlg = false;
			bq->isEnableCpPd = false;
			bq->isMiSvidFlg = false;
			bq2589x_disable_hvdcp(bq);

			dev_info(bq->dev, "%s:adapter removed\n", __func__);
			if (bq->vbus_type != BQ2589X_VBUS_OTG)
				bq2589x_adc_stop(bq);
			bq->status &= ~BQ2589X_STATUS_PG;
			bq->status &= ~BQ2589X_STATUS_PLUGIN;
			cancel_delayed_work_sync(&bq->force_work);
			power_supply_changed(bq->usb_psy);
			schedule_work(&bq->adapter_out_work);
		} else if ((status & BQ2589X_PG_STAT_MASK) && !(bq->status & BQ2589X_STATUS_PG)){
			dev_info(bq->dev, "%s:adapter plugged in\n", __func__);
			bq->status |= BQ2589X_STATUS_PLUGIN;
			bq->status |= BQ2589X_STATUS_PG;

			if (bq->can_bc12 == false)
				bq->vbus_type = BQ2589X_VBUS_USB_DCP;

			bq2589x_use_absolute_vindpm(bq, true);
			bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
				BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
			power_supply_changed(bq->usb_psy);
			schedule_work(&bq->adapter_in_work);
		}
	}

	if (fault && !(bq->status & BQ2589X_STATUS_FAULT))
		bq->status |= BQ2589X_STATUS_FAULT;
	else if (!fault && (bq->status & BQ2589X_STATUS_FAULT))
		bq->status &= ~BQ2589X_STATUS_FAULT;

	charge_status = (status & BQ2589X_CHRG_STAT_MASK) >> BQ2589X_CHRG_STAT_SHIFT;
	if (charge_status == BQ2589X_CHRG_STAT_IDLE) {
		if(bq->is_sc89890h) {
			bq->usb_switch_flag = true;
		}
		dev_info(bq->dev, "%s:not charging,bq->usb_switch_flag %d\n", __func__,bq->usb_switch_flag);
	} else if (charge_status == BQ2589X_CHRG_STAT_PRECHG)
		dev_info(bq->dev, "%s:precharging\n", __func__);
	else if (charge_status == BQ2589X_CHRG_STAT_FASTCHG)
		dev_info(bq->dev, "%s:fast charging\n", __func__);
	else if (charge_status == BQ2589X_CHRG_STAT_CHGDONE)
		dev_info(bq->dev, "%s:charge done!\n", __func__);

	if (fault) {
		dev_info(bq->dev, "%s:charge fault:%02x\n", __func__,fault);
		if(fault & 0x40) {
			dev_info(bq->dev, "%s:otg error ,boost ovp fault\n", __func__);
			bq2589x_reset_chip(bq);
			msleep(5);
			bq2589x_init_device(bq);
		}
		if(fault & 0x80){
			bq2589x_reset_chip(bq);
			msleep(5);
			bq2589x_init_device(bq);
		}
		if(fault & 0x10) {
			dev_info(bq->dev, "%s:input fault\n", __func__);
			bq2589x_reset_chip(bq);
			msleep(5);
			bq2589x_init_device(bq);
		}
	}
	bq2589x_get_chg_type(bq);
	dev_err(bq->dev, "%s: charge_type=%d\n", __func__, bq->charge_type);
}

static irqreturn_t bq2589x_charger_interrupt(int irq, void *data)
{
	struct bq2589x *bq = data;

	dev_info(bq->dev, "%s: charger device bq25890 irq detected, revision:%d,bq->charge_type %d\n",
			__func__, bq->revision, bq->charge_type);
	if (bq->usb_switch_flag && (bq->charge_type == POWER_SUPPLY_TYPE_UNKNOWN)) {
		//bq2589x_usb_switch(bq, true);
		bq->usb_switch_flag = false;
	}

	if(!bq->resume_completed)
		schedule_delayed_work(&bq->irq_work, msecs_to_jiffies(100));
	else

		schedule_delayed_work(&bq->irq_work, 0);
	return IRQ_HANDLED;
}

static int bq_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct bq2589x *bq = iio_priv(indio_dev);
	int rc = 0;
	u8 state;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_CHARGE_DONE:
		*val1 = bq2589x_is_charge_done(bq);
		break;
	case PSY_IIO_MAIN_CHAGER_HZ:
		rc = bq2589x_get_hiz_mode(bq, &state);
		if(!rc)
			*val1 = (int)state;
		break;
	case PSY_IIO_MAIN_INPUT_CURRENT_SETTLED:
		*val1 = bq->cfg.input_current;
		break;
	case PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED:
		*val1 = bq->cfg.charge_voltage;
		break;
	case PSY_IIO_MAIN_CHAGER_CURRENT:
		*val1 = bq2589x_adc_read_charge_current(bq);
		break;
	case PSY_IIO_CHARGING_ENABLED:
		*val1 = !!bq2589x_charge_status(bq);
		break;
	case PSY_IIO_SC_BUS_VOLTAGE:
		*val1 = bq2589x_adc_read_vbus_volt(bq);
		break;
	case PSY_IIO_SC_BATTERY_VOLTAGE:
		*val1 = bq2589x_adc_read_battery_volt(bq);
		break;
	case PSY_IIO_OTG_ENABLE:
		*val1 = bq->cfg.otg_status;
		break;
	case PSY_IIO_MAIN_CHAGER_TERM:
		*val1 = bq->cfg.term_current;
		break;
	case PSY_IIO_BATTERY_VOLTAGE_TERM:
		*val1 = bq->cfg.battery_voltage_term;
		break;
	case PSY_IIO_CHARGER_STATUS:
		*val1 = bq2589x_charge_status(bq);
		break;
	case PSY_IIO_CHARGE_TYPE:
		*val1 = bq2589x_get_chg_type(bq);
		//pr_err("%s: PSY_IIO_CHARGE_TYPE=%d\n", __func__, *val1);
		break;
	case PSY_IIO_ENABLE_CHAGER_TERM:
		*val1 = bq->cfg.enable_term;
		break;
	case PSY_IIO_ENABLE_CP_PD:
		*val1 = bq->isEnableCpPd;
		break;

	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int bq_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct bq2589x *bq = iio_priv(indio_dev);
	int rc = 0;
	union power_supply_propval val = {0,};

	switch (chan->channel) {
	case PSY_IIO_MAIN_CHAGER_HZ:
		if(val1 > 0) {
			bq2589x_enter_hiz_mode(bq);
			bq->hz_flag = false;
		} else {
			bq2589x_exit_hiz_mode(bq);
			bq->usb_switch_flag = false;
			if(val1 == 0 && bq->is_sc89890h)
				bq->hz_flag = true;
			else
				bq->hz_flag = false;
		}
		break;
	case PSY_IIO_MAIN_INPUT_CURRENT_SETTLED:
		if(val1 >= 3100)
			val1 = 3100;
		bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
			BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
		bq2589x_set_input_current_limit(bq, val1);
		break;
	case PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED:
		bq2589x_use_absolute_vindpm(bq, true);
		bq2589x_set_input_volt_limit(bq, val1);
		pr_err("bq2589x_use_absolute_vindpm %d\n", val1);
		break;
	case PSY_IIO_MAIN_CHAGER_CURRENT:
		if(val1 >= 3100)
			val1 = 3100;
		bq->cfg.input_current = val1;
		bq2589x_set_charge_current(bq, val1);
		if(val1 == 100)
			bq2589x_set_chargevoltage(bq, 4608);
		break;
	case PSY_IIO_CHARGING_ENABLED:
		if(val1)
			bq2589x_enable_charger(bq);
		else
			bq2589x_disable_charger(bq);
		break;
	case PSY_IIO_OTG_ENABLE:
		if(val1 > 1) {
			//bq2589x_usb_switch(g_bq, false);
			bq->stop_rerun = true;
		} else {
			bq->stop_rerun = false;
		//gpio_direction_output(g_bq->otg_gpio, val1);
		if(val1) {
			//bq2589x_usb_switch(g_bq, !val1);
			bq->cfg.otg_status = true;
			bq2589x_exit_hiz_mode(bq);
			bq2589x_disable_charger(bq);
			bq2589x_enable_otg(bq);
			bq2589x_set_otg_volt(bq, 5300);
			if(bq->batt_psy)
				power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
			if(val.intval > 5)
				bq2589x_set_otg_current(bq, 1800);
			else
				bq2589x_set_otg_current(bq, 1500);
			if(!bq->wakeup_flag) {
				__pm_stay_awake(bq->wt_ws);
				bq->wakeup_flag = 1;
				pr_err("otg workup\n");
			}
		} else {
		//	bq2589x_usb_switch(g_bq, !val1);
			bq->cfg.otg_status = false;
			bq2589x_disable_otg(bq);
			bq2589x_enable_charger(bq);
			if(bq->wakeup_flag) {
				__pm_relax(bq->wt_ws);
				bq->wakeup_flag = 0;
				pr_err("wt otg relax\n");
			}
		}
		}
		break;
	case PSY_IIO_MAIN_CHAGER_TERM:
		/*if(bq->batt_psy)
			power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
		if (bq->is_sc89890h && val.intval >= 100) {
			val1 -= 64;
		}*/
		bq2589x_set_term_current(bq, val1);
		break;
	case PSY_IIO_BATTERY_VOLTAGE_TERM:
		bq2589x_set_chargevoltage(bq, val1);
		break;
	case PSY_IIO_ENABLE_CHAGER_TERM:
		bq->cfg.enable_term = val1;
		bq2589x_enable_term(bq, bq->cfg.enable_term);
		break;
	case PSY_IIO_ENABLE_CP_PD:
		bq->isEnableCpPd = val1;
		pr_err("PSY_IIO_ENABLE_CP_PD %d\n", val1);
		break;

	default:
		pr_debug("Unsupported BQ25890 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int bq_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct bq2589x *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(bq25890_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info bq25890h_iio_info = {
	.read_raw	= bq_iio_read_raw,
	.write_raw	= bq_iio_write_raw,
	.of_xlate	= bq_iio_of_xlate,
};

static int bq_init_iio_psy(struct bq2589x *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(bq25890_iio_psy_channels);
	int rc, i;

	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &bq25890h_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->name = "bq25890h_chg";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;

	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = bq25890_iio_psy_channels[i].channel_num;
		chan->type = bq25890_iio_psy_channels[i].type;
		chan->datasheet_name =
			bq25890_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			bq25890_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			bq25890_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register QG IIO device, rc=%d\n", rc);

	pr_err("BQ25890H IIO device, rc=%d\n", rc);
	return rc;
}

static uint32_t bq2589x_get_adapter_svid(struct bq2589x *bq)
{
	struct pd_source_cap_ext cap_ext;
	struct tcpm_svid_list vid_list;
	uint32_t adapter_svid = 0;
	uint32_t adapter_id = 0;
	uint32_t adapter_fw_ver = 0;
	uint32_t adapter_hw_ver = 0;
	int ret;
	int i = 0;
	uint32_t pd_vdos[8];

	ret = tcpm_inquire_pd_partner_inform(bq->tcpc, pd_vdos);
	if (ret == TCPM_SUCCESS) {
		//pr_err("find adapter id success.\n");
		for (i = 0; i < 8; i++)
			pr_err("VDO[%d] : %08x\n", i, pd_vdos[i]);

		adapter_svid = pd_vdos[0] & 0x0000FFFF;
		adapter_id = pd_vdos[2] & 0x0000FFFF;
		pr_err("adapter_svid = %04x adapter_id = %08x\n", adapter_svid,adapter_id);

		ret = tcpm_inquire_pd_partner_svids(bq->tcpc, &vid_list);
		//pr_err("[%s] tcpm_inquire_pd_partner_svids, ret=%d!\n", __func__, ret);
		if (ret == TCPM_SUCCESS) {
			//pr_err("discover svid number is %d\n", vid_list.cnt);
			for (i = 0; i < vid_list.cnt; i++) {
				pr_err("SVID[%d] : %04x\n", i, vid_list.svids[i]);
				if (vid_list.svids[i] == USB_PD_MI_SVID)
					adapter_svid = USB_PD_MI_SVID;
				if (vid_list.svids[i] == USB_PD_HW_SVID)
					adapter_svid = USB_PD_HW_SVID;
			}
		}
	} else {
		ret = tcpm_dpm_pd_get_source_cap_ext(bq->tcpc,
			NULL, &cap_ext);
		if (ret == TCPM_SUCCESS) {
			adapter_svid = cap_ext.vid & 0x0000FFFF;
			adapter_id = cap_ext.pid & 0x0000FFFF;
			adapter_fw_ver = cap_ext.fw_ver & 0x0000FFFF;
			adapter_hw_ver = cap_ext.hw_ver & 0x0000FFFF;
			pr_err("adapter_svid=%04x,adapter_id=%08x,adapter_fw_ver=%08x,adapter_hw_ver=%08x\n",
				adapter_svid, adapter_id, adapter_fw_ver, adapter_hw_ver);
		} else {
			pr_err("[%s] get adapter message failed!\n", __func__);
			return 0;
		}
	}

	return adapter_svid;
}

static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
				     unsigned long event, void *data)
{
  	struct bq2589x *bq = container_of(nb, struct bq2589x, tcp_nb);
    struct tcp_notify *noti = data;
    uint32_t adapter_svid = 0;

    switch (event) {
    case TCP_NOTIFY_PD_STATE:
       switch (noti->pd_state.connected) {
       case PD_CONNECT_NONE:
           pr_err("%s: notify pd detached\n", __func__);
		   bq->pd_status = PD_CONNECT_NONE;
	   	   //bq->pd_flag = true;
	   	   //wake_up(&bq->pd_wait);
           break;
       case PD_CONNECT_HARD_RESET:
           pr_err("%s: notify pd hardreset\n", __func__);
           break;
       //case PD_CONNECT_PE_READY_SNK:
       case PD_CONNECT_PE_READY_SNK_PD30:
	   	   //pr_err("notify pd30\n");
	       adapter_svid = bq2589x_get_adapter_svid(bq);
	       if(USB_PD_MI_SVID == adapter_svid){
		   	  pr_err("%s: SUCCESS get pd SVID 0x%x\n", __func__, USB_PD_MI_SVID);
		      bq->pd_flag = true;
		      bq->pd_status = noti->pd_state.connected;
			  bq->isMiSvidFlg = true;
	   	      wake_up(&bq->pd_wait);
		   } else if(0 == adapter_svid){
		   	  pr_err("SUCCESS get NONSTAND SVID\n", __func__);
		      bq->pd_flag = true;
		      bq->pd_status = noti->pd_state.connected;
	   	      wake_up(&bq->pd_wait);
		   } else {
			  pr_err("%s: FAIL get pd SVID 0x%x\n", __func__, adapter_svid);
		   }
	   	   break;
	   case PD_CONNECT_TYPEC_ONLY_SNK:
	   	   pr_err("%s: notify only snk\n", __func__);
		   bq->pd_status = noti->pd_state.connected;
	   	   bq->pd_flag = false;
		   bq->usb_flag = true;
	   	   wake_up(&bq->pd_wait);
           break;
	   case PD_CONNECT_PE_READY_SNK_APDO:
	   	   pr_err("%s: isMiSvidFlg=%d notify pd apdo\n", __func__, bq->isMiSvidFlg);
		   if(!bq->isMiSvidFlg) {
			   bq->pd_status = noti->pd_state.connected;
			   bq->pd_flag = true;
			   wake_up(&bq->pd_wait);
		   }
		   break;
       default:
           break;
        }
    case TCP_NOTIFY_TYPEC_STATE:
       switch (noti->pd_state.connected) {
       case PD_CONNECT_PE_READY_SNK_PD30:
	   	   //pr_err("notify pd30\n");
		   if(!bq->isMiSvidFlg) {
		       adapter_svid = bq2589x_get_adapter_svid(bq);
		       if(USB_PD_HW_SVID == adapter_svid){
			   	  pr_err("SUCCESS get pd HW SVID 0x%x\n", __func__, USB_PD_HW_SVID);
			      bq->pd_flag = true;
			      bq->pd_status = noti->pd_state.connected;
		   	      wake_up(&bq->pd_wait);
			   } else {
				  pr_err("%s: FAIL get pd HW SVID 0x%x\n", __func__, adapter_svid);
			   }
		   }
	   	   break;
       default:
           break;
        }
    default:
        break;
    }

    return NOTIFY_OK;
}

static void tcp_notify_workfunc(struct work_struct *work)
{
    int ret = 0;
    static int reg_flag = 0;
	struct bq2589x *bq = container_of(work, struct bq2589x, tcp_work.work);

    if (!bq->tcpc) {
        bq->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!bq->tcpc)
            pr_err("get tcpc dev fail\n");
    }

    if (bq->tcpc && !reg_flag) {
        /* register tcp notifier callback */
        bq->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
        ret = register_tcp_dev_notifier(bq->tcpc, &bq->tcp_nb,
                                        TCP_NOTIFY_TYPE_USB);
        if (ret < 0) {
            pr_err("register tcpc notifier for chg fail\n");
            return;
        }
        reg_flag = 1;

        return;
    } else {
        pr_err("register tcpc notifier for chg fail\n");
        schedule_delayed_work(&bq->tcp_work, msecs_to_jiffies(2000));
   }
}

static int bq2589x_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct iio_dev *indio_dev;
	struct bq2589x *bq;
	int irqn;
	int ret;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*bq));
	bq = iio_priv(indio_dev);
	if (!bq) {
		dev_err(&client->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	bq->indio_dev = indio_dev;
	bq->dev = &client->dev;
	bq->client = client;
	i2c_set_clientdata(client, bq);

	bq->usb_switch_flag = true;
	bq->force_exit_flag = false;
	bq->hz_flag = false;
	bq->stop_rerun = false;
	bq->shutdown_flag = false;
	bq->pd_status = -1;
	bq->pd_flag = false;
	bq->can_bc12 = false;
	bq->bc12_done = false;
	bq->usb_flag = false;
	bq->enterBc12Flg = false;
	bq->isEnableCpPd = false;
	bq->isHw22p5Flg = false;
	bq->isFloatFlg = false;
	bq->isMiSvidFlg = false;
	bq->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
	bq->resume_completed = true;
	bq->poweroffchg_flag = get_chg_boot_mode();
	pr_err("bq->poweroffchg_flag=%d\n",bq->poweroffchg_flag);
	init_waitqueue_head(&bq->pd_wait);


	ret = bq2589x_detect_device(bq);
	if (!ret && bq->part_no == BQ25890) {
		bq->status |= BQ2589X_STATUS_EXIST;
		bq->is_sc89890h = true;
		dev_info(bq->dev, "%s: charger device bq25890 detected, revision:%d\n", __func__, bq->revision);
	} else if (!ret && bq->part_no == SYV690) {
		bq->status |= BQ2589X_STATUS_EXIST;
		bq->is_sc89890h = false;
		dev_info(bq->dev, "%s: charger device SYV690 detected, revision:%d\n", __func__, bq->revision);
/*m6 charge add code start*/
	} else if (!ret && bq->part_no == SC89890H) {
		bq->status |= BQ2589X_STATUS_EXIST;
		bq->is_sc89890h = true;
		dev_info(bq->dev, "%s: charger device SC89890H detected, revision:%d\n", __func__, bq->revision);
/*m6 charge add code end*/
	} else {
		dev_info(bq->dev, "%s: no master charger device found:%d\n", __func__, ret);
		return -ENODEV;
	}

	ret = bq_init_iio_psy(bq);
	if (ret < 0) {
		pr_err("Failed to initialize BQ IIO PSY, rc=%d\n", ret);
	}

	bq->wt_ws = wakeup_source_register(bq->dev, "bq25890_wakeup");
	if (!bq->wt_ws) {
		pr_err("wt chg workup fail!\n");
		wakeup_source_unregister(bq->wt_ws);
	}

	bq->batt_psy = power_supply_get_by_name("battery");
	bq->usb_psy = power_supply_get_by_name("usb");
	g_bq = bq;

	if (client->dev.of_node)
		bq2589x_parse_dt(&client->dev, bq);

	ret = bq2589x_init_device(bq);
	if (ret) {
		dev_err(bq->dev, "device init failure: %d\n", ret);
		goto err_0;
	}
	ret = gpio_request(bq->irq_gpio, "bq2589x irq pin");
	if (ret) {
		dev_err(bq->dev, "%s: %d gpio request failed\n", __func__, bq->irq_gpio);
		goto err_0;
	}

	irqn = gpio_to_irq(bq->irq_gpio);
	if (irqn < 0) {
		dev_err(bq->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		ret = irqn;
		goto err_1;
	}
	client->irq = irqn;

	INIT_DELAYED_WORK(&bq->irq_work, bq2589x_charger_irq_workfunc);
	INIT_WORK(&bq->adapter_in_work, bq2589x_adapter_in_workfunc);
	INIT_WORK(&bq->adapter_out_work, bq2589x_adapter_out_workfunc);
	INIT_DELAYED_WORK(&bq->monitor_work, bq2589x_monitor_workfunc);
	INIT_DELAYED_WORK(&bq->ico_work, bq2589x_ico_workfunc);
	INIT_DELAYED_WORK(&bq->force_work, bq2589x_force_workfunc);
	INIT_DELAYED_WORK(&bq->tcp_work, tcp_notify_workfunc);
	schedule_delayed_work(&bq->tcp_work, msecs_to_jiffies(1000));

	ret = sysfs_create_group(&bq->dev->kobj, &bq2589x_attr_group);
	if (ret) {
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_irq;
	}

	ret = request_irq(client->irq, bq2589x_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "bq2589x_charger1_irq", bq);
	if (ret) {
		dev_err(bq->dev, "%s:Request IRQ %d failed: %d\n", __func__, client->irq, ret);
		goto err_irq;
	} else {
		dev_info(bq->dev, "%s:irq = %d\n", __func__, client->irq);
	}

	enable_irq_wake(irqn);
	//if (bq->is_sc89890h)
		//hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "BQ25890H_CHARGER");
	//else
		//hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "SYV690_CHARGER");
	device_init_wakeup(bq->dev, 1);

	//bq2589x_usb_switch(bq, true);
	bq2589x_force_dpdm_done(bq);
	bq2589x_dump_regs(bq);
	pr_err("%s successfully\n", __func__);
	return 0;

err_irq:
	cancel_delayed_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_delayed_work_sync(&bq->monitor_work);
	cancel_delayed_work_sync(&bq->ico_work);
	cancel_delayed_work_sync(&bq->force_work);
	cancel_delayed_work_sync(&bq->tcp_work);
err_1:
err_0:
	g_bq = NULL;
	return ret;
}

static void bq2589x_charger_shutdown(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);

	if (!bq)
		return;

	bq2589x_disable_otg(bq);
	bq2589x_enable_charger(bq);
	bq2589x_exit_hiz_mode(bq);
	bq2589x_adc_start(bq, true);
	bq2589x_adc_stop(bq);
	msleep(2);
	dev_info(bq->dev, "%s: shutdown\n", __func__);
	bq->shutdown_flag = true;
	g_bq = NULL;
	return;
}

static struct of_device_id bq2589x_charger_match_table[] = {
	{.compatible = "ti,bq2589x-1",},
	{},
};
MODULE_DEVICE_TABLE(of, bq2589x_charger_match_table);

static const struct i2c_device_id bq2589x_charger_id[] = {
	{ "bq2589x-1", BQ25890 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2589x_charger_id);

static int bq2589x_suspend(struct device *dev)
{
	struct bq2589x *bq = dev_get_drvdata(dev);

	dev_err(bq->dev, "bq2589x_suspend\n");
	if (device_may_wakeup(dev) && bq->client->irq) {
		enable_irq_wake(bq->client->irq);
		bq->resume_completed = false;
		dev_err(bq->dev, "enable_irq_wake\n");
	}
	//disable_irq(bq->client->irq);

	return 0;
}

static int bq2589x_resume(struct device *dev)
{
	struct bq2589x *bq = dev_get_drvdata(dev);

	dev_err(bq->dev, "bq2589x_resume\n");
	//enable_irq(bq->client->irq);
	if (device_may_wakeup(dev) && bq->client->irq) {
		disable_irq_wake(bq->client->irq);
		bq->resume_completed = true;
		dev_err(bq->dev, "disable_irq_wake\n");
	}

	return 0;
}

static const struct dev_pm_ops bq2589x_pm_ops = {
        .suspend = bq2589x_suspend,
        .resume = bq2589x_resume,
};

static struct i2c_driver bq2589x_charger_driver = {
	.driver		= {
		.name	= "bq2589x-1",
		.of_match_table = bq2589x_charger_match_table,
		.pm   = &bq2589x_pm_ops,
	},
	.id_table	= bq2589x_charger_id,

	.probe		= bq2589x_charger_probe,
	.shutdown   = bq2589x_charger_shutdown,
};

module_i2c_driver(bq2589x_charger_driver);

MODULE_DESCRIPTION("TI BQ2589x Charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
