/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_CHARGER_MANAGER_H__
#define __HQ_CHARGER_MANAGER_H__

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
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>

#include "../../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"

#include "hq_voter.h"
#include "hq_charger_class.h"
#include "hq_cp_class.h"
#include "hq_fg_class.h"
#include "hq_utils.h"
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "xm_adapter_class.h"
#endif

#include "hq_cp_policy.h"

#if IS_ENABLED(CONFIG_HQ_THERMAL_POLICY)
#include "hq_thermal_policy.h"
#endif

#if IS_ENABLED(CONFIG_HQ_JEITA_POLICY)
#include "hq_jeita_policy.h"
#endif

#if IS_ENABLED(CONFIG_HQ_TERM_RECHARGE_POLICY)
#include "hq_term_recharge_policy.h"
#endif

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
#include "xm_batt_health.h"
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "tcpm.h"
#include "tcpci_core.h"
#include "tcpci_typec.h"
#endif

#if IS_ENABLED(CONFIG_HQ_SHUTDOWN_POLICY)
#include "hq_shutdown_policy.h"
#endif

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
#include "hq_reverse_charge_policy.h"
#endif

#include "../battery_secrete/battery_auth_class.h"

#define NORMAL_CHG_VOLTAGE_MAX             4480000
#define FAST_CHG_VOLTAGE_MAX               4530000
#define VOLTAGE_MAX                        11000000
#define CURRENT_MAX                        12400000
#define INPUT_CURRENT_LIMIT                6100000
#define CP_EN_MAIN_CHG_CURR                100
#define TYPICAL_CAPACITY                   6500000
#define SUPER_CHARGE_POWER                 50

#define POWER_SUPPLY_MANUFACTURER          "HUAQIN"
#define POWER_SUPPLY_MODEL_NAME            "Main chg Driver"

#define BATTERY_WARM_TEMP                  480
#define BATTERY_HOT_TEMP                   580
#define BATTERY_COLD_TEMP                  -100

enum charger_vbus_type {
	CHARGER_VBUS_NONE,
	CHARGER_VBUS_USB_SDP,
	CHARGER_VBUS_USB_CDP,
	CHARGER_VBUS_USB_DCP,
	CHARGER_VBUS_HVDCP,
	CHARGER_VBUS_UNKNOWN,
	CHARGER_VBUS_NONSTAND,
	CHARGER_VBUS_OTG,
	CHARGER_VBUS_TYPE_NUM,
};

enum battery_temp_level {
	TEMP_LEVEL_COLD,
	TEMP_LEVEL_COOL,
	TEMP_LEVEL_GOOD,
	TEMP_LEVEL_WARM,
	TEMP_LEVEL_HOT,
	TEMP_LEVEL_MAX,
};

enum charge_mode {
	NORMAL_CHARGE_MODE,
	FFC_CHARGE_MODE,
	ALL_CHARGE_MODE,
};

enum screen_state {
	SCREEN_STATE_UNKONW = 0,
	SCREEN_STATE_BLACK  = 1,
	SCREEN_STATE_BRIGHT = 2,
	SCREEN_STATE_BLACK_TO_BRIGHT = 3,
};

enum board_version {
	OTHER_VERSION = 0,
	EEA_VERSION,
};

enum charger_fw_event {
	CHG_FW_EVT_ADAPTER_PLUGOUT = 0,
	CHG_FW_EVT_ADAPTER_PLUGIN,
	CHG_FW_EVT_FG_I2C_ERR,
	CHG_FW_EVT_SMART_ENDURA_TRIG,
	CHG_FW_EVT_LPD,
	CHG_FW_EVT_CP_EN,
	CHG_FW_EVT_VBUS_UVLO,
	CHG_FW_EVT_IBAT_OCP,
};

struct charger_manager {
	struct device *dev;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	bool run_thread;

	struct timer_list charger_timer;

	struct delayed_work charger_manager_misc_work;
	struct mutex charger_manager_misc_work_lock;

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	struct delayed_work wait_usb_ready_work;
	int get_usb_rdy_cnt;
	struct device_node *usb_node;
#endif

#if IS_ENABLED(CONFIG_HQ_THERMAL_POLICY)
	struct hq_thermal_policy *thermal_policy;
#endif

#if IS_ENABLED(CONFIG_HQ_JEITA_POLICY)
	struct hq_jeita_policy *jeita_policy;
#endif

#if IS_ENABLED(CONFIG_HQ_TERM_RECHARGE_POLICY)
	struct hq_term_recharge_policy *term_recharge_policy;
#endif

#if IS_ENABLED(CONFIG_HQ_SHUTDOWN_POLICY)
	struct hq_shutdown_policy *shutdown_policy;
#endif

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
	struct reverse_charge_policy *reverse_charge_policy;
#endif

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	struct xm_batt_health *batt_health;
#endif

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	struct xm_smart_chg *smart_chg;
#endif

	/* notifier add here */
	struct notifier_block charger_changed_nb;  /* notifier block of charger ic changed */
	struct notifier_block charger_nb;          /* notifier block of charger ic event */
	struct notifier_block fg_nb;          	   /* notifier block of fg ic event */
	struct notifier_block psy_nb;              /* notifier block of power supply property changed */
	struct notifier_block thermal_nb;          /* notifier block of thermal event */
	struct notifier_block disp_nb;             /* notifier block of display event */
	struct notifier_block audio_nb;            /* notifier block of audio event */
	struct notifier_block cid_nb;            /* notifier block of cid event */
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc;
	struct notifier_block tcpc_nb;    /* notifier block of tcpc event */
#endif

	/* ext_dev add here */
	struct charger_dev *charger;
	struct chargerpump_dev *master_cp_chg;
	struct chargerpump_dev *slave_cp_chg;
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	struct adapter_device *pd_adapter;
#endif
	struct fuel_gauge_dev *fuel_gauge;
	struct auth_device *auth_dev;


	/* flag add here */
	int pd_active;
	bool is_pr_swap;
	bool pd_contract_update;
	int qc3_mode;
	int input_suspend;
	bool qc_detected;
	bool dcp_power_detected;
	bool adapter_plug_in;
	bool usb_online;
	bool shutdown_delay;
	bool last_shutdown_delay;
	int soc;
	int rsoc;
	int ibat;
	int vbat;
	int tbat;
	int ibus;
	int chg_status;
	enum vbus_type vbus_type;
	int32_t chg_adc[CHG_ADC_MAX];
	int typec_mode;
	int batt_cycle;
	int fake_batt_cycle;
	int batt_raw_soh;
	int fg_raw_soh;
	int new_fg_raw_soh;
	bool is_dr_swap;
	int board_version;
	bool pd_done;
	bool pd_verifed;
	bool outdoor_chg;
	/* ffc_to_normal is true, means switch to normal charging */
	bool ffc_to_nomal;

	int batt_id;
	int charge_mode;
	bool is_usage_update;
	int cp_master_ok;

	/* psy add here */
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *fg_psy;
	struct power_supply *chg_psy;
	struct power_supply *cp_master_psy;
	struct power_supply *cp_slave_psy;
	struct power_supply_desc usb_psy_desc;

	/* voter add here */
	struct votable *main_fcc_votable;
	struct votable *fv_votable;
	struct votable *main_icl_votable;
	struct votable *iterm_votable;
	struct votable *main_chg_disable_votable;
	struct votable *cp_disable_votable;
	struct votable *total_fcc_votable;

	/* charge current settings accroding to charger type */
	int pd_curr_max;
	int pd_volt_max;
	int none_type_current;
	int sdp_current;
	int float_current;
	int cdp_current;
	int dcp_current;
	int hvdcp_current;
	int hvdcp_input_current;
	int hvdcp3_current;
	int hvdcp3_input_current;
	int pd2_current;
	int pd2_input_current;

	int charge_power_max;
	const char *model_name;

	int apdo_max;

	/* misc */
	bool warm_stop_charge;
	bool cold_stop_charge;
	bool hot_stop_charge;
	int screen_state;
	int audio_status;
	int cid_state;
	int board_temp;
	bool shipmode_flag;
	int mtbf_mode;

	/* detect single cell */
	bool single_cell_det;
	int c_car_in;
	int v_car_in;
	int c_car_out;
	int v_car_out;
	struct alarm start_cell_det_work_timer;
	struct alarm cell_det_work_timer;

	bool en_floatgnd;
	bool ui_cc_toggle;
	bool cid_status;
	bool soft_cid;
	bool typec_attach;
	struct mutex wakelock_mutex;
	struct wakeup_source *cm_wakelock;
	struct alarm otg_ui_close_timer;
	struct alarm set_soft_cid_timer;
	struct delayed_work handle_cc_status_work;
	struct delayed_work set_otg_ui_work;

	struct delayed_work set_detcell_work;

	struct mutex update_auth_param_lock;
	struct delayed_work update_fg_raw_soh_work;
	struct delayed_work update_fg_cycle_work;
	int fg_cycle;
	int new_fg_cycle;

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	struct delayed_work batt_soh20_aging_test;
#endif

#if IS_ENABLED(CONFIG_RUST_DETECTION)
	struct charger_dev *typec_switch_chg;
	struct delayed_work rust_detection_work;
	bool lpd_flag;
	int lpd_charging_limit;
#endif

	/* fake */
	int pps_fast_mode;

	/* float retry detect */
	struct delayed_work float_retry_detect_work;;
	atomic_t float_retry_pending;

	/* safetytimer 24h timeout */
	bool chg_timeout;
};

void xm_uevent_report(struct charger_manager *manager);

extern struct chargerpump_policy *g_policy;

/*********extern func/struct/int end***********/
extern int charger_manager_get_current(struct charger_manager *manager, int *curr);

#endif
