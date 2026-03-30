#ifndef __LINUX_HUAQIN_REVERSE_CHARGE_POLICY_H__
#define __LINUX_HUAQIN_REVERSE_CHARGE_POLICY_H__

#define	VBUS_0V	0
#define	VBUS_5V	5000
#define	VBUS_9V	9000

enum reverse_charge_watt{
	REVCHG_1_5W = 0,
	REVCHG_7_5W,
	REVCHG_QUICK_9W,
	REVCHG_QUICK_18W,
};

enum report_reverse_charge_state{
	REVCHG_NORMAL = 0,
	REVCHG_QUICK,
	REVCHG_DISABLE_BCL,
	REVCHG_ENABLE_BCL,
};

struct reverse_charge_policy {
	struct device *dev;
	struct fuel_gauge_dev *fuel_gauge;

	/* psy */
	struct power_supply *batt_psy;
	struct power_supply *bms_psy;
	struct power_supply *usb_psy;

	struct wakeup_source *reverse_charge_wakelock;

	bool revchg_bcl_flag;
	bool reverse_quick_charge_flag;
	bool in_otg_mode;
	bool pd30_source;
	bool otg_curr_lmt_flag;

	int otg_vbus_level;
	uint32_t reverse_adapter_svid;

	enum reverse_charge_watt reverse_power_mode;

	struct delayed_work reverse_charge_check_work;

};

#endif