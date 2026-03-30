// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_HUAQIN_FG_CLASS_H__
#define __LINUX_HUAQIN_FG_CLASS_H__

enum err_stat {
	FG_ERR_AUTH_FAIL = 0x01,
	FG_EER_I2C_FAIL,
	FG_ERR_CHG_WATT = 0x10,
};

struct fuel_gauge_dev;
struct fuel_gauge_ops {
	int (*get_soc_decimal)(struct fuel_gauge_dev *);
	int (*get_soc_decimal_rate)(struct fuel_gauge_dev *);
	int (*get_raw_soc)(struct fuel_gauge_dev *);
	int (*get_rsoc)(struct fuel_gauge_dev *);
	int (*set_rsoc_update0)(struct fuel_gauge_dev *fuel_gauge, bool value);
	int (*get_soh)(struct fuel_gauge_dev *);
	int (*set_soh)(struct fuel_gauge_dev *fuel_gauge, int value);
	int (*set_fake_soh)(struct fuel_gauge_dev *fuel_gauge, int value);
	int (*get_c_car)(struct fuel_gauge_dev *);
	int (*get_v_car)(struct fuel_gauge_dev *);
	int (*set_fastcharge_mode)(struct fuel_gauge_dev *, bool);
	int (*get_fastcharge_mode)(struct fuel_gauge_dev *);
	int (*get_batt_id)(struct fuel_gauge_dev *);
	int (*get_batt_auth)(struct fuel_gauge_dev *);
	int (*get_batt_id_voltage)(struct fuel_gauge_dev *);
	int (*get_dod_count)(struct fuel_gauge_dev *);
	int (*set_term_voltage)(struct fuel_gauge_dev *fuel_gauge, int value);
	int (*get_manufacturing_date)(struct fuel_gauge_dev *, char *);
	int (*get_soh_sn)(struct fuel_gauge_dev *, char *);
	int (*get_first_usage_date)(struct fuel_gauge_dev *, char *);
	int (*set_first_usage_date)(struct fuel_gauge_dev *, const char *, size_t);
	int (*set_rsoc_report_100)(struct fuel_gauge_dev *);
	int (*get_isc_status)(struct fuel_gauge_dev *fuel_gauge);
	unsigned long (*get_calc_rvalue)(struct fuel_gauge_dev *fuel_gauge);
	int (*check_fg_status)(struct fuel_gauge_dev *);
};

struct fuel_gauge_dev {
	struct device dev;
	char *name;
	void *private;
	struct fuel_gauge_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
	int shutdown_vol;
	int shutdown_delay_vol;
	u8 delta_soh;
};

struct fuel_gauge_dev *fuel_gauge_find_dev_by_name(const char *name);
struct fuel_gauge_dev *fuel_gauge_register(char *name, struct device *parent,
			struct fuel_gauge_ops *ops, void *private);

void *fuel_gauge_get_private(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_unregister(struct fuel_gauge_dev *fuel_gauge);

int fuel_gauge_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool);
int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_raw_soc(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_rsoc(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_rsoc_update0(struct fuel_gauge_dev *fuel_gauge, bool value);
int fuel_gauge_get_soh(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_soh(struct fuel_gauge_dev *fuel_gauge, int value);
int fuel_gauge_set_fake_soh(struct fuel_gauge_dev *fuel_gauge, int value);
int fuel_gauge_get_c_car(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_v_car(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_batt_id(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_batt_auth(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_batt_id_voltage(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_dod_count(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_term_voltage(struct fuel_gauge_dev *fuel_gauge, int value);
int fuel_gauge_get_manufacturing_date(struct fuel_gauge_dev *fuel_gauge, char *buf);
int fuel_gauge_get_soh_sn(struct fuel_gauge_dev *fuel_gauge, char *buf);
int fuel_gauge_get_first_usage_date(struct fuel_gauge_dev *fuel_gauge, char *buf);
int fuel_gauge_set_first_usage_date(struct fuel_gauge_dev *fuel_gauge, const char *buf, size_t count);
int fuel_gauge_set_fg_rsoc100(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_isc_status(struct fuel_gauge_dev *fuel_gauge);
unsigned long fuel_gauge_get_calc_rvalue(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_check_fg_status(struct fuel_gauge_dev *fuel_gauge);
#endif
