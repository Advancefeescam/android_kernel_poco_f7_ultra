#ifndef __LX_CHARGER_MANAGER_H__
#define __LX_CHARGER_MANAGER_H__

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
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

#include "../charger_class/lx_charger_class.h"

#define CHARGER_VINDPM_USE_DYNAMIC         1
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT1    3800
#define CHARGER_VINDPM_DYNAMIC_VALUE1      4000
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT2    4200
#define CHARGER_VINDPM_DYNAMIC_VALUE2      4400
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT3    4300
#define CHARGER_VINDPM_DYNAMIC_VALUE3      4500
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT4    4400
#define CHARGER_VINDPM_DYNAMIC_VALUE4      4600
#define CHARGER_VINDPM_DYNAMIC_VALUE5      4700
#define FLOAT_DELAY_TIME                   2000
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
#define WAIT_USB_RDY_TIME               100
#define WAIT_USB_RDY_MAX_CNT            300
#endif

#define NORMAL_CHG_VOLTAGE_MAX             4480000
#define FAST_CHG_VOLTAGE_MAX               4530000
#define VOLTAGE_MAX                        11000000
#define CURRENT_MAX                        12400000
#define INPUT_CURRENT_LIMIT                6100000
#define CP_EN_MAIN_CHG_CURR                100
#define TYPICAL_CAPACITY                   5350000
#define MTBF_CURRENT                       1500

#define POWER_SUPPLY_MANUFACTURER          "LIXUN"
#define POWER_SUPPLY_MODEL_NAME            "Main chg Driver"

#define FASTCHARGE_MIN_CURR                1800
#define CHARGER_MANAGER_LOOP_TIME          5000    // 5s
#define CHARGER_MANAGER_LOOP_TIME_OUT      20000   // 20s
#define MAX_UEVENT_LENGTH                  50
#define SHUTDOWN_DELAY_VOL_LOW             3300
#define SHUTDOWN_DELAY_VOL_COLD_TEMP       3050
#define SHUTDOWN_DELAY_VOL_HIGH            3400

#define BATTERY_WARM_TEMP                  480
#define BATTERY_HOT_TEMP                   580
#define BATTERY_COLD_TEMP                  -100

#define SUPER_CHARGE_POWER                 50

#define MIAN_CHG_ADC_LENGTH                180
#define PD20_ICHG_MULTIPLE                 1800
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
#define FG_I2C_ERR_SOC                     15
#define FG_I2C_ERR_VBUS                    6500
#endif

#define MBTF_SOC_LOW_THRESHOLD             (60-2)
#define MBTF_SOC_HIGH_THRESHOLD            (75+2)

#define AGING_SOC_LOW_THRESHOLD            (70-2)
#define AGING_SOC_HIGH_THRESHOLD           (75+2)

enum battery_temp_level {
	TEMP_LEVEL_COLD,
	TEMP_LEVEL_COOL,
	TEMP_LEVEL_GOOD,
	TEMP_LEVEL_WARM,
	TEMP_LEVEL_HOT,
	TEMP_LEVEL_MAX,
};

enum {
	PD_THERM_PARSE_ERROR = 1,
	QC2_THERM_PARSE_ERROR,
};

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
enum smart_chg_functype{
	SMART_CHG_STATUS_FLAG = 0,
	SMART_CHG_FEATURE_MIN_NUM = 1,
	SMART_CHG_NAVIGATION = 1,
	SMART_CHG_OUTDOOR_CHARGE,
	SMART_CHG_LOW_FAST = 3,
	SMART_CHG_ENDURANCE_PRO = 4,
	/* add new func here */
	SMART_CHG_FEATURE_MAX_NUM = 16,
};

struct smart_chg {
	bool en_ret;
	int active_status;
	int func_val;
};

struct charger_screen_monitor {
       struct notifier_block charger_panel_notifier;
       int screen_state;
};

enum blank_flag{
	NORMAL = 0,
	BLACK_TO_BRIGHT = 1,
	BRIGHT = 2,
	BLACK = 3,
};

#define CYCLE_COUNT_MAX 4
#endif

#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
#define PRODUCT_NAME_MAP_MAX_INDEX		2

enum product_index {
	UNKOWN = 0,
	EEA,
};

struct product_name_stru {
	char product_name[64];
	enum product_index index;
};
#endif

/*power-control*/
enum power_control_mode {
	DEFAULT_MODE = 0,
	MTBF_MODE = 1,
	AGING_MODE = 2,
};

enum otg_status{
	DIS_OTG = 0,
	BUCK_OTG,
	PUMP_OTG,
};

enum reverse_quick_charge_state{
	REVCHG_NORMAL = 0,
	REVCHG_QUICK_9,
	REVCHG_QUICK_22_5,
};

struct timespec {
    long tv_sec; /* seconds */
    long tv_nsec; /* nanoseconds */
};

struct charger_manager {
	struct device *dev;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	bool run_thread;

	struct timer_list charger_timer;
	struct delayed_work bc12_retry_work;
	int bc12_retry_count;
	#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	struct delayed_work wait_usb_ready_work;
	int get_usb_rdy_cnt;
	struct device_node *usb_node;
	#endif
	/* notifier add here */
	struct notifier_block charger_nb;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
#endif
#if IS_ENABLED(CONFIG_MIEV)        
	struct notifier_block charger_changed_nb;  
	struct notifier_block psy_nb;              
	struct notifier_block cp_nb;
	struct notifier_block dfs_nb;
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
	struct mutex report_lock;
	struct chargerpump_policy *cp_policy;

	/* flag add here */
	int pd_active;
	bool is_pr_swap;
	bool pd_contract_update;
	int qc3_mode;
	int charge_control_limit_max;
	bool qc_detected;
	bool adapter_plug_in;
	bool usb_online;
	bool shutdown_delay;
	bool last_shutdown_delay;
        int rsoc;
	int soc;
	int ibat;
	int vbat;
	int tbat;
	int chg_status;
	int32_t chg_adc[CHG_ADC_MAX];
	int typec_mode;
	int batt_cycle;
	int fake_batt_cycle;
	bool is_dr_swap;
	bool pd30_source;
	bool revchg_bcl;
	bool reverse_quick_charge_enabled;
	int last_pdo_caps;
	int ibat_check_cnt;
	enum otg_status otg_stat;
	struct delayed_work reverse_quick_charge_work;
	struct wakeup_source *reverse_charge_wakelock;

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
	struct votable *cp_disable_votable;
	struct votable *total_fcc_votable;
	struct timespec64 ts64;
	struct timespec tv;
	struct rtc_time tm;

	/* charge current add here*/
	int pd_curr_max;
	int pd_volt_max;
	int usb_current;
	int float_current;
	int cdp_current;
	int dcp_current;
	int hvdcp_charge_current;
	int hvdcp_input_current;
	int hvdcp3_charge_current;
	int hvdcp3_input_current;
	int pd2_charge_current;
	int pd2_input_current;
	int input_power_over;
	const char *model_name;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	int xm_outdoor_current;
	int apdo_max;
#endif

	/*thermal add here*/
	bool thermal_enable;
	int thermal_parse_flags;
	int system_temp_level;
	int *pd_thermal_mitigation;
	int *qc2_thermal_mitigation;
	int pd_thermal_levels;
	int qc2_thermal_levels;

	/********dts setting********/
	bool shipmode;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	struct smart_chg smart_charge[SMART_CHG_FEATURE_MAX_NUM + 1];
	int smart_chg_cmd;
	struct delayed_work xm_charge_work;
	struct delayed_work thermal_restore_work;

	int smart_batt;
	bool night_charging;
	int cyclecount[CYCLE_COUNT_MAX];
	struct charger_screen_monitor sm;
	int thermal_board_temp;
	int *pd_thermal_mitigation_fast;
	bool low_fast_plugin_flag;
	bool pps_fast_mode;
        int low_fast_ffc;
	enum blank_flag b_flag;
	int  is_full_flag;
	int smart_fv;
	bool fv_overvoltage_flag;
#endif
#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
	struct fuel_algo_data *fuel_algo;
#endif
#if IS_ENABLED(CONFIG_LIXUN_SOFT_ITERM_SUPPORT)
	int soft_term_status;
	int soft_term_check_cnt;
	struct delayed_work soft_iterm_work;
#endif

	bool en_floatgnd;
	bool ui_cc_toggle;
	bool cid_status;
	bool typec_attach;
	struct delayed_work hrtime_otg_work;
	struct alarm rust_det_work_timer;
	struct delayed_work set_cc_drp_work;

#if IS_ENABLED(CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT)
	struct delayed_work check_batt_cycle_count;
#endif

#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
	char product_name[64];
	char product_name_index;
#endif
	bool plug_in_soc100_flag;
	bool single_cell_det;
	int c_car_in;
	int v_car_in;
	int c_car_out;
	int v_car_out;
	int half_cell;
	bool en_single_cell;
	struct alarm start_cell_det_work_timer;
	struct alarm cell_det_work_timer;

#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
	struct delayed_work batt_soh20_aging_test;
#endif

	bool authentic;

#ifdef FACTORY_BUILD
	int ato_soc_user_control;
	bool is_ato_soc_control;
#endif
};

/*********extern func/struct/int start***********/
int cm_usb_psy_register(struct charger_manager *manager);
int cm_battery_psy_register(struct charger_manager *manager);
int cm_charger_psy_register(struct charger_manager *manager);
int lx_usb_sysfs_create_group(struct charger_manager *manager);
int lx_batt_sysfs_create_group(struct charger_manager *manager);
void lx_set_prop_system_temp_level(struct charger_manager *manager, char *voter_name);
int charger_manager_get_current(struct charger_manager *manager, int *curr);
void disable_chg_comm_ctrl(struct charger_manager *manager, const char *client_str, bool en);
bool is_disable_chg_by_client(struct charger_manager *manager, const char *client_str);
bool is_input_suspend_by_client(struct charger_manager *manager, const char *client_str);
bool is_disable_chg(struct charger_manager *manager);
bool is_input_suspend(struct charger_manager *manager);
int xm_chg_dfs_init(struct charger_manager *manager);
int chargerpump_policy_init(struct charger_manager *manager);
int xm_pd_adapter_init(struct charger_manager *manager);
int qomcharger_policy_init(struct charger_manager *manager);
int pd_adapter_init(struct charger_manager *manager);

enum xm_chg_uevent_type {
	CHG_UEVENT_DEFAULT_TYPE,
	CHG_UEVENT_SOC_DECIMAL,
	CHG_UEVENT_SOC_DECIMAL_RATE,
	CHG_UEVENT_QUICK_CHARGE_TYPE,
	CHG_UEVENT_SHUTDOWN_DELAY,
	CHG_UEVENT_CONNECTOR_TEMP,
	CHG_UEVENT_NTC_ALARM,
	CHG_UEVENT_LPD_DETECTION,
	CHG_UEVENT_REVERSE_QUICK_CHARGE,
	CHG_UEVENT_MAX_TYPE,
	CHG_UEVENT_CC_SHORT_VBUS,
};

extern int xm_charge_uevent_report(int event_type, int event_value);
extern int xm_charge_uevents_bundle_report(int bundle_type, ...);
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
void xm_uevent_report(struct charger_manager *manager);
#endif
// #if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
// extern struct adapter_device *get_adapter_by_name(const char *name);
// extern int adapter_get_usbpd_verifed(struct adapter_device *adapter_dev, bool *verifed);
// #endif
extern bool is_mtbf_mode_func(void);

/*********extern func/struct/int end***********/
#endif
