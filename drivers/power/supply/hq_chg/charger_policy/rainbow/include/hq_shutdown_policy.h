#ifndef __HQ_SHUTDOWN_POLICY_H__
#define __HQ_SHUTDOWN_POLICY_H__

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_SHUTDOWN]"
#endif

#define DEFAULT_SHUTDOWN_VOL            3350
#define DEFAULT_SHUTDOWN_DELAY_VOL      3400
#define INCREASE_FREQUENCY_VOL          3500
#define DEFAULT_FG_TERM_VOL             3051
#define MAX_TERMV_SOH_TABLE_CNT         6

enum hq_temp_level {
	BAT_COLD,
	BAT_LITTLE_COLD,
	BAT_COOL,
	BAT_NORMAL,
	BAT_HOT,
};

struct delta_termV_SOH {
	u8 delta_termV_index;
	u8 delta_soh;
};

static struct delta_termV_SOH delta_termV_SOH_table[] = {
	{1, 2}, /*51 - 100mV*/
	{2, 3}, /*101 - 150mV*/
	{3, 4}, /*151 - 200mV*/
	{4, 6}, /*201 - 250mV*/
	{5, 7}, /*251 - 300mV*/
	{6, 9}, /*301 - 350mV*/
};

struct hq_shutdown_policy {
	struct device *dev;
	struct mutex shutdown_check_lock;
	struct fuel_gauge_dev *fuel_gauge;

	/* psy */
	struct power_supply *batt_psy;
	struct power_supply *bms_psy;
	struct power_supply *usb_psy;

	struct delayed_work policy_check_work;

	bool last_shutdown_delay;
	bool shutdown_delay;
	bool SOC0_shutdown;
	int shutdown_vol;
	int shutdown_delay_vol;
	int shutdown_fg_vol;
	int vbat;
	int tbat;
	int ibat;
	int batt_status;
	int cycle_count;
	int dod_count;
	int fg_soc;
	int batt_soc;
};

#endif
