/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __BQ25960_H__
#define __BQ25960_H__

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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/version.h>

/* Register 00h */
#define BQ25960_REG_00					0x00
#define	BQ25960_BAT_OVP_DIS_MASK			0x80
#define	BQ25960_BAT_OVP_DIS_SHIFT		7
#define	BQ25960_BAT_OVP_ENABLE			0
#define	BQ25960_BAT_OVP_DISABLE			1

#define BQ25960_BAT_OVP_MASK				0x7F
#define BQ25960_BAT_OVP_SHIFT			0
#define BQ25960_BAT_OVP_BASE				3491
#define BQ25960_BAT_OVP_LSB				9985

/* Register 01h */
#define BQ25960_REG_01					0x01
#define BQ25960_BAT_OVP_ALM_DIS_MASK		0x80
#define BQ25960_BAT_OVP_ALM_DIS_SHIFT	7
#define BQ25960_BAT_OVP_ALM_ENABLE		0
#define BQ25960_BAT_OVP_ALM_DISABLE		1

#define BQ25960_BAT_OVP_ALM_MASK			0x7F
#define BQ25960_BAT_OVP_ALM_SHIFT		0
#define BQ25960_BAT_OVP_ALM_BASE			3500
#define BQ25960_BAT_OVP_ALM_LSB			10

/* Register 02h */
#define BQ25960_REG_02					0x02
#define	BQ25960_BAT_OCP_DIS_MASK			0x80
#define	BQ25960_BAT_OCP_DIS_SHIFT		7
#define BQ25960_BAT_OCP_ENABLE			0
#define BQ25960_BAT_OCP_DISABLE			1

#define BQ25960_BAT_OCP_MASK				0x7F
#define BQ25960_BAT_OCP_SHIFT			0
#define BQ25960_BAT_OCP_BASE				0
#define BQ25960_BAT_OCP_LSB				1025

/* Register 03h */
#define BQ25960_REG_03					0x03
#define BQ25960_BAT_OCP_ALM_DIS_MASK		0x80
#define BQ25960_BAT_OCP_ALM_DIS_SHIFT	7
#define BQ25960_BAT_OCP_ALM_ENABLE		0
#define BQ25960_BAT_OCP_ALM_DISABLE		1

#define BQ25960_BAT_OCP_ALM_MASK			0x7F
#define BQ25960_BAT_OCP_ALM_SHIFT		0
#define BQ25960_BAT_OCP_ALM_BASE			0
#define BQ25960_BAT_OCP_ALM_LSB			100

/* Register 04h */
#define BQ25960_REG_04					0x04
#define	BQ25960_BAT_UCP_ALM_DIS_MASK		0x80
#define	BQ25960_BAT_UCP_ALM_DIS_SHIFT	7
#define	BQ25960_BAT_UCP_ALM_ENABLE		0
#define	BQ25960_BAT_UCP_ALM_DISABLE		1

#define	BQ25960_BAT_UCP_ALM_MASK			0x7F
#define	BQ25960_BAT_UCP_ALM_SHIFT		0
#define	BQ25960_BAT_UCP_ALM_BASE			0
#define	BQ25960_BAT_UCP_ALM_LSB			50

/* Register 05h */
#define BQ25960_REG_05					0x05
#define BQ25960_BUS_UCP_DIS_MASK			0x80
#define BQ25960_BUS_UCP_DIS_SHIFT		7
#define BQ25960_BUS_UCP_ENABLE			0
#define BQ25960_BUS_UCP_DISABLE			1

#define BQ25960_BUS_UCP_MASK			0x40
#define BQ25960_BUS_UCP_SHIFT			6
#define BQ25960_BUS_UCP_250MA			1


#define BQ25960_BUS_RCP_DIS_MASK			0x20
#define BQ25960_BUS_RCP_DIS_SHIFT		5
#define BQ25960_BUS_RCP_ENABLE			0
#define BQ25960_BUS_RCP_DISABLE			1


#define BQ25960_BUS_RCP_MASK				0x10
#define BQ25960_BUS_RCP_SHIFT			4
#define BQ25960_BUS_RCP_300MA			0

#define BQ25960_CHG_CONFIG_MASK				0x08
#define BQ25960_CHG_CONFIG_SHIFT		3

#define BQ25960_BUS_ERRHI_DIS_MASK			0x04
#define BQ25960_BUS_ERRHI_DIS_SHIFT		2
#define BQ25960_BUS_ERRHI_ENABLE			0
#define BQ25960_BUS_ERRHI_DISABLE		1

/* Register 06h */
#define BQ25960_REG_06					0x06
#define BQ25960_VBUS_PD_EN_MASK				0x80
#define BQ25960_VBUS_PD_EN_SHIFT		7
#define BQ25960_VBUS_PD_ENABLE			1
#define BQ25960_VBUS_PD_DISABLE			0

#define BQ25960_BUS_OVP_MASK				0x7F
#define BQ25960_BUS_OVP_SHIFT			0
#define BQ25960_BUS_OVP_BASE				7000
#define BQ25960_BUS_OVP_LSB				50

/* Register 07h */
#define BQ25960_REG_07					0x07
#define BQ25960_BUS_OVP_ALM_DIS_MASK		0x80
#define BQ25960_BUS_OVP_ALM_DIS_SHIFT	7
#define BQ25960_BUS_OVP_ALM_ENABLE		0
#define BQ25960_BUS_OVP_ALM_DISABLE		1

#define BQ25960_BUS_OVP_ALM_MASK			0x7F
#define BQ25960_BUS_OVP_ALM_SHIFT		0
#define BQ25960_BUS_OVP_ALM_BASE			7000
#define BQ25960_BUS_OVP_ALM_LSB			50

/* Register 08h */
#define BQ25960_REG_08					0x08
#define BQ25960_BUS_OCP_RESERVED_MASK		0xE0

#define BQ25960_BUS_OCP_MASK			0x1F
#define BQ25960_BUS_OCP_SHIFT			0
#define	BQ25960_BUS_OCP_BASE			10175
#define	BQ25960_BUS_OCP_LSB				254


/* Register 09h */
#define BQ25960_REG_09						0x09
#define BQ25960_BUS_OCP_ALM_DIS_MASK			0x80
#define BQ25960_BUS_OCP_ALM_DIS_SHIFT		7
#define BQ25960_BUS_OCP_ALM_ENABLE			0
#define BQ25960_BUS_OCP_ALM_DISABLE			1

#define BQ25960_BUS_OCP_ALM_MASK				0x1F
#define BQ25960_BUS_OCP_ALM_SHIFT			0
#define BQ25960_BUS_OCP_ALM_BASE				1000
#define BQ25960_BUS_OCP_ALM_LSB				250

/* Register 0Ah */
#define BQ25960_REG_0A					0x0A
#define BQ25960_TDIE_FLT_DIS_MASK			0x80
#define BQ25960_TDIE_FLT_DIS_SHIFT		7
#define BQ25960_TDIE_FLT_ENABLE			0
#define BQ25960_TDIE_FLT_DISABLE		1
	
#define BQ25960_TDIE_FLT_MASK			0x60
#define BQ25960_TDIE_FLT_SHIFT			5
#define BQ25960_TDIE_FLT_100C			1
#define BQ25960_TDIE_FLT_120C			2
#define BQ25960_TDIE_FLT_140C			3

#define BQ25960_TDIE_ALM_DIS_MASK			0x10
#define BQ25960_TDIE_ALM_DIS_SHIFT		4
#define BQ25960_TDIE_ALM_ENABLE			0
#define BQ25960_TDIE_ALM_DISABLE		1

#define BQ25960_TSBUS_FLT_DIS_MASK			0x08
#define BQ25960_TSBUS_FLT_DIS_SHIFT		3
#define BQ25960_TSBUS_FLT_ENABLE			0
#define BQ25960_TSBUS_FLT_DISABLE		1

#define BQ25960_TSBAT_FLT_DIS_MASK			0x04
#define BQ25960_TSBAT_FLT_DIS_SHIFT		2
#define BQ25960_TSBAT_FLT_ENABLE		0
#define BQ25960_TSBAT_FLT_DISABLE		1

/* Register 0Bh */
#define BQ25960_REG_0B						0x0B
#define BQ25960_TDIE_ALM_MASK					0xFF
#define BQ25960_TDIE_ALM_SHIFT				0
#define BQ25960_TDIE_ALM_BASE					25
#define BQ25960_TDIE_ALM_LSB				5

/* Register 0Ch */
#define BQ25960_REG_0C						0x0C
#define BQ25960_TSBUS_FLT_MASK					0xFF
#define BQ25960_TSBUS_FLT_SHIFTK			0
#define BQ25960_TSBUS_FLT_BASE					0
#define BQ25960_TSBUS_FLT_LSB				0.19531

/* Register 0Dh */
#define BQ25960_REG_0D						0x0D
#define BQ25960_TSBAT_FLT_MASK					0xFF
#define BQ25960_TSBAT_FLT_SHIFT				0
#define BQ25960_TSBAT_FLT_BASE					0
#define BQ25960_TSBAT_FLT_LSB				0.19531

/* Register 0Eh */
#define BQ25960_REG_0E						0x0E
#define BQ25960_VAC1_OVP_MASK					0xE0
#define BQ25960_VAC1_OVP_SHIFT				5
#define BQ25960_VAC1_OVP_6500MV				0
#define BQ25960_VAC1_OVP_10500MV			1
#define BQ25960_VAC1_OVP_12000MV			2
#define BQ25960_VAC1_OVP_14000MV			3
#define BQ25960_VAC1_OVP_16000MV			4
#define BQ25960_VAC1_OVP_18000MV			5

#define BQ25960_VAC2_OVP_MASK					0x1C
#define BQ25960_VAC2_OVP_SHIFT				2
#define BQ25960_VAC2_OVP_6500MV				0
#define BQ25960_VAC2_OVP_10500MV			1
#define BQ25960_VAC2_OVP_12000MV			2
#define BQ25960_VAC2_OVP_14000MV			3
#define BQ25960_VAC2_OVP_16000MV			4
#define BQ25960_VAC2_OVP_18000MV			5

#define BQ25960_VAC1_PD_EN_MASK					0X02
#define BQ25960_VAC1_PD_EN_SHIFT			1
#define BQ25960_VAC1_PD_EN_ENABL			1
#define BQ25960_VAC1_PD_EN_DISABLE			0

#define BQ25960_VAC2_PD_EN_MASK					0X01
#define BQ25960_VAC2_PD_EN_SHIFT			0
#define BQ25960_VAC2_PD_EN_ENABL			1
#define BQ25960_VAC2_PD_EN_DISABLE			0

/* Register 0Fh */
#define BQ25960_REG_0F						0x0F
#define BQ25960_REG_RESET_MASK					0x80
#define BQ25960_REG_RESET_SHIFT				7
#define BQ25960_REG_RESET_ENABLE			1
#define BQ25960_REG_RESET_DISABLE			0

#define BQ25960_EN_HIZ_MASK						0x40
#define BQ25960_EN_HIZ_SHIFT				6
#define BQ25960_EN_HIZ_ENABLE				1
#define BQ25960_EN_HIZ_DISABLE				0

#define BQ25960_EN_OTG_MASK						0x20
#define BQ25960_EN_OTG_SHIFT				5
#define BQ25960_EN_OTG_DONTALLOW_CTR		0
#define BQ25960_EN_OTG_ALLOW_CTR			1

#define BQ25960_CHG_EN_MASK						0x10
#define BQ25960_CHG_EN_SHIFT				4
#define BQ25960_CHG_ENABLE				1
#define BQ25960_CHG_DISABLE				0

#define BQ25960_EN_BYPASS_MASK					0x08
#define BQ25960_EN_BYPASS_SHIFT				3
#define BQ25960_EN_BYPASS_ENABLE			1
#define BQ25960_EN_BYPASS_DISABLE			0

/* Register 10h */
#define BQ25960_REG_10						0x10
#define BQ25960_FSW_SET_MAS						0xE0
#define BQ25960_FSW_SET_SHIFT				5
#define BQ25960_FSW_SET_187P5KHZ			0
#define BQ25960_FSW_SET_250KHZ				1
#define BQ25960_FSW_SET_300KHZ				2
#define BQ25960_FSW_SET_375KHZ				3
#define BQ25960_FSW_SET_500KHZ				4
#define BQ25960_FSW_SET_750KHZ				5

#define BQ25960_WATCHDOG_TIMER_MASK			0x18
#define BQ25960_WATCHDOG_TIMER_SHIFT		3
#define BQ25960_WATCHDOG_TIMER_0P5S			0
#define BQ25960_WATCHDOG_TIMER_1S			1
#define BQ25960_WATCHDOG_TIMER_5S			2
#define BQ25960_WATCHDOG_TIMER_30S			3

#define BQ25960_WATCHDOG_DIS_MASK			0x04
#define BQ25960_WATCHDOG_DIS_SHIFT			2
#define BQ25960_WATCHDOG_ENABLE				0
#define BQ25960_WATCHDOG_DISABLE			1

/* Register 11h */
#define BQ25960_REG_11						0x11
#define BQ25960_RSNS_MASK						0x80
#define BQ25960_RSNS_SHIFT					7
#define BQ25960_RSNS_VAL_2					0
#define BQ25960_RSNS_VAL_5					1

#define BQ25960_SS_TIMEOUT_MASK				0X70
#define BQ25960_SS_TIMEOUT_SHIFT			4
#define BQ25960_SS_TIMEOUT_DISABLE			0
#define BQ25960_SS_TIMEOUT_6P25MS			0
#define BQ25960_SS_TIMEOUT_12P5MS			1
#define BQ25960_SS_TIMEOUT_25MS				2
#define BQ25960_SS_TIMEOUT_50MS				3
#define BQ25960_SS_TIMEOUT_100MS			4
#define BQ25960_SS_TIMEOUT_400MS			5
#define BQ25960_SS_TIMEOUT_1500MS			6
#define BQ25960_SS_TIMEOUT_10000MS			7

/* Register 12h */
#define BQ25960_REG_12						0x12
#define BQ25960_BAT_OVP_FLT_MASK_MASK		0x80
#define BQ25960_BAT_OVP_FLT_MASK_SHIFT		7
#define BQ25960_BAT_OVP_FLT_MASK_ENABLE		0
#define BQ25960_BAT_OVP_FLT_MASK_DISABLE		1

#define BQ25960_BAT_OCP_FLT_MASK_MASK		0x80
#define BQ25960_BAT_OCP_FLT_MASK_SHIFT		7
#define BQ25960_BAT_OCP_FLT_MASK_ENABLE		0
#define BQ25960_BAT_OCP_FLT_MASK_DISABLE		1

#define BQ25960_BUS_OVP_FLT_MASK_MASK		0x80
#define BQ25960_BUS_OVP_FLT_MASK_SHIFT		7
#define BQ25960_BUS_OVP_FLT_MASK_ENABLE		0
#define BQ25960_BUS_OVP_FLT_MASK_DISABLE		1

#define BQ25960_BUS_OCP_FLT_MASK_MASK		0x10
#define BQ25960_BUS_OCP_FLT_MASK_SHIFT		4
#define BQ25960_BUS_OCP_FLT_MASK_ENABLE		1
#define BQ25960_BUS_OCP_FLT_MASK_DISABLE		0

#define BQ25960_TSBUS_TSBAT_ALM_MASK_MASK	0x08
#define BQ25960_TSBUS_TSBAT_ALM_MASK_SHIFT	3
#define BQ25960_TSBUS_TSBAT_ALM_MASK_ENABLE	1
#define BQ25960_TSBUS_TSBAT_ALM_MASK_DISABLE	0

#define BQ25960_TSBAT_FLT_MASK_MASK			0x04
#define BQ25960_TSBAT_FLT_MASK_SHIFT			2
#define BQ25960_TSBAT_FLT_MASK_ENABLE		1
#define BQ25960_TSBAT_FLT_MASK_DISABLE		0

#define BQ25960_TSBUS_FLT_MASK_MASK			0x02
#define BQ25960_TSBUS_FLT_MASK_SHIFT			1
#define BQ25960_TSBUS_FLT_MASK_ENABLE		1
#define BQ25960_TSBUS_FLT_MASK_DISABLE		0

#define BQ25960_TDIE_ALM_MASK_MASK			0x01
#define BQ25960_TDIE_ALM_MASK_SHIFT			0
#define BQ25960_TDIE_ALM_MASK_ENABLE			1
#define BQ25960_TDIE_ALM_MASK_DISABLE		0

/* Register 13h */
#define BQ25960_REG_13						0x13
#define BQ25960_DEV_ID_MASK					0x0F
#define BQ25960_DEV_ID_SHIFT					0

/* Register 14h */
#define BQ25960_REG_14						0x14

/* Register 15h */
#define BQ25960_REG_15						0x15
#define BQ25960_VBUS_ADC_DIS_MASK			0x80
#define BQ25960_VBUS_ADC_DIS_SHIFT			7
#define BQ25960_VBUS_ADC_ENABLE				0
#define BQ25960_VBUS_ADC_DISABLE				1

#define BQ25960_VAC_ADC_DIS_MASK				0x40
#define BQ25960_VAC_ADC_DIS_SHIFT			6
#define BQ25960_VAC_ADC_ENABLE				0
#define BQ25960_VAC_ADC_DISABLE				1

#define BQ25960_VOUT_ADC_DIS_MASK			0x20
#define BQ25960_VOUT_ADC_DIS_SHIFT			5
#define BQ25960_VOUT_ADC_ENABLE				0
#define BQ25960_VOUT_ADC_DISABLE				1

#define BQ25960_VBAT_ADC_DIS_MASK			0x10
#define BQ25960_VBAT_ADC_DIS_SHIFT			4
#define BQ25960_VBAT_ADC_ENABLE				0
#define BQ25960_VBAT_ADC_DISABLE				1

#define BQ25960_IBAT_ADC_DIS_MASK			0x08
#define BQ25960_IBAT_ADC_DIS_SHIFT			3
#define BQ25960_IBAT_ADC_ENABLE				0
#define BQ25960_IBAT_ADC_DISABLE				1

#define BQ25960_TSBUS_ADC_DIS_MASK			0x04
#define BQ25960_TSBUS_ADC_DIS_SHIFT			2
#define BQ25960_TSBUS_ADC_ENABLE				0
#define BQ25960_TSBUS_ADC_DISABLE			1

#define BQ25960_TSBAT_ADC_DIS_MASK			0x02
#define BQ25960_TSBAT_ADC_DIS_SHIFT			1
#define BQ25960_TSBAT_ADC_ENABLE				0
#define BQ25960_TSBAT_ADC_DISABLE			1

#define BQ25960_TDIE_ADC_DIS_MASK			0x01
#define BQ25960_TDIE_ADC_DIS_SHIFT			0
#define BQ25960_TDIE_ADC_ENABLE				0
#define BQ25960_TDIE_ADC_DISABLE				1

/* Register 16h */
#define BQ25960_REG_16						0x16
#define BQ25960_IBUS_POL_H_MASK				0x0F
#define BQ25960_IBUS_ADC_LSB				    1.5625

/* Register 17h */
#define BQ25960_REG_17						0x17
#define BQ25960_IBUS_POL_L_MASK				0xFF

/* Register 18h */
#define BQ25960_REG_18						0x18
#define BQ25960_VBUS_POL_H_MASK				0x0F
#define BQ25960_VBUS_ADC_LSB					3.75

/* Register 19h */
#define BQ25960_REG_19						0x19
#define BQ25960_VBUS_POL_L_MASK				0xFF

/* Register 1Ah */
#define BQ25960_REG_1A						0x1A
#define BQ25960_VAC_POL_H_MASK				0x0F
#define BQ25960_VAC_ADC_LSB					5

/* Register 1Bh */
#define BQ25960_REG_1B						0x1B
#define BQ25960_VAC_POL_L_MASK				0xFF

/* Register 1Ch */
#define BQ25960_REG_1C						0x1C
#define BQ25960_VOUT_POL_H_MASK				0x0F
#define BQ25960_VOUT_ADC_LSB					1.25

/* Register 1Dh */
#define BQ25960_REG_1D						0x1D
#define BQ25960_VOUT_POL_L_MASK				0x0F

/* Register 1Eh */
#define BQ25960_REG_1E						0x1E
#define BQ25960_VBAT_POL_H_MASK				0x0F
#define BQ25960_VBAT_ADC_LSB 				1.25

/* Register 1Fh */
#define BQ25960_REG_1F						0x1F
#define BQ25960_VBAT_POL_L_MASK				0xFF

/* Register 20h */
#define BQ25960_REG_20						0x20
#define BQ25960_IBAT_POL_H_MASK				0x0F
#define BQ25960_IBAT_ADC_LSB 				3.125

/* Register 21h */
#define BQ25960_REG_21						0x21
#define BQ25960_IBAT_POL_L_MASK				0xFF

/* Register 22h */
#define BQ25960_REG_22						0x22
#define BQ25960_DEVICE_ID_MASK				0x0F
#define BQ25960_DEVICE_ID_SHIFT				0



/* Register 23h */
#define BQ25960_REG_23						0x23
#define BQ25960_ADC_EN_MASK						0x80
#define BQ25960_ADC_EN_SHIFT				7
#define BQ25960_ADC_ENABLE					1
#define BQ25960_ADC_DISABLE					0

#define BQ25960_ADC_RATE_MASK				0x40
#define BQ25960_ADC_RATE_SHIFT				6
#define BQ25960_ADC_RATE_CONTINOUS			0
#define BQ25960_ADC_RATE_ONESHOT			1

#define BQ25960_IBUS_ADC_DIS_MASK			0x02
#define BQ25960_IBUS_ADC_DIS_SHIFT			1
#define BQ25960_IBUS_ADC_ENABLE				0
#define BQ25960_IBUS_ADC_DISABLE			1

#define BQ25960_REG23_VBUS_ADC_DIS_MASK		   0x01
#define BQ25960_REG23_VBUS_ADC_DIS_SHIFT		0
#define BQ25960_REG23_VBUS_ADC_ENABLE			0
#define BQ25960_REG23_VBUS_ADC_DISABLE			1

/* Register 24h */
#define BQ25960_REG_24						0x24
#define BQ25960_TSBAT_POL_H_MASK				0x03
#define BQ25960_TSBAT_ADC_LSB 				0.09766

/* Register 25h~37h only read */
#define BQ25960_REG_25						0x25
#define BQ25960_REG_27						0x27
#define BQ25960_REG_29						0x29
#define BQ25960_REG_2B						0x2B
#define BQ25960_REG_2D						0x2D
#define BQ25960_REG_2F						0x2F
#define BQ25960_REG_31						0x31
#define BQ25960_REG_33						0x33
#define BQ25960_REG_35						0x35
#define BQ25960_REG_37						0x37
#define BQ25960_REG_A8						0xA8
#define BQ25960_REG_FA						0xFA
#define BQ25960_REG_F9						0xF9
#define BQ25960_REG_A0						0xA0
#define BQ25960_REG_9A						0x9A
#define BQ25960_REG_9B						0x9B


enum bq25960_adc_ch {
	BQ25960_ADC_IBUS,
	BQ25960_ADC_VBUS,
	BQ25960_ADC_VAC1,
	BQ25960_ADC_VAC2,
	BQ25960_ADC_VOUT,
	BQ25960_ADC_VBAT,
	BQ25960_ADC_IBAT,
	BQ25960_ADC_TSBUS,
	BQ25960_ADC_TSBAT,
	BQ25960_ADC_TDIE,
	BQ25960_ADC_MAX_NUM,
};

#define BQ25960_ROLE_STDALONE           0
#define BQ25960_ROLE_SLAVE              1
#define BQ25960_ROLE_MASTER             2
#define BQ_1_1_MODE                     1
#define BQ_2_1_MODE                     0

enum {
	BQ25960_STDALONE,
	BQ25960_SLAVE,
	BQ25960_MASTER,
};

static int bq25960_mode_data[] = {
	[BQ25960_STDALONE] = BQ25960_ROLE_STDALONE,
	[BQ25960_MASTER] = BQ25960_ROLE_MASTER,
	[BQ25960_SLAVE] = BQ25960_ROLE_SLAVE,
};

#define	BAT_OVP_ALARM                BIT(7)
#define BAT_OCP_ALARM                BIT(6)
#define	BUS_OVP_ALARM                BIT(5)
#define	BUS_OCP_ALARM                BIT(4)
#define	BAT_UCP_ALARM                BIT(3)
#define	VBUS_INSERT                  BIT(2)
#define VBAT_INSERT                  BIT(1)
#define	ADC_DONE                     BIT(0)

#define BAT_OVP_FAULT                BIT(7)
#define BAT_OCP_FAULT                BIT(6)
#define BUS_OVP_FAULT                BIT(5)
#define BUS_OCP_FAULT                BIT(4)
#define TBUS_TBAT_ALARM              BIT(3)
#define TS_BAT_FAULT                 BIT(2)
#define	TS_BUS_FAULT                 BIT(1)
#define	TS_DIE_FAULT                 BIT(0)

/*below used for comm with other module*/
#define	BAT_OVP_FAULT_SHIFT          0
#define	BAT_OCP_FAULT_SHIFT          1
#define	BUS_OVP_FAULT_SHIFT          2
#define	BUS_OCP_FAULT_SHIFT          3
#define	BAT_THERM_FAULT_SHIFT        4
#define	BUS_THERM_FAULT_SHIFT        5
#define	DIE_THERM_FAULT_SHIFT        6

#define	BAT_OVP_FAULT_MASK           (1 << BAT_OVP_FAULT_SHIFT)
#define	BAT_OCP_FAULT_MASK           (1 << BAT_OCP_FAULT_SHIFT)
#define	BUS_OVP_FAULT_MASK           (1 << BUS_OVP_FAULT_SHIFT)
#define	BUS_OCP_FAULT_MASK           (1 << BUS_OCP_FAULT_SHIFT)
#define	BAT_THERM_FAULT_MASK         (1 << BAT_THERM_FAULT_SHIFT)
#define	BUS_THERM_FAULT_MASK         (1 << BUS_THERM_FAULT_SHIFT)
#define	DIE_THERM_FAULT_MASK         (1 << DIE_THERM_FAULT_SHIFT)

#define	BAT_OVP_ALARM_SHIFT          0
#define	BAT_OCP_ALARM_SHIFT          1
#define	BUS_OVP_ALARM_SHIFT          2
#define	BUS_OCP_ALARM_SHIFT          3
#define	BAT_THERM_ALARM_SHIFT        4
#define	BUS_THERM_ALARM_SHIFT        5
#define	DIE_THERM_ALARM_SHIFT        6
#define BAT_UCP_ALARM_SHIFT          7

#define	BAT_OVP_ALARM_MASK           (1 << BAT_OVP_ALARM_SHIFT)
#define	BAT_OCP_ALARM_MASK           (1 << BAT_OCP_ALARM_SHIFT)
#define	BUS_OVP_ALARM_MASK           (1 << BUS_OVP_ALARM_SHIFT)
#define	BUS_OCP_ALARM_MASK           (1 << BUS_OCP_ALARM_SHIFT)
#define	BAT_THERM_ALARM_MASK         (1 << BAT_THERM_ALARM_SHIFT)
#define	BUS_THERM_ALARM_MASK         (1 << BUS_THERM_ALARM_SHIFT)
#define	DIE_THERM_ALARM_MASK         (1 << DIE_THERM_ALARM_SHIFT)
#define	BAT_UCP_ALARM_MASK           (1 << BAT_UCP_ALARM_SHIFT)

#define VBAT_REG_STATUS_SHIFT        0
#define IBAT_REG_STATUS_SHIFT        1

#define VBAT_REG_STATUS_MASK         (1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK         (1 << VBAT_REG_STATUS_SHIFT)

#define ADC_REG_BASE BQ25960_REG_25

struct bq25960_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ocp_th;
	int bat_ocp_alm_th;

	bool bus_ovp_alm_disable;
	bool bus_ocp_disable;
	bool bus_ocp_alm_disable;

	int bus_ovp_th;
	int bus_ovp_alm_th;
	int bus_ocp_th;
	int bus_ocp_alm_th;

	bool bat_ucp_alm_disable;

	int bat_ucp_alm_th;
	int ac_ovp_th;

	bool bat_therm_disable;
	bool bus_therm_disable;
	bool die_therm_disable;

	int bat_therm_th; /*in %*/
	int bus_therm_th; /*in %*/
	int die_therm_th; /*in degC*/

	int sense_r_mohm;
};

struct bq25960 {
	struct device *dev;
	struct i2c_client *client;
	struct i2c_client *reverse_client;
	bool reverse_flag;
	int part_no;
	int revision;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex charging_disable_lock;

	bool batt_present;
	bool vbus_present;

	bool usb_present;
	bool charge_enabled; /* Register bit status */

	bool is_bq25960;
	int  vbus_error;

	/* ADC reading */
	int vbat_volt;
	int vbus_volt;
	int vout_volt;
	int vac_volt;

	int ibat_curr;
	int ibus_curr;

	int bat_temp;
	int bus_temp;
	int die_temp;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int  prev_alarm;
	int  prev_fault;

	int chg_ma;
	int chg_mv;
	int charge_state;

	struct bq25960_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct delayed_work monitor_work;
	struct dentry *debug_root;
	struct iio_dev *indio_dev;
	struct iio_chan_spec *iio_chan;
	struct iio_channel *int_iio_chans;

	struct chargerpump_dev *master_cp_chg;
	struct chargerpump_dev *slave_cp_chg;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
	struct delayed_work set_rcp_work;
};

enum bq25960_notify {
	BQ25960_NOTIFY_OTHER = 0,
	BQ25960_NOTIFY_IBUSUCPF,
	BQ25960_NOTIFY_VBUSOVPALM,
	BQ25960_NOTIFY_VBATOVPALM,
	BQ25960_NOTIFY_IBUSOCP,
	BQ25960_NOTIFY_VBUSOVP,
	BQ25960_NOTIFY_IBATOCP,
	BQ25960_NOTIFY_VBATOVP,
	BQ25960_NOTIFY_VOUTOVP,
	BQ25960_NOTIFY_VDROVP,
	BQ25960_NOTIFY_VACOVP,
	BQ25960_NOTIFY_TDIEFLT,
};

struct bq25960_flag_bit {
	int notify;
	int mask;
	char *name;
};

struct bq25960_intr_flag {
	int reg;
	int len;
	struct bq25960_flag_bit bit[8];
};

#endif /* __BQ25960_H__ */

