/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __HQ_CHG_H__
#define __HQ_CHG_H__

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include "hq_power_supply.h"

#define THERMAL_LEVEL_MAX		16

static int normal_charger_thermal_table[THERMAL_LEVEL_MAX] = {
	6000000,5400000,5000000,4500000,4000000,3500000,3000000,2700000,
	2500000,2300000,2100000,1800000,1500000,900000,500000,300000,
};

static int fast_charger_thermal_table[THERMAL_LEVEL_MAX] = {
	3000000,2700000,2500000,2300000,2100000,1900000,1700000,1500000,
	1000000,1000000,1000000,1000000,1000000,900000,500000,300000,
};

static int super_charger_thermal_table[THERMAL_LEVEL_MAX] = {
	3000000,2700000,2500000,2300000,2100000,1900000,1700000,1500000,
	1000000,1000000,1000000,1000000,1000000,900000,500000,300000,
};

#define JEITA_TEMP_T0			100
#define JEITA_TEMP_T1			0
#define JEITA_TEMP_T2			50
#define JEITA_TEMP_T3			100
#define JEITA_TEMP_T4			150
#define JEITA_TEMP_T5			350
#define JEITA_TEMP_T6			480
#define JEITA_TEMP_T7			600

#define JEITA_TEMP_BELOW_T1_CC			490
#define	JEITA_TEMP_T1_TO_T2_CC			980
#define	JEITA_TEMP_T2_TO_T3_CC			2450
#define	JEITA_TEMP_T3_TO_T4_CC			3920
#define	JEITA_TEMP_T4_TO_T5_CC			6000
#define	JEITA_TEMP_T5_TO_T6_CC			6000
#define	JEITA_TEMP_ABOVE_T6_CC			2450

#define SDP_INPUT_CHARGE_LIMIT				500
#define SDP_BATTERY_CHARGE_LIMIT			500
#define FLOAT_INPUT_CHARGE_LIMIT				1000
#define FLOAT_BATTERY_CHARGE_LIMIT			1000
#define CDP_INPUT_CHARGE_LIMIT				1500
#define CDP_BATTERY_CHARGE_LIMIT			1500
#define DCP_INPUT_CHARGE_LIMIT				2000
#define DCP_BATTERY_CHARGE_LIMIT			2000
#define QC_INPUT_CHARGE_LIMIT				3200
#define QC_BATTERY_CHARGE_LIMIT			3200
#define PD_INPUT_CHARGE_LIMIT				3000
#define PD_BATTERY_CHARGE_LIMIT			3000

#define HQ_IBAT_VOTER		"HQ_IBAT_VOTER"
#define HQ_IBUS_VOTER		"HQ_IBUS_VOTER"
#define HQ_CV_VOTER		"HQ_CV_VOTER"
#define HQ_ITERM_VOTER		"HQ_ITERM_VOTER"

#define HQ_IBAT_JEITA		"HQ_IBAT_JEITA"
#define HQ_IBUS_JEITA		"HQ_IBUS_JEITA"
#define HQ_IBAT_CHARGER_TYPE		"HQ_IBAT_CHARGER_TYPE"
#define HQ_IBUS_CHARGER_TYPE		"HQ_IBUS_CHARGER_TYPE"
#define HQ_IBAT_THERMAL		"HQ_IBAT_THERMAL"
#define HQ_CV_JEITA		"HQ_CV_JEITA"
#define HQ_ITERM_JEITA		"HQ_ITERM_JEITA"

struct jeita_parametes {
	bool		jeita_enable;
	int		jeita_temp_t0;
	int		jeita_temp_t1;
	int		jeita_temp_t2;
	int		jeita_temp_t3;
	int		jeita_temp_t4;
	int		jeita_temp_t5;
	int		jeita_temp_t6;
	int		jeita_temp_t7;

	int		jeita_temp_below_t1_cc;
	int		jeita_temp_t1_to_t2_cc;
	int		jeita_temp_t2_to_t3_cc;
	int		jeita_temp_t3_to_t4_cc;
	int		jeita_temp_t4_to_t5_cc;
	int		jeita_temp_t5_to_t6_cc;
	int		jeita_temp_above_t6_cc;

	int		jeita_cv0;
	int		jeita_cv1;
	int		jeita_cv2;
};

struct chg_type_current_limit {
	int		sdp_input_charge_limit;
	int		sdp_battery_charge_limit;
	int		float_input_charge_limit;
	int		float_battery_charge_limit;
	int		cdp_input_charge_limit;
	int		cdp_battery_charge_limit;
	int		dcp_input_charge_limit;
	int		dcp_battery_charge_limit;
	int		qc_input_charge_limit;
	int		qc_battery_charge_limit;
	int		pd_input_charge_limit;
	int		pd_battery_charge_limit;
};

struct thermal_parametes {
	bool		thermal_enable;
	int		normal_charger_thermal_table[THERMAL_LEVEL_MAX];
	int		fast_charger_thermal_table[THERMAL_LEVEL_MAX];
	int		super_charger_thermal_table[THERMAL_LEVEL_MAX];
};

struct batt_dt_props {
	struct jeita_parametes		jeita_para;
	struct chg_type_current_limit		chg_type_curr_limit;
	struct thermal_parametes		thermal_table_paras;
};

struct sw_chg_parametes {

};

struct cp_chg_parametes {

};

struct fg_chg_parametes {
	int temp;
	int vbat;
	int soc;
};

struct other_parametes {

};

struct batt_chg_fg_props {
	struct sw_chg_parametes		sw_chg_paras;
	struct cp_chg_parametes		cp_chg_paras;
	struct fg_chg_parametes		fg_paras;
	struct other_parametes		other_paras;
};

enum batt_ic_type {
	IC_UNKNOW = 0,
	IC_FUEL_GAUGE,
	IC_MAIN_CHARGE,
	IC_CHARGE_PUMP_MASTER,
	IC_CHARGE_PUMP_SLAVE,
	IC_UNSUPPORT,
};

enum batt_vendor {
	VENDOR_UNKNOW = 0,
	VENDOR_FIRST,
	VENDOR_SECOND,
};

struct batt_iio_category {
	int               start;
	int               end;
	enum batt_ic_type type;
};

struct charge_ic_info {
	enum batt_ic_type   type;
	enum batt_vendor    vendor;
	int                 offset;
	bool                init;
};

struct batt_chg {
	struct platform_device    *pdev;
	struct device             *dev;

	struct power_supply       *batt_psy;
	struct power_supply       *usb_psy;

	struct iio_dev            *indio_dev;
	struct iio_chan_spec      *batt_iio_chan_spec;
	struct iio_channel        *batt_iio_chans;
	struct iio_channel        **batt_ext_iio_chans;

	struct wakeup_source      *batt_ws;

	struct batt_dt_props      batt_dt;
	struct batt_chg_fg_props	batt_chg_fg;
	struct delayed_work       batt_chg_work;
	struct delayed_work       batt_init_ic_work;

	struct class              extern_class;

	// save ic info
	struct charge_ic_info     main_charge_info;
	struct charge_ic_info     cp_master_info;
	struct charge_ic_info     cp_slave_info;
	struct charge_ic_info     fg_info;

	bool                      is_bringup;

	struct votable *hq_ibat_votable;
	struct votable *hq_ibus_votable;
	struct votable *hq_cv_votable;
	struct votable *hq_iterm_votable;
};

struct battery_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define BATTERY_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define BATTERY_CHAN_ENERGY(_name, _num)			\
	BATTERY_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct battery_iio_channels battery_iio_channels[] = {
	BATTERY_CHAN_ENERGY("pd_active", PSY_IIO_PD_ACTIVE)
	BATTERY_CHAN_ENERGY("pd_usb_suspend_supported", PSY_IIO_PD_USB_SUSPEND_SUPPORTED)
	BATTERY_CHAN_ENERGY("pd_in_hard_reset", PSY_IIO_PD_IN_HARD_RESET)
	BATTERY_CHAN_ENERGY("pd_current_max", PSY_IIO_PD_CURRENT_MAX)
	BATTERY_CHAN_ENERGY("pd_voltage_min", PSY_IIO_PD_VOLTAGE_MIN)
	BATTERY_CHAN_ENERGY("pd_voltage_max", PSY_IIO_PD_VOLTAGE_MAX)
	BATTERY_CHAN_ENERGY("real_type", PSY_IIO_USB_REAL_TYPE)
	BATTERY_CHAN_ENERGY("otg_enable", PSY_IIO_OTG_ENABLE)
	BATTERY_CHAN_ENERGY("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	BATTERY_CHAN_ENERGY("apdo_max_volt", PSY_IIO_APDO_MAX_VOLT)
	BATTERY_CHAN_ENERGY("apdo_max_curr", PSY_IIO_APDO_MAX_CURR)
	BATTERY_CHAN_ENERGY("typec_mode", PSY_IIO_TYPEC_MODE)
};

//#define BATT_IIO_CHANNEL_ENUM_START          0
//#define FG_IIO_ENUM_START                    0             // fuel gaued start
//#define MAIN_IIO_ENUM_START                  30            // main charge start
//#define CP_MASTER_IIO_ENUM_START             60            // first master charge pump start eg:ln8000
//#define CP_SLAVE_IIO_ENUM_START              80            // first slaver charge pump start eg:ln8000
//#define CP_MASTER_SECOND_IIO_ENUM_START      100           // second master charge pump start eg:sc8551
//#define CP_SLAVE_SECOND_IIO_ENUM_START       120           // second slave charge pump start eg:sc8551


#define CP_MASTER_OFFSET                     (CP_MASTER_SECOND_CHARGING_ENABLED - CP_MASTER_CHARGING_ENABLED)
#define CP_SLAVE_OFFSET                      (CP_SLAVE_SECOND_CHARGING_ENABLED - CP_SLAVE_CHARGING_ENABLED)

enum batt_iio_option {
	BATT_IIO_OPT_READ = 1,
	BATT_IIO_OPT_WRITE = 2,
};

enum batt_ext_iio_channels {
	BATT_IIO_CHANNEL_ENUM_START = 0,
	BATT_IIO_CHANNEL_START = BATT_IIO_CHANNEL_ENUM_START,

	// fuel gaued start
	FG_IIO_ENUM_START = BATT_IIO_CHANNEL_START,
	FG_PRESENT = FG_IIO_ENUM_START,
	FG_STATUS,
	FG_CAPACITY,
	FG_CURRENT_NOW,
	FG_VOLTAGE_NOW,
	FG_VOLTAGE_MAX,
	FG_CHARGE_FULL,
	FG_RESISTANCE_ID,
	FG_TEMP,
	FG_CYCLE_COUNT,
	FG_CHARGE_FULL_DESIGN,
	FG_TIME_TO_FULL_NOW,
	FG_TIME_TO_EMPTY_NOW,
	FG_FCC_MAX,
	FG_CHIP_OK,
	FG_BATTERY_AUTH,
	FG_SOC_DECIMAL,
	FG_SOC_DECIMAL_RATE,
	FG_BATTERY_ID,
	FG_CC_SOC,
	FG_SHUTDOWN_DELAY,
	FG_FASTCHARGE_MODE,
	FG_IIO_ENUM_END = FG_FASTCHARGE_MODE,

	// main charge start
	MAIN_IIO_ENUM_START,
	MAIN_CHARGER_DONE = MAIN_IIO_ENUM_START,
	MAIN_CHARGER_HZ,
	MAIN_INPUT_CURRENT_SETTLED,
	MAIN_INPUT_VOLTAGE_SETTLED,
	MAIN_CHAGER_CURRENT,
	MAIN_CHARGING_ENABLED,
	MAIN_OTG_ENABLE,
	MAIN_CHAGER_TERM,
	MAIN_CHARGER_VOLTAGE_TERM,
	MAIN_CHARGER_STATUS,
	MAIN_CHARGER_TYPE,
	MAIN_BUS_VOLTAGE,
	MAIN_VBAT_VOLTAGE,
	MAIN_ENBALE_CHAGER_TERM,
	MAIN_IIO_ENUM_END = MAIN_ENBALE_CHAGER_TERM,

	// main charge start
	MAIN_SECOND_IIO_ENUM_START,
	MAIN_SECOND_IIO_ENUM_RESERVER1 = MAIN_SECOND_IIO_ENUM_START,
	MAIN_SECOND_IIO_ENUM_RESERVER2,
	MAIN_SECOND_IIO_ENUM_END = MAIN_SECOND_IIO_ENUM_RESERVER2,

	// first master charge pump start
	CP_MASTER_IIO_ENUM_START,
	CP_MASTER_CHARGING_ENABLED = CP_MASTER_IIO_ENUM_START,
	CP_MASTER_BATTERY_VOLTAGE,
	CP_MASTER_BUS_VOLTAGE,
	CP_MASTER_BUS_CURRENT,
	CP_MASTER_PRESENT,
	CP_MASTER_ADC_ENABLE,
	CP_MASTER_IIO_ENUM_END = CP_MASTER_ADC_ENABLE,

	// first slave charge pump start
	CP_SLAVE_IIO_ENUM_START,
	CP_SLAVE_CHARGING_ENABLED = CP_SLAVE_IIO_ENUM_START,
	CP_SLAVE_BATTERY_VOLTAGE,
	CP_SLAVE_BUS_VOLTAGE,
	CP_SLAVE_BUS_CURRENT,
	CP_SLAVE_PRESENT,
	CP_SLAVE_ADC_ENABLE,
	CP_SLAVE_IIO_ENUM_END = CP_SLAVE_ADC_ENABLE,

	// second master charge pump start
	CP_MASTER_SECOND_IIO_ENUM_START,
	CP_MASTER_SECOND_CHARGING_ENABLED = CP_MASTER_SECOND_IIO_ENUM_START,
	CP_MASTER_SECOND_BATTERY_VOLTAGE,
	CP_MASTER_SECOND_BUS_VOLTAGE,
	CP_MASTER_SECOND_BUS_CURRENT,
	CP_MASTER_SECOND_PRESENT,
	CP_MASTER_SECOND_ADC_ENABLE,
	CP_MASTER_SECOND_IIO_ENUM_END = CP_MASTER_SECOND_ADC_ENABLE,

	// second slave charge pump start
	CP_SLAVE_SECOND_IIO_ENUM_START,
	CP_SLAVE_SECOND_CHARGING_ENABLED = CP_SLAVE_SECOND_IIO_ENUM_START,
	CP_SLAVE_SECOND_BATTERY_VOLTAGE,
	CP_SLAVE_SECOND_BUS_VOLTAGE,
	CP_SLAVE_SECOND_BUS_CURRENT,
	CP_SLAVE_SECOND_PRESENT,
	CP_SALVE_SECOND_ADC_ENABLE,
	CP_SLAVE_SECOND_IIO_ENUM_END = CP_SALVE_SECOND_ADC_ENABLE,

	BATT_IIO_CHANNEL_END = CP_SLAVE_SECOND_IIO_ENUM_END,
};

static const char * const batt_ext_iio_chan_name[] = {
	// fuel gaued start
	[FG_PRESENT] = "present",
	[FG_STATUS] = "status",
	[FG_CAPACITY] = "capacity",
	[FG_CURRENT_NOW] = "current_now",
	[FG_VOLTAGE_NOW] = "voltage_now",
	[FG_VOLTAGE_MAX] = "voltage_max",
	[FG_CHARGE_FULL] = "charge_full",
	[FG_RESISTANCE_ID] = "resistance_id",
	[FG_TEMP] = "temp",
	[FG_CYCLE_COUNT] = "cycle_count",
	[FG_CHARGE_FULL_DESIGN] = "charge_full_design",
	[FG_TIME_TO_FULL_NOW] = "time_to_full_now",
	[FG_TIME_TO_EMPTY_NOW] = "time_to_empty_now",
	[FG_FCC_MAX] = "therm_curr",
	[FG_CHIP_OK] = "chip_ok",
	[FG_BATTERY_AUTH] = "battery_auth",
	[FG_SOC_DECIMAL] = "soc_decimal",
	[FG_SOC_DECIMAL_RATE] = "soc_decimal_rate",
	[FG_BATTERY_ID] = "battery_id",
	[FG_CC_SOC] = "rsoc",
	[FG_SHUTDOWN_DELAY] = "shutdown_delay",
	[FG_FASTCHARGE_MODE] = "fastcharge_mode",

	// main charge start
	[MAIN_CHARGER_DONE] = "charge_done",
	[MAIN_CHARGER_HZ] = "main_chager_hz",
	[MAIN_INPUT_CURRENT_SETTLED] = "main_input_current_settled",
	[MAIN_INPUT_VOLTAGE_SETTLED] = "input_voltage_settled",
	[MAIN_CHAGER_CURRENT] = "main_charge_current",
	[MAIN_CHARGING_ENABLED] = "charger_enable",
	[MAIN_OTG_ENABLE] = "otg_enable",
	[MAIN_CHAGER_TERM] = "main_charger_term",
	[MAIN_CHARGER_VOLTAGE_TERM] = "batt_voltage_term",
	[MAIN_CHARGER_STATUS] = "charger_status",
	[MAIN_CHARGER_TYPE] = "charger_type",
	[MAIN_BUS_VOLTAGE] = "vbus_voltage",
	[MAIN_VBAT_VOLTAGE] = "vbat_voltage",
	[MAIN_ENBALE_CHAGER_TERM] = "enable_charger_term",

	// first master charge pump start
	[CP_MASTER_CHARGING_ENABLED] = "charging_enabled",
	[CP_MASTER_BATTERY_VOLTAGE] = "sc_battery_voltage",
	[CP_MASTER_BUS_VOLTAGE] = "sc_bus_voltage",
	[CP_MASTER_BUS_CURRENT] = "sc_bus_current",
	[CP_MASTER_PRESENT] = "present",
	[CP_MASTER_ADC_ENABLE] = "sc_enable_adc",

	// second master charge pump start
	[CP_MASTER_SECOND_CHARGING_ENABLED] = "ln_charging_enabled",
	[CP_MASTER_SECOND_BATTERY_VOLTAGE] = "ln_battery_voltage",
	[CP_MASTER_SECOND_BUS_VOLTAGE] = "ln_bus_voltage",
	[CP_MASTER_SECOND_BUS_CURRENT] = "ln_bus_current",
	[CP_MASTER_SECOND_PRESENT] = "ln_present",
	[CP_MASTER_SECOND_ADC_ENABLE] = NULL,

	// first slave charge pump start
	[CP_SLAVE_CHARGING_ENABLED] = "charging_enabled_slave",
	[CP_SLAVE_BUS_VOLTAGE] = "sc_bus_voltage_slave",
	[CP_SLAVE_BUS_CURRENT] = "sc_bus_current_slave",
	[CP_SLAVE_PRESENT] = "present_slave",
	[CP_SLAVE_ADC_ENABLE] = "sc_enable_adc_slave",

	// second slave charge pump start
	[CP_SLAVE_SECOND_CHARGING_ENABLED] = "ln_charging_enabled_slave",
	[CP_SLAVE_SECOND_BUS_VOLTAGE] = "ln_bus_voltage_slave",
	[CP_SLAVE_SECOND_BUS_CURRENT] = "ln_bus_current_slave",
	[CP_SLAVE_SECOND_PRESENT] = "ln_present_slave",
	[CP_SALVE_SECOND_ADC_ENABLE] = NULL,
};


int batt_read_iio(struct batt_chg *chg, int channel, int *val);
int batt_write_iio(struct batt_chg *chg, int channel, int *val);


int batt_read_iio_force(struct batt_chg *chg, int channel, int *val);
int batt_write_iio_force(struct batt_chg *chg, int channel, int *val);

#endif
