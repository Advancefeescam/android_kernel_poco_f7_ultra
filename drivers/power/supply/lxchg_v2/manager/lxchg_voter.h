/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __PMIC_VOTER_H
#define __PMIC_VOTER_H

#include <linux/mutex.h>

#define CHARGER_TYPE_VOTER             "CHARGER_TYPE_VOTER"
#define MAN_MADE_SUSPNED_VOTER         "MAN_MADE_SUSPNED_VOTER"
#define JEITA_VOTER                    "JEITA_VOTER"
#define CALL_THERMAL_DAEMON_VOTER      "CALL_THERMAL_DAEMON_VOTER"
#define TEMP_THERMAL_DAEMON_VOTER      "TEMP_THERMAL_DAEMON_VOTER"
#define QC_POLICY_VOTER                "QC_POLICY_VOTER"
#define ITER_VOTER                     "ITER_VOTER"
#define TOTAL_FFC_VOTER                "TOTAL_FFC_VOTER"
#define TYPEC_SINK_VBUS_VOTER          "TYPEC_SINK_VBUS_VOTER"
#define ATO_SOC_LIMIT_VOTER            "ATO_SOC_LIMIT_VOTER"
#define USER_SUSPEND_VOTER             "USER_SUSPEND_VOTER"
#define DISABLE_CHARGE_VOTER           "DISABLE_CHARGE_VOTER"
#define LPD_DECTEED_VOTER              "LPD_DECTEED_VOTER"
#define STOP_CHARGE_FOR_BATOVP_VOTER   "STOP_CHARGE_FOR_BATOVP_VOTER"

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
#define FG_I2C_ERR                     "FG_I2C_ERR"
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#define SMART_BATT_VOTER               "SMART_BATT_VOTER"
#define CYCLE_COUNT_VOTER              "CYCLE_COUNT_VOTER"
#define NIGHT_CHARGING_VOTER           "NIGHT_CHARGING_VOTER"
#define ENDURANCE_VOTER                "ENDURANCE_VOTER"

#define SMOOTH_NEW_VOTER               "SMOOTH_NEW_VOTER"
#define NAVIGAITION_VOTER                "NAVIGAITION_VOTER"
#define FV_OVERVOLTAGE_VOTER	       "FV_OVERVOLTAGE_VOTER"
#endif
#define CHG_CYCLE_VOTER                "CHG_CYCLE_VOTER"
#define SOFT_ITERM_VOTER                "SOFT_ITERM_VOTER"

#define false 0
#define true  1
#define NUM_MAX_CLIENTS		32
#define DEBUG_FORCE_CLIENT	"DEBUG_FORCE_CLIENT"

struct client_vote {
	bool	enabled;
	int	value;
};

struct votable {
	const char		*name;
	const char		*override_client;
	struct list_head	list;
	struct client_vote	votes[NUM_MAX_CLIENTS];
	int			num_clients;
	int			type;
	int			effective_client_id;
	int			effective_result;
	int			override_result;
	struct mutex		vote_lock;
	void			*data;
	int			(*callback)(struct votable *votable,
						void *data,
						int effective_result,
						const char *effective_client);
	char			*client_strs[NUM_MAX_CLIENTS];
	bool			voted_on;
	struct dentry		*root;
	u32			force_val;
	bool			force_active;
};

enum votable_type {
	VOTE_MIN,
	VOTE_MAX,
	VOTE_SET_ANY,
	NUM_VOTABLE_TYPES,
};

bool is_client_vote_enabled(struct votable *votable, const char *client_str);
bool is_client_vote_enabled_locked(struct votable *votable,
							const char *client_str);
bool is_override_vote_enabled(struct votable *votable);
bool is_override_vote_enabled_locked(struct votable *votable);
int get_client_vote(struct votable *votable, const char *client_str);
int get_client_vote_locked(struct votable *votable, const char *client_str);
int get_effective_result(struct votable *votable);
int get_effective_result_locked(struct votable *votable);
const char *get_effective_client(struct votable *votable);
const char *get_effective_client_locked(struct votable *votable);
int vote(struct votable *votable, const char *client_str, bool state, int val);
int vote_override(struct votable *votable, const char *override_client,
		  bool state, int val);
int rerun_election(struct votable *votable);
struct votable *find_votable(const char *name);
struct votable *create_votable(const char *name,
				int votable_type,
				int (*callback)(struct votable *votable,
						void *data,
						int effective_result,
						const char *effective_client),
				void *data);
void destroy_votable(struct votable *votable);
void lock_votable(struct votable *votable);
void unlock_votable(struct votable *votable);
void vote_clean(struct votable *votable);

#endif /* __PMIC_VOTER_H */
