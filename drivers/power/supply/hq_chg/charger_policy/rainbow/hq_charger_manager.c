// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include "hq_printk.h"
#include "hq_charger_manager_internal.h"
#include "hq_charger_manager.h"
//#include "mi_thermal_notify.h"
//#include "hqsys_pcba.h"
#include "hq_notify.h"
#include "../../../mtk_battery.h"
#include "xm_chg_uevent.h"
#include "mi_thermal_notify.h"

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
#include <../../../../../../misc/xiaomi_usb_touch_notifier.h>
#endif

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_CM]"
#endif

/******************************************************************************/
/*                      DECLARE EXTERNAL FUNCTIONS                            */
/******************************************************************************/
extern void batt_soh20_aging_test(struct work_struct *work);
extern void update_battery_cycle_count(struct work_struct *work);

#if IS_ENABLED(CONFIG_HQ_THERMAL_POLICY)
extern int hq_thermal_policy_init(struct charger_manager *manager);
extern int hq_thermal_policy_deinit(struct charger_manager *manager);
extern int hq_thermal_policy_run(struct charger_manager *manager);
#endif

#if IS_ENABLED(CONFIG_HQ_JEITA_POLICY)
extern int hq_jeita_policy_init(struct charger_manager *manager);
extern int hq_jeita_policy_deinit(struct charger_manager *manager);
extern int hq_jeita_policy_run(struct charger_manager *manager);
#endif

#if IS_ENABLED(CONFIG_HQ_TERM_RECHARGE_POLICY)
extern int hq_term_recharge_policy_init(struct charger_manager *manager);
extern int hq_term_recharge_policy_deinit(struct charger_manager *manager);
extern int hq_term_recharge_policy_run(struct charger_manager *manager);
extern int hq_term_recharge_policy_stop(struct charger_manager *manager);
#endif

#if IS_ENABLED(CONFIG_HQ_SHUTDOWN_POLICY)
extern int hq_shutdown_policy_init(struct charger_manager *manager);
extern int hq_shutdown_policy_deinit(struct charger_manager *manager);
extern int hq_shutdown_policy_run(struct charger_manager *manager);
#endif

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
extern int reverse_charge_policy_init(struct charger_manager *manager);
extern int reverse_charge_policy_deinit(struct charger_manager *manager);
extern int reverse_charge_policy_stop(struct charger_manager *manager);
extern void reverse_charge_func(struct charger_manager *manager);
#endif

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
extern int xm_batt_health_init(struct charger_manager *manager);
extern int xm_batt_health_deinit(struct charger_manager *manager);
extern int xm_batt_health_run(struct charger_manager *manager);
extern int xm_batt_health_stop(struct charger_manager *manager);
#endif

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
extern int xm_smart_chg_init(struct charger_manager *manager);
extern int xm_smart_chg_deinit(struct charger_manager *manager);
extern int xm_smart_chg_run(struct charger_manager *manager);
extern int xm_smart_chg_stop(struct charger_manager *manager);
#endif

extern int charger_manager_misc_init(struct charger_manager *manager);
extern int charger_manager_misc_deinit(struct charger_manager *manager);
extern int charger_manager_misc_run(struct charger_manager *manager);
extern int charger_manager_misc_stop(struct charger_manager *manager);

extern int charger_manager_chg_psy_register(struct charger_manager *manager);
extern int charger_manager_usb_psy_register(struct charger_manager *manager);
extern int charger_manager_batt_psy_register(struct charger_manager *manager);
extern int hq_batt_sysfs_create_group(struct charger_manager *manager);
extern int hq_usb_sysfs_create_group(struct charger_manager *manager);

extern int audio_status_notifier_register_client(struct notifier_block *nb);
extern int audio_status_notifier_unregister_client(struct notifier_block *nb);

/******************************************************************************/
/*                               STATIC DEFINE                                */
/******************************************************************************/
static int float_retry_cnt;

#if IS_ENABLED(CONFIG_BC12_RETRY_FOR_MI_PD)
static bool bc12_retry_flag = 1;
#endif

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
static struct xiaomi_usb_notify_data xiaomi_touch_usb_data;
#endif

/******************************************************************************/
/*                     CHARGER MANAGER CORE FUNCTIONS                         */
/******************************************************************************/
static int charger_manager_wake_thread(struct charger_manager *manager)
{
	manager->run_thread = true;
	wake_up(&manager->wait_queue);

	return 0;
}

/******************************************************************************/
/*                         POWER SUPPLY NOTIFIER                              */
/******************************************************************************/
static int charger_manager_psy_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	/* TODO: fire routines related to power supply property changed
	 * WARNING: psy notifier is atomic notifier
	 */

	return NOTIFY_OK;
}

static int charger_manager_register_psy_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->psy_nb.notifier_call = charger_manager_psy_notifier_call;

	ret = power_supply_reg_notifier(&manager->psy_nb);
	if (ret < 0) {
		hq_err("couldn't register psy notifier ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static void charger_manager_unregister_psy_notifier(struct charger_manager *manager)
{
	power_supply_unreg_notifier(&manager->psy_nb);
}

/******************************************************************************/
/*                            THERMAL NOTIFIER                                */
/******************************************************************************/
static int charger_manager_thermal_notifier_call(struct notifier_block *notifier,
						unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(notifier,
							struct charger_manager, thermal_nb);

	switch (event) {
	case EVENT_BOARDTEMP_CHANGE:
		manager->board_temp = (*(int *)data);
		hq_info("thermal notifier board_temp = %d\n", manager->board_temp);
		break;
	default:
		hq_err("unsupported thermal notifier event: %d\n", event);
		break;
	}

	return NOTIFY_DONE;
}

static int charger_manager_register_thermal_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->thermal_nb.notifier_call = charger_manager_thermal_notifier_call;

	ret = mi_thermal_reg_notifier(&manager->thermal_nb);
	if (ret < 0) {
		hq_err("couldn't register thermal notifier, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int charger_manager_unregister_thermal_notifier(struct charger_manager *manager)
{
	return mi_thermal_unreg_notifier(&manager->thermal_nb);
}

/******************************************************************************/
/*                         CHARGER CHANGED NOTIFIER                           */
/******************************************************************************/
static int charger_manager_charger_changed_notifier_call(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_changed_nb);

	charger_manager_wake_thread(manager);

	return NOTIFY_OK;
}

static int charger_manager_register_charger_changed_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->charger_changed_nb.notifier_call = charger_manager_charger_changed_notifier_call;

	ret = charger_register_notifier(&manager->charger_changed_nb);
	if (ret < 0) {
		hq_err("couldn't register charger notifier, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int charger_manager_unregister_charger_changed_notifier(struct charger_manager *manager)
{

	return charger_unregister_notifier(&manager->charger_changed_nb);
}

/******************************************************************************/
/*                             CHARGER NOTIFIER                               */
/******************************************************************************/
static int charger_manager_charger_notifier_call(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_nb);

	struct cm_notify *noti = data;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	switch (event) {
	case CHARGER_EVENT_DEFAULT:
		hq_info("received charger notifier event: %d\n", event);
		break;
	case CHARGER_EVENT_BC12_DONE:
		hq_info("received charger notifier event: %d\n", event);
		break;
	case CHARGER_EVENT_HVDCP_DONE:
		hq_info("received charger notifier event: %d\n", event);
		break;
	case CHARGER_EVENT_CHG_TIMEOUT:
		manager->chg_timeout = true;
		hq_info("received charger notifier event: %d\n", event);
		break;
	case CHARGER_EVENT_CID_DETECT:

		manager->cid_state = noti->cid_state;

		if (noti->cid_state) {
			if (!manager->typec_attach) {
				ret = alarm_try_to_cancel(&manager->set_soft_cid_timer);
				if (ret < 0) {
					hq_err("callback was running, skip timer\n");
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 5;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
				hq_info("set_soft_cid_timer alarm timer start:%d, %lld %ld\n", ret,
					end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&manager->set_soft_cid_timer, ktime);
			}
		} else {
			if (manager->screen_state == SCREEN_STATE_BRIGHT) {
				manager->soft_cid = false;
			} else if (manager->screen_state == SCREEN_STATE_BLACK && manager->soft_cid == true){
				cancel_delayed_work_sync(&manager->handle_cc_status_work);
				queue_delayed_work(system_freezable_wq, &manager->handle_cc_status_work, 0);
			}
		}
		hq_info("received charger notifier event: %d, cid_state = %d\n", event, manager->cid_state);
		break;
	default:
		hq_err("unsupported charger notifier event: %d\n", event);
		break;
	}

	return NOTIFY_OK;
}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
static int charger_manager_fg_notifier_call(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, fg_nb);

	hq_info("received fg notifier event: %d\n", event);

	switch (event) {
	case FG_DAEMON_CMD_SET_AGING_FACTOR:
		manager->new_fg_raw_soh = (*(int *)data) / 100;
		schedule_delayed_work(&manager->update_fg_raw_soh_work, msecs_to_jiffies(2000));
		break;
	case FG_DAEMON_CMD_SET_BAT_CYCLES:
		manager->new_fg_cycle = (*(int *)data);
		schedule_delayed_work(&manager->update_fg_cycle_work, msecs_to_jiffies(0));
		break;
	default:
		hq_err("unsupported charger notifier event: %d\n", event);
		break;
	}

	return NOTIFY_OK;
}
#endif

static int charger_manager_register_charger_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->charger_nb.notifier_call = charger_manager_charger_notifier_call;

	ret = hq_charger_notifier_register(&manager->charger_nb);
	if (ret < 0) {
		hq_err("couldn't register charger notifier, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
static int charger_manager_register_fg_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->fg_nb.notifier_call = charger_manager_fg_notifier_call;

	ret = hq_fg_notifier_register(&manager->fg_nb);
	if (ret < 0) {
		hq_err("couldn't register fg notifier, ret = %d\n", ret);
		return ret;
	}

	return ret;
}
#endif

static int charger_manager_unregister_charger_notifier(struct charger_manager *manager)
{

	return hq_charger_notifier_unregister(&manager->charger_nb);
}

/******************************************************************************/
/*                              TCPC NOTIFIER                                 */
/******************************************************************************/
static int charger_manager_tcpc_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct charger_manager *manager =
				container_of(nb, struct charger_manager, tcpc_nb);

	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	hq_info("noti event: %d %d\n", (int)event, (int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
			manager->pd_curr_max = noti->vbus_state.ma;
			manager->pd_volt_max = noti->vbus_state.mv;

			vote(manager->main_icl_votable, TYPEC_SINK_VBUS_VOTER, true, manager->pd_curr_max);
			hq_info("TCP_NOTIFY_SINK_VBUS pd_curr_max = %d\n", manager->pd_curr_max);

			#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
			if (manager->pd_curr_max == 0) {
				chargerpump_set_enable(manager->master_cp_chg, false);
				hq_info("PD pd_curr_max = 0 disable cp !!!\n");
			}
			#endif
		}
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		hq_info("source vbus %dmV %dmA type(0x%02X)\n",
				    noti->vbus_state.mv, noti->vbus_state.ma, noti->vbus_state.type);
#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
		if (manager->reverse_charge_policy) {
			if(noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT)
				manager->reverse_charge_policy->pd30_source = true;
			if(noti->vbus_state.mv)
				manager->reverse_charge_policy->otg_vbus_level = noti->vbus_state.mv;
			else
				manager->reverse_charge_policy->otg_vbus_level = 0;

			reverse_charge_func(manager);
		} else {
			hq_info("reverse_charge_policy is NULL\n");
		}
		break;
#endif
		ret = charger_set_otg(manager->charger, !!noti->vbus_state.mv);
		if (ret < 0)
			hq_err("%s set otg fail");
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.new_state == TYPEC_UNATTACHED)
			manager->pd_active = CHARGE_PD_INVALID;

		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager or tcpc is nullptr\n");
			break;
		}

		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!manager->typec_attach) {
			hq_info("typec plug in, polarity = %d\n", noti->typec_state.polarity);
			manager->typec_attach = true;
			manager->cid_status = true;
			if (manager->ui_cc_toggle) {
				ret = alarm_try_to_cancel(&manager->otg_ui_close_timer);
				if (ret < 0) {
					hq_err("callback was running, skip timer\n");
				}
				hq_info("OTG ON:typec plug in, cancel hrtimer\n");
			}

		} else if (old_state != TYPEC_UNATTACHED &&
						new_state == TYPEC_UNATTACHED &&
						manager->typec_attach) {
			hq_info("typec plug out\n");
			manager->typec_attach = false;
			manager->cid_status = false;
			if (manager->ui_cc_toggle) {
				ret = alarm_try_to_cancel(&manager->otg_ui_close_timer);
				if (ret < 0) {
					hq_err("callback was running, skip timer\n");
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);

				hq_info("OTG ON:alarm timer start:%d, %lld %ld\n", ret,
						end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&manager->otg_ui_close_timer, ktime);
			}
#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
			reverse_charge_policy_stop(manager);
#endif
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		manager->is_pr_swap = true;
		if (noti->swap_state.new_role == PD_ROLE_SINK)
			manager->pd_active = 10;
		break;
	case TCP_NOTIFY_DR_SWAP:
		manager->is_dr_swap = true;
		break;
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_done = true;
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			//manager->pd_contract_update = false;
			manager->is_dr_swap = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			manager->pd_done = true;
			//manager->pd_contract_update = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_PPS_ACTIVE;
			xm_uevent_report(manager);
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			manager->pd_done = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_ACTIVE;
			manager->is_dr_swap = false;
			break;
		case PD_CONNECT_TYPEC_ONLY_SNK_DFT:
		case PD_CONNECT_TYPEC_ONLY_SNK:
			manager->pd_done = true;
			break;
		default:
			break;
		}
		charger_manager_wake_thread(manager);
		break;
	case TCP_NOTIFY_HARD_RESET_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			//manager->pd_contract_update = false;
			manager->is_dr_swap = false;
			break;
		}
		break;
	default:
		break;
	}

	if (!IS_ERR_OR_NULL(manager->charger)) {
		manager->charger->m_pd_active = manager->pd_active;
	}

	return NOTIFY_OK;
}

static int charger_manager_register_tcpc_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->tcpc_nb.notifier_call = charger_manager_tcpc_notifier_call;

	ret = register_tcp_dev_notifier(manager->tcpc, &manager->tcpc_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		hq_err("couldn't register tcpc notifier, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int charger_manager_unregister_tcpc_notifier(struct charger_manager *manager)
{
	return unregister_tcp_dev_notifier(manager->tcpc, &manager->tcpc_nb, TCP_NOTIFY_TYPE_ALL);
}

/******************************************************************************/
/*                              DISP NOTIFIER                                 */
/******************************************************************************/
static int charger_manager_disp_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	int blank = *(int *)v;
	struct charger_manager *manager = container_of(nb, struct charger_manager, disp_nb);

	if (!(val == MTK_DISP_EARLY_EVENT_BLANK || val == MTK_DISP_EVENT_BLANK)) {
		hq_debug("event(%lu) do not need process\n", val);
		return NOTIFY_OK;
	}

	switch (blank) {
	case MTK_DISP_BLANK_UNBLANK: //power on
		manager->screen_state = SCREEN_STATE_BRIGHT;
		if (!manager->cid_state) {
			manager->soft_cid = false;
		}
		hq_info("screen_state = %d (1: black 2: bright)\n", manager->screen_state);

		break;
	case MTK_DISP_BLANK_POWERDOWN: //power off
		manager->screen_state = SCREEN_STATE_BLACK;
		hq_info("screen_state = %d (1: black 2: bright)\n", manager->screen_state);

		break;
	}

	if (manager->soft_cid) {
		cancel_delayed_work_sync(&manager->handle_cc_status_work);
		queue_delayed_work(system_freezable_wq, &manager->handle_cc_status_work, 0);
	}

	return NOTIFY_OK;
}

static int charger_manager_register_disp_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->disp_nb.notifier_call = charger_manager_disp_notifier_call;
	ret = mtk_disp_notifier_register("charger_manager", &manager->disp_nb);
	if (ret < 0) {
		hq_err("couldn't register disp notifier, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int charger_manager_unregister_disp_notifier(struct charger_manager *manager)
{
	return mtk_disp_notifier_unregister(&manager->disp_nb);
}

/******************************************************************************/
/*                              AUDIO NOTIFIER                                 */
/******************************************************************************/

static int charger_manager_audio_status_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	struct charger_manager *manager = container_of(nb, struct charger_manager, audio_nb);

	if (manager == NULL) {
		hq_err("manager is NULL\n");
		return NOTIFY_DONE;
	}

	manager->audio_status = val;

	if (manager->soft_cid) {
		cancel_delayed_work_sync(&manager->handle_cc_status_work);
		queue_delayed_work(system_freezable_wq, &manager->handle_cc_status_work, 0);
	}

	hq_err("audio_status = %lu\n", val);

	return NOTIFY_OK;
}

static int charger_manager_register_audio_status_notifier(struct charger_manager *manager)
{
	int ret = 0;

	if (manager == NULL) {
		hq_err("manager is NULL\n");
		return -1;
	}

	manager->audio_nb.notifier_call = charger_manager_audio_status_notifier_call;
	ret = audio_status_notifier_register_client(&manager->audio_nb);
	if (ret < 0) {
		hq_err("couldn't register audio_status notifier, ret = %d\n", ret);
	}

	return ret;
}

static int charger_manager_unregister_audio_status_notifier(struct charger_manager *manager)
{
	int ret = 0;

	if (manager == NULL) {
		hq_err("manager is NULL\n");
		return -1;
	}

	ret = audio_status_notifier_unregister_client(&manager->audio_nb);
	if (ret < 0) {
		hq_err("couldn't register audio_status notifier, ret = %d\n", ret);
	}

	return ret;
}

/******************************************************************************/

static void set_otg_ui_work_func(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, set_otg_ui_work.work);

	if (manager == NULL || manager->tcpc == NULL) {
		hq_err("manager or manager->tcpc is NULL\n");
		return;
	}

	if (manager->typec_attach) {
		return;
	}

	mutex_lock(&manager->wakelock_mutex);
	__pm_stay_awake(manager->cm_wakelock);

	hq_info("typec_attach = %d, ui_cc_toggle = %d\n",
			manager->typec_attach, manager->ui_cc_toggle);

	if (manager->ui_cc_toggle) {
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_DRP);
		hq_info("set cc drp\n");
	} else {
		if (!manager->soft_cid) {
			tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_SNK);
			hq_info("set cc rd\n");
		} else {
			cancel_delayed_work_sync(&manager->handle_cc_status_work);
			queue_delayed_work(system_freezable_wq, &manager->handle_cc_status_work, 0);
		}
	}

	hq_info("end\n");

	__pm_relax(manager->cm_wakelock);
	mutex_unlock(&manager->wakelock_mutex);

	return;
}

static void handle_cc_status_work_func(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, handle_cc_status_work.work);

	if (manager == NULL || manager->tcpc == NULL) {
		hq_err("manager or manager->tcpc is NULL\n");
		return;
	}

	hq_info("typec_attach = %d, screen_state = %d, audio_status = %d, manager->soft_cid = %d, manager->ui_cc_toggle = %d\n",
			manager->typec_attach, manager->screen_state, manager->audio_status, manager->soft_cid, manager->ui_cc_toggle);

	if (manager->typec_attach) {
		return;
	}

	if (!manager->soft_cid || manager->ui_cc_toggle) {
		return;
	}

	mutex_lock(&manager->wakelock_mutex);
	__pm_stay_awake(manager->cm_wakelock);

	if (manager->audio_status) {
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_TRY_SNK);
		hq_info("set cc drp\n");
	} else {
		if (manager->screen_state == SCREEN_STATE_BLACK) {
			tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_SNK);
			hq_info("set cc rd\n");
		} else if (manager->screen_state == SCREEN_STATE_BRIGHT) {
			tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_TRY_SNK);
			hq_info("set cc drp\n");
		}
	}

	__pm_relax(manager->cm_wakelock);
	mutex_unlock(&manager->wakelock_mutex);

	hq_info("end\n");

	return;
}

static enum alarmtimer_restart otg_ui_close_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, otg_ui_close_timer);

	if (manager != NULL) {
		manager->ui_cc_toggle = false;
		cancel_delayed_work(&manager->set_otg_ui_work);
		queue_delayed_work(system_freezable_wq, &manager->set_otg_ui_work, 0);
	}

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart set_soft_cid_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, set_soft_cid_timer);
	hq_info("enter\n");

	if (manager != NULL && !manager->typec_attach) {
		manager->soft_cid = true;
		cancel_delayed_work(&manager->handle_cc_status_work);
		queue_delayed_work(system_freezable_wq, &manager->handle_cc_status_work, 0);
	}

	hq_info("end\n");
	return ALARMTIMER_NORESTART;
}

int charger_manager_get_current(struct charger_manager *manager, int *curr)
{
	int val;
	int ret = 0;
#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	union power_supply_propval pval;
#endif

	*curr = 0;

	ret = charger_get_adc(manager->charger, CHG_ADC_IBUS, &val);
	if (ret < 0) {
		hq_err("Couldn't read input curr ret=%d\n", ret);
	} else
		*curr += val;

#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	if (manager->cp_master_psy) {

		ret = power_supply_get_property(manager->cp_master_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			hq_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	if (manager->cp_slave_psy) {
		ret = power_supply_get_property(manager->cp_slave_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			hq_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}
#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

#endif /* CONFIG_CHARGE_ARCH_CHARGEPUMP */

	return 0;
}
EXPORT_SYMBOL(charger_manager_get_current);

static int main_chg_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		hq_err("the value of main fcc is error.\n");
		return value;
	}

	ret = charger_set_ichg(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set ichg fail.\n");
	}

	return ret;
}

static int main_chg_fv_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_set_term_volt(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set term volt fail.\n");
	}

	return ret;
}

static int main_chg_icl_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		hq_err("the value of main chg icl is error.\n");
		return value;
	}

	ret = charger_set_input_curr_lmt(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set icl fail.\n");
	}

	return ret;
}

static int main_chg_iterm_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_set_term_curr(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set iterm fail.\n");
	}

	return ret;
}

static int total_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;

	if (value >= FASTCHARGE_MIN_CURR && (g_policy->state == POLICY_RUNNING)) {
		vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
		vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
	} else {
		vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, false, 0);
		if (value >= 0)
			vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, value);
	}

	return 0;
}

static int main_chg_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_disable_power_path(manager->charger, enable);
	if (ret < 0) {
		hq_err("charger disable_power_path fail.\n");
	}

	return ret;
}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
static int cp_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	int ret = 0;
	struct charger_manager *manager = data;
	struct chargerpump_dev *master_cp_chg = manager->master_cp_chg;

#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	struct chargerpump_dev *slave_cp_chg = manager->slave_cp_chg;
#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

	if (manager->cp_master_psy) {
		ret = chargerpump_set_enable(master_cp_chg, enable);
		if (ret < 0) {
			hq_err("master_cp_chg set chg fail.\n");
		}
	}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	if (manager->cp_slave_psy) {
		ret = chargerpump_set_enable(slave_cp_chg, enable);
		if (ret < 0) {
			hq_err("slave_cp_chg set chg fail.\n");
		}
	}
#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

	return ret;
}
#endif /* CONFIG_CHARGE_ARCH_CHARGEPUMP */

static int charger_manager_create_votable(struct charger_manager *manager)
{
	int ret = 0;

	if (manager->charger) {
		manager->main_fcc_votable = create_votable("MAIN_FCC", VOTE_MIN, main_chg_fcc_vote_callback, manager);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			hq_err("fail create MAIN_FCC voter.\n");
			return PTR_ERR(manager->main_fcc_votable);
		}

		manager->fv_votable = create_votable("MAIN_FV", VOTE_MIN, main_chg_fv_vote_callback, manager);
		if (IS_ERR_OR_NULL(manager->fv_votable)) {
			hq_err("fail create MAIN_FV voter.\n");
			return PTR_ERR(manager->fv_votable);
		}

		manager->main_icl_votable = create_votable("MAIN_ICL", VOTE_MIN, main_chg_icl_vote_callback, manager);
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			hq_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->main_icl_votable);
		}

		manager->iterm_votable = create_votable("MAIN_ITERM", VOTE_MIN, main_chg_iterm_vote_callback, manager);
		if (IS_ERR_OR_NULL(manager->iterm_votable)) {
			hq_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->iterm_votable);
		}

		manager->main_chg_disable_votable = create_votable("MAIN_CHG_DISABLE", VOTE_SET_ANY, main_chg_disable_vote_callback, manager);
		if (IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
			hq_err("fail create MAIN_CHG_DISABLE voter.\n");
			return PTR_ERR(manager->main_chg_disable_votable);
		}

		manager->total_fcc_votable = create_votable("TOTAL_FCC", VOTE_MIN, total_fcc_vote_callback, manager);
		if (IS_ERR_OR_NULL(manager->total_fcc_votable)) {
			hq_err("fail create TOTAL_FCC voter.\n");
			return PTR_ERR(manager->total_fcc_votable);
		}
	}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	if (manager->cp_master_psy || manager->cp_slave_psy) {
		manager->cp_disable_votable = create_votable("CP_DISABLE", VOTE_SET_ANY, cp_disable_vote_callback, manager);
		if (IS_ERR_OR_NULL(manager->cp_disable_votable)) {
			hq_err("fail create CP_DISABLE voter.\n");

			return PTR_ERR(manager->cp_disable_votable);
		}
	}
#endif

	return ret;
}

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
static int charger_monitor_fg_i2c_status(struct charger_manager *manager)
{
	int ret = 0, vbus;
	int  fg_status = 0;
	struct adapter_dev *adapter = adapter_find_dev_by_name("pd_adapter1");
	static bool hvdcp_fall_down;
	union power_supply_propval pval;

	if (IS_ERR_OR_NULL(manager->fuel_gauge))  {
		hq_err("failed to get fuel gauge\n");
		return -EINVAL;
	}

	fg_status = fuel_gauge_check_fg_status(manager->fuel_gauge);

	if (fg_status & FG_ERR_AUTH_FAIL || fg_status & FG_EER_I2C_FAIL || fg_status & FG_ERR_CHG_WATT || !manager->cp_master_ok) {
		if (manager->cp_master_ok) {
			vote(manager->fv_votable, FG_I2C_ERR, true, 4100);
		}
		if (fg_status & FG_EER_I2C_FAIL || !manager->cp_master_ok) {
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 0);
			mdelay(100);
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 500);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 500);
		} else if (fg_status & FG_ERR_AUTH_FAIL || fg_status & FG_ERR_CHG_WATT) {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 2000);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 2000);
		}

		if(manager->vbus_type == VBUS_TYPE_HVDCP) {
			charger_qc2_vbus_mode(manager->charger, 5000);
			hvdcp_fall_down = true;
			hq_info("HVDCP 9V fall 5V\n");
		} else if (manager->pd_active) {
			adapter_set_cap(adapter, 0, 5000, 2000);
			hq_info("PD 9V fall 5V\n");
		}

		hq_info("fg  status: %d\n", fg_status);
	} else {

		if (manager->vbus_type == VBUS_TYPE_HVDCP && !manager->lpd_charging_limit && hvdcp_fall_down) {
			power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			vbus = pval.intval;
			if (vbus < 7200 && vbus > 4200) {
				charger_qc2_vbus_mode(manager->charger, 9000);
				hvdcp_fall_down = false;
				hq_info("HVDCP 5V raise 9V\n");
			}
		}

		vote(manager->fv_votable, FG_I2C_ERR, false, 0);
		vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
		vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
	}
	return ret;
}
#endif

static void charger_manager_monitor(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	uint32_t adc_buf_len = 0;
	uint8_t i = 0;
	char adc_buf[MIAN_CHG_ADC_LENGTH + 1] = {0};
	uint32_t iterm = 0;
	uint32_t fv = 0;
	int ichg = 0;
	bool charge_en = 0;

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		hq_err("battery psy is null\n");
		return;
	}

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		hq_err("get battery soc error.\n");
	else
		manager->soc = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		hq_err("get battery volt error.\n");
	else
		manager->vbat = pval.intval / 1000;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		hq_err("get battery current error.\n");
	else
		manager->ibat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0)
		hq_err("get battery temperature error.\n");
	else
		manager->tbat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0)
		hq_err("get charge status error.\n");
	else
		manager->chg_status = pval.intval;

	charger_get_term_curr(manager->charger, &iterm);

	charger_get_term_volt(manager->charger, &fv);

	charge_en = charger_get_chg(manager->charger);

	charger_get_ichg(manager->charger, &ichg);

	charger_get_input_curr_lmt(manager->charger, &manager->ibus);

	charger_get_vbus_type(manager->charger, &manager->vbus_type);

	manager->rsoc = fuel_gauge_get_rsoc(manager->fuel_gauge);

	hq_info("[Battery] soc: %d, rsoc: %d, ibat: %d, vbat: %d, tbat: %d, chg_status: %d\n",
				manager->soc, manager->rsoc, manager->ibat, manager->vbat,
				manager->tbat, manager->chg_status);
	hq_info("[CHG_REG] ibus: %d, ichg: %d, charge_en: %d, iterm: %d, fv: %d\n",
				manager->ibus, ichg, charge_en, iterm, fv);

	power_supply_changed(manager->usb_psy);
	power_supply_changed(manager->batt_psy);
	power_supply_changed(manager->chg_psy);

	for (i = 0; i < CHG_ADC_MAX; i++) {
		ret = charger_get_adc(manager->charger, i, &manager->chg_adc[i]);
		if (ret < 0) {
			hq_info("get adc failed\n");
			continue;
		}
		adc_buf_len += sprintf(adc_buf + adc_buf_len,
						"%s: %d, ", adc_name[i], manager->chg_adc[i]);
	}

	if (adc_buf_len > MIAN_CHG_ADC_LENGTH)
		adc_buf[MIAN_CHG_ADC_LENGTH] = '\0';
	hq_info("[CHG_ADC] %s\n", adc_buf);

	ret = adapter_get_usbpd_verifed(manager->pd_adapter, &manager->pd_verifed);
	if (ret < 0){
		hq_err("Couldn't get usbpd verifed ret=%d\n", ret);
	}
}

static int charger_manager_check_vindpm(struct charger_manager *manager, uint32_t vbat)
{
	struct charger_dev *charger = manager->charger;
	int ret = 0;

#if CHARGER_VINDPM_USE_DYNAMIC
	if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT1) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE1);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT2) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE2);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT3) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT4) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE4);
	} else {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE5);
	}
#else
	ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
#endif

	if (ret < 0) {
		hq_err("Failed to set vindpm, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int charger_manager_check_iindpm(struct charger_manager *manager, uint32_t vbus_type)
{
	int ret = 0;
	int ichg_ma = 0;
	int icl_ma = 0;

	switch (vbus_type) {
	case VBUS_TYPE_FLOAT:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_NONE:
		ichg_ma = manager->none_type_current;
		icl_ma = manager->none_type_current;
		break;
	case VBUS_TYPE_SDP:
		ichg_ma = manager->sdp_current;
		icl_ma = manager->sdp_current;
		break;
	case VBUS_TYPE_NON_STAND:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_CDP:
		ichg_ma = manager->cdp_current;
		icl_ma = manager->cdp_current;
		break;
	case VBUS_TYPE_DCP:
		if (manager->pd_active || manager->outdoor_chg) {
			ichg_ma = 1900;
			icl_ma = 1900;
		} else {
			ichg_ma = manager->dcp_current;
			icl_ma = manager->dcp_current;
		}
		break;
	case VBUS_TYPE_HVDCP:
		ichg_ma = manager->hvdcp_current;
		icl_ma = manager->hvdcp_input_current;
		break;
	case VBUS_TYPE_HVDCP_3:
	case VBUS_TYPE_HVDCP_3P5:
		ichg_ma = manager->hvdcp3_current;
		icl_ma = manager->hvdcp3_input_current;
		break;
	default:
		ichg_ma = manager->none_type_current;
		icl_ma = manager->none_type_current;
		break;
	}

	if ((manager->pd_active == CHARGE_PD_ACTIVE && vbus_type) || manager->pd_active == 10) {
		if (manager->pd_volt_max == 5000) {  //C-to-C
			ichg_ma = manager->pd_curr_max;
			icl_ma = manager->pd_curr_max;
			hq_err("c-to-c ichg_ma = %d\n", ichg_ma);
		} else {  //PD2.0
			ichg_ma = manager->pd_curr_max * PD20_ICHG_MULTIPLE / 1000;  //1.8 of fixed current
			manager->pd_curr_max = min(manager->pd_curr_max, 2000);
			icl_ma = manager->pd_curr_max;
			hq_err("fixed current ichg_ma = %d\n", ichg_ma);
		}
	}

	if (manager->mtbf_mode && (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)) {
		ichg_ma = MTBF_MODE_CDP_CURRENT;
		icl_ma = MTBF_MODE_CDP_CURRENT;
		hq_info("mtbf_mode=%d icl=%d ichg=%d\n", manager->mtbf_mode, icl_ma, ichg_ma);
	}

	vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, icl_ma);
	vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, ichg_ma);

	hq_info("ichg_ma = %d, icl_ma = %d, pd_active = %d vbus_type = %d, mtbf_mode = %d\n",
		ichg_ma, icl_ma, manager->pd_active, vbus_type, manager->mtbf_mode);

	return ret;
}

static void charger_manager_check_inputsuspend(struct charger_manager *manager, int soc)
{
	int mtbf_mode = manager->mtbf_mode;

	switch (mtbf_mode) {
	case DEFAULT_MODE:
		vote(manager->main_chg_disable_votable, POWER_CONTROL_VOTER, false, 0);
		break;
	case MTBF_MODE:
		if (soc >= 75) {
			vote(manager->main_chg_disable_votable, POWER_CONTROL_VOTER, true, 1);
			hq_info("In MTBF MODE the soc >75 \n");
		} else if (soc <= 60) {
			vote(manager->main_chg_disable_votable, POWER_CONTROL_VOTER, false, 0);
			hq_info("In MTBF MODE the soc < 60 \n");
		}
		break;
	case AGING_MODE:
		if (soc >= 75) {
			vote(manager->main_chg_disable_votable, POWER_CONTROL_VOTER, true, 1);
			hq_info("In AGING_MODE the soc >75 \n");
		} else if (soc <= 70) {
			vote(manager->main_chg_disable_votable, POWER_CONTROL_VOTER, false, 0);
			hq_info("In AGING_MODE the soc < 70 \n");
		}
		break;
	case MTBF_CAM_MODE:
		vote(manager->main_chg_disable_votable, POWER_CONTROL_VOTER, false, 0);
		hq_info("In MTBF_CAM_MODE\n");
		break;
	}
	if (manager->input_suspend && manager->ibus != 0) {
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, false, 0);
		hq_info("input_suspend is enable but ibus has value\n");
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, true, 1);
	}

	return;
}

static void charger_manager_timer_func(struct timer_list *timer)
{
	struct charger_manager *manager = container_of(timer,
							struct charger_manager, charger_timer);
	charger_manager_wake_thread(manager);
}

int charger_manager_start_timer(struct charger_manager *manager, uint32_t ms)
{
	del_timer(&manager->charger_timer);
	manager->charger_timer.expires = jiffies + msecs_to_jiffies(ms);
	manager->charger_timer.function = charger_manager_timer_func;
	add_timer(&manager->charger_timer);
	return 0;
}
EXPORT_SYMBOL(charger_manager_start_timer);

static int reset_vote(struct charger_manager *manager)
{
	vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, false, 0);
	vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, false, 0);
	//vote(manager->total_fcc_votable, JEITA_VOTER, false, 0);
	vote(manager->total_fcc_votable, CHG_CYCLE_VOTER, false, 0);
	vote(manager->fv_votable, CHG_CYCLE_VOTER, false, 0);
	vote(manager->fv_votable, XM_BATT_HEALTH_VOTER, false, 0);
	vote(manager->fv_votable, JEITA_VOTER, false, 0);
	vote(manager->main_icl_votable, TYPEC_SINK_VBUS_VOTER, false, 0);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	vote(manager->fv_votable, FG_I2C_ERR, false, 0);
	vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
	vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
#endif
	vote(manager->total_fcc_votable, CHG_PROTECT_VOTER, false, 0);

	vote(manager->iterm_votable, TERM_RECHARGE_VOTER, false, 0);
	vote(manager->fv_votable, TERM_RECHARGE_VOTER, false, 0);
	vote(manager->total_fcc_votable, TERM_RECHARGE_VOTER, false, 0);

	vote(manager->main_chg_disable_votable, POWER_CONTROL_VOTER, false, 0);
	vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, false, 0);

	return 0;
}

static int rerun_vote(struct charger_manager *manager)
{
	rerun_election(manager->main_chg_disable_votable);
	rerun_election(manager->total_fcc_votable);
	rerun_election(manager->main_fcc_votable);
	rerun_election(manager->main_icl_votable);

	return 0;
}

static void float_retry_detect_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, float_retry_detect_work.work);

	hq_info("float_retry_cnt = %d, vbus_type = %d\n", float_retry_cnt, manager->vbus_type);

	if (float_retry_cnt <= 3) {
		if (float_retry_cnt > 0 && manager->vbus_type != VBUS_TYPE_FLOAT) {
			float_retry_cnt = 0;
			hq_info("detect %s after %d times", vbus_type_str[manager->vbus_type], float_retry_cnt);
			return;
		}

		float_retry_cnt++;

		charger_force_dpdm(manager->charger);

		schedule_delayed_work(&manager->float_retry_detect_work, msecs_to_jiffies(FLOAT_DELAY_TIME));
	} else {
		hq_err("float retry detect failed\n");
	}
}

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
static bool get_usb_ready(struct charger_manager *manager)
{
	bool ready = true;

	if (IS_ERR_OR_NULL(manager->usb_node))
		manager->usb_node = of_parse_phandle(manager->dev->of_node, "usb", 0);
	if (!IS_ERR_OR_NULL(manager->usb_node)) {
		ready = !of_property_read_bool(manager->usb_node, "cdp-block");
		if (ready || manager->get_usb_rdy_cnt % 10 == 0)
			hq_info("usb ready = %d\n", ready);
	} else
		hq_err("usb node missing or invalid\n");

	if (ready == false && (manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT || manager->pd_active)) {
		if (manager->pd_active)
			manager->get_usb_rdy_cnt = 0;
		hq_info("cdp-block timeout or pd adapter\n");
		return true;
	}

	return ready;
}

static void wait_usb_ready_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, wait_usb_ready_work.work);

	if (get_usb_ready(manager) || manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT)
		charger_force_dpdm(manager->charger);
	else {
		manager->get_usb_rdy_cnt++;
		schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(WAIT_USB_RDY_TIME));
	}
}
#endif

#if IS_ENABLED(CONFIG_BATT_VERIFY)
static void update_fg_raw_soh_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, update_fg_raw_soh_work.work);

	int delta_raw_soh = 0;
	int battery_raw_soh = 100;
	int set_raw_soh = 0;
	int tmp_raw_soh = 0;
	int new_fg_raw_soh = 100;
	int ret = 0;

	mutex_lock(&manager->update_auth_param_lock);

	/* step0: get new_fg_raw_soh from MTK FG */
	new_fg_raw_soh = manager->new_fg_raw_soh;
	/* step1: MTK fg deamon is ready once reach here, do some deffer initializations */
	if (manager->fg_raw_soh < 0) {
		/* initialize fg_raw_soh */
		manager->fg_raw_soh = new_fg_raw_soh;
		if (manager->board_version == EEA_VERSION) {
			ret = auth_device_get_raw_soh(manager->auth_dev, &battery_raw_soh);
			if (ret) {
				hq_err("[EEA] read auth raw_soh error\n");
				manager->batt_raw_soh = new_fg_raw_soh;
				goto rerun_work;
			} else {
				manager->batt_raw_soh = battery_raw_soh;
				hq_info("[EEA] read auth raw_soh success\n");
			}
		} else {
			manager->batt_raw_soh = new_fg_raw_soh;
		}
		hq_info("success to init battery raw_soh, batt_raw_soh = %d, fg_raw_soh = %d\n",
				manager->batt_raw_soh, manager->fg_raw_soh);
	}
	mdelay(20);
	/* step2:
	 * delta_raw_soh: get delta_raw_soh from MTK FG
	 * new_fg_raw_soh: get new_fg_raw_soh from MTK FG
	 * manager->fg_raw_soh: get old_fg_raw_soh from MTK FG
	 */
	delta_raw_soh = new_fg_raw_soh - manager->fg_raw_soh;
	hq_info("new_fg_raw_soh = %d, old_fg_raw_soh = %d, batt_raw_soh = %d, delta_raw_soh = %d\n",
			new_fg_raw_soh, manager->fg_raw_soh, manager->batt_raw_soh, delta_raw_soh);

	/* step3:
	 * Battery raw_soh update policy
	 * 1. EEA: save latest fg raw_soh to battery secret ic and update cm battery raw_soh
	 * 2. Non-EEA: keep battery raw_soh follow lastest fg raw_soh
	 */
	if (manager->board_version == EEA_VERSION) {
		if (!manager->auth_dev) {
			hq_err("[EEA] failed to get battery auth device\n");
			goto rerun_work;
		}

		ret = auth_device_get_raw_soh(manager->auth_dev, &battery_raw_soh);
		if (ret != 0) {
			hq_err("[EEA] first read auth raw_soh error\n");
			goto rerun_work;
		}
		mdelay(20);

		set_raw_soh = battery_raw_soh + delta_raw_soh;

		ret = auth_device_set_raw_soh(manager->auth_dev, set_raw_soh);
		if (ret != 0) {
			hq_err("[EEA] write auth raw_soh error\n");
			goto rerun_work;
		}
		mdelay(20);

		ret = auth_device_get_raw_soh(manager->auth_dev, &tmp_raw_soh);
		if (ret != 0) {
			hq_err("[EEA] second read auth raw_soh error\n");
			goto rerun_work;
		}

		if (set_raw_soh != tmp_raw_soh) {
			hq_err("[EEA] update raw_soh fail, retry update raw soh\n");
			goto rerun_work;
		}

		manager->fg_raw_soh = new_fg_raw_soh;
		manager->batt_raw_soh = tmp_raw_soh;
		hq_info("[EEA] success to update raw_soh, batt_raw_soh = %d, fg_raw_soh = %d\n",
			manager->batt_raw_soh, manager->fg_raw_soh);
	} else {
		manager->fg_raw_soh = new_fg_raw_soh;
		manager->batt_raw_soh = manager->fg_raw_soh;
		hq_info("success to update raw_soh, batt_raw_soh = %d, fg_raw_soh = %d\n",
			manager->batt_raw_soh, manager->fg_raw_soh);
	}
	fuel_gauge_set_soh(manager->fuel_gauge, manager->batt_raw_soh);

	mutex_unlock(&manager->update_auth_param_lock);
	return;

rerun_work:
	mutex_unlock(&manager->update_auth_param_lock);
	schedule_delayed_work(&manager->update_fg_raw_soh_work, msecs_to_jiffies(30000));
}

static void update_fg_cycle_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, update_fg_cycle_work.work);
	int delta_cycle = 0;
	u32 battery_cycle = 0;
	u32 set_cycle = 0;
	u32 tmp_cycle = 0;
	int new_fg_cycle = 0;
	int ret = 0;

	mutex_lock(&manager->update_auth_param_lock);

	/* step0: get new_fg_cycle from MTK FG */
	new_fg_cycle = manager->new_fg_cycle;
	/* step1: MTK fg deamon is ready once reach here, do some deffer initializations */
	if (manager->fg_cycle < 0) {
		/* initialize fg_cycle */
		manager->fg_cycle = new_fg_cycle;
		if (manager->board_version == EEA_VERSION) {
			ret = auth_device_get_cycle_count(manager->auth_dev, &battery_cycle);
			if (ret) {
				hq_err("[EEA] read auth cycle count error\n");
				manager->batt_cycle = new_fg_cycle;
				goto rerun_work;
			} else {
				manager->batt_cycle = battery_cycle;
				hq_info("[EEA] read auth cycle count success\n");
			}
		} else {
			manager->batt_cycle = new_fg_cycle;
		}
		hq_info("success to init battery cycle, batt_cycle = %d, fg_cycle = %d\n",
				manager->batt_cycle, manager->fg_cycle);
	}
	mdelay(20);
	/* step2:
	 * delta_cycle: get delta_cycle from MTK FG
	 * new_fg_cycle: get new_fg_cycle from MTK FG
	 * manager->fg_cycle: get old_fg_cycle from MTK FG
	 */
	delta_cycle = new_fg_cycle - manager->fg_cycle;
	hq_info("new_fg_cycle = %d, old_fg_cycle = %d, batt_cycle = %d, delta_cycle = %d\n",
			new_fg_cycle, manager->fg_cycle, manager->batt_cycle, delta_cycle);

	/* step3:
	 * Battery cycle update policy
	 * 1. EEA: save latest fg cycle to battery secret ic and update cm battery cycle
	 * 2. Non-EEA: keep battery cycle follow lastest fg cycle
	 */
	if (manager->board_version == EEA_VERSION) {
		if (!manager->auth_dev) {
			hq_err("[EEA] failed to get battery auth device\n");
			goto rerun_work;
		}

		ret = auth_device_get_cycle_count(manager->auth_dev, &battery_cycle);
		if (ret != 0) {
			hq_err("[EEA] first read auth cycle count error\n");
			goto rerun_work;
		}
		mdelay(20);

		set_cycle = battery_cycle + delta_cycle;
		ret = auth_device_set_cycle_count(manager->auth_dev, set_cycle, battery_cycle);
		if (ret != 0) {
			hq_err("[EEA] write auth cycle count error\n");
			goto rerun_work;
		}
		mdelay(20);

		ret = auth_device_get_cycle_count(manager->auth_dev, &tmp_cycle);
		if (ret != 0) {
			hq_err("[EEA] second read auth cycle count error\n");
			goto rerun_work;
		}

		if (set_cycle != tmp_cycle) {
			hq_err("[EEA] update cycle fail, retry update cycle\n");
			goto rerun_work;
		}

		manager->fg_cycle = new_fg_cycle;
		manager->batt_cycle = tmp_cycle;
		hq_info("[EEA] success to update cycle count, batt_cycle = %d, fg_cycle = %d\n",
			manager->batt_cycle, manager->fg_cycle);
	} else {
		manager->fg_cycle = new_fg_cycle;
		manager->batt_cycle = manager->fg_cycle;
		hq_info("success to update cycle count, batt_cycle = %d, fg_cycle = %d\n",
			manager->batt_cycle, manager->fg_cycle);
	}

	mutex_unlock(&manager->update_auth_param_lock);
	return;

rerun_work:
	mutex_unlock(&manager->update_auth_param_lock);
	schedule_delayed_work(&manager->update_fg_cycle_work, msecs_to_jiffies(30000));
}
#endif

#if IS_ENABLED(CONFIG_RUST_DETECTION)
static void rust_detection_work_func(struct work_struct *work)
{
	struct timespec64 time;
	union power_supply_propval pval;
	ktime_t tmp_time = 0;
	struct adapter_dev *adapter = adapter_find_dev_by_name("pd_adapter1");
	struct charger_manager *manager = container_of(work, struct charger_manager, rust_detection_work.work);
	int res = 0;
	static int rust_det_interval = 5000;
	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if (time.tv_sec < 50) {
		hq_err("boot don't enter\n");
		goto out;
	}

	if (IS_ERR_OR_NULL(adapter)) {
		hq_err("cann't get pd_adapter1\n");
		goto out;
	}

	if (manager->typec_switch_chg == NULL) {
		manager->typec_switch_chg = charger_find_dev_by_name("typec_switch_chg");
		if (manager->typec_switch_chg)
			hq_err("found typec_switch_chg\n");
		else {
			hq_err("not found typec_switch_chg\n");
			goto out;
		}
	}

	charger_rust_detection_enable(manager->typec_switch_chg, true);
	msleep(50);
	res = charger_rust_detection_read_res(manager->typec_switch_chg);
	hq_err("res=%d\n", res);
	if (res == true) {
		hq_err("typec is detected lpd\n");
		manager->lpd_flag = true;
		hq_chargermanager_notifier_call_chain(CHG_FW_EVT_LPD, &manager->lpd_flag);
	} else if (res < 0) {
		hq_err("typec is detected error\n");
		manager->lpd_flag = false;
	} else {
		hq_err("typec isn't detected lpd\n");
		manager->lpd_flag = false;
		hq_chargermanager_notifier_call_chain(CHG_FW_EVT_LPD, &manager->lpd_flag);
	}

	xm_charge_uevent_report(CHG_UEVENT_LPD_DETECTION, manager->lpd_flag);

	if (manager->lpd_charging_limit) {
		vote(manager->total_fcc_votable, LPD_DETECT_VOTER, true, 1500);
		vote(manager->main_icl_votable, LPD_DETECT_VOTER, true, 1500);
		hq_info("%s lpd_limit:%d\n", __func__, manager->lpd_charging_limit);
		if (manager->vbus_type == VBUS_TYPE_HVDCP) {
			power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			if (pval.intval > 7200) {
				charger_qc2_vbus_mode(manager->charger, 5000);
				hq_info("%s lpd_limit:%d, set qc vbus 5V\n", __func__, manager->lpd_charging_limit);
			}
		} else if (manager->pd_active > 0) {
			power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			if (pval.intval > 7200) {
				adapter_set_cap(adapter, 0, 5000, 2000);
				hq_info("%s lpd_limit:%d, set pd vbus 5V\n", __func__, manager->lpd_charging_limit);
			}
		}
	} else {
		vote(manager->total_fcc_votable, LPD_DETECT_VOTER, false, 0);
		vote(manager->main_icl_votable, LPD_DETECT_VOTER, false, 0);
	}
out:
	schedule_delayed_work(&manager->rust_detection_work, msecs_to_jiffies(rust_det_interval));
}
#endif

#if IS_ENABLED(CONFIG_HQ_HIGH_VOLTAGE_DCP)
static int charger_manager_dcp_power_detect(struct charger_manager *manager)
{
	int vindpm_state = 0;
	int iindpm = 100;
	int ichg_max = 3000;
	int iindpm_step = 200;
	int vbus_volt = 0;
	int vbat_volt = 0;

	charger_get_adc(manager->charger, CHG_ADC_VBAT, &vbat_volt);
	charger_manager_check_vindpm(manager, vbat_volt);

	// step1: check 5V1.5A dcp power gear
	iindpm = 1550;
	vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, iindpm);
	vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, ichg_max);
	msleep(1500);
	charger_get_vindpm_state(manager->charger, &vindpm_state);
	charger_get_adc(manager->charger, CHG_ADC_VBUS, &vbus_volt);
	hq_info("5V1.5A gear detect: iindpm = %dma, vindpm_state = %d, vbus_volt = %dmv\n", iindpm, vindpm_state, vbus_volt);
	if (vindpm_state || vbus_volt < 4450) {
		manager->dcp_current = 1000;
		goto detect_done;
	}

	// step2: check 5V2A dcp power gear
	iindpm = 2000;
	vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, iindpm);
	msleep(500);
	charger_get_vindpm_state(manager->charger, &vindpm_state);
	charger_get_adc(manager->charger, CHG_ADC_VBUS, &vbus_volt);
	hq_info("5V2A gear detect: iindpm = %dma, vindpm_state = %d, vbus_volt = %dmv\n", iindpm, vindpm_state, vbus_volt);
	if (vindpm_state || vbus_volt < 4450) {
		manager->dcp_current = 1500;
		goto detect_done;
	}

	// step3: check 5V3A dcp power gear
	iindpm = 2000;
	do {
		iindpm += iindpm_step;

		vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, iindpm);

		msleep(500);

		charger_get_vindpm_state(manager->charger, &vindpm_state);
		charger_get_adc(manager->charger, CHG_ADC_VBUS, &vbus_volt);
		hq_info("5V3A gear detect: iindpm = %dma, vindpm_state = %d, vbus_volt = %dmv\n", iindpm, vindpm_state, vbus_volt);

		if (vindpm_state || vbus_volt < 4450) {
			manager->dcp_current = 2000;
			goto detect_done;
		} else {
			manager->dcp_current = iindpm;
		}
	} while (iindpm < 3000);

detect_done:
	hq_info("dcp power detect %dmA gear\n", manager->dcp_current);
	vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, manager->dcp_current);
	vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, manager->dcp_current);

	return 0;
}
#endif

static void charger_manager_charger_type_detect(struct charger_manager *manager)
{
	int total_fcc = 0, iterm = 0;
	static int i = 0;

	charger_get_online(manager->charger, &manager->usb_online);
	charger_get_vbus_type(manager->charger, &manager->vbus_type);
#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
	if (manager->reverse_charge_policy) {
		if (manager->reverse_charge_policy->in_otg_mode)
			manager->usb_online = 0;
	}
#endif
	if (manager->usb_online != manager->adapter_plug_in) {
		manager->adapter_plug_in = manager->usb_online;
		if (manager->adapter_plug_in) {
			pm_stay_awake(manager->dev);
			hq_info("adapter plug in\n");
			vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
			xiaomi_touch_usb_data.usb_touch_enable = XIAOMI_USB_ENABLE;
			xiaomi_usb_touch_notifier_call_chain(XIAOMI_TOUCH_USB_SWITCH, &xiaomi_touch_usb_data);
#endif
			manager->chg_timeout = false;
			manager->qc_detected = false;
			manager->dcp_power_detected = false;
			manager->ffc_to_nomal = false;
			charger_adc_enable(manager->charger, true);

#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
			chargerpump_set_enable_adc(manager->master_cp_chg, true);

#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
			chargerpump_set_enable_adc(manager->slave_cp_chg, true);
			#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

			#endif /* CONFIG_CHARGE_ARCH_CHARGEPUMP */

			charger_set_term(manager->charger, true);
			rerun_vote(manager);
			vote(manager->total_fcc_votable, JEITA_VOTER, true, 500);

			/* NOTE: do adapter plug in routines below */
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
			xm_batt_health_run(manager);
#endif

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
			xm_smart_chg_run(manager);
#endif

			charger_manager_misc_run(manager);

#if IS_ENABLED(CONFIG_HQ_TERM_RECHARGE_POLICY)
			hq_term_recharge_policy_run(manager);
#endif

#if IS_ENABLED(CONFIG_RUST_DETECTION)
			schedule_delayed_work(&manager->rust_detection_work, msecs_to_jiffies(0));
#endif

			hq_chargermanager_notifier_call_chain(CHG_FW_EVT_ADAPTER_PLUGIN, NULL);

		} else {
#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
			chargerpump_set_enable_adc(manager->master_cp_chg, false);

#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
			chargerpump_set_enable_adc(manager->slave_cp_chg, false);
#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

#endif /* CONFIG_CHARGE_ARCH_CHARGEPUMP */

			charger_adc_enable(manager->charger, false);

#if IS_ENABLED(CONFIG_BC12_RETRY_FOR_MI_PD)
			bc12_retry_flag = 1;
#endif
			hq_info("adapter plug out\n");

			vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
			manager->dcp_current = DEFAULT_DCP_CURRENT;
			manager->pd_done = false;
			manager->chg_timeout = false;
			manager->ffc_to_nomal = false;

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
			xiaomi_touch_usb_data.usb_touch_enable = XIAOMI_USB_DISABLE;
			xiaomi_usb_touch_notifier_call_chain(XIAOMI_TOUCH_USB_SWITCH, &xiaomi_touch_usb_data);
#endif

#if IS_ENABLED(CONFIG_RUST_DETECTION)
			cancel_delayed_work_sync(&manager->rust_detection_work);
#endif
			cancel_delayed_work_sync(&manager->float_retry_detect_work);
			atomic_set(&manager->float_retry_pending, 0);
			float_retry_cnt = 0;

#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
#endif
			chargerpump_policy_stop(g_policy);
			reset_vote(manager);
			vote(manager->fv_votable, JEITA_VOTER, true, 4448);//for >45 temp, plut out,fv reset to VBAT:4448

			/* NOTE: do adapter plug out routines below */
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
			xm_batt_health_stop(manager);
#endif

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
			xm_smart_chg_stop(manager);
#endif

			charger_manager_misc_stop(manager);

#if IS_ENABLED(CONFIG_HQ_TERM_RECHARGE_POLICY)
			hq_term_recharge_policy_stop(manager);
#endif
			manager->pd_adapter->verify_done = false;
			hq_chargermanager_notifier_call_chain(CHG_FW_EVT_ADAPTER_PLUGOUT, NULL);

			pm_relax(manager->dev);
		}
	}

	hq_info("usb_online= %d, bc_type = %s, input_suspend = %d, pd_active = %d pd_done = %d\n",
		manager->usb_online, vbus_type_str[manager->vbus_type], manager->input_suspend, manager->pd_active, manager->pd_done);

#if IS_ENABLED(CONFIG_BC12_RETRY_FOR_MI_PD)
	if (manager->pd_active == 2 && manager->vbus_type != VBUS_TYPE_NONE && manager->vbus_type != VBUS_TYPE_DCP && bc12_retry_flag) {
		charger_force_dpdm(manager->charger);
		bc12_retry_flag = 0;
		hq_info("Retry for bc_type!\n");
	}
#endif

	total_fcc = get_effective_result(manager->total_fcc_votable);
	iterm = get_effective_result(manager->iterm_votable);
	if (manager->tbat > 200 && manager->tbat <= 450 &&
			manager->pd_verifed && (g_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || g_policy->cp_charge_done)) {

		/* when soc > 90 and total_fcc is close to iterm or total_fcc is less than iterm need to switch to nomal charge*/
		if (manager->soc > 90 && ((total_fcc - iterm) < 320 || total_fcc < iterm)) {
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
			manager->charge_mode = NORMAL_CHARGE_MODE;
			manager->ffc_to_nomal = true;
		} else if (!manager->ffc_to_nomal) {
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, true);
			manager->charge_mode = FFC_CHARGE_MODE;
		}

	} else {
		fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
		manager->charge_mode = NORMAL_CHARGE_MODE;
	}

	if (manager->adapter_plug_in) {
#if IS_ENABLED(CONFIG_HQ_THERMAL_POLICY)
		hq_thermal_policy_run(manager);
#endif

#if IS_ENABLED(CONFIG_HQ_JEITA_POLICY)
		hq_jeita_policy_run(manager);
#endif
	}

	if (!manager->adapter_plug_in)
		return;

	if (!manager->is_pr_swap) {
		switch (manager->vbus_type) {
		case VBUS_TYPE_NONE:
			charger_force_dpdm(manager->charger);
			break;
		case VBUS_TYPE_NON_STAND:
		case VBUS_TYPE_FLOAT:
			if (!atomic_read(&manager->float_retry_pending) && manager->pd_active != 1) {
				hq_info("float type detect, rerun bc1.2\n");
				atomic_set(&manager->float_retry_pending, 1);
				schedule_delayed_work(&manager->float_retry_detect_work, msecs_to_jiffies(0));
			}
			rerun_election(manager->main_icl_votable);
			break;
		case VBUS_TYPE_SDP:
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
			if (!get_usb_ready(manager)) {
				if (manager->get_usb_rdy_cnt == 0)
					schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(0));
			}
#endif
			break;
		default:
			break;
		}
	} else
		manager->vbus_type = VBUS_TYPE_FLOAT;

	if (manager->vbus_type == VBUS_TYPE_SDP || manager->vbus_type == VBUS_TYPE_CDP)
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
	else
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (charger_monitor_fg_i2c_status(manager)) {
		hq_info("fg i2c error\n");
}
#endif

	// if (manager->pd_contract_update) {
	// 	manager->pd_contract_update = false;
	// 	if (g_policy->state != POLICY_RUNNING)
	// 		chargerpump_policy_stop(g_policy);
	// }

	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START))
		chargerpump_policy_start(g_policy);

	/* for high-power consumption scenarios , when soc < 90 and g_policy->state is not activated need to switch to FFC charge*/
	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE && manager->soc < 90 && g_policy->state != POLICY_RUNNING && g_policy->state != POLICY_NO_SUPPORT) {
		if (manager->ibat > 0) {
			i++;
		}

		if (i > 3) {
			i = 0;
			chargerpump_policy_start(g_policy);
			hq_info("low soc cp policy start\n");
		}
	}

	if (g_policy->state != POLICY_RUNNING) {
		if (manager->vbus_type == VBUS_TYPE_DCP &&
			!manager->qc_detected && !manager->pd_active && manager->pd_done) {
			manager->qc_detected = true;
			charger_qc_identify(manager->charger, manager->qc3_mode);
			/* TODO: 1.temporary delay for wait qc detect done
			 * 2.update manager's vbus type to avoid dcp_power_det
			 */
			msleep(3000);
			charger_get_vbus_type(manager->charger, &manager->vbus_type);
		}
	}

#if IS_ENABLED(CONFIG_HQ_HIGH_VOLTAGE_DCP)
	if (g_policy->state != POLICY_RUNNING) {
		if (manager->vbus_type == VBUS_TYPE_DCP &&
			!manager->dcp_power_detected && !manager->pd_active && manager->pd_done) {
			manager->dcp_power_detected = true;
			charger_manager_dcp_power_detect(manager);
		}
	}
#endif

	/*
	 * WORKAROUNG: fix samsung EP-TA200 adapter HVDCP issue,
	 * We shouldn't set icl/ichg before vbus rise to 9v,
	 * as it would trigered vindpm.
	 * TODO: add notifier chain between main_chg.ko and subpmic_xxx.ko to notify hvdcp done.
	 */
	msleep(100);

	charger_manager_check_vindpm(manager, manager->chg_adc[CHG_ADC_VBAT]);
	charger_manager_check_iindpm(manager, manager->vbus_type);

	charger_manager_check_inputsuspend(manager, manager->soc);
}

static int charger_manager_thread_fn(void *data)
{
	struct charger_manager *manager = data;
	int ret = 0;

	while (true) {
		ret = wait_event_interruptible(manager->wait_queue,
							manager->run_thread);
		if (kthread_should_stop() || ret) {
			hq_err("exits(%d)\n", ret);
			break;
		}

		manager->run_thread = false;

		charger_manager_monitor(manager);

		charger_manager_charger_type_detect(manager);

		if (!manager->adapter_plug_in)
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME_OUT);
		else
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME);
	}
	return 0;
}

static int charger_manager_check_dependencies(struct charger_manager *manager)
{

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is err or null\n");
		return -EFAULT;
	}

	manager->charger = charger_find_dev_by_name("primary_chg");
	if (!manager->charger) {
		hq_err("failed to find primary_chg device\n");
		return -EPROBE_DEFER;
	}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	manager->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (!manager->master_cp_chg) {
		hq_err("failed to find master_cp_chg device\n");
		return -EPROBE_DEFER;
	}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	manager->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	if (!manager->slave_cp_chg) {
		hq_err("failed to find slave_cp_chg device\n");
		return -EPROBE_DEFER;
	}
#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

#endif /* CONFIG_CHARGE_ARCH_CHARGEPUMP */

	manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!manager->fuel_gauge) {
		hq_err("failed find fuel_gauge device\n");
		return -EPROBE_DEFER;
	}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	/* TODO: why the psy must named sc-cp-master? change to cp-master is ok */
	manager->cp_master_psy = power_supply_get_by_name("sc-cp-master");
	if (!manager->cp_master_psy) {
		hq_err("failed to get cp_master_psy\n");
		return -EPROBE_DEFER;
	}

#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	manager->cp_slave_psy = power_supply_get_by_name("sc-cp-slave");
	if (!manager->cp_slave_psy) {
		hq_err("failed to get cp_slave_psy\n");
		return -EPROBE_DEFER;
	}
#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

#endif /* CONFIG_CHARGE_ARCH_CHARGEPUMP */

	manager->fg_psy = power_supply_get_by_name("bms");
	if (!manager->fg_psy) {
		hq_err("failed to get bms psy\n");
		return -EPROBE_DEFER;
	}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!manager->tcpc) {
		hq_err("failed to get tcpc device\n");
		return -EPROBE_DEFER;
	}
#endif

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	manager->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!manager->pd_adapter) {
		hq_err("failed to get pd_adapter device\n");
		return -EPROBE_DEFER;
	}
#endif

	/*
	 * NOTE: althrough failed to get auth device, keep cm going on for adb functions,
	 * and be care of the null pointer KE issue. we need simulate encryption failure
	 * test case.
	 */
	if (manager->board_version == EEA_VERSION) {
#if IS_ENABLED(CONFIG_BATT_VERIFY)
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (!manager->auth_dev) {
			hq_err("failed to get battery auth device, but will going on\n");
			//return -EPROBE_DEFER;
		}
#endif
	}

	hq_info("charger manager dependencies check pass\n");

	return 0;
}

/* bring up todo
static void charger_manager_pcba_init(struct charger_manager *manager)
{
	struct PCBA_MSG *pcba_msg = get_pcba_msg();

	manager->board_version = OTHER_VERSION;
	if (!IS_ERR_OR_NULL(pcba_msg->sku)) {
		if ((strstr(pcba_msg->sku, "eea")))
			manager->board_version = EEA_VERSION;
		hq_info("board version = %d\n", manager->board_version);
	} else
		hq_err("get board version fail\n");
}
*/

static int charger_manager_power_supply_init(struct charger_manager *manager)
{
	int ret = 0;

	ret = charger_manager_usb_psy_register(manager);
	if (ret < 0) {
		hq_err("usb power supply regitser failed, ret = %d\n", ret);
		return ret;
	}

	ret = charger_manager_batt_psy_register(manager);
	if (ret < 0) {
		hq_err("battery power supply regitser failed, ret = %d\n", ret);
		return ret;
	}

	ret = charger_manager_chg_psy_register(manager);
	if (ret < 0) {
		hq_err("charger power supply regitser failed, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int charger_manager_sysfs_init(struct charger_manager *manager)
{
	int ret = 0;

	ret = hq_batt_sysfs_create_group(manager);
	if (ret < 0) {
		hq_err("battery sysfs group create failed, ret = %d\n", ret);
		return ret;
	}

	ret = hq_usb_sysfs_create_group(manager);
	if (ret < 0) {
		hq_err("usb sysfs group create failed, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
static int charger_manager_fg_param_init(struct charger_manager *manager)
{
	int ret = 0;
	int i = 0;

	/* initialize battery cycle get actual battery cycle count as soon as possible */
	if (manager->board_version == EEA_VERSION) {
		for (i = 0; i < 3; i++) {
			ret = auth_device_get_cycle_count(manager->auth_dev, &manager->batt_cycle);
			if (ret != 0) {
				manager->batt_cycle = 0;
				hq_err("[EEA] failed to init battery cycle count\n");
			} else {
				hq_info("[EEA] success to init battery cycle count, batt_cycle = %d\n",
					manager->batt_cycle);
				break;
			}
		}
	} else {
		/* default zero, deffer initialize with fg cycle after fg deamon ready */
		manager->batt_cycle = 0;
	}

	mdelay(20);
	/* initialize battery soh get actual battery soh as soon as possible */
	if (manager->board_version == EEA_VERSION) {
		for (i = 0; i < 3; i++) {
			ret = auth_device_get_raw_soh(manager->auth_dev, &manager->batt_raw_soh);
			if (ret != 0) {
				manager->batt_raw_soh = 100;
				hq_err("[EEA] failed to init battery raw soh\n");
			} else {
				hq_info("[EEA] success to init battery raw soh, batt_raw_soh = %d\n",
					manager->batt_raw_soh);
				break;
			}
		}
	} else {
		/* default zero, deffer initialize with fg soh after fg deamon ready */
		manager->batt_raw_soh = 100;
	}

	/* just set invalid value here as fg deamon not ready */
	manager->fg_cycle = -1;
	manager->fg_raw_soh = -1;

	/* update raw_soh cycle work init */
	mutex_init(&manager->update_auth_param_lock);

	INIT_DELAYED_WORK(&manager->update_fg_raw_soh_work, update_fg_raw_soh_work);
	INIT_DELAYED_WORK(&manager->update_fg_cycle_work, update_fg_cycle_work);

	return ret;
}
#endif

static int charger_manager_register_notifiers(struct charger_manager *manager)
{
	int ret = 0;

	ret = charger_manager_register_charger_changed_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register charger changed notifier, ret = %d\n", ret);
		// return ret;
	}

	ret = charger_manager_register_psy_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register psy notifier ret = %d\n", ret);
		// return ret;
	}

	ret = charger_manager_register_thermal_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register thermal notifier, ret = %d\n", ret);
		// return ret;
	}

	ret = charger_manager_register_disp_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register disp notifier, ret = %d\n", ret);
		// return ret;
	}

	ret = charger_manager_register_charger_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register charger notifier, ret = %d\n", ret);
		// return ret;
	}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	ret = charger_manager_register_fg_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register fg notifier, ret = %d\n", ret);
		// return ret;
	}
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = charger_manager_register_tcpc_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register tcpc notifier, ret = %d\n", ret);
		// return ret;
	}
#endif

	ret = charger_manager_register_audio_status_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register audio status notifier, ret = %d\n", ret);
		// return ret;
	}

	return ret;
}

static int charger_manager_unregister_notifiers(struct charger_manager *manager)
{
	int ret = 0;

	ret = charger_manager_unregister_charger_changed_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister charger changed notifier, ret = %d\n", ret);
		// return ret;
	}

	charger_manager_unregister_psy_notifier(manager);

	ret = charger_manager_unregister_thermal_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister thermal notifier, ret = %d\n", ret);
		// return ret;
	}

	ret = charger_manager_unregister_disp_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister disp notifier, ret = %d\n", ret);
		// return ret;
	}

	ret = charger_manager_unregister_charger_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister charger notifier, ret = %d\n", ret);
		// return ret;
	}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = charger_manager_unregister_tcpc_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister tcpc notifier, ret = %d\n", ret);
		// return ret;
	}
#endif

	ret = charger_manager_unregister_audio_status_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister audio status notifier, ret = %d\n", ret);
		// return ret;
	}

	return ret;
}

#ifdef CONFIG_OF
static int charger_manager_parse_dts(struct charger_manager *manager)
{
	struct device_node *node = manager->dev->of_node;
	int ret = 0;

	manager->en_floatgnd = of_property_read_bool(node, "hq_chg_manager,en_floatgnd");

#ifndef KERNEL_FACTORY_HQ_CHG
	manager->single_cell_det = of_property_read_bool(node, "hq_chg_manager,single_cell_det");
#endif //KERNEL_FACTORY_HQ_CHG

	ret = of_property_read_u32(node, "hq_chg_manager,qc3_mode", &manager->qc3_mode);
	if (ret < 0) {
		hq_err("default not support qc3\n");
		manager->qc3_mode = 0; /* default not support qc3 */
	}

	ret = of_property_read_u32(node, "hq_chg_manager,none_type_current", &manager->none_type_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_NONE_TYPE_CURRENT:%d\n", DEFAULT_NONE_TYPE_CURRENT);
		manager->sdp_current = DEFAULT_NONE_TYPE_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,sdp_current", &manager->sdp_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_SDP_CURRENT:%d\n", DEFAULT_SDP_CURRENT);
		manager->sdp_current = DEFAULT_SDP_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,float_current", &manager->float_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_FLOAT_CURRENT:%d\n", DEFAULT_FLOAT_CURRENT);
		manager->float_current = DEFAULT_FLOAT_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,dcp_current", &manager->dcp_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_DCP_CURRENT:%d\n", DEFAULT_DCP_CURRENT);
		manager->dcp_current = DEFAULT_DCP_CURRENT;
	}
	ret = of_property_read_u32(node, "hq_chg_manager,cdp_current", &manager->cdp_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_CDP_CURRENT:%d\n", DEFAULT_CDP_CURRENT);
		manager->cdp_current = DEFAULT_CDP_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,hvdcp_current", &manager->hvdcp_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_HVDCP_CURRENT:%d\n", DEFAULT_HVDCP_CURRENT);
		manager->hvdcp_current = DEFAULT_HVDCP_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,hvdcp_input_current", &manager->hvdcp_input_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_HVDCP_INPUT_CURRENT:%d\n", DEFAULT_HVDCP_INPUT_CURRENT);
		manager->hvdcp_input_current = DEFAULT_HVDCP_INPUT_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,hvdcp3_current", &manager->hvdcp3_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_HVDCP3_CURRENT:%d\n", DEFAULT_HVDCP3_CURRENT);
		manager->hvdcp3_current = DEFAULT_HVDCP3_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,hvdcp3_input_current", &manager->hvdcp3_input_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_HVDCP3_INPUT_CURRENT:%d\n", DEFAULT_HVDCP3_INPUT_CURRENT);
		manager->hvdcp3_input_current = DEFAULT_HVDCP3_INPUT_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,pd2_current", &manager->pd2_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_PD2_CURRENT:%d\n", DEFAULT_PD2_CURRENT);
		manager->pd2_current = DEFAULT_PD2_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,pd2_input_current", &manager->pd2_input_current);
	if (ret < 0) {
		hq_err("use default DEFAULT_PD2_INPUT_CURRENT:%d\n", DEFAULT_PD2_INPUT_CURRENT);
		manager->pd2_input_current = DEFAULT_PD2_INPUT_CURRENT;
	}

	ret = of_property_read_u32(node, "hq_chg_manager,charge_power_max", &manager->charge_power_max);
	if (ret < 0) {
		hq_err("failed to parse charge_power_max from dts\n");
	}

	ret = of_property_read_string(node, "hq_chg_manager,model_name", &manager->model_name);
	if (ret < 0) {
		hq_err("failed to parse model_name from dts\n");
	}

	return ret;
}
#else
static int charger_manager_parse_dts(struct charger_manager *manager)
{
	return 0;
}
#endif

static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_manager *manager;
	int ret = 0;

	hq_info("version:%s vendor:%s build:%s\n",
		CHARGER_MANAGER_VERSION, CHARGER_MANAGER_VENDOR, CHARGER_MANAGER_BUILD_TYPE);

	manager = devm_kzalloc(&pdev->dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->dev = &pdev->dev;
	platform_set_drvdata(pdev, manager);

	/* NOTE: get pcba msg as early as possible */
	/* bring up todo
	charger_manager_pcba_init(manager);
	*/

	ret = charger_manager_check_dependencies(manager);
	if (ret < 0) {
		hq_err("charger manager dependencies check failed\n");
		return ret;
	}

	ret = charger_manager_parse_dts(manager);
	if (ret < 0) {
		hq_err("charger manager parse dts failed\n");
		return -EINVAL;
	}

	manager->fake_batt_cycle = 0xFFFF;

	ret = charger_manager_create_votable(manager);
	if (ret < 0) {
		hq_err("charger manager create votable failed\n");
		return -EINVAL;
	}

	ret = charger_manager_power_supply_init(manager);
	if (ret < 0) {
		hq_err("charger manager power supply init failed\n");
		return -EINVAL;
	}

	ret = charger_manager_sysfs_init(manager);
	if (ret < 0) {
		hq_err("charger manager sysfs init failed\n");
		return -EINVAL;
	}

	/*
	 * WARNING: wait queue must initialized before charger notifier
	 * as notifer may try to wakeup wait queue when it not initalized
	 */
	init_waitqueue_head(&manager->wait_queue);

	ret = charger_manager_register_notifiers(manager);
	if (ret < 0) {
		hq_err("charger manager register notifiers failed\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	charger_manager_fg_param_init(manager);
#endif

	ret = charger_manager_misc_init(manager);
	if (ret < 0) {
		hq_err("charger manager misc failed\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_HQ_THERMAL_POLICY)
	ret = hq_thermal_policy_init(manager);
	if (ret < 0) {
		hq_err("thermal policy init failed, ret = %d\n", ret);
		return -EINVAL;
	}
#endif

#if IS_ENABLED(CONFIG_HQ_JEITA_POLICY)
	ret = hq_jeita_policy_init(manager);
	if (ret < 0) {
		hq_err("jeita policy init failed, ret = %d\n", ret);
		return -EINVAL;
	}
#endif

#if IS_ENABLED(CONFIG_HQ_TERM_RECHARGE_POLICY)
	ret = hq_term_recharge_policy_init(manager);
	if (ret < 0) {
		hq_err("terminate and recharge policy init failed, ret = %d\n", ret);
		return -EINVAL;
	}
#endif

#if IS_ENABLED(CONFIG_HQ_SHUTDOWN_POLICY)
	ret = hq_shutdown_policy_init(manager);
	if (ret < 0) {
		hq_err("shutdown policy init failed, ret = %d\n", ret);
		return -EINVAL;
	}
#endif

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
	ret = reverse_charge_policy_init(manager);
	if (ret < 0) {
		hq_err("reverse charge policy init failed, ret = %d\n", ret);
		return -EINVAL;
	}
#endif

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	ret = xm_batt_health_init(manager);
	if (ret < 0) {
		hq_err("xm battery health feature init failed, ret = %d\n", ret);
		return -EINVAL;
	}
#endif

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	ret = xm_smart_chg_init(manager);
	if (ret < 0) {
		hq_err("xm smart charge feature init failed, ret = %d\n", ret);
		return -EINVAL;
	}
#endif

	INIT_DELAYED_WORK(&manager->float_retry_detect_work, float_retry_detect_work);

#if IS_ENABLED(CONFIG_RUST_DETECTION)
	INIT_DELAYED_WORK(&manager->rust_detection_work, rust_detection_work_func);
#endif

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	INIT_DELAYED_WORK(&manager->wait_usb_ready_work, wait_usb_ready_work);
#endif //CONFIG_USB_MTK_HDRC

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->cid_status = false;
	alarm_init(&manager->otg_ui_close_timer, ALARM_BOOTTIME, otg_ui_close_timer_handler);
	alarm_init(&manager->set_soft_cid_timer, ALARM_BOOTTIME, set_soft_cid_timer_handler);
	INIT_DELAYED_WORK(&manager->handle_cc_status_work, handle_cc_status_work_func);
	INIT_DELAYED_WORK(&manager->set_otg_ui_work, set_otg_ui_work_func);
#endif //CONFIG_TCPC_CLASS

	mutex_init(&manager->wakelock_mutex);
	manager->cm_wakelock =
		wakeup_source_register(manager->dev, "charger manager wakelock");

	/* add flags init value here*/
	manager->batt_id = fuel_gauge_get_batt_id(manager->fuel_gauge);
	/* TODO: just set FFC_CHARGE_MODE, next commit will fix charge_mode update issue */
	manager->charge_mode = FFC_CHARGE_MODE;
	manager->mtbf_mode = 0;
	atomic_set(&manager->float_retry_pending, 0);

	manager->cp_master_ok = 0;
	manager->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (!manager->master_cp_chg) {
		hq_err("get cp master fail\n");
	} else {
		chargerpump_get_chip_ok(manager->master_cp_chg, &manager->cp_master_ok);
		hq_info("cp master chip ok = %d\n", manager->cp_master_ok);
	}

	/* charger manager therad initialized */
	device_init_wakeup(manager->dev, true);
	manager->run_thread = true;
	manager->thread = kthread_run(charger_manager_thread_fn, manager,
								"charger_manager_thread");

#if IS_ENABLED(CONFIG_HQ_SHUTDOWN_POLICY)
	hq_shutdown_policy_run(manager);
#endif

	hq_info("success ...\n");

	return 0;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *manager = platform_get_drvdata(pdev);

	wakeup_source_unregister(manager->cm_wakelock);
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	cancel_delayed_work_sync(&manager->rust_detection_work);
#endif

#if IS_ENABLED(CONFIG_HQ_THERMAL_POLICY)
	hq_thermal_policy_deinit(manager);
#endif

#if IS_ENABLED(CONFIG_HQ_JEITA_POLICY)
	hq_jeita_policy_deinit(manager);
#endif

#if IS_ENABLED(CONFIG_HQ_TERM_RECHARGE_POLICY)
	hq_term_recharge_policy_deinit(manager);
#endif

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
	reverse_charge_policy_deinit(manager);
#endif

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	xm_batt_health_deinit(manager);
#endif

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	xm_smart_chg_deinit(manager);
#endif

	charger_manager_misc_deinit(manager);

	charger_manager_unregister_notifiers(manager);

	return 0;
}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);

	//Avoid raising VBUS to 12V after shutdown
	if (manager == NULL) {
		hq_err("manager get null!!\n");
		return;
	}

#if IS_ENABLED(CONFIG_RUST_DETECTION)
	cancel_delayed_work_sync(&manager->rust_detection_work);
#endif

	if (manager->vbus_type == VBUS_TYPE_HVDCP) {
		ret = charger_disable_dpdm_block(manager->charger);
		if (ret < 0)
			hq_err("set dpdm_block_disable fail\n");
		else
			hq_info("set dpdm_block_disable success\n");
	}

	if (manager->tcpc->ops->set_dis_vconn_ov) {
		manager->tcpc->ops->set_dis_vconn_ov(manager->tcpc, true);
		hq_info("set ship mode TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV\n");
	}
}

static const struct of_device_id charger_manager_match[] = {
	{.compatible = "huaqin,hq_chg_manager",},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct platform_driver charger_manager_driver = {
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.driver = {
		.name = "hq_chg_manager",
		.of_match_table = charger_manager_match,
	},
};

static int __init charger_manager_init(void)
{
	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_exit(void)
{
	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_exit);

MODULE_DESCRIPTION("Huaqin Charger Manager Core");
MODULE_LICENSE("GPL");
