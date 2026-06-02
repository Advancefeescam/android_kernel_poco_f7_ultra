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

#include "lxchg_class.h"
#include "mtk_battery.h"
#include "mtk_gauge.h"


#define CHARGER_VINDPM_USE_DYNAMIC         1
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT1    4000
#define CHARGER_VINDPM_DYNAMIC_VALUE1      4300
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT2    4500
#define CHARGER_VINDPM_DYNAMIC_VALUE2      4400
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT3    4300
#define CHARGER_VINDPM_DYNAMIC_VALUE3      4500
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT4    4400
#define FLOAT_DELAY_TIME                   2000
#define CHARGER_VINDPM_DYNAMIC_VALUE4      4700

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
#define TYPICAL_CAPACITY                   6000000
#define MTBF_CURRENT                       1500

#define POWER_SUPPLY_MANUFACTURER          "LIXUN"
#define POWER_SUPPLY_MODEL_NAME            "Main chg Driver"

#define FASTCHARGE_MIN_CURR                1800
#define CHARGER_MANAGER_LOOP_TIME_LOW_TEMP 2000    // 2s
#define CHARGER_MANAGER_LOOP_TIME          5000    // 5s
#define CHARGER_MANAGER_LOOP_TIME_OUT      20000   // 20s
#define MAX_UEVENT_LENGTH                  50
#define SHUTDOWN_DELAY_VOL_LOW             3300
#define SHUTDOWN_DELAY_VOL_COLD_TEMP       3150
#define SHUTDOWN_DELAY_VOL_HIGH            3450

#define RECHARGE_RSOC_THRESHOLD            9710
#define EEA_RECHARGE_UISOC_THRESHOLD       95

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
#define MAX_BAT_NUM_SUPPORT 3

enum battery_temp_level {
	TEMP_LEVEL_COLD,
	TEMP_LEVEL_COOL,
	TEMP_LEVEL_GOOD,
	TEMP_LEVEL_WARM,
	TEMP_LEVEL_HOT,
	TEMP_LEVEL_MAX,
};

enum {
	FFC_THERM_PARSE_ERROR = 1,
	NORMAL_THERM_PARSE_ERROR,
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
       int screen_on;
};

enum blank_flag{
	DEFAULT_STAT = 0,
	BLACK_TO_BRIGHT = 1,
	BRIGHT = 2,
	BLACK = 3,
};

#define CYCLE_COUNT_MAX 4
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

enum uisoc_track_reason{
	NO_TRACK = 0,
	FULL_INCREASE,
	RECOVER_FROM_FULL,
	RECHG_DECREASE,
	RECOVER_FROM_RECHG
};

enum reverse_quick_charge_state{
	REVCHG_LOW_POWER,
	REVCHG_LIMIT,
	REVCHG_NORMAL,
	REVCHG_QUICK_9,
	REVCHG_QUICK_22_5,
};

struct timespec {
    long tv_sec; /* seconds */
    long tv_nsec; /* nanoseconds */
};

struct jeita_ffc_cfg {
	int low_threshold;
	int high_threshold;
	int ffc_ibat_1;
	int ffc_vbat_1;
	int ffc_ibat_2;
	int ffc_vbat_2;
	int ffc_ibat_3;
	int ffc_fv;
	int ffc_iterm[MAX_BAT_NUM_SUPPORT];
};
struct jeita_normal_cfg {
	int low_threshold;
	int high_threshold;
	int normal_ibat_1;
	int normal_vbat_1;
	int normal_ibat_2;
	int normal_fv;
	int normal_iterm[MAX_BAT_NUM_SUPPORT];
};

struct rechg_fv_offset_cfg {
	int temp_level_l;
	int temp_level_h;
	int value;
};

struct chg_cycle_data {
	int cycle_array_size;
	int *cycle_data;
	int temp_array_size;
	int *temp_data;
	int **vol_data;
	int vol_array_rows;
	int vol_array_cols;
	int **curr_data;
	int curr_array_rows;
	int curr_array_cols;
	int last_curr_value;
	int last_vol_index;
	int last_temp_index;
};

struct lxchg_jeita {
	struct device *dev;
	int plugin_normal_index;
	int plugin_ffc_index;
	bool normal_curr_shake;
	bool fcc_curr_shake;
	/* jeita config */
	struct jeita_ffc_cfg *ffc_cfg;
	struct jeita_normal_cfg *normal_cfg;
	struct rechg_fv_offset_cfg *rechg_fv_offset_data;
	int ffc_cfg_cnt;
	int normal_cfg_cnt;
	int rechg_fv_offset_data_cnt;

	struct chg_cycle_data *ffc_cycle_data;
	struct chg_cycle_data *normal_cycle_data;
	void (*jeita_handle)(struct lxchg_jeita *jeita);
	int			final_iterm;
	int			soft_iterm_threshold;
	int			soft_fv_threshold;
	int			soft_soc_threshold;
	int			hard_iterm;
	int			soft_cc;
	int			soft_rechg_offset;
	int			soft_rechg_rsoc;
};

struct charger_manager {
	struct device *dev;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	bool run_thread;
	struct timer_list charger_timer;
	struct delayed_work bc12_retry_work;
	int bc12_retry_count;
	bool bc12_retry_for_mi_pd;
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
	bool boot_complete;
	bool not_charging;
	bool above_45_temp_term;
	bool is_pr_swap;
	bool pd_contract_update;
	int qc3_support;
	bool qc_detected;
	bool charger_online;
	bool delay_iindpm;
	bool otg_online;
	bool shutdown_delay;
	bool last_shutdown_delay;
	bool jeita_stop_charge;
	bool fg_not_ready;
	int rsoc;
	int term_rsoc;
	int uisoc;
	int max_uisoc;
	int ibat;
	int avg_ibat;
	int vbat;
	int tbat;
	int vbus;
	int ibus;
	int fv;
	int ichg;
	int iterm;
	int ilimit;
	int chg_status;
	int temp_compensate;
	int rsoc_keep100_sec;
	int uisoc_keep100_sec;
	bool hiz_en;
	bool is_charge_done;
	int32_t chg_adc[CHG_ADC_MAX];
	int typec_mode;
	int boot_gm_cycle;
	int batt_cycle;
	int charge_full_car;
	int fake_tbat;
	int fake_soc;
	int fake_batt_cycle;
	bool pd30_source;
	bool revchg_bcl;
	bool reverse_quick_charge_enabled;
	int aicl_test;
	int last_pdo_caps;
	int ibat_check_cnt;
	u32 bootmode;
	bool ldp_event;
	bool soft_reset_state;
	enum chg_type real_type;
	enum otg_status otg_stat;
	struct delayed_work reverse_quick_charge_work;
	struct wakeup_source *reverse_charge_wakelock;
	struct lxchg_jeita *jeita;
	struct mtk_gauge *mtk_gauge;

	/* psy add here */
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *fg_psy;
	struct power_supply *chg_psy;
	struct power_supply *cp_master_psy;
	struct power_supply *cp_slave_psy;
	struct power_supply_desc usb_psy_desc;
	struct power_supply *mtk_gauge_psy;

	/* voter add here */
	int vote_fv;
	int vote_iterm;
	int vote_icharge;
	int vote_total_fcc;
	int vote_cp_disable;
	int vote_input_limit;
	const char *total_effect_voter;
	const char *icharge_effect_voter;
	const char *input_effect_voter;
	const char *fv_effect_voter;
	const char *iterm_effect_voter;
	struct votable *icharge_votable;
	struct votable *fv_votable;
	struct votable *input_limit_votable;
	struct votable *iterm_votable;
	struct votable *cp_disable_votable;
	struct votable *total_fcc_votable;
	struct timespec64 ts64;
	struct timespec tv;
	struct rtc_time tm;

	/* charge current add here*/
	int pd_curr_max;
	int pd_volt_max;
	int usb_iterm;
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
	int *ffc_thermal_mitigation;
	int *normal_thermal_mitigation;
	int ffc_thermal_levels;
	int normal_thermal_levels;

	/********dts setting********/
	bool shipmode;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	struct smart_chg smart_charge[SMART_CHG_FEATURE_MAX_NUM + 1];
	int smart_chg_cmd;
	struct delayed_work xm_charge_work;
	struct delayed_work thermal_restore_work;
	bool is_eea;
	int smart_batt;
	bool night_charging;
	struct charger_screen_monitor sm;
	int thermal_board_temp;
	int fake_board_thermal;
	int *xmchg_low_soc_fast;
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
	bool support_ui_otg;
	bool ui_cc_toggle;
	bool cid_status;
	bool typec_attach;
	struct alarm rust_det_work_timer;
	struct delayed_work hrtime_toggle_work;
	struct delayed_work set_cc_drp_work;
	struct delayed_work smooth_work;
	struct delayed_work qc_detected_retry_work;

	bool plug_in_soc100_flag;
	int c_car_in;
	int v_car_in;
	int c_car_out;
	int v_car_out;

#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
	struct delayed_work batt_soh20_aging_test;
#endif

	bool authentic;

#ifdef FACTORY_BUILD
	int ato_soc_user_control;
	bool is_ato_soc_control;
#endif

	/********soft iterm********/
	bool eea_is_term;
	int soft_term_status;
	int soft_term_check_cnt;
};

/*********extern func/struct/int start***********/
extern int lxchg_usb_psy_register(struct charger_manager *manager);
extern int lxchg_battery_psy_register(struct charger_manager *manager);
extern int lxchg_charger_psy_register(struct charger_manager *manager);
extern int lx_usb_sysfs_create_group(struct charger_manager *manager);
extern int lx_batt_sysfs_create_group(struct charger_manager *manager);
extern void lx_set_prop_system_temp_level(struct charger_manager *manager, char *voter_name);
extern int charger_manager_get_current(struct charger_manager *manager, int *curr);
extern void xm_smart_stop_charge_ctrl(struct charger_manager *manager, const char *client_str, bool en);
extern bool is_disable_chg_by_client(struct charger_manager *manager, const char *client_str);
extern bool is_input_suspend_by_client(struct charger_manager *manager, const char *client_str);
extern bool is_disable_chg(struct charger_manager *manager);
extern bool is_input_suspend(struct charger_manager *manager);
extern int xm_chg_dfs_init(struct charger_manager *manager);
extern int chargerpump_policy_init(struct charger_manager *manager);
extern int xm_pd_adapter_init(struct charger_manager *manager);
extern int qomcharger_policy_init(struct charger_manager *manager);
extern int pd_adapter_init(struct charger_manager *manager);
extern int lxchg_class_init(void);
extern int ds28e30_secret_init(void);
extern int slg_secret_init(void);
extern int stick_secret_init(void);
extern int bq28z610_fg_init(void);
extern int mt6358_fg_init(void);
extern int sc6601_base_init(void);
extern int sc6601_charger_init(void);
extern int sc6601_cid_init(void);
extern int nu6601_base_init(void);
extern int nu6601_charger_init(void);
extern int sc8541_chargepump_init(void);
extern int bq25960_chargepump_init(void);
extern void soft_iterm_work(struct work_struct *work);
#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
extern int lx_fuel_algo_init(struct charger_manager *manager);
#endif

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
