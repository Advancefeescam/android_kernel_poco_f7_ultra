#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include "lxchg_voter.h"
#include "lxchg_manager.h"
#include "lx_qc_policy.h"

static const unsigned char *qcm_sm_state_str[] = {
	"QCM_STATE_ENTRY",
	"QCM_STATE_INIT_VBUS",
	"QCM_STATE_ENABLE_CP",
	"QCM_STATE_TUNE",
	"QCM_STATE_EXIT",
};

static int log_level = 1;

static bool disable_slave_qc3_18 = true;
module_param_named(disable_slave_qc3_18, disable_slave_qc3_18, bool, 0600);

static int tune_step_ibat_qc3_27 = 1400;
module_param_named(tune_step_ibat_qc3_27, tune_step_ibat_qc3_27, int, 0600);

static int tune_step_ibat_qc3_18 = 1900;
module_param_named(tune_step_ibat_qc3_18, tune_step_ibat_qc3_18, int, 0600);

static int tune_step_ibat_qc35 = 200;
module_param_named(tune_step_ibat_qc35, tune_step_ibat_qc35, int, 0600);

static int vbus_low_gap = 200;
module_param_named(vbus_low_gap, vbus_low_gap, int, 0600);

static int vbus_high_gap = 450;
module_param_named(vbus_high_gap, vbus_high_gap, int, 0600);

static bool qcm_check_charger_dev(struct qcomcharger_policy *qc_policy)
{

	qc_policy->charger = charger_find_dev_by_name("primary_chg");
	if (IS_ERR_OR_NULL(qc_policy->charger)) {
		qcm_err("charger is NULL.\n");
		return PTR_ERR(qc_policy->charger);
	}

	qc_policy->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (IS_ERR_OR_NULL(qc_policy->master_cp_chg)) {
		qcm_err("master_cp_chg is NULL.\n");
		return PTR_ERR(qc_policy->master_cp_chg);
	}

	qc_policy->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	if (IS_ERR_OR_NULL(qc_policy->slave_cp_chg))
		qcm_err("slave_cp_chg is %d.\n", PTR_ERR(qc_policy->slave_cp_chg));

	return true;
}

static bool qcm_check_psy(struct qcomcharger_policy *qc_policy)
{
	qc_policy->usb_psy = power_supply_get_by_name("usb");
	if (!qc_policy->usb_psy) {
		qcm_err("failed to get usb_psy\n");
		return false;
	}

	qc_policy->batt_psy = power_supply_get_by_name("battery");
	if (!qc_policy->batt_psy) {
		qcm_err("failed to get batt_psy\n");
		return false;
	}

	return true;
}

static bool qcm_check_votable(struct qcomcharger_policy *qc_policy)
{
	qc_policy->input_limit_votable = find_votable("INPUT_VOTE");
	if (!qc_policy->input_limit_votable) {
		qcm_err("failed to get input_limit_votable\n");
		return false;
	}

	qc_policy->total_fcc_votable = find_votable("TOTAL_VOTE");
	if (!qc_policy->total_fcc_votable) {
		qcm_err("failed to get total_fcc_votable\n");
		return false;
	}

	return true;
}

static void qcm_pulse_dpdm(struct qcomcharger_policy *qc_policy, int target_vbus, int count)
{
	int delta_vbus = 0;

	qcm_info("target_vbus = %d, adapter_vbus = %d, qc_policy->tune_step_vbus = %d, count = %d\n",
				target_vbus, qc_policy->adapter_vbus, qc_policy->tune_step_vbus, count);

	if (target_vbus) {
		delta_vbus = target_vbus - qc_policy->adapter_vbus;
		count = abs(delta_vbus) / qc_policy->tune_step_vbus;
		qcm_err("delta_vbus = %d, count = %d\n", delta_vbus, count);
		if (count && qc_policy->tune_step_vbus == QC3_VBUS_STEP) {
			if (delta_vbus > 0)
				charger_qc3_vbus_puls(qc_policy->charger, plus, count);
			else
				charger_qc3_vbus_puls(qc_policy->charger, minus, count);
		} else if (count && qc_policy->tune_step_vbus == QC35_VBUS_STEP) {
			if (delta_vbus > 0)
				charger_qc3_vbus_puls(qc_policy->charger, plus, count);
			else
				charger_qc3_vbus_puls(qc_policy->charger, minus, count);
		}
	} else if (count > 0) {
		if (qc_policy->tune_step_vbus == QC3_VBUS_STEP)
			charger_qc3_vbus_puls(qc_policy->charger, plus, abs(count));
		else if (qc_policy->tune_step_vbus == QC35_VBUS_STEP)
			charger_qc3_vbus_puls(qc_policy->charger, plus, abs(count));
	} else if (count < 0) {
		if (qc_policy->tune_step_vbus == QC3_VBUS_STEP)
			charger_qc3_vbus_puls(qc_policy->charger, minus, abs(count));
		else if (qc_policy->tune_step_vbus == QC35_VBUS_STEP)
			charger_qc3_vbus_puls(qc_policy->charger, minus, abs(count));
	}
}

static void qcm_charge_tune(struct qcomcharger_policy *qc_policy)
{
	if ((-qc_policy->ibat) < qc_policy->target_fcc)
		qc_policy->final_step = qc_policy->ibat_step = (qc_policy->target_fcc - (-qc_policy->ibat)) / qc_policy->tune_step_ibat;
	else if ((-qc_policy->ibat) > qc_policy->target_fcc)
		qc_policy->final_step = qc_policy->ibat_step = -(((-qc_policy->ibat) - qc_policy->target_fcc) / qc_policy->tune_step_ibat + 1);
	else
		qc_policy->final_step = qc_policy->ibat_step = 0;

	if (qc_policy->adapter_vbus > qc_policy->max_vbus)
		qc_policy->vbus_step = -((qc_policy->adapter_vbus - qc_policy->max_vbus) / qc_policy->tune_step_vbus + 1);
	else
		qc_policy->vbus_step = 0;

	if ((qc_policy->cp_total_ibus + 100)> qc_policy->max_ibus)
		qc_policy->ibus_step = -(((qc_policy->cp_total_ibus + 100) - qc_policy->max_ibus) / qc_policy->tune_step_ibus + 1);
	else
		qc_policy->ibus_step = 0;

	if (qc_policy->vbus_step)
		qc_policy->final_step = min(qc_policy->final_step, qc_policy->vbus_step);
	if (qc_policy->ibus_step)
		qc_policy->final_step = min(qc_policy->final_step, qc_policy->ibus_step);
	if (qc_policy->final_step)
		qc_policy->final_step = cut_cap(qc_policy->final_step, (-qc_policy->max_step), qc_policy->max_step);

	if (qc_policy->final_step) {
		if (!qc_policy->anti_wave_count)
			qcm_pulse_dpdm(qc_policy, 0, qc_policy->final_step);
		qc_policy->anti_wave_count++;
		if (qc_policy->anti_wave_count >= (qc_policy->tune_step_vbus == QC3_VBUS_STEP ? ANTI_WAVE_COUNT_QC3 : ANTI_WAVE_COUNT_QC35))
			qc_policy->anti_wave_count = 0;
	} else {
		qc_policy->anti_wave_count = 0;
	}
}

__maybe_unused
static bool qcm_check_taper_charge(struct qcomcharger_policy *qc_policy)
{
	int cv_vbat = 0;

	cv_vbat = qc_policy->cv_vbat_ffc;
	if (qc_policy->vbat > cv_vbat)
		qc_policy->taper_count++;
	else
		qc_policy->taper_count = 0;

	if (qc_policy->taper_count > MAX_TAPER_COUNT)
		return true;
	else
		return false;
}

static int qcm_check_condition(struct qcomcharger_policy *qc_policy)
{
	#if 0
	if (qc_policy->sm_state == QCM_STATE_TUNE && qcm_check_taper_charge(qc_policy))
		return QCM_SM_EXIT;
	else if (qc_policy->sm_state == QCM_STATE_TUNE && (!qc_policy->master_cp_enable || (!qc_policy->disable_slave && !qc_policy->slave_cp_enable)))
		return QCM_SM_HOLD;
	else if (qc_policy->sm_state == QCM_STATE_TUNE && qc_policy->master_cp_ibus <= MIN_CP_IBUS && qc_policy->slave_cp_ibus <= MIN_CP_IBUS)
		return QCM_SM_EXIT;
	else if (qc_policy->input_suspend)
		return QCM_SM_HOLD;
	else if (0/*add jeita judge*/)
		return QCM_SM_HOLD;
	else if (qc_policy->target_fcc < MIN_ENTRY_FCC)
		return QCM_SM_HOLD;
	else if (qc_policy->sm_state == QCM_STATE_ENTRY && qc_policy->soc > qc_policy->high_soc)
		return QCM_SM_EXIT;
	else
	#endif
	return QCM_SM_CONTINUE;
}

static void qcm_move_state(struct qcomcharger_policy *qc_policy, enum qcm_sm_state new_state)
{
	qcm_info("sm_state change:%s -> %s\n", qcm_sm_state_str[qc_policy->sm_state], qcm_sm_state_str[new_state]);
	qc_policy->sm_state = new_state;
	qc_policy->no_delay = true;
}

static void qcm_handle_sm(struct qcomcharger_policy *qc_policy)
{
	int entry_vbus = 0;

	switch (qc_policy->sm_state) {
		case QCM_STATE_ENTRY:
			qc_policy->tune_vbus_count = 0;
			qc_policy->enable_cp_count = 0;
			qc_policy->taper_count = 0;
			qc_policy->anti_wave_count = 0;

			qc_policy->sm_status = qcm_check_condition(qc_policy);
			if (qc_policy->sm_status == QCM_SM_EXIT) {
				qcm_info("QCM_SM_EXIT, don't start sm\n");
				break;
			} else if (qc_policy->sm_status == QCM_SM_HOLD) {
				break;
			} else if (qc_policy->sm_status == QCM_SM_CONTINUE) {
				vote(qc_policy->input_limit_votable, QC_POLICY_VOTER, true, QCM_MAIN_CHG_ICL);
				qcm_move_state(qc_policy, QCM_STATE_INIT_VBUS);
			}
			break;
		case QCM_STATE_INIT_VBUS:
			qc_policy->tune_vbus_count++;
			if (qc_policy->tune_vbus_count == 1) {
				entry_vbus = qc_policy->vbat * 2 + (vbus_low_gap + vbus_high_gap) / 2;
				qcm_pulse_dpdm(qc_policy, entry_vbus, 0);
				break;
			}

			if (qc_policy->tune_vbus_count >= 15) {
				qcm_err("failed to tune VBUS to target window, exit QCM\n");
				qcm_move_state(qc_policy, QCM_STATE_EXIT);
				break;
			}

			if (qc_policy->adapter_vbus <= qc_policy->vbat * 2 + vbus_low_gap) {
				qcm_pulse_dpdm(qc_policy, 0, 1);
			} else if (qc_policy->adapter_vbus >= qc_policy->vbat * 2 + vbus_high_gap) {
				qcm_pulse_dpdm(qc_policy, 0, -1);
			} else {
				qcm_info("success to tune VBUS to target window\n");
				qcm_move_state(qc_policy, QCM_STATE_ENABLE_CP);
				break;
			}
			break;
		case QCM_STATE_ENABLE_CP:
			qc_policy->enable_cp_count++;
			if (qc_policy->enable_cp_count >= 5) {
				qcm_err("failed to enable charge pump, exit PDM\n");
				qcm_move_state(qc_policy, QCM_STATE_EXIT);
				break;
			}

			if (!qc_policy->master_cp_enable)
				chargerpump_set_enable(qc_policy->master_cp_chg, true);
			if (!qc_policy->disable_slave && !qc_policy->slave_cp_enable)
				chargerpump_set_enable(qc_policy->slave_cp_chg, true);

			if (qc_policy->master_cp_enable && (qc_policy->disable_slave || (!qc_policy->disable_slave && qc_policy->slave_cp_enable))) {
				qcm_info("success to enable charge pump\n");
				charger_set_term(qc_policy->charger, false);
				qcm_move_state(qc_policy, QCM_STATE_TUNE);
			} else {
				if (qc_policy->enable_cp_count != 1)
					qcm_err("failed to enable charge pump, try again\n");
				break;
			}
			break;
		case QCM_STATE_TUNE:
			qc_policy->sm_status = qcm_check_condition(qc_policy);
			if (qc_policy->sm_status == QCM_SM_EXIT) {
				qcm_info("taper charge done\n");
				qcm_move_state(qc_policy, QCM_STATE_EXIT);
			} else if (qc_policy->sm_status == QCM_SM_HOLD) {
				qcm_move_state(qc_policy, QCM_STATE_EXIT);
			} else {
				qcm_charge_tune(qc_policy);
			}
			break;
		case QCM_STATE_EXIT:
			qc_policy->tune_vbus_count = 0;
			qc_policy->enable_cp_count = 0;
			qc_policy->taper_count = 0;
			qc_policy->anti_wave_count = 0;

			charger_set_hiz(qc_policy->charger, true);
			chargerpump_set_enable(qc_policy->master_cp_chg, false);
			chargerpump_set_enable(qc_policy->slave_cp_chg, false);
			msleep(100);
			charger_qc2_vbus_mode(qc_policy->charger, QC2_VBUS_9V);
			vote(qc_policy->input_limit_votable, QC_POLICY_VOTER, false, 0);

			if (qc_policy->sm_status == QCM_SM_EXIT)
				msleep(500);
			charger_set_term(qc_policy->charger, true);
			charger_set_hiz(qc_policy->charger, false);
			qcm_move_state(qc_policy, QCM_STATE_ENTRY);
			break;
		default:
			qcm_err("No sm_state defined! Move to stop charging\n");
			break;
		}
}

static bool qcom_get_qc_type(struct qcomcharger_policy *qc_policy)
{
	if (qc_policy->charger->bc12_type == VBUS_TYPE_HVDCP_3 || qc_policy->charger->bc12_type == VBUS_TYPE_HVDCP_3P5)
		return true;
	else
		return false;
}

static void qcm_update_qc3_type(struct qcomcharger_policy *qc_policy)
{
	if (qc_policy->qc3_type ) {
		qc_policy->disable_slave = false;
		qc_policy->max_step = QC3_MAX_TUNE_STEP;
		qc_policy->tune_step_vbus = QC3_VBUS_STEP;
		qc_policy->tune_step_ibat = tune_step_ibat_qc3_27;
		qc_policy->tune_step_ibus = qc_policy->tune_step_ibus_qc3_27;
		qc_policy->max_ibus = qc_policy->max_ibus_qc3_27w;
		qc_policy->max_ibat = qc_policy->max_ibat_qc3_27w;
	}
}

static void qcm_update_charge_status(struct qcomcharger_policy *qc_policy)
{
	union power_supply_propval val = {0,};

	charger_get_adc(qc_policy->charger, CHG_ADC_VBUS, &qc_policy->adapter_vbus);
	charger_get_adc(qc_policy->charger, CHG_ADC_IBUS, &qc_policy->sw_ibus);
	chargerpump_get_adc_value(qc_policy->master_cp_chg, CP_ADC_IBUS, &qc_policy->master_cp_ibus);
	chargerpump_get_adc_value(qc_policy->slave_cp_chg, CP_ADC_IBUS, &qc_policy->slave_cp_ibus);
	chargerpump_get_is_enable(qc_policy->master_cp_chg, &qc_policy->master_cp_enable);
	chargerpump_get_is_enable(qc_policy->slave_cp_chg, &qc_policy->slave_cp_enable);
	qc_policy->cp_total_ibus = qc_policy->master_cp_ibus + qc_policy->slave_cp_ibus;

	qc_policy->qc3_type = qcom_get_qc_type(qc_policy);

	power_supply_get_property(qc_policy->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	qc_policy->soc = val.intval;

	power_supply_get_property(qc_policy->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	qc_policy->ibat = val.intval / 1000;

	power_supply_get_property(qc_policy->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	qc_policy->vbat = val.intval / 1000;

	qc_policy->input_suspend = !get_effective_result(qc_policy->input_limit_votable);

	qcm_update_qc3_type(qc_policy);
	qc_policy->target_fcc = 6000; /*get_effective_result(qc_policy->total_fcc_votable);*/

	qcm_info("BUS = [%d %d %d %d %d], CP = [%d %d], BAT = [%d %d %d], STEP = [%d %d %d %d], FCC = [%d], FFC_CMD = [%d]\n",
		qc_policy->adapter_vbus, qc_policy->sw_ibus, qc_policy->master_cp_ibus, qc_policy->slave_cp_ibus, qc_policy->cp_total_ibus,
		qc_policy->master_cp_enable, qc_policy->slave_cp_enable, qc_policy->soc, qc_policy->vbat, qc_policy->ibat,
		qc_policy->ibat_step, qc_policy->vbus_step, qc_policy->ibus_step, qc_policy->final_step,
		qc_policy->target_fcc, qc_policy->input_suspend);
}

static void qcm_main_sm(struct work_struct *work)
{
	struct qcomcharger_policy *qc_policy = container_of(work, struct qcomcharger_policy, main_sm_work.work);
	int sm_delay = QCM_SM_DELAY_400MS;

	qcm_update_charge_status(qc_policy);
	qcm_handle_sm(qc_policy);

	if (qc_policy->sm_state == QCM_STATE_ENTRY && qc_policy->sm_status == QCM_SM_EXIT) {
		qcm_info("exit QCM\n");
	} else {
		if (qc_policy->no_delay) {
			sm_delay = 0;
			qc_policy->no_delay = false;
		} else {
			switch (qc_policy->sm_state) {
			case QCM_STATE_ENTRY:
				sm_delay = QCM_SM_DELAY_500MS;
				break;
			case QCM_STATE_INIT_VBUS:
				sm_delay = QCM_SM_DELAY_500MS;
				break;
			case QCM_STATE_EXIT:
			case QCM_STATE_ENABLE_CP:
				sm_delay = QCM_SM_DELAY_200MS;
				break;
			case QCM_STATE_TUNE:
				sm_delay = QCM_SM_DELAY_400MS;
				break;
			default:
				qcm_err("not supportted qcm_sm_state\n");
				break;
			}
		}
		schedule_delayed_work(&qc_policy->main_sm_work, msecs_to_jiffies(sm_delay));
	}
}

static void qcm_psy_change(struct work_struct *work)
{
	struct qcomcharger_policy *qc_policy = container_of(work, struct qcomcharger_policy, psy_change_work.work);

	qc_policy->qc3_type = qcom_get_qc_type(qc_policy);

	if (qc_policy->qc3_type && !qc_policy->qcm_sm_busy) {
		qc_policy->qcm_sm_busy = true;
		schedule_delayed_work(&qc_policy->main_sm_work, 0);
	} else if (!qc_policy->qc3_type && qc_policy->qcm_sm_busy) {
		cancel_delayed_work_sync(&qc_policy->main_sm_work);
		vote(qc_policy->input_limit_votable, QC_POLICY_VOTER, false, 0);
		chargerpump_set_enable(qc_policy->master_cp_chg, false);
		chargerpump_set_enable(qc_policy->slave_cp_chg, false);
		qc_policy->tune_vbus_count = 0;
		qc_policy->enable_cp_count = 0;
		qc_policy->taper_count = 0;
		qc_policy->anti_wave_count = 0;
		qcm_move_state(qc_policy, QCM_STATE_ENTRY);
		qc_policy->qcm_sm_busy = false;
	}

	qc_policy->psy_notify_busy = false;
	return;
}

static int qcm_psy_notifier_cb(struct notifier_block *nb, unsigned long ev, void *data)
{
	struct qcomcharger_policy *qc_policy = container_of(nb, struct qcomcharger_policy, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	spin_lock_irqsave(&qc_policy->psy_change_lock, flags);
	if (strcmp(psy->desc->name, "usb") == 0 && !qc_policy->psy_notify_busy) {
		qc_policy->psy_notify_busy = true;
		schedule_delayed_work(&qc_policy->psy_change_work, 0);
	}
	spin_unlock_irqrestore(&qc_policy->psy_change_lock, flags);

	return NOTIFY_OK;
}

static int qcm_parse_dt(struct charger_manager *manager, struct qcomcharger_policy *qc_policy)
{
	struct device_node *node = of_find_node_by_name(manager->dev->of_node, "lx_qc_policy");
	int ret = 0;

	if (!node) {
		qcm_err("device tree node missing\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "max_vbus", &qc_policy->max_vbus);
	ret = of_property_read_u32(node, "max_ibus_qc3_18w", &qc_policy->max_ibus_qc3_18w);
	ret = of_property_read_u32(node, "max_ibat_qc3_18w", &qc_policy->max_ibat_qc3_18w);
	ret = of_property_read_u32(node, "max_ibus_qc3_27w", &qc_policy->max_ibus_qc3_27w);
	ret = of_property_read_u32(node, "max_ibat_qc3_27w", &qc_policy->max_ibat_qc3_27w);
	ret = of_property_read_u32(node, "max_ibus_qc35", &qc_policy->max_ibus_qc35);
	ret = of_property_read_u32(node, "max_ibat_qc35", &qc_policy->max_ibat_qc35);
	ret = of_property_read_u32(node, "tune_step_ibus_qc3_27", &qc_policy->tune_step_ibus_qc3_27);
	ret = of_property_read_u32(node, "tune_step_ibus_qc3_18", &qc_policy->tune_step_ibus_qc3_18);
	ret = of_property_read_u32(node, "tune_step_ibus_qc35", &qc_policy->tune_step_ibus_qc35);
	ret = of_property_read_u32(node, "tune_step_ibat_qc3_27", &tune_step_ibat_qc3_27);
	ret = of_property_read_u32(node, "tune_step_ibat_qc3_18", &tune_step_ibat_qc3_18);
	ret = of_property_read_u32(node, "tune_step_ibat_qc35", &tune_step_ibat_qc35);
	ret = of_property_read_u32(node, "high_soc", &qc_policy->high_soc);
	ret = of_property_read_u32(node, "cv_vbat", &qc_policy->cv_vbat);
	ret = of_property_read_u32(node, "cv_vbat_ffc", &qc_policy->cv_vbat_ffc);

	return ret;
}

int qomcharger_policy_init(struct charger_manager *manager)
{
	int ret = 0;
	struct qcomcharger_policy *qc_policy;

	qc_policy = devm_kzalloc(manager->dev, sizeof(struct qcomcharger_policy), GFP_KERNEL);
	if (!qc_policy)
		return -ENOMEM;

	qc_policy->tune_vbus_count = 0;
	qc_policy->enable_cp_count = 0;
	qc_policy->taper_count = 0;
	qc_policy->sm_state = QCM_STATE_ENTRY;
	qc_policy->qcm_sm_busy = false;
	qc_policy->psy_notify_busy = false;
	spin_lock_init(&qc_policy->psy_change_lock);

	ret = qcm_parse_dt(manager, qc_policy);
	if (ret) {
		qcm_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

	if (!qcm_check_charger_dev(qc_policy)) {
		qcm_err("failed to check charger device\n");
		return -ENODEV;
	}

	if (!qcm_check_psy(qc_policy)) {
		qcm_err("failed to check psy\n");
		return -ENODEV;
	}

	if (!qcm_check_votable(qc_policy)) {
		qcm_err("failed to check votable\n");
	}

	INIT_DELAYED_WORK(&qc_policy->psy_change_work, qcm_psy_change);
	INIT_DELAYED_WORK(&qc_policy->main_sm_work, qcm_main_sm);

	qc_policy->nb.notifier_call = qcm_psy_notifier_cb;
	ret = power_supply_reg_notifier(&qc_policy->nb);
	if (ret < 0) {
		qcm_err("failed to register psy notifier rc = %d\n", ret);
		return ret;
	}

	qcm_info("QCM probe success\n");
	return ret;
}
