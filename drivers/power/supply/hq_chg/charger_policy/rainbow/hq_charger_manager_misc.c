// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/syscore_ops.h>

#include "hq_printk.h"
#include "hq_charger_manager.h"
#include "hq_charger_manager_misc.h"
#include "hq_charger_manager_internal.h"

/******************************************************************************/
/*                           SOH2.0 FUNCTIONS                                 */
/******************************************************************************/
#if IS_ENABLED(CONFIG_BATT_VERIFY)
void batt_soh20_aging_test(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
			struct charger_manager, batt_soh20_aging_test.work);

	union power_supply_propval pval = {0,};
	int ret = 0;
	static int rsoc_count;
	int fake_cycle_count = 0;
	int fake_raw_soh = 100;

	hq_err("__enter__\n");

	if (manager->batt_psy == NULL) {
		manager->batt_psy = power_supply_get_by_name("battery");
		if (manager->batt_psy == NULL) {
			hq_err("manager->batt_psy is NULL\n");
			goto out;
		}
	}
	if (manager->fuel_gauge == NULL) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (manager->fuel_gauge == NULL) {
			hq_err("manager->fuel_gauge is NULL\n");
			goto out;
		}
	}

	ret = power_supply_get_property(manager->batt_psy,
		POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		hq_err("failed to get cycle_count prop\n");
		goto out;
	} else
		fake_cycle_count = pval.intval;

	fake_raw_soh -= (fake_cycle_count/100);
	fuel_gauge_set_fake_soh(manager->fuel_gauge, fake_raw_soh);

	ret = power_supply_get_property(manager->batt_psy,
		POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0) {
		hq_err("failed to get cycle_count prop\n");
		goto out;
	} else {
		if (pval.intval < 0)
			rsoc_count++;
	}

	if (rsoc_count > 13) {
		rsoc_count = 0;
		pval.intval = fake_cycle_count + 1;
		ret = power_supply_set_property(manager->batt_psy,
			POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret < 0) {
			hq_err("failed to get cycle_count prop\n");
			goto out;
		}
	}

out:
	schedule_delayed_work(&manager->batt_soh20_aging_test, msecs_to_jiffies(3000));
}
#endif

/******************************************************************************/
/*                           SHIPMODE FUNCTIONS                               */
/******************************************************************************/
static void shipmode_syscore_shutdown(void)
{
	int ret = 0;
	struct charger_dev *charger = charger_find_dev_by_name("primary_chg");
	#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	struct chargerpump_dev *master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	struct chargerpump_dev *slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	#endif
	#endif

	if (charger == NULL) {
		hq_err("charger is NULL\n");
		return;
	}

	#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	if (master_cp_chg == NULL) {
		hq_err("master_cp_chg is NULL\n");
		return;
	}
	#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	if (slave_cp_chg == NULL) {
		hq_err("slave_cp_chg is NULL\n");
		return;
	}
	#endif
	#endif

	hq_info("shipmode_flag = %d, shutdown!\n", charger->shipmode_flag);

	/* NOTE: do pre shipmode shutdown actions, eg. disable charger/chargerpump ADC */
	ret = charger_adc_enable(charger, false);
	if (ret < 0) {
		hq_err("disable charger adc failed\n");
	}

	#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	chargerpump_set_enable_adc(master_cp_chg, false);

	#if IS_ENABLED(CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP)
	chargerpump_set_enable_adc(slave_cp_chg, false);
	#endif /* CONFIG_CHARGE_ARCH_DUAL_CHARGEPUMP */

	#endif /* CONFIG_CHARGE_ARCH_CHARGEPUMP */

	if (charger->shipmode_flag) {
		charger->shipmode_flag = false;
		ret = charger_set_shipmode(charger, true);
		if (ret < 0) {
			hq_err("set ship mode fail\n");
		} else {
			hq_info("set ship mode success\n");
		}
	}
}

static struct syscore_ops shipmode_syscore_ops = {
	.shutdown = shipmode_syscore_shutdown,
};

static void charger_manager_misc_shipmode_init(struct charger_manager *manager)
{

	/*
	 * NOTE: Register syscore ops, just for, when restart in shipmode,
	 * it will reproduce any device shutdown fail
	 * So first, do device_shutdown, and then syscore ops->shutdown
	 */
	register_syscore_ops(&shipmode_syscore_ops);
}

/******************************************************************************/
/*                     STOP CHARGE PROTECT FUNCTIONS                          */
/******************************************************************************/
#ifdef STOP_CHARGE_PROTECT
static int do_stop_charge_protect_work(struct charger_manager *manager)
{
	bool is_charge_done;
	/* battery warm stop charge protection */
	charger_is_charge_done(manager->charger, &is_charge_done);

	if (manager->tbat >= WARM_STOP_CHARGE_TBAT && (manager->vbat >= WARM_STOP_CHARGE_VBAT
			|| (is_charge_done && !manager->warm_stop_charge))) {
		manager->warm_stop_charge = true;
		vote(manager->total_fcc_votable, CHG_PROTECT_VOTER, true, 0);
		hq_info("warm_stop_charge = %d, charge done : %d, stop charge\n", manager->warm_stop_charge, is_charge_done);
	}

	if (manager->warm_stop_charge) {
		if ((manager->tbat <= (WARM_STOP_CHARGE_TBAT - RECHG_TBAT_WARM_OFFSET)) ||
			(manager->vbat <= (WARM_STOP_CHARGE_VBAT - RECHG_VBAT_OFFSET))) {
			manager->warm_stop_charge = false;
			vote(manager->total_fcc_votable, CHG_PROTECT_VOTER, false, 0);
			charger_set_rechg_volt(manager->charger, 100); //rechg, term volt 300-->100
			hq_info("warm_stop_charge = %d, recover charge\n", manager->warm_stop_charge);
		}else{
			hq_info("warm_stop_charge = %d, vbat and temp not low!\n", manager->warm_stop_charge);
		}
	}

	/* battery hot stop charge protection */
	if (manager->tbat >= HOT_STOP_CHARGE_TBAT) {
		manager->hot_stop_charge = true;
		vote(manager->total_fcc_votable, HOT_STOP_CHG_VOTER, true, 0);
		hq_info("hot_stop_charge = %d, stop charge\n", manager->hot_stop_charge);
	}

	if (manager->hot_stop_charge) {
		if (manager->tbat <= HOT_RECOVER_CHARGE_TBAT) {
			manager->hot_stop_charge = false;
			vote(manager->total_fcc_votable, HOT_STOP_CHG_VOTER, false, 0);
			hq_info("hot_stop_charge = %d, recover charge\n", manager->hot_stop_charge);
		}
	}

	/* battery cold stop charge protection */
	if (manager->tbat <= COLD_STOP_CHARGE_TBAT) {
		manager->cold_stop_charge = true;
		vote(manager->total_fcc_votable, CHG_PROTECT_VOTER, true, 0);
		hq_info("cold_stop_charge = %d, stop charge\n", manager->cold_stop_charge);
	}

	if (manager->cold_stop_charge) {
		if (manager->tbat >= (COLD_STOP_CHARGE_TBAT + RECHG_TBAT_COLD_OFFSET)) {
			manager->cold_stop_charge = false;
			vote(manager->total_fcc_votable, CHG_PROTECT_VOTER, false, 0);
			hq_info("cold_stop_charge = %d, recover charge\n", manager->cold_stop_charge);
		}
	}

	return 0;
}
#endif

/******************************************************************************/
/*                     SINGLE-CELL DETECT FUNCTIONS                           */
/******************************************************************************/
static void set_detcell_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, set_detcell_work.work);
	int total_fcc = 0, main_fcc = 0, iterm_vote = 0;

	total_fcc = get_effective_result(manager->total_fcc_votable);
	main_fcc = get_effective_result(manager->main_fcc_votable);
	iterm_vote = get_effective_result(manager->iterm_votable);

	/******************once battery cell broken, fcc need half******************/
	vote(manager->total_fcc_votable, SINGLE_CELL_VOTER, true, total_fcc/2);
	vote(manager->main_fcc_votable, SINGLE_CELL_VOTER,true, main_fcc/2);
	vote(manager->iterm_votable, SINGLE_CELL_VOTER, true, 250);
	hq_info("voting for single_cell:%d:%d:%d\n", iterm_vote/2, total_fcc/2, main_fcc/2);
}

static void judge_cell_status(struct charger_manager *manager)
{
	int c_car_offset = 0;
	int v_car_offset = 0;
	static int count;

	if(manager->input_suspend){
		hq_info("[Detcell] input_suspend enable, skip Detcell\n");
		return;
	}

	manager->c_car_out = fuel_gauge_get_c_car(manager->fuel_gauge);
	manager->v_car_out = fuel_gauge_get_v_car(manager->fuel_gauge);

	c_car_offset = abs(manager->c_car_out - manager->c_car_in);
	v_car_offset = abs(manager->v_car_out - manager->v_car_in);
	hq_info("[Detcell] c_offset: %d, v_offset: %d, cr_out: %d, v_out: %d, vbat:%d, ibat:%d\n",
		c_car_offset, v_car_offset, manager->c_car_out, manager->v_car_out, manager->vbat, manager->ibat);

	if ( v_car_offset  * 10 >= c_car_offset * 17) { ///// release note:  V / C >= 1.7 , broken battery
		if (count >= 1) {
			hq_info("[Detcell] cell is broken!\n");
			schedule_delayed_work(&manager->set_detcell_work, 0);
		} else {
			count++;
			hq_info("[Detcell] Detcell count add %d\n", count);
			vote(manager->total_fcc_votable, SINGLE_CELL_VOTER,false, 0);
			vote(manager->main_fcc_votable, SINGLE_CELL_VOTER,false, 0);
			vote(manager->iterm_votable, SINGLE_CELL_VOTER,false, 0);
		}
	} else {
		hq_info("[Detcell] Detcell cell is ok\n");
		vote(manager->total_fcc_votable, SINGLE_CELL_VOTER,false, 0);
		vote(manager->main_fcc_votable, SINGLE_CELL_VOTER,false, 0);
		vote(manager->iterm_votable, SINGLE_CELL_VOTER,false, 0);
		count = 0;
	}
}

static enum alarmtimer_restart cell_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
			struct charger_manager, cell_det_work_timer);
	if(manager != NULL)
		judge_cell_status(manager);

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart start_cell_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, start_cell_det_work_timer);
	if(manager != NULL)
	{
		manager->v_car_in = fuel_gauge_get_v_car(manager->fuel_gauge);
		if (manager->v_car_in <= 0 || manager->ibat > -1000000) {
			hq_info("[Detcell] ibat too low or vcar error, exit cell_det_work_timer\n");
			return ALARMTIMER_NORESTART;
		} else {
			manager->c_car_in = fuel_gauge_get_c_car(manager->fuel_gauge);
			hq_info("[Detcell] c_car_in = %d, v_car_in = %d, vbat: %d, ibat: %d\n",
				manager->c_car_in, manager->v_car_in, manager->vbat, manager->ibat);

			ret = alarm_try_to_cancel(&manager->cell_det_work_timer);
			if (ret < 0) {
				hq_err("[Detcell] callback was running, skip timer\n");
				return ret;
			}
			ktime_now = ktime_get_boottime();
			time_now = ktime_to_timespec64(ktime_now);
			end_time.tv_sec = time_now.tv_sec + 1200;
			end_time.tv_nsec = time_now.tv_nsec + 0;
			ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
			alarm_start(&manager->cell_det_work_timer, ktime);
		}
	}
	return ALARMTIMER_NORESTART;
}

static int charger_manager_misc_start_detcell(struct charger_manager *manager)
{
	/* initialize flags before misc run */
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	if (!manager->input_suspend && manager->vbat <= 4150 && manager->tbat <= 400 && manager->tbat >= 150) {
		//before start timer,try to cancle
		ret = alarm_try_to_cancel(&manager->start_cell_det_work_timer);
		if (ret < 0) {
			hq_err("[Detcell] alarm was running, skip timer\n");
			return ret;
		}
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		end_time.tv_sec = time_now.tv_sec + 30;
		end_time.tv_nsec = time_now.tv_nsec + 0;
		ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
		alarm_start(&manager->start_cell_det_work_timer, ktime);
		hq_info("[Detcell] wait 30S to detect, vbat: %d, temp_bat:%d\n",manager->vbat, manager->tbat);
	}

	return ret;
}

/******************************************************************************/
/*                     REGISTER DUMP DEBUG FUNCTIONS                          */
/******************************************************************************/
#ifdef REGISTER_DUMP_DEBUG
static int do_register_dump_debug_work(struct charger_manager *manager)
{
	if ((manager->vbus_type == VBUS_TYPE_HVDCP) && (manager->ibat < -3600000)) {
		charger_dump_registers(manager->charger);
	}

	return 0;
}
#endif

/******************************************************************************/
/*                        CM MISC INTERNAL FUNCTIONS                          */
/******************************************************************************/
static int charger_manager_misc_start_routines(struct charger_manager *manager)
{
	/* initialize flags before misc run */
	manager->warm_stop_charge = false;
	manager->cold_stop_charge = false;
	manager->hot_stop_charge = false;

	return 0;
}

static int charger_manager_misc_stop_routines(struct charger_manager *manager)
{
	/* cleanup flags and voter after misc stop */
	manager->warm_stop_charge = false;
	manager->cold_stop_charge = false;
	manager->hot_stop_charge = false;
	vote(manager->total_fcc_votable, HOT_STOP_CHG_VOTER, false, 0);
	vote(manager->total_fcc_votable, CHG_PROTECT_VOTER, false, 0);
	vote(manager->total_fcc_votable, SINGLE_CELL_VOTER,false, 0);
	vote(manager->main_fcc_votable, SINGLE_CELL_VOTER,false, 0);
	vote(manager->iterm_votable, SINGLE_CELL_VOTER,false, 0);

	return 0;
}

static void charger_manager_misc_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
							struct charger_manager, charger_manager_misc_work.work);

	mutex_lock(&manager->charger_manager_misc_work_lock);

#ifdef STOP_CHARGE_PROTECT
	do_stop_charge_protect_work(manager);
#endif

#ifdef REGISTER_DUMP_DEBUG
	do_register_dump_debug_work(manager);
#endif

	mutex_unlock(&manager->charger_manager_misc_work_lock);

	schedule_delayed_work(&manager->charger_manager_misc_work, msecs_to_jiffies(3000));
}


int charger_manager_misc_init(struct charger_manager *manager)
{
	if (IS_ERR_OR_NULL(manager)) {
		return -EINVAL;
	}

	/* TODO: single cell deetct init */
	if (manager->single_cell_det){
		alarm_init(&manager->start_cell_det_work_timer, ALARM_BOOTTIME, start_cell_det_work_timer_handler);
		alarm_init(&manager->cell_det_work_timer, ALARM_BOOTTIME, cell_det_work_timer_handler);
	}
	INIT_DELAYED_WORK(&manager->set_detcell_work, set_detcell_work);

	charger_manager_misc_shipmode_init(manager);

	mutex_init(&manager->charger_manager_misc_work_lock);

	manager->warm_stop_charge = false;
	manager->cold_stop_charge = false;
	manager->hot_stop_charge = false;

	INIT_DELAYED_WORK(&manager->charger_manager_misc_work, charger_manager_misc_work);

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	INIT_DELAYED_WORK(&manager->batt_soh20_aging_test, batt_soh20_aging_test);
#endif

	return 0;
}

int charger_manager_misc_run(struct charger_manager *manager)
{
	if (IS_ERR_OR_NULL(manager)) {
		return -EINVAL;
	}

	/* WARNING: Don't add charger manager misc functions before the line */
	charger_manager_misc_start_routines(manager);

	if (manager->single_cell_det)
		charger_manager_misc_start_detcell(manager);

	schedule_delayed_work(&manager->charger_manager_misc_work, msecs_to_jiffies(3000));

	return 0;
}

int charger_manager_misc_stop(struct charger_manager *manager)
{
	int ret = 0;
	if (IS_ERR_OR_NULL(manager)) {
		return -EINVAL;
	}

	if (manager->single_cell_det){
		ret = alarm_try_to_cancel(&manager->start_cell_det_work_timer);
		if (ret < 0) {
			hq_err("[Detcell] start_cell_det_work_timer callback was running, skip timer\n");
		}
		ret = alarm_try_to_cancel(&manager->cell_det_work_timer);
		if (ret < 0) {
			hq_err("[Detcell] cell_det_work_timer callback was running, skip timer\n");
		}
		hq_info(" [Detcell] alarm_try_to_cancel\n");
	}

	cancel_delayed_work_sync(&manager->charger_manager_misc_work);

	/* WARNING: Don't add charger manager functions after the line */
	charger_manager_misc_stop_routines(manager);

	return 0;
}

int charger_manager_misc_deinit(struct charger_manager *manager)
{
	cancel_delayed_work_sync(&manager->charger_manager_misc_work);

	cancel_delayed_work_sync(&manager->set_detcell_work);

	return 0;
}
