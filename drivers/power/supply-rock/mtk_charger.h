/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/alarmtimer.h>
#include "charger_class.h"
#include "adapter_class.h"
#include "mtk_charger_algorithm_class.h"
#include <linux/power_supply.h>
#include "mtk_smartcharging.h"

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

#define SC_TAG "smartcharging"

extern int chr_get_debug_level(void);

#define chr_err(fmt, args...)					\
do {								\
	if (1) {	\
		pr_err(fmt, ##args);				\
	}							\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_INFO_LEVEL) {	\
		pr_notice_ratelimited(fmt, ##args);		\
	}							\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

struct mtk_charger;
struct charger_data;
/*L19A HQ-194293 add recharge ways by tongjiacheng at 2022/04/20 start*/
#define BATTERY_CV 4450000
/*L19A HQ-194293 add recharge ways by tongjiacheng at 2022/04/20 end*/
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		2050000
#define NON_STD_AC_CHARGER_CURRENT		1000000
#define CHARGING_HOST_CHARGER_CURRENT		1500000

/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1800000 /* 1.8 A */

/*L19A HQ-194293 add recharge ways by tongjiacheng at 2022/04/20 start*/
#define RECHARGER_UISOC_LIMIT 99 /*99%*/
/*L19A HQ-194293 add recharge ways by tongjiacheng at 2022/04/20 end*/

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)
#define CHG_TYPEC_WD_STATUS	(1 << 6)
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 start*/
/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	3
#define MAX_CHARGE_TEMP  55
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 end*/
#define MAX_ALG_NO 10
/*L19A HQ-216528 add cycle count by tongjiacheng at 2022/06/08 start*/
#define FFC_CV_1 4448000
#define FFC_CV_2 4432000
#define FFC_CV_3 4416000
#define CHG_CYCLEC_COUNT_LEVEL1 1
#define CHG_CYCLEC_COUNT_LEVEL2 100
#define CHG_CYCLEC_COUNT_LEVEL3 300
/*L19A HQ-216528 add cycle count by tongjiacheng at 2022/06/08 end*/

enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH
};

enum chg_dev_notifier_events {
	EVENT_FULL,
	EVENT_RECHARGE,
	EVENT_DISCHARGE,
};

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 start*/
/* sw jeita */
#define JEITA_TEMP_ABOVE_T5_CV	4100000
#define JEITA_TEMP_T4_TO_T5_CV	4100000
#define JEITA_TEMP_T3_TO_T4_CV	4450000
#define JEITA_TEMP_T2_TO_T3_CV	4450000
#define JEITA_TEMP_T1_TO_T2_CV	4450000
#define JEITA_TEMP_T0_TO_T1_CV	4450000
#define JEITA_TEMP_BELOW_T0_CV	4450000
#define JEITA_TEMP_T4_TO_T5_CC	2450000
#define JEITA_TEMP_T3_TO_T4_CC	3000000
#define JEITA_TEMP_T2_TO_T3_CC	1470000
#define JEITA_TEMP_T1_TO_T2_CC	490000
#define JEITA_TEMP_T0_TO_T1_CC	490000
#define JEITA_TEMP_BELOW_T0_CC	490000
#define TEMP_T5_THRES  55
#define TEMP_T5_THRES_MINUS_X_DEGREE 55
#define TEMP_T4_THRES  45
#define TEMP_T4_THRES_PLUS_X_DEGREE 45
#define TEMP_T3_THRES  15
#define TEMP_T3_THRES_PLUS_X_DEGREE 15
#define TEMP_T2_THRES  10
#define TEMP_T2_THRES_PLUS_X_DEGREE 10
#define TEMP_T1_THRES  5
#define TEMP_T1_THRES_PLUS_X_DEGREE 5
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_NEG_10_THRES 0
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 end*/
/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 start*/
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_T4_TO_T5,
	TEMP_ABOVE_T5,
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	int cc;
	bool charging;
	bool error_recovery_flag;
};
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 end*/
struct mtk_charger_algorithm {

	int (*do_algorithm)(struct mtk_charger *info);
	int (*enable_charging)(struct mtk_charger *info, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*do_dvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_dvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int charging_host_charger_current;
	int non_std_ac_charger_current;
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 start*/
	/* sw jeita */
	int jeita_temp_above_t5_cv;
	int jeita_temp_t4_to_t5_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	int jeita_temp_t4_to_t5_cc;
	int jeita_temp_t3_to_t4_cc;
	int jeita_temp_t2_to_t3_cc;
	int jeita_temp_t1_to_t2_cc;
	int jeita_temp_t0_to_t1_cc;
	int jeita_temp_below_t0_cc;
	int temp_t5_thres;
	int temp_t5_thres_minus_x_degree;
	int temp_t4_thres;
	int temp_t4_thres_plus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_plus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;
/*L19A HQ-194878 add jeita ways by miaozhichao at 2022/04/27 end*/
	/* battery temperature protection */
	int mtk_temperature_recharge_support;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;

	/* dynamic mivr */
	int min_charger_voltage_1;
	int min_charger_voltage_2;
	int max_dmivr_charger_current;

	/*L19A HQ-194293 add recharge ways by tongjiacheng at 2022/04/20 start*/
	int recharger_uisoc_limit;
	/*L19A HQ-194293 add recharge ways by tongjiacheng at 2022/04/20 end*/
};

struct charger_data {
	int input_current_limit;
	int charging_current_limit;

	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
};

enum chg_data_idx_enum {
	CHG1_SETTING,
	CHG2_SETTING,
	DVCHG1_SETTING,
	DVCHG2_SETTING,
	CHGS_SETTING_MAX,
};

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;
	struct charger_device *dvchg1_dev;
	struct notifier_block dvchg1_nb;
	struct charger_device *dvchg2_dev;
	struct notifier_block dvchg2_nb;

	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

	struct power_supply_desc psy_dvchg_desc1;
	struct power_supply_config psy_dvchg_cfg1;
	struct power_supply *psy_dvchg1;

	struct power_supply_desc psy_dvchg_desc2;
	struct power_supply_config psy_dvchg_cfg2;
	struct power_supply *psy_dvchg2;

	struct power_supply  *chg_psy;
	struct power_supply  *bat_psy;
	struct adapter_device *pd_adapter;
	struct notifier_block pd_nb;
	struct notifier_block tcpc_nb;
	struct mutex pd_lock;
	int pd_type;
	bool pd_reset;

	u32 bootmode;
	u32 boottype;

	int chr_type;
	int usb_type;
	int usb_state;

	struct mutex cable_out_lock;
	int cable_out_cnt;

	/* system lock */
	spinlock_t slock;
	struct wakeup_source *charger_wakelock;
	struct mutex charger_lock;

	/* thread related */
	wait_queue_head_t  wait_que;
	bool charger_thread_timeout;
	unsigned int polling_interval;
	bool charger_thread_polling;

	/* alarm timer */
	struct alarm charger_timer;
	struct timespec64 endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;
	ktime_t timer_cb_duration[8];

	/* notify charger user */
	struct srcu_notifier_head evt_nh;

	/* common info */
	int log_level;
	bool usb_unlimited;
	bool charger_unlimited;
	bool disable_charger;
	bool disable_aicl;
	int battery_temp;
	bool can_charging;
	bool cmd_discharging;
	bool safety_timeout;
	int safety_timer_cmd;
	bool vbusov_stat;
	bool is_chg_done;
	/* ATM */
	bool atm_enabled;
	bool enable_type_c;
	const char *algorithm_name;
	struct mtk_charger_algorithm algo;

	/* dtsi custom data */
	struct charger_custom_data data;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* sw safety timer */
	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;
	struct timespec64 charging_begin_time;

	/* vbat monitor, 6pin bat */
	bool batpro_done;
	bool enable_vbat_mon;
	bool enable_vbat_mon_bak;
	int old_cv;
	bool stop_6pin_re_en;
	int vbat0_flag;

	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	struct chg_alg_device *alg[MAX_ALG_NO];
	struct notifier_block chg_alg_nb;
	bool enable_hv_charging;

	/* water detection */
	bool water_detected;

	bool enable_dynamic_mivr;

	/* fast charging algo support indicator */
	bool enable_fast_charging_indicator;
	unsigned int fast_charging_indicator;

	/* diasable meta current limit for testing */
	unsigned int enable_meta_current_limit;

	struct smartcharging sc;

	/*daemon related*/
	struct sock *daemo_nl_sk;
	u_int g_scd_pid;
	struct scd_cmd_param_t_1 sc_data;

	/*charger IC charging status*/
	bool is_charging;

	ktime_t uevent_time_check;

	bool force_disable_pp[CHG2_SETTING + 1];
	bool enable_pp[CHG2_SETTING + 1];
	struct mutex pp_lock[CHG2_SETTING + 1];
/*L19AT code for HQ-256463 by miaozhichao at 2022/11/28 start*/
	struct delayed_work battery_psy_detect_work;
/*L19AT code for HQ-256463 by miaozhichao at 2022/11/28 end*/
/*L19A L19A-15 add thermal limit current by tongjiacheng at 2022/04/26 start*/
	int system_temp_level;
/*L19A L19A-15 add thermal limit current by tongjiacheng at 2022/04/26 end*/
/*L19A HQ-194263 add  quick charge type by tongjiacheng at 2022/05/10 start*/
	struct power_supply *psy_usb;
	struct power_supply_desc psy_usb_desc;
	struct power_supply_config psy_usb_cfg;
/*L19A HQ-194263add  quick charge type by tongjiacheng at 2022/05/10 end*/
/*L19A HQ-216528 add cycle count by tongjiacheng at 2022/06/08 start*/
	bool enable_sw_ffc;
	int ffc_cv_1;
	int ffc_cv_2;
	int ffc_cv_3;
	int chg_cycle_count_level1;
	int chg_cycle_count_level2;
	int chg_cycle_count_level3;
/*L19A HQ-216528 add cycle count by tongjiacheng at 2022/06/08 end*/
};

static inline int mtk_chg_alg_notify_call(struct mtk_charger *info,
					  enum chg_alg_notifier_events evt,
					  int value)
{
	int i;
	struct chg_alg_notify notify = {
		.evt = evt,
		.value = value,
	};

	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i])
			chg_alg_notifier_call(info->alg[i], &notify);
	}
	return 0;
}

/* functions which framework needs*/
extern int mtk_basic_charger_init(struct mtk_charger *info);
extern int mtk_pulse_charger_init(struct mtk_charger *info);
extern int get_uisoc(struct mtk_charger *info);
extern int get_battery_voltage(struct mtk_charger *info);
extern int get_battery_temperature(struct mtk_charger *info);
extern int get_battery_current(struct mtk_charger *info);
extern int get_vbus(struct mtk_charger *info);
extern int get_ibat(struct mtk_charger *info);
extern int get_ibus(struct mtk_charger *info);
extern bool is_battery_exist(struct mtk_charger *info);
extern int get_charger_type(struct mtk_charger *info);
extern int get_usb_type(struct mtk_charger *info);
extern int disable_hw_ovp(struct mtk_charger *info, int en);
extern bool is_charger_exist(struct mtk_charger *info);
extern int get_charger_temperature(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_charging_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_input_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_zcv(struct mtk_charger *info,
	struct charger_device *chg);
extern void _wake_up_charger(struct mtk_charger *info);

/* functions for other */
extern int mtk_chg_enable_vbus_ovp(bool enable);
/*L19A L19A-15 add thermal limit current by tongjiacheng at 2022/04/26 start*/
extern int charger_manager_get_system_temp_level(void);
void charger_manager_set_system_temp_level( int temp_level);
/*L19A L19A-15 add thermal limit current by tongjiacheng at 2022/04/26 end*/

#endif /* __MTK_CHARGER_H */
