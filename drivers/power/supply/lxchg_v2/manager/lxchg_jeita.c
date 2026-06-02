// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "lxchg_manager.h"
#include "lxchg_printk.h"
#include "lxchg_voter.h"
#include "lxchg_jeita.h"
#include "lxchg_notify.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"

#ifdef TAG
#undef TAG
#define TAG "[LX_CHG_JEITA]"
#endif

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))



static int lx_jeita_get_normal_index(struct charger_manager *manager)
{
	int new_index = 0;
	int temp = manager->tbat;
	static int cur_index = 0;
	struct lxchg_jeita *jeita = manager->jeita;

	if (temp < jeita->normal_cfg[0].low_threshold)
		new_index = 0;
	else if (temp > jeita->normal_cfg[jeita->normal_cfg_cnt - 1].high_threshold)
		new_index = jeita->normal_cfg_cnt - 1;
	else
		for (new_index = 0; new_index < jeita->normal_cfg_cnt; new_index++)
			if (is_between(jeita->normal_cfg[new_index].low_threshold, jeita->normal_cfg[new_index].high_threshold, temp))
				break;

	if (jeita->plugin_normal_index == -1) {
		jeita->plugin_normal_index = 5; //25
		cur_index = new_index;
	}

	if ((cur_index > new_index && jeita->plugin_normal_index < cur_index && jeita->normal_cfg[new_index].high_threshold - temp < TEMP_SHAKE_OFFSET) ||
		(cur_index < new_index && jeita->plugin_normal_index > cur_index && temp - (jeita->normal_cfg[new_index].low_threshold-1) < TEMP_SHAKE_OFFSET)) {
		lx_info("%d->%d temp shake !\n", new_index, cur_index);
		new_index = cur_index;
	}

	cur_index = new_index;

	lx_info("temp = %d, first_index = %d, cur_index = %d, new_index = %d\n",
		temp, jeita->plugin_normal_index, cur_index, new_index);
	return cur_index;
}

static int lx_jeita_get_ffc_index(struct charger_manager *manager)
{
	int new_index = 0;
	int temp = manager->tbat;
	static int cur_index = 0;
	int low_threshold = 0;
	int high_threshold = 0;

	struct lxchg_jeita *jeita = manager->jeita;

	if (temp < jeita->ffc_cfg[0].low_threshold)
		new_index = -1;
	else if ( temp > jeita->ffc_cfg[jeita->ffc_cfg_cnt - 1].high_threshold)
		new_index = jeita->ffc_cfg_cnt;
	else
		for (new_index = 0; new_index < jeita->ffc_cfg_cnt; new_index++)
			if (is_between(jeita->ffc_cfg[new_index].low_threshold, jeita->ffc_cfg[new_index].high_threshold, temp))
				break;

	if (new_index == -1) {
		low_threshold = jeita->ffc_cfg[0].low_threshold - 1;
	} else if (new_index == jeita->ffc_cfg_cnt) {
		high_threshold = jeita->ffc_cfg[jeita->ffc_cfg_cnt - 1].high_threshold;
	} else {
		high_threshold = jeita->ffc_cfg[new_index].high_threshold;
		low_threshold = jeita->ffc_cfg[new_index].low_threshold - 1;
	}

	if (jeita->plugin_ffc_index == -1) {
		jeita->plugin_ffc_index = 1; //35
		cur_index = new_index;
	}

	if ((cur_index > new_index && jeita->plugin_ffc_index < cur_index && high_threshold - temp < TEMP_SHAKE_OFFSET) ||
		(cur_index < new_index && jeita->plugin_ffc_index > cur_index && temp - low_threshold < TEMP_SHAKE_OFFSET)) {
		lx_info("%d->%d temp shake !\n", new_index, cur_index);
		new_index = cur_index;
	}

	cur_index = new_index;

	lx_info("temp = %d, first_index = %d, cur_index = %d, new_index = %d\n",
		temp, jeita->plugin_ffc_index, cur_index, new_index);

	if (cur_index == -1 || cur_index == jeita->ffc_cfg_cnt)
		return -1;
	else
		return cur_index;
}

__maybe_unused
static int charge_fast_to_normal_5v(struct charger_manager *manager)
{
	//static int check_cnt = 0;
	int ret = 0;

	if (manager->pd_volt_max != 9000)
		return 0;

	lx_info("type = %d, pd_volt_max = %d, vbus = %d\n", manager->real_type, manager->pd_volt_max, manager->vbus);
	if (manager->real_type == VBUS_TYPE_PD_PPS) {
			ret = tcpm_set_apdo_charging_policy(manager->tcpc,DPM_CHARGING_POLICY_PPS, 5000, 3000, NULL);
			if (ret) {
				lx_err("pps switch 5V3A fail, ret = %d, retry 5V2A\n", ret);
				ret = tcpm_set_apdo_charging_policy(manager->tcpc,DPM_CHARGING_POLICY_PPS, 5000, 2000, NULL);
				if (ret) {
					lx_err("pps switch 5V2A fail, ret = %d, retry other 5V3A\n", ret);
					ret = tcpm_dpm_pd_request(manager->tcpc, 5000, 3000, NULL);
					if (ret) {
						lx_err("user other, pps switch 5V3A fail, ret = %d, retry other 5V2A\n", ret);
						ret = tcpm_dpm_pd_request(manager->tcpc, 5000, 2000, NULL);
						if (ret)
							lx_err("I have no idea!\n");
					}
				}
			}
	} else if (manager->real_type == VBUS_TYPE_PD) {
			ret = tcpm_set_pd_charging_policy(manager->tcpc, DPM_CHARGING_POLICY_VSAFE5V, NULL);
			if (ret)
				lx_err("pd switch failed, ret = %d\n", ret);
	}
#if 0
	if (manager->vbus > 6500 && (manager->vbat > (manager->vote_fv - 50)) && (manager->avg_ibat < 1800)) {
		if (check_cnt++ > 5) {
			if(manager->real_type == VBUS_TYPE_PD ||
					(manager->real_type == VBUS_TYPE_PD_PPS && manager->cp_policy->cp_charge_done)) {
					lx_info("exit pd/pps fast to normal\n");
				adapter_set_cap(manager->cp_policy->adapter, manager->cp_policy->cap_nr, 5000, 2000);
			} else if(manager->real_type == VBUS_TYPE_HVDCP) {
				lx_info("exit QC fast to normal\n");
				charger_set_qc_term_vbus(manager->charger);
			}
			check_cnt = 0;
		}
	} else {
		check_cnt = 0;
	}
#endif
	return 0;
}

static int jeita_handle(struct charger_manager *manager)
{
	struct lxchg_jeita *jeita = manager->jeita;
	int nor_index = 0;
	int fcc_index = 0;
	int final_ibat = 0;
	int final_vterm = 0;
	int final_iterm = 0;
	int temp_now, vol_now, batt_id;
	struct jeita_normal_cfg *nor_cfg = jeita->normal_cfg;
	struct jeita_ffc_cfg *ffc_cfg = jeita->ffc_cfg;
	const char *total_fcc_voter = get_effective_client(manager->total_fcc_votable);

	if (!manager->auth_dev) {
		lx_err("failed to get auth device, defalut batt id = 0\n");
		batt_id = 0;
	} else {
		batt_id = manager->auth_dev->battery_id;
		if (batt_id > MAX_BAT_NUM_SUPPORT -1)
			batt_id = 0;
	}

	vol_now = manager->vbat;
	temp_now = manager->tbat;

	if (temp_now < 0) {
		charger_set_rechg_volt(manager->charger, LOW_TEMP_RECHG_OFFSET);
	} else {
		charger_set_rechg_volt(manager->charger, NOR_TEMP_RECHG_OFFSET);
	}

	nor_index = lx_jeita_get_normal_index(manager);
	if (nor_cfg[nor_index].normal_vbat_1 != -1) {
		if (vol_now < nor_cfg[nor_index].normal_vbat_1) {
			if (jeita->normal_curr_shake) {
				if ((vol_now < (nor_cfg[nor_index].normal_vbat_1 - COLD_RECHG_VOLT_OFFSET))) {
					final_ibat = nor_cfg[nor_index].normal_ibat_1;
					jeita->normal_curr_shake = false;
				} else {
					final_ibat = nor_cfg[nor_index].normal_ibat_2;
				}
			} else {
				final_ibat = nor_cfg[nor_index].normal_ibat_1;
			}
		} else {
			final_ibat = nor_cfg[nor_index].normal_ibat_2;
			jeita->normal_curr_shake = true;
		}
	} else {
		final_ibat = nor_cfg[nor_index].normal_ibat_1;
	}

	final_iterm = nor_cfg[nor_index].normal_iterm[batt_id];
	final_vterm = nor_cfg[nor_index].normal_fv;
	lx_info("normal ibat = %d, iterm = %d, vterm = %d\n", final_ibat, final_iterm, final_vterm);
	/*raise SDP hard iterm*/
	//if (manager->real_type == VBUS_TYPE_SDP)
	//	final_iterm = manager->usb_iterm;

	fcc_index = lx_jeita_get_ffc_index(manager);
	if (fcc_index == -1 && manager->cp_policy->state == POLICY_RUNNING) {
		chargerpump_policy_stop(manager->cp_policy);
		rerun_election(manager->total_fcc_votable);
		lx_info("disable cp\n");
	} else if(fcc_index != -1) { //15~45
		if (manager->auth_dev && manager->master_cp_chg && !manager->ldp_event &&
			manager->pd_active == CHARGE_PD_PPS_ACTIVE && manager->cp_policy->state == POLICY_NO_START) {
			chargerpump_policy_start(manager->cp_policy);
			lx_info("enable cp\n");
		} else if (manager->pd_adapter->verifed && (manager->cp_policy->sm == PM_STATE_CHARGERPUMP_CC_CV ||
			(manager->cp_policy->cp_charge_done && manager->vote_total_fcc > ffc_cfg[fcc_index].ffc_iterm[batt_id]))) {
			if (vol_now <= ffc_cfg[fcc_index].ffc_vbat_1) {
				if (jeita->fcc_curr_shake) {
					if ((vol_now < (ffc_cfg[fcc_index].ffc_vbat_1 - COLD_RECHG_VOLT_OFFSET))) {
						final_ibat = ffc_cfg[fcc_index].ffc_ibat_1;
						jeita->fcc_curr_shake = false;
					} else {
						final_ibat = ffc_cfg[fcc_index].ffc_ibat_2;
					}
				} else {
					final_ibat = ffc_cfg[fcc_index].ffc_ibat_1;
				}
			} else if (vol_now <= ffc_cfg[fcc_index].ffc_vbat_2 - 15) {
				final_ibat = ffc_cfg[fcc_index].ffc_ibat_2;
				jeita->fcc_curr_shake = true;
			} else {
				final_ibat = ffc_cfg[fcc_index].ffc_ibat_3;
			}
			final_iterm = ffc_cfg[fcc_index].ffc_iterm[batt_id];
			final_vterm = ffc_cfg[fcc_index].ffc_fv;
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, true);
		}else
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
	}

	if (!manager->auth_dev || !manager->master_cp_chg || manager->ldp_event) {
		charge_fast_to_normal_5v(manager);
	}

	if (!manager->auth_dev && final_ibat > 2000) {
		lx_err("battery is not auth, force 2A!\n");
		final_ibat = 2000;
	}

	if (manager->ldp_event && final_ibat > 1600) {
		lx_err("cp is not matched, force 1.6A!\n");
		final_ibat = 1600;
	}

	if (!manager->master_cp_chg && final_ibat > 1000) {
		lx_err("cp is not matched, force 1A!\n");
		final_ibat = 1000;
	}

	if (manager->charger_online) {
		vote(manager->total_fcc_votable, JEITA_VOTER, true, final_ibat);
		vote(manager->iterm_votable, ITER_VOTER, true, final_iterm);
		vote(manager->fv_votable, JEITA_VOTER, true, final_vterm);

		manager->vote_total_fcc = get_effective_result(manager->total_fcc_votable);
		manager->vote_iterm = get_effective_result(manager->iterm_votable);
		manager->vote_fv = get_effective_result(manager->fv_votable);

		if (!IS_ERR_OR_NULL(total_fcc_voter) && (!strcmp(total_fcc_voter, STOP_CHARGE_FOR_BATOVP_VOTER)))
			rerun_election(manager->fv_votable);

		lx_info("sm:%d, verifed=%d, ibat_set=%d, fv_set=%d, iterm_set=%d, batt_id=%d, cp_done=%d, index=%d-%d, charge_done=%d\n",
					manager->cp_policy->sm, manager->pd_adapter->verifed, final_ibat, final_vterm, final_iterm, batt_id,
					manager->cp_policy->cp_charge_done, nor_index, fcc_index, manager->is_charge_done);
	}

	return 0;
}

static void chg_cycle_dump(struct chg_cycle_data *cycle_data)
{
	int i = 0, j = 0;

	for (i = 0; i <  cycle_data->temp_array_size; i++)
		lx_info("temp_array[%d] = %d", i, cycle_data->temp_data[i]);

	for (i = 0; i < cycle_data->cycle_array_size; i++)
		lx_info("cycle_array[%d] = %d", i, cycle_data->cycle_data[i]);

	for (i = 0; i < cycle_data->vol_array_rows; i++)
		for (j = 0; j < cycle_data->vol_array_cols; j++)
			lx_info("vol_array[%d][%d] = %d",i, j, cycle_data->vol_data[i][j]);

	for (i = 0; i < cycle_data->curr_array_rows; i++)
		for (j = 0; j < cycle_data->curr_array_cols; j++)
			lx_info("curr_array[%d][%d] = %d",i, j, cycle_data->curr_data[i][j]);
}

static int chg_cycle_get_temp_index(struct chg_cycle_data *cycle_data, int temp)
{
	int index = 0, i;

	if (temp < cycle_data->temp_data[0] || temp > cycle_data->temp_data[cycle_data->temp_array_size - 1])
		index = -1;

	for (i = 0; i < cycle_data->temp_array_size; i++) {
		if (temp > cycle_data->temp_data[i] && temp <= cycle_data->temp_data[cycle_data->temp_array_size - 1])
			index = i;
	}

	return index;
}

static int chg_cycle_get_cycle_count_index(struct chg_cycle_data *cycle_data, int cycle_count)
{
	int index = 0, i;

	for (i = 0; i < cycle_data->cycle_array_size; i++) {
		if (cycle_count >= cycle_data->cycle_data[i])
			index = i;
	}

	return index;
}

static int chg_cycle_get_vol_index(struct chg_cycle_data *cycle_data, int vol, int array_row)
{
	int index = 0, i;

	for (i = 0; i < cycle_data->vol_array_cols; i++) {
		if (vol <= cycle_data->vol_data[array_row][i]) {
			index = i;
			break;
		} else if (vol > cycle_data->vol_data[array_row][cycle_data->vol_array_cols -1]) {
			index = cycle_data->vol_array_cols -1;
			break;
		}
	}

	return index;
}

static void chg_cycle_handle(struct charger_manager *manager)
{
	struct lxchg_jeita *jeita = manager->jeita;
	int cycle_index = 0, temp_index = 0, vol_index = 0;
	int fv_vote_value = 0, curr_vote_value = 0;
	int array_row;

	if (!IS_ERR_OR_NULL(jeita->ffc_cycle_data) && (manager->pd_adapter->verifed &&
			(manager->cp_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || manager->cp_policy->cp_charge_done))) {
		temp_index = chg_cycle_get_temp_index(jeita->ffc_cycle_data, manager->tbat);
		if (temp_index == -1) {
			lx_err("temp_index is -1!, return\n");
			vote(manager->total_fcc_votable, CHG_CYCLE_VOTER, false, 0);
			vote(manager->fv_votable, CHG_CYCLE_VOTER, false, 0);
			manager->vote_total_fcc = get_effective_result(manager->total_fcc_votable);
			manager->vote_fv = get_effective_result(manager->fv_votable);
			return;
		}
		cycle_index = chg_cycle_get_cycle_count_index(jeita->ffc_cycle_data, manager->batt_cycle);

		array_row = temp_index * jeita->ffc_cycle_data->vol_array_rows + cycle_index;
		fv_vote_value = jeita->ffc_cycle_data->vol_data[array_row][jeita->ffc_cycle_data->vol_array_cols - 1];

		vol_index = chg_cycle_get_vol_index(jeita->ffc_cycle_data, manager->vbat, array_row);
		curr_vote_value = jeita->ffc_cycle_data->curr_data[array_row][vol_index];
		if (curr_vote_value > jeita->ffc_cycle_data->last_curr_value && temp_index == jeita->ffc_cycle_data->last_temp_index) {
			if (abs(manager->vbat - jeita->ffc_cycle_data->vol_data[array_row][jeita->ffc_cycle_data->last_vol_index]) <= CHG_CYCLE_RESTORE_VOL)
				curr_vote_value = jeita->ffc_cycle_data->last_curr_value == 0 ? curr_vote_value : jeita->ffc_cycle_data->last_curr_value;
                        else
				jeita->ffc_cycle_data->last_vol_index = vol_index >= 1 ? (vol_index -1) : vol_index;
		} else
			jeita->ffc_cycle_data->last_vol_index = vol_index >= 1 ? (vol_index -1) : vol_index;
		jeita->ffc_cycle_data->last_curr_value = curr_vote_value;
		jeita->ffc_cycle_data->last_temp_index = temp_index;
	} else if (!IS_ERR_OR_NULL(jeita->normal_cycle_data)) {
		temp_index = chg_cycle_get_temp_index(jeita->normal_cycle_data, manager->tbat);
		if (temp_index == -1) {
			lx_err("temp_index is -1!, return\n");
			vote(manager->total_fcc_votable, CHG_CYCLE_VOTER, false, 0);
			vote(manager->fv_votable, CHG_CYCLE_VOTER, false, 0);
			manager->vote_total_fcc = get_effective_result(manager->total_fcc_votable);
			manager->vote_fv = get_effective_result(manager->fv_votable);
			return;
		}
		cycle_index = chg_cycle_get_cycle_count_index(jeita->normal_cycle_data, manager->batt_cycle);

		array_row = temp_index * jeita->normal_cycle_data->vol_array_rows + cycle_index;
		fv_vote_value = jeita->normal_cycle_data->vol_data[array_row][jeita->normal_cycle_data->vol_array_cols - 1];

		vol_index = chg_cycle_get_vol_index(jeita->normal_cycle_data, manager->vbat, array_row);
		curr_vote_value = jeita->normal_cycle_data->curr_data[array_row][vol_index];
		if (curr_vote_value > jeita->normal_cycle_data->last_curr_value && temp_index == jeita->normal_cycle_data->last_temp_index) {
			if (abs(manager->vbat - jeita->normal_cycle_data->vol_data[array_row][jeita->normal_cycle_data->last_vol_index]) <= CHG_CYCLE_RESTORE_VOL)
				curr_vote_value = jeita->normal_cycle_data->last_curr_value == 0 ? curr_vote_value : jeita->normal_cycle_data->last_curr_value;
			else
				jeita->normal_cycle_data->last_vol_index = vol_index;
		} else
			jeita->normal_cycle_data->last_vol_index = vol_index;
		jeita->normal_cycle_data->last_curr_value = curr_vote_value;
		jeita->normal_cycle_data->last_temp_index = temp_index;
	}

	lx_info("cycle_cnt = %d, cycle_curret = %d, cycle_fv = %d\n", manager->batt_cycle, curr_vote_value, fv_vote_value);
	vote(manager->total_fcc_votable, CHG_CYCLE_VOTER, curr_vote_value == 0? false: true, curr_vote_value);
	vote(manager->fv_votable, CHG_CYCLE_VOTER, fv_vote_value == 0? false: true, fv_vote_value);
	manager->vote_total_fcc = get_effective_result(manager->total_fcc_votable);
	manager->vote_fv = get_effective_result(manager->fv_votable);
}

/*START_ERP*/
void get_term_and_rechg_offset(struct charger_manager *manager)
{
	int i = 0;

	manager->jeita->soft_rechg_offset = NOR_TEMP_RECHG_OFFSET;

	for (i = 0; i < manager->jeita->rechg_fv_offset_data_cnt; i++) {
		if (is_between(manager->jeita->rechg_fv_offset_data[i].temp_level_l,
			manager->jeita->rechg_fv_offset_data[i].temp_level_h, manager->tbat)) {
				if (manager->jeita->rechg_fv_offset_data[i].value > 0) {
					manager->jeita->soft_rechg_offset = manager->jeita->rechg_fv_offset_data[i].value;
					break;
				}
			}
	}
	if (manager->batt_cycle > BATT_CYCLE_COUNT_OVER_800) {
		manager->jeita->soft_rechg_offset += 80;
		manager->jeita->soft_soc_threshold = 98;
	} else if (manager->batt_cycle > BATT_CYCLE_COUNT_OVER_100) {
		manager->jeita->soft_rechg_offset += 50;
		manager->jeita->soft_soc_threshold = 98;
	} else {
		manager->jeita->soft_rechg_offset += 50;
		manager->jeita->soft_soc_threshold = 100;
	}

	manager->jeita->soft_rechg_rsoc = RECHARGE_RSOC_THRESHOLD;

	if (manager->is_eea)
		manager->jeita->soft_soc_threshold = 99;
}

static void soft_term_and_recharge_check(struct charger_manager *manager)
{
	struct lxchg_jeita *jeita = manager->jeita;
	int soft_term_cnt = 3;

	if (manager->fg_not_ready) {
		lx_err("fg is not ready!!\n");
		return;
	}
	jeita->soft_fv_threshold = ST_THRESHOLD_LOW_LIMIT_FV(manager->vote_fv);
	jeita->soft_iterm_threshold = SW_THRESHOLD_LIMIT_ITERM(manager->vote_iterm);

	if (manager->vote_total_fcc < 600)
		jeita->soft_fv_threshold += 20;

	lx_info("vote_fv = %d, soft_fv_threshold = %d, vote_iterm = %d, soft_iterm_threshold = %d\n",
                manager->vote_fv, jeita->soft_fv_threshold, manager->vote_iterm, jeita->soft_iterm_threshold);

	get_term_and_rechg_offset(manager);

	if (manager->real_type == VBUS_TYPE_SDP) {
		soft_term_cnt = 5;
		jeita->soft_fv_threshold = manager->vote_fv - 20;
		jeita->soft_iterm_threshold = manager->vote_iterm + 10;
		lx_info("soft_fv_threshold_SDP = %d, soft_iterm_threshold_SDP = %d\n", jeita->soft_fv_threshold, jeita->soft_iterm_threshold);
	}

	/*soft iterm function need satisfy
	  *1:uisoc = 100, ERP:uisoc = 99
	  *2:the state of charge must be charging
	  *3:not lower than cut-off voltage
	  *4:iterm standard
	  */

	lx_info("soc:%d %d, status:%d, vbat:%d %d, ibat:%d %d, check_cnt:%d, eea=%d\n", manager->uisoc, jeita->soft_soc_threshold,
		manager->chg_status, manager->vbat, jeita->soft_fv_threshold, manager->ibat/1000, jeita->soft_iterm_threshold,
		manager->soft_term_check_cnt, manager->is_eea);

	if ((manager->uisoc >= jeita->soft_soc_threshold || manager->vote_fv <= TEMP_45_TO_58_FV) &&
		(manager->chg_status == POWER_SUPPLY_STATUS_CHARGING && !manager->above_45_temp_term) &&
		manager->vbat >= jeita->soft_fv_threshold &&
		manager->ibat/1000 <= jeita->soft_iterm_threshold &&
		manager->ibat/1000 > 0) {
		if (manager->soft_term_check_cnt >= soft_term_cnt) {
			lxchg_psy_updata(CHARGE_EVENT_SOFT_TERM);
		} else {
			manager->soft_term_check_cnt++;
		}
	} else if (manager->chg_status == POWER_SUPPLY_STATUS_FULL) {
		lx_info("start check recharge\n");
		if (manager->is_eea ? manager->uisoc < EEA_RECHARGE_UISOC_THRESHOLD : manager->rsoc < jeita->soft_rechg_rsoc)
			lxchg_psy_updata(CHARGE_EVENT_SOFT_RECHG);
	} else if (manager->above_45_temp_term) {
		if (manager->vbat < (manager->vote_fv - jeita->soft_rechg_offset))
			lxchg_psy_updata(CHARGE_EVENT_SOFT_RECHG);
	}
}

/*END_ERP*/
void lxchg_jeita_handler(struct lxchg_jeita *jeita)
{
	struct charger_manager *manager = (struct charger_manager *)dev_get_drvdata(jeita->dev);

	if (manager->soft_reset_state) {
		lx_err("soft reset ... skip updata jeita\n");
		manager->soft_reset_state = false;
		return;
	}

	jeita_handle(manager);
	chg_cycle_handle(manager);
	soft_term_and_recharge_check(manager);
}

static struct chg_cycle_data* chg_cycle_cfg_parse_dts(struct lxchg_jeita *jeita, struct device_node *node)
{
	struct chg_cycle_data *cycle_data;
	int total_length = 0, cycle_data_length = 0, temp_data_length = 0;
	int vol_data_length = 0, curr_data_length = 0, ret = 0, row, col;

	cycle_data_length = of_property_count_elems_of_size(node, "cycle_count_data", sizeof(u32));
	if (cycle_data_length < 0) {
		lx_err("failed to read total_length of cycle_count_data\n");
		return NULL;
	}

	temp_data_length = of_property_count_elems_of_size(node, "cycle_temp_data", sizeof(u32));
	if (temp_data_length < 0) {
		lx_err("failed to read total_length of temp_data_length\n");
		return NULL;
	}

	vol_data_length = of_property_count_elems_of_size(node, "cycle_vol_data", sizeof(u32));
	if (vol_data_length < 0) {
		lx_err("failed to read total_length of vol_data_length\n");
		return NULL;
	}

	curr_data_length = of_property_count_elems_of_size(node, "cycle_curr_data", sizeof(u32));
	if (curr_data_length < 0) {
		lx_err("failed to read total_length of curr_data_length\n");
		return NULL;
	}
	total_length = cycle_data_length + temp_data_length + vol_data_length + curr_data_length + 9 * sizeof(int);

	cycle_data = (struct chg_cycle_data *)devm_kzalloc(jeita->dev, sizeof(int) * total_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data)) {
		lx_err("malloc cycle_data fail\n");
		return NULL;
	}
	cycle_data->cycle_array_size = cycle_data_length;
	cycle_data->temp_array_size = temp_data_length;
	cycle_data->vol_array_rows = cycle_data_length * (temp_data_length - 1);
	cycle_data->vol_array_cols = vol_data_length / cycle_data->vol_array_rows;
	cycle_data->curr_array_rows = cycle_data->vol_array_rows;
	cycle_data->curr_array_cols = cycle_data->vol_array_cols;

	cycle_data->cycle_data = (int *)devm_kzalloc(jeita->dev, sizeof(int) * cycle_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->cycle_data)) {
		lx_err("malloc cycle_data->cycle_data fail\n");
		return NULL;
	}
	ret |= of_property_read_u32_array(node, "cycle_count_data", cycle_data->cycle_data, cycle_data_length);
	if (ret)
	{
		lx_err("failed to parse cycle_data\n");
		return NULL;
	}

	cycle_data->temp_data = (int *)devm_kzalloc(jeita->dev, sizeof(int) * temp_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->temp_data)) {
		lx_err("malloc cycle_data->temp_data fail\n");
		return NULL;
	}
	ret |= of_property_read_u32_array(node, "cycle_temp_data", cycle_data->temp_data, temp_data_length);
	if (ret)
	{
		lx_err("failed to parse temp_data\n");
		return NULL;
	}

	cycle_data->vol_data = (int **)devm_kzalloc(jeita->dev, sizeof(int*) * vol_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->vol_data)) {
		lx_err("malloc cycle_data->vol_data fail\n");
		return NULL;
	}
	for (row = 0; row < cycle_data->vol_array_rows; row++) {
		cycle_data->vol_data[row] = (int *)devm_kzalloc(jeita->dev, sizeof(int) * cycle_data->vol_array_cols, GFP_KERNEL);
		if (IS_ERR_OR_NULL(cycle_data->vol_data[row])) {
			lx_err("malloc cycle_data->vol_data[%d] fail\n", row);
			return NULL;
		}
		for (col = 0; col < cycle_data->vol_array_cols; col++) {
			ret = of_property_read_u32_index(node, "cycle_vol_data", row * cycle_data->vol_array_cols + col, &(cycle_data->vol_data[row][col]));
			if (ret) {
				lx_err("failed to parse cycle_vol_data\n");
				return NULL;
			}
		}
	}

	cycle_data->curr_data = (int **)devm_kzalloc(jeita->dev, sizeof(int*) * curr_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->curr_data)) {
		lx_err("malloc cycle_data->curr_data fail\n");
		return NULL;
	}
	for (row = 0; row < cycle_data->curr_array_rows; row++) {
		cycle_data->curr_data[row] = (int *)devm_kzalloc(jeita->dev, sizeof(int) * cycle_data->curr_array_cols, GFP_KERNEL);
		if (IS_ERR_OR_NULL(cycle_data->curr_data[row])) {
			lx_err("malloc cycle_data->curr_data[%d] fail\n", row);
			return NULL;
		}
		for (col = 0; col < cycle_data->curr_array_cols; col++) {
			ret = of_property_read_u32_index(node, "cycle_curr_data", row * cycle_data->curr_array_cols + col, &(cycle_data->curr_data[row][col]));
			if (ret) {
				lx_err("failed to parse cycle_curr_data\n");
				return NULL;
			}
		}
	}

	chg_cycle_dump(cycle_data);

	return cycle_data;
}

static void chg_cycle_cfg_init(struct lxchg_jeita *jeita)
{
	struct device_node *node = jeita->dev->of_node;
	struct device_node *ffc_cycle_chg_node = NULL;
	struct device_node *normal_cycle_chg_node = NULL;

	if (!IS_ERR_OR_NULL(node)) {
		ffc_cycle_chg_node = of_find_node_by_name(node, "ffc_chg_cfg_bat_cycle");
		//power_max <= 45W, use eea configs
		normal_cycle_chg_node = of_find_node_by_name(node, "normal_chg_cfg_bat_cycle");
	}

	if (!IS_ERR_OR_NULL(ffc_cycle_chg_node))
		jeita->ffc_cycle_data = chg_cycle_cfg_parse_dts(jeita, ffc_cycle_chg_node);
	else
		lx_info("cat not found ffc_cycle_chg_node\n");


	if (!IS_ERR_OR_NULL(normal_cycle_chg_node))
		jeita->normal_cycle_data = chg_cycle_cfg_parse_dts(jeita, normal_cycle_chg_node);
	else
		lx_info("cat not found normal_cycle_chg_node\n");
}

static bool lx_parse_jeita_dt(struct device_node *node, struct lxchg_jeita *jeita)
{
	int total_length = 0;
	bool ret = 0;
	int i = 0;

	total_length = of_property_count_elems_of_size(node, "jeita_normal_cfg", sizeof(u32));
	if (total_length < 0) {
		lx_err("failed to read total_length of normal_cfg\n");
		return 0;
	}
	if (total_length % JEITA_NORMAL_CFG_LINE_LEN) {
		lx_err("normal_cfg not a tuple element\n");
		return 0;
	}
	jeita->normal_cfg = devm_kzalloc(jeita->dev, sizeof(u32) * total_length, GFP_KERNEL);
	if (!jeita->normal_cfg) {
		lx_err("malloc rechg_fv_cfg fail\n");
		return 0;
	}
	ret |= of_property_read_u32_array(node, "jeita_normal_cfg", (u32 *)jeita->normal_cfg, total_length);
	if (ret) {
		lx_err("failed to parse normal_cfg\n");
		return 0;
	}
	jeita->normal_cfg_cnt = total_length / JEITA_NORMAL_CFG_LINE_LEN;

	total_length = of_property_count_elems_of_size(node, "jeita_ffc_cfg", sizeof(u32));
	if (total_length < 0) {
		lx_err("failed to read total_length of ffc_cfg\n");
		return 0;
	}
	if (total_length % JEITA_FCC_CFG_LINE_LEN) {
		lx_err("ffc_cfg not a tuple element\n");
		return 0;
	}
	jeita->ffc_cfg = devm_kzalloc(jeita->dev, sizeof(u32) * total_length, GFP_KERNEL);
	if (!jeita->ffc_cfg) {
		lx_err("malloc rechg_fv_cfg fail\n");
		return 0;
	}
	ret |= of_property_read_u32_array(node, "jeita_ffc_cfg", (u32 *)jeita->ffc_cfg, total_length);
	if (ret) {
		lx_err("failed to parse ffc_cfg\n");
		return 0;
	}
	jeita->ffc_cfg_cnt = total_length / JEITA_FCC_CFG_LINE_LEN;

	total_length = of_property_count_elems_of_size(node, "rechg_fv_offset_cfg", sizeof(u32));
	if (total_length < 0) {
		lx_err("failed to read total_length of rechg_fv_cfg\n");
		return 0;
	}
	if (total_length % JEITA_RECHG_LINE_LEN) {
		lx_err("rechg_fv_cfg not a tuple element\n");
		return 0;
	}
	jeita->rechg_fv_offset_data = devm_kzalloc(jeita->dev, sizeof(int) * total_length, GFP_KERNEL);
	if (!jeita->rechg_fv_offset_data) {
		lx_err("malloc rechg_fv_cfg fail\n");
		return 0;
	}
	ret |= of_property_read_u32_array(node, "rechg_fv_offset_cfg", (u32 *)jeita->rechg_fv_offset_data, total_length);
	if (ret) {
		lx_err("failed to parse rechg_fv_offset_cfg\n");
		return 0;
	}
	jeita->rechg_fv_offset_data_cnt = total_length / JEITA_RECHG_LINE_LEN;

	for (i = 0; i < jeita->normal_cfg_cnt; i++)
		lx_info("[normal_cfg]%d %d %d %d %d %d %d %d %d\n",
					jeita->normal_cfg[i].low_threshold, jeita->normal_cfg[i].high_threshold, jeita->normal_cfg[i].normal_ibat_1,
					jeita->normal_cfg[i].normal_vbat_1, jeita->normal_cfg[i].normal_ibat_2, jeita->normal_cfg[i].normal_iterm[0],
					jeita->normal_cfg[i].normal_iterm[1], jeita->normal_cfg[i].normal_iterm[2], jeita->normal_cfg[i].normal_fv);

	for (i = 0; i < jeita->ffc_cfg_cnt; i++)
		lx_info("[ffc_cfg]%d %d %d %d %d %d %d %d %d %d %d\n",
					jeita->ffc_cfg[i].low_threshold, jeita->ffc_cfg[i].high_threshold, jeita->ffc_cfg[i].ffc_ibat_1,
					jeita->ffc_cfg[i].ffc_vbat_1, jeita->ffc_cfg[i].ffc_ibat_2, jeita->ffc_cfg[i].ffc_vbat_2,
					jeita->ffc_cfg[i].ffc_ibat_3, jeita->ffc_cfg[i].ffc_fv, jeita->ffc_cfg[i].ffc_iterm[0],
					jeita->ffc_cfg[i].ffc_iterm[1],	jeita->ffc_cfg[i].ffc_iterm[2]);

	return !ret;
}

int lx_jeita_init(struct charger_manager *manager)
{
	struct device_node *node = manager->dev->of_node;
	struct device_node *jeita_node = NULL;
	struct lxchg_jeita *jeita = NULL;
	int rc = 0;

	jeita = devm_kzalloc(manager->dev, sizeof(*jeita), GFP_KERNEL);
	if (!jeita)
		return -ENOMEM;

	manager->jeita = jeita;
	jeita->dev = manager->dev;

	jeita_node = of_find_node_by_name(node, "lxchg_jeita");
	if (!jeita_node) {
		lx_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = lx_parse_jeita_dt(jeita_node, jeita);
	if(!rc){
		lx_err("lx_parse_jeita_dt failed\n");
		return -EINVAL;
	}

	chg_cycle_cfg_init(jeita);

	jeita->plugin_normal_index = -1;
	jeita->plugin_ffc_index = -1;
	jeita->jeita_handle = lxchg_jeita_handler;

	lx_info("lx_jeita_init success\n");

	return 0;
}

