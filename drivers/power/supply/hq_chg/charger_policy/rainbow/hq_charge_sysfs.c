// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019 The Linux Foundation. All rights reserved.
 */

#include "hq_charger_manager.h"
#include "xm_chg_uevent.h"
#if IS_ENABLED(CONFIG_XM_SMART_CHG)
#include "xm_smart_chg.h"
#include "hq_voter.h"
#endif
#include "hq_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[HQ_CHG_CM]"
#endif

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
extern int reverse_charge_policy_run(struct charger_manager *manager);
#endif

static const char *real_type_txt[] = {
	"None",
	"USB",
	"USB_CDP",
	"DCP",
	"USB_HVDCP",
	"USB_FLOAT",
	"Non-Stand",
	"USB_HVDCP_3",
	"USB_HVDCP_3P5",
	"USB_PD",
	"PD_PPS",
};

static int battery_temp[5] = {-100, 150, 480, 520, 600};

enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,    /* USB, DCP, CDP, Float */
	QUICK_CHARGE_FAST,          /* PD, QC2, QC3 */
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,         /* verified PD(apdo_max < 50W), QC3.5, QC3-27W */
	QUICK_CHARGE_SUPER,         /* verified PD(apdo_max >= 50W) */
};

struct quick_charge {
	enum vbus_type adap_type;
	enum quick_charge_type adap_cap;
};

static struct quick_charge quick_charge_map[10] = {
	{ VBUS_TYPE_SDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_DCP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_CDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_NON_STAND, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_PD, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP_3, QUICK_CHARGE_TURBE },
	{ VBUS_TYPE_HVDCP_3P5, QUICK_CHARGE_TURBE },
	{ 0, 0 },
};

static int charger_usb_get_property(struct power_supply *psy,
						enum power_supply_property psp,
						union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	bool online = false;
	enum vbus_type vbus_type;
	int ret = 0;
	int volt = 0;
	int curr = 0;

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager->tcpc);
		}
	}

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->charger)) {
		hq_err("manager charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		ret = charger_get_vbus_type(manager->charger, &vbus_type);
		if (ret < 0)
			hq_err("Couldn't get usb type ret=%d\n", ret);
		#ifdef KERNEL_FACTORY_HQ_CHG
		if (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)
		#else
		if (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP ||
			((vbus_type == VBUS_TYPE_FLOAT || vbus_type == VBUS_TYPE_NON_STAND) && manager->is_dr_swap))
		/***** when dongle(support pd) plug in, it would send DR_SWAP,
		dongle would connect pd and southchip vbus_type would become VBUS_TYPE_FLOAT,
		dongle would connect pd and nulta vbus_type would become VBUS_TYPE_NON_STAND,
		so we would set type as usb so that extcon_usb set phone_role as devices *****/
		#endif
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (manager->pd_active != 2 &&
			manager->tcpc->adapt_pid == 0xf663 && manager->tcpc->adapt_vid == 0x2b01)
			charger_get_vbus_type(manager->charger, &manager->vbus_type);

		if (manager->tcpc->adapt_pid == 0xf663 &&
			manager->tcpc->adapt_vid == 0x2b01 && manager->vbus_type == 0)
			msleep(1000);

		ret = charger_get_online(manager->charger, &online);
		if (ret < 0)
			val->intval = 0;
		/*****when detect MI-PD adapter plug in, it would do once hard reset,
		Causing charging interruption, so we need get bc_type to set online*****/
		else if (manager->vbus_type == 0)
			val->intval = 0;
		else
			val->intval = online;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = VOLTAGE_MAX;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = charger_get_adc(manager->charger, CHG_ADC_VBUS, &volt);
		if (ret < 0) {
			hq_err("Couldn't read input volt ret=%d\n", ret);
			return ret;
		}
		val->intval = volt;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = charger_manager_get_current(manager, &curr);
		if (ret < 0) {
			hq_err("Couldn't read input curr ret=%d\n", ret);
			return ret;
		}
		val->intval = curr;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = CURRENT_MAX;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = INPUT_CURRENT_LIMIT;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = POWER_SUPPLY_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = POWER_SUPPLY_MODEL_NAME;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int charger_usb_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static int usb_psy_is_writeable(struct power_supply *psy, enum power_supply_property psp)
{
	switch(psp) {
	default:
		return 0;
	}
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = usb_props,
	.num_properties = ARRAY_SIZE(usb_props),
	.get_property = charger_usb_get_property,
	.set_property = charger_usb_set_property,
	.property_is_writeable = usb_psy_is_writeable,
};

int charger_manager_usb_psy_register(struct charger_manager *manager)
{
	struct power_supply_config usb_psy_cfg = { .drv_data = manager,};

	memcpy(&manager->usb_psy_desc, &usb_psy_desc, sizeof(manager->usb_psy_desc));

	manager->usb_psy = devm_power_supply_register(manager->dev, &manager->usb_psy_desc,
							&usb_psy_cfg);
	if (IS_ERR(manager->usb_psy)) {
		hq_err("usb psy register failed\n");
		return PTR_ERR(manager->usb_psy);
	}
	return 0;
}
EXPORT_SYMBOL(charger_manager_usb_psy_register);

static int get_battery_health(struct charger_manager *manager)
{
	union power_supply_propval pval;
	int battery_health = POWER_SUPPLY_HEALTH_GOOD;
	int ret = 0;

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("failed to get temp prop\n");
		return -EINVAL;
	}

	if (pval.intval <= battery_temp[TEMP_LEVEL_COLD])
		battery_health = POWER_SUPPLY_HEALTH_COLD;
	else if (pval.intval <= battery_temp[TEMP_LEVEL_COOL])
		battery_health = POWER_SUPPLY_HEALTH_COOL;
	else if (pval.intval <= battery_temp[TEMP_LEVEL_GOOD])
		battery_health = POWER_SUPPLY_HEALTH_GOOD;
	else if (pval.intval <= battery_temp[TEMP_LEVEL_WARM])
		battery_health = POWER_SUPPLY_HEALTH_WARM;
	else if (pval.intval < battery_temp[TEMP_LEVEL_HOT])
		battery_health = POWER_SUPPLY_HEALTH_HOT;
	else
		battery_health = POWER_SUPPLY_HEALTH_OVERHEAT;

	return battery_health;
}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
static void charger_get_batt_manufacturing_date(struct charger_manager *manager, char *manufacturing_date)
{
	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		hq_err("manager->auth_dev is_err_or_null\n");
		return;
	}

	manufacturing_date[3] = manager->auth_dev->batt_sn[6];
	manufacturing_date[6] = manager->auth_dev->batt_sn[8];
	manufacturing_date[7] = manager->auth_dev->batt_sn[9];

	switch (manager->auth_dev->batt_sn[7]){
		case 'A':
		case 'a':
			manufacturing_date[4] = '1';
			manufacturing_date[5] = '0';
		break;
		case 'B':
		case 'b':
			manufacturing_date[4] = '1';
			manufacturing_date[5] = '1';
		break;
		case 'C':
		case 'c':
			manufacturing_date[4] = '1';
			manufacturing_date[5] = '2';
		break;
		default:
			manufacturing_date[5] = manager->auth_dev->batt_sn[7];
		break;
	}
	hq_info("manufacturing_date:%s\n", manufacturing_date);
}

static int charger_get_batt_first_usage_date(struct charger_manager *manager, char* first_usage_date)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			hq_err("manager->auth_dev is_err_or_null\n");
			return -1;
		}
	}

	ret = auth_device_get_first_usage_date(manager->auth_dev, first_usage_date, 6);

	return ret;
}

static void charger_set_batt_first_usage_date(struct charger_manager *manager, u8* first_usage_date)
{
	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			hq_err("manager->auth_dev is_err_or_null\n");
			return;
		}
	}

	hq_info("first_usage_date:%s\n", first_usage_date);

	auth_device_set_first_usage_date(manager->auth_dev, first_usage_date, 6);

}
#endif


#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
static void charger_get_fg_manufacturing_date(struct charger_manager *manager, char *buf)
{
	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	fuel_gauge_get_manufacturing_date(manager->fuel_gauge, buf);
out:
	hq_info("manufacturing_date:%s\n", buf);
}

static int charger_get_fg_first_usage_date(struct charger_manager *manager, char* first_usage_date)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			return -EINVAL;
		}
	}

	ret = fuel_gauge_get_first_usage_date(manager->fuel_gauge, first_usage_date);

	return ret;
}

static void charger_set_fg_first_usage_date(struct charger_manager *manager, const char *buf, size_t count)
{
	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			return;
		}
	}

	hq_info("first_usage_date:%s\n", buf);

	fuel_gauge_set_first_usage_date(manager->fuel_gauge, buf, count);

}
#endif

static int charger_batt_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	union power_supply_propval pval;
	int state = 0, status = 0;
	bool warm_stop_charge = false;
	int __maybe_unused bat_volt = 0;
	int ret = 0;
	int __maybe_unused fv = 0;
	static bool eea_report_full = false;

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (manager->fg_psy == NULL)
		manager->fg_psy = power_supply_get_by_name("bms");

	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		hq_err("failed to get bms psy\n");
		return PTR_ERR(manager->fg_psy);
	}

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
#if IS_ENABLED(CONFIG_HQ_SHUTDOWN_POLICY)
			if (!IS_ERR_OR_NULL(manager->shutdown_policy)) {
				if (manager->shutdown_policy->SOC0_shutdown) {
					val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
					hq_info("soc0 shutdown report dischg\n");
					break;
				}
			}
#endif
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				hq_err("failed to get chg status prop\n");
				break;
			}

			if (manager->board_version == EEA_VERSION) {
				//stop charging for hardware report full status
				if (status == POWER_SUPPLY_STATUS_FULL) {
					manager->rsoc = fuel_gauge_get_rsoc(manager->fuel_gauge);
					if (manager->rsoc == 100 || eea_report_full) {
						status = POWER_SUPPLY_STATUS_FULL;
						eea_report_full = true;
						hq_info("hard report full\n");
					} else {
						status = POWER_SUPPLY_STATUS_CHARGING;
					}
				//stop charging for software report full status
				} else if (manager->term_recharge_policy->soft_charge_status == POWER_SUPPLY_STATUS_FULL ||
					manager->term_recharge_policy->terminated) {
						manager->rsoc = fuel_gauge_get_rsoc(manager->fuel_gauge);
						if (manager->rsoc == 100 || eea_report_full) {
							status = POWER_SUPPLY_STATUS_FULL;
							if (!IS_ERR_OR_NULL(manager->batt_psy) && manager->soc == 99 && !eea_report_full)
								power_supply_changed(manager->batt_psy);
							eea_report_full = true;
							hq_info("soft report full\n");
						}
				} else {
					eea_report_full = false;
					hq_info("eea_report_full set false\n");
				}
			} else {
				if (manager->term_recharge_policy->terminated)
					status = POWER_SUPPLY_STATUS_FULL;
			}

			/* temperature stop charge overlay charge status */
			warm_stop_charge = manager->warm_stop_charge;
			if (status != POWER_SUPPLY_STATUS_DISCHARGING && status != POWER_SUPPLY_STATUS_FULL) {
				if (manager->tbat >= BATTERY_HOT_TEMP || manager->tbat <= BATTERY_COLD_TEMP)
					status = POWER_SUPPLY_STATUS_NOT_CHARGING;
				else if ((manager->tbat >= BATTERY_WARM_TEMP || warm_stop_charge))
					status = POWER_SUPPLY_STATUS_CHARGING;
				else if (((manager->tbat < BATTERY_WARM_TEMP) && (manager->vbat < 4100)))
					status = POWER_SUPPLY_STATUS_CHARGING;
			}

			/* smart_chg stop charge overlay charge status*/
			#if IS_ENABLED(CONFIG_XM_SMART_CHG)
			if (IS_ERR_OR_NULL(manager->smart_chg)) {
				return PTR_ERR(manager->smart_chg);
			}

			if (status != POWER_SUPPLY_STATUS_CHARGING) {
				if (manager->smart_chg->stop_charge) {
					status = POWER_SUPPLY_STATUS_CHARGING;
				}
			}
			#endif /* CONFIG_XM_SMART_CHG */

			/* stop charge overlay charge status */
			if (IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
				hq_err("failed to get main_chg_disable_votable\n");
			} else {
				if (get_effective_result(manager->main_chg_disable_votable)) {
					status = POWER_SUPPLY_STATUS_DISCHARGING;
				}
			}

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
			if (manager->reverse_charge_policy) {
				if (manager->reverse_charge_policy->in_otg_mode)
					status = POWER_SUPPLY_STATUS_DISCHARGING;
			}
#endif
			val->intval = status;
			break;

		case POWER_SUPPLY_PROP_HEALTH:
			ret = get_battery_health(manager);
			if (ret < 0)
				break;
			val->intval = ret;
			break;

		case POWER_SUPPLY_PROP_PRESENT:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_PRESENT, &pval);
			if (ret < 0) {
				hq_err("failed to get online prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				hq_err("failed to get chg type prop\n");
				break;
			}
			val->intval = state;
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
			ret = power_supply_get_property(manager->fg_psy,
					POWER_SUPPLY_PROP_CAPACITY, &pval);
			if (ret < 0) {
				hq_err("failed to get capaticy prop\n");
				break;
			} else
				val->intval = pval.intval;

#if IS_ENABLED(CONFIG_HQ_SHUTDOWN_POLICY)
			if (IS_ERR_OR_NULL(manager->shutdown_policy)) {
				if (val->intval <= 1)
					val->intval = 1;
			} else {
				if (((val->intval <= 1) && !manager->shutdown_policy->SOC0_shutdown) || manager->shutdown_policy->shutdown_delay)
					val->intval = 1;
			}
#else
			if (val->intval <= 1)
				val->intval = 1;
#endif

			if (manager->board_version == EEA_VERSION) {
				if ((manager->soc == 100) && (pval.intval == 100)
					&& (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING)) {
					val->intval = pval.intval;
					hq_info("uisoc is 100, need disable charge\n");
				} else if (pval.intval == 100 && manager->soc != 1) {
					if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING) {
						val->intval = 99;
						hq_info("hold uisoc 99 until charge status full\n");
					} else if (manager->chg_status
								== POWER_SUPPLY_STATUS_FULL) {
						val->intval = 100;
						hq_info("need report 100 when charge status full\n");
					}
				}
			}
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_fg_status(manager->fuel_gauge);
			if (!(ret & FG_EER_I2C_FAIL)) {
#endif
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0) {
					hq_err("failed to get voltage-now prop\n");
					break;
				}
				val->intval = pval.intval;
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				charger_get_adc(manager->charger, CHG_ADC_VBAT, &bat_volt);
				if (!bat_volt) {
					charger_adc_enable(manager->charger, true);
					charger_get_adc(manager->charger, CHG_ADC_VBAT, &bat_volt);
				}
				val->intval = bat_volt * 1000;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
			ret = fuel_gauge_get_fastcharge_mode(manager->fuel_gauge);
			if (ret)
				val->intval = FAST_CHG_VOLTAGE_MAX;
			else
				val->intval = NORMAL_CHG_VOLTAGE_MAX;
			#else
			#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
			if (IS_ERR_OR_NULL(manager->pd_adapter)) {
				hq_err("manager->pd_adapter is_err_or_null\n");
				return PTR_ERR(manager->pd_adapter);
			}

			ret = adapter_get_usbpd_verifed(manager->pd_adapter, &pd_verifed);
			if (ret < 0){
				hq_err("Couldn't get usbpd verifed ret=%d\n", ret);
				return ret;
			}
			#endif
			/*
			 * TODO: voltage_max depending on current charging status:FFC or NORMAL,
			 * so when the charging status is FFC?
			 * we need a judgment logic follow HQ-410839 and comment on the code below tempporarily
			 */
			// if (g_policy == NULL) {
			// 	val->intval = NORMAL_CHG_VOLTAGE_MAX;
			// } else {
			// 	if (manager->tbat > TEMP_LEVEL_15 && manager->tbat <= TEMP_LEVEL_45 &&
			// 		pd_verifed && (g_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || g_policy->cp_charge_done))
			// 		val->intval = FAST_CHG_VOLTAGE_MAX;
			// 	else
			// 		val->intval = NORMAL_CHG_VOLTAGE_MAX;
			// }
			val->intval = NORMAL_CHG_VOLTAGE_MAX;
			#endif
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			if (IS_ERR_OR_NULL(manager->thermal_policy)) {
				hq_err("thermal policy is_err_or_null\n");
				break;
			}

			val->intval = manager->thermal_policy->thermal_level;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
			if (IS_ERR_OR_NULL(manager->thermal_policy)) {
				hq_err("thermal policy is_err_or_null\n");
				break;
			}

			val->intval = manager->thermal_policy->thermal_level_max;
			break;

		case POWER_SUPPLY_PROP_CURRENT_NOW:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
			if (ret < 0) {
				hq_err("failed to get current_now prop\n");
				break;
			}
			//Negative value represents charging, unit ma
			val->intval = -pval.intval;
			break;

		case POWER_SUPPLY_PROP_TEMP:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
			if (ret < 0) {
				hq_err("failed to get temp prop\n");
				break;
			}
			val->intval = pval.intval;
			manager->tbat = pval.intval;
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
			break;

		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			/*
			 * NOTE: /sys/class/power_supply/battery/cycle_count support report
			 * fake battery cycle for test
			 * Don't update fake_batt_cycle to battery secret ic!!!
			 */
			if (manager->fake_batt_cycle != 0xFFFF) {
				val->intval = manager->fake_batt_cycle;
				break;
			}

			#if IS_ENABLED(CONFIG_CHARGE_ARCH_BATT_AUTH)
			val->intval = manager->batt_cycle;
			#else
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
			if (ret < 0) {
				hq_err("failed to get cycle_count prop\n");
				val->intval = 0;
			} else
				val->intval = pval.intval;
			#endif

			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			val->intval = TYPICAL_CAPACITY;
			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
			if (ret < 0) {
				hq_err("failed to get charge_full prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = manager->model_name;

			break;

		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
			if (ret < 0) {
				hq_err("failed to get charge_counter prop\n");
				break;
			}
			val->intval = pval.intval / 1000;		//mAh
			break;
		case POWER_SUPPLY_PROP_AUTHENTIC:
			if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
				manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
				if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
					hq_err("manager->fuel_gauge is_err_or_null\n");
					val->intval = 0;
					break;
				}
			}
			val->intval = fuel_gauge_get_batt_auth(manager->fuel_gauge);
			break;
		default:
			break;
	}
	return 0;
}

static int charger_batt_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
			if (IS_ERR_OR_NULL(manager->thermal_policy)) {
				hq_err("thermal policy is_err_or_null\n");
				break;
			}

			/* TODO: replace with limit(val, down_limit, up_limit) common function */
			if (val->intval >= manager->thermal_policy->thermal_level_max) {
				manager->thermal_policy->thermal_level = manager->thermal_policy->thermal_level_max-1;
				hq_err("invalid thermal_level: %d more than maximum level: %d\n",
					val->intval, (manager->thermal_policy->thermal_level_max - 1));
			} else if (val->intval < 0) {
				manager->thermal_policy->thermal_level = 0;
				hq_err("invalid thermal_level: %d less than minium level: 0\n", val->intval);
			} else {
				manager->thermal_policy->thermal_level = val->intval;
			}

			manager->thermal_policy->thermal_type = TEMP_THERMAL_TYPE;
			hq_info("set thermal_level = %d, thermal_type = %d\n",
				manager->thermal_policy->thermal_level, manager->thermal_policy->thermal_type);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			if (IS_ERR_OR_NULL(manager->thermal_policy)) {
				hq_err("thermal policy is_err_or_null\n");
				break;
			}

			/* TODO: replace with limit(val, down_limit, up_limit) common function */
			if (val->intval >= manager->thermal_policy->thermal_level_max) {
				manager->thermal_policy->thermal_level = manager->thermal_policy->thermal_level_max - 1;
				hq_err("invalid thermal_level: %d more than maximum level: %d\n",
					val->intval, (manager->thermal_policy->thermal_level_max - 1));
			} else if (val->intval < 0) {
				manager->thermal_policy->thermal_level = 0;
				hq_err("invalid thermal_level: %d less than minium level: 0\n", val->intval);
			} else {
				manager->thermal_policy->thermal_level = val->intval;
			}

			manager->thermal_policy->thermal_type = CALL_THERMAL_TYPE;
			hq_info("set thermal_level = %d, thermal_type = %d\n",
				manager->thermal_policy->thermal_level, manager->thermal_policy->thermal_type);
			break;

		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			manager->fake_batt_cycle = val->intval;
			hq_info("set fake_batt_cycle = %d\n", manager->fake_batt_cycle);
			break;

		default:
			break;
	}
	return 0;
}

static int batt_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			return 1;
		default:
			break;
	}
	return 0;
}

static enum power_supply_property batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_AUTHENTIC,
};

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_props,
	.num_properties = ARRAY_SIZE(batt_props),
	.get_property = charger_batt_get_property,
	.set_property = charger_batt_set_property,
	.property_is_writeable = batt_prop_is_writeable,
};

int charger_manager_batt_psy_register(struct charger_manager *manager)
{
	struct power_supply_config batt_psy_cfg = { .drv_data = manager,};
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	manager->batt_psy = devm_power_supply_register(manager->dev, &batt_psy_desc,
							&batt_psy_cfg);
	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		hq_err("batt psy register failed\n");
		return PTR_ERR(manager->batt_psy);
	}
	hq_info("batt psy register success\n");
	return 0;
}
EXPORT_SYMBOL(charger_manager_batt_psy_register);


static int charger_chg_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	union power_supply_propval pval;
	enum vbus_type vbus_type;
	int ret = 0;

	if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(manager->usb_psy) || IS_ERR_OR_NULL(manager->batt_psy)) {
		hq_err("manager or usb psy or batt psy is_err_or_null\n");
		return -ENOMEM;
	}
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			ret = power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			if (ret < 0) {
				hq_err("get online status form usb psy fail\n");
				val->intval = 0;
				break;
			}
			val->intval = pval.intval;
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
			if (IS_ERR_OR_NULL(manager->fv_votable)) {
				hq_err("fv votable is_err_or_null\n");
				return -ENOMEM;
			}
			val->intval = get_effective_result(manager->fv_votable);
			break;
		case POWER_SUPPLY_PROP_STATUS:
			if (manager->board_version == EEA_VERSION)
				break;
			ret = power_supply_get_property(manager->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
			if (ret < 0) {
				hq_err("get online status form batt psy fail\n");
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
				break;
			}
			val->intval = pval.intval;
			break;
		case POWER_SUPPLY_PROP_USB_TYPE:
			ret = charger_get_vbus_type(manager->charger, &vbus_type);
			if (ret < 0)
				hq_err("Couldn't get usb type ret=%d\n", ret);
			if (vbus_type == VBUS_TYPE_NONE)
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			else
				val->intval = POWER_SUPPLY_USB_TYPE_PD;
			break;
		default:
			break;
	}
	return 0;
}

static int charger_chg_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	switch (prop) {
		default:
			break;
	}
	return 0;
}

static int chg_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
		default:
			break;
	}
	return 0;
}

static enum power_supply_property chg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
};

static enum power_supply_usb_type hq_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static char *hq_charger_supplied_to[] = {
	"bms"
};

static const struct power_supply_desc chg_psy_desc = {
	.name = "charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = chg_props,
	.num_properties = ARRAY_SIZE(chg_props),
	.get_property = charger_chg_get_property,
	.set_property = charger_chg_set_property,
	.property_is_writeable = chg_prop_is_writeable,
	.usb_types = hq_charger_usb_types,
	.num_usb_types = ARRAY_SIZE(hq_charger_usb_types),
};

int charger_manager_chg_psy_register(struct charger_manager *manager)
{
	struct power_supply_config chg_psy_cfg = { .drv_data = manager,};
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}
	chg_psy_cfg.supplied_to = hq_charger_supplied_to;
	chg_psy_cfg.num_supplicants = ARRAY_SIZE(hq_charger_supplied_to);

	manager->chg_psy = devm_power_supply_register(manager->dev, &chg_psy_desc,
							&chg_psy_cfg);
	if (IS_ERR_OR_NULL(manager->chg_psy)) {
		hq_err("chg psy register failed\n");
		return PTR_ERR(manager->chg_psy);
	}
	hq_info("chg psy register success\n");
	return 0;
}
EXPORT_SYMBOL(charger_manager_chg_psy_register);

static void charger_manager_from_psy(struct device *dev, struct charger_manager **manager)
{
	struct power_supply *psy;
	if (IS_ERR_OR_NULL(dev)) {
		hq_err("dev is_err_or_null\n");
		return;
	}

	psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(psy)) {
		hq_err("psy is_err_or_null\n");
		return;
	}

	*manager = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(*manager)) {
		hq_err("manager is_err_or_null\n");
		return;
	}
}

void xm_uevent_report(struct charger_manager *manager);

static ssize_t real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	int ret;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->charger)) {
		hq_err("manager->charger is_err_or_null\n");
		goto out;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		hq_err("Couldn't get usb type ret=%d\n", ret);
		goto out;
	}

out:
	if (manager->pd_active == CHARGE_PD_ACTIVE)
		vbus_type = VBUS_TYPE_PD;
	else if (manager->pd_active == CHARGE_PD_PPS_ACTIVE)
		vbus_type = VBUS_TYPE_PD_PPS;

	/*
	 * WORKAROUNG FOR BUGO84-2473: fix ZZ-0250 adapter non-stand issue,
	 * ZZ-0250 is detected as NON-STANDARD
	 * This change aims to make it shows "USB-FLOAT"
	 * in engineer test instead of non-stand
	 */
	if (vbus_type == VBUS_TYPE_NON_STAND)
		vbus_type = VBUS_TYPE_FLOAT;


	xm_uevent_report(manager);

	return sprintf(buf, "%s\n", real_type_txt[vbus_type]);
}

static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
static ssize_t cp_icl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	int ret = 0;
	int cp_icl = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->master_cp_chg)) {
		hq_err("manager->master_cp_chg is_err_or_null\n");
		return PTR_ERR(manager->master_cp_chg);
	}

	ret = chargerpump_get_adc_value(manager->master_cp_chg, CP_ADC_IBUS, &cp_icl);
	if (ret < 0){
		hq_err("Couldn't get cp_ibus ret=%d\n", ret);
		cp_icl = 0;
	}

	return sprintf(buf, "%d\n", cp_icl);
}

static struct device_attribute cp_icl_attr =
	__ATTR(cp_icl, 0644, cp_icl_show, NULL);
#endif

static ssize_t det_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* TODO: for detect battery cell damage */
	return sprintf(buf, "%d\n", 0);
}

static struct device_attribute det_cell_attr =
	__ATTR(det_cell, 0444, det_cell_show, NULL);

static void manual_set_cc_toggle(struct charger_manager *manager, bool en)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	if (IS_ERR_OR_NULL(manager))
		return;

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager->tcpc is_err_or_null\n");
			return;
		}
	}

	manager->ui_cc_toggle = en;

	if(!manager->typec_attach && en)
	{
		hq_info("typec is not attached set cc toggle\n");
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_DRP);
	} else if (!manager->typec_attach && !en) {
		if (!manager->soft_cid) {
			hq_info("set cc not toggle\n");
			tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_SNK);
		} else {
			cancel_delayed_work_sync(&manager->handle_cc_status_work);
			queue_delayed_work(system_freezable_wq, &manager->handle_cc_status_work, 0);
		}
	} else {
		hq_info("typec is attached, not set cc toggle\n");
	}

	if(en && !manager->cid_status)
	{
		ret = alarm_try_to_cancel(&manager->otg_ui_close_timer);
		if (ret < 0) {
			hq_err("callback was running, skip timer\n");
			return;
		}
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		end_time.tv_sec = time_now.tv_sec + 600;
		end_time.tv_nsec = time_now.tv_nsec + 0;
		ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

		hq_info("alarm timer start:%d, %lld %ld\n", ret,
			end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&manager->otg_ui_close_timer, ktime);
	}else{
		ret = alarm_try_to_cancel(&manager->otg_ui_close_timer);
		if (ret < 0) {
			hq_err("callback was running, skip timer\n");
			return;
		}
		hq_info("ui disable cc toggle : stop hrtimer\n");
	}

	return;
}

static ssize_t otg_ui_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	hq_info("otg_ui_support : %d\n",manager->en_floatgnd);

	return sprintf(buf, "%d\n", manager->en_floatgnd);
}

static struct device_attribute otg_ui_support_attr =
	__ATTR(otg_ui_support, 0444, otg_ui_support_show, NULL);

static ssize_t cid_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	hq_info("cid_status : %d\n",manager->cid_status);

	return sprintf(buf, "%d\n", manager->cid_status);
}

static struct device_attribute cid_status_attr =
	__ATTR(cid_status, 0444, cid_status_show, NULL);


static ssize_t cc_toggle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val = 0;

	hq_info("cc_toggle_store start\n");
	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manual_set_cc_toggle(manager,!!val);

	hq_info("cc_toggle_store end\n");
	return count;
}

static ssize_t cc_toggle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	hq_info("cc_toggle: %d\n", !!manager->ui_cc_toggle);

	return sprintf(buf, "%d\n", (!!manager->ui_cc_toggle));
}
static struct device_attribute cc_toggle_attr =
	__ATTR(cc_toggle, 0644, cc_toggle_show, cc_toggle_store);


static ssize_t auth_dev_batt_cycle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		hq_err("failed to get battery auth device\n");
		return PTR_ERR(manager->auth_dev);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	ret = auth_device_get_cycle_count(manager->auth_dev, &auth_cycle_count);
	if (ret != 0) {
		hq_err("read auth cycle count error\n");
		return -EINVAL;
	}
	mdelay(20);

	ret = auth_device_set_cycle_count(manager->auth_dev, val, auth_cycle_count);
	if (ret != 0) {
		hq_err("write auth cycle count error\n");
		return -EINVAL;
	}
#endif

	return count;
}

static ssize_t auth_dev_batt_cycle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	u32 auth_cycle_count = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		hq_err("failed to get battery auth device\n");
		return PTR_ERR(manager->auth_dev);
	}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	ret = auth_device_get_cycle_count(manager->auth_dev, &auth_cycle_count);
	if (ret != 0) {
		hq_err("read auth cycle count error\n");
		return -EINVAL;
	}
#endif

	return sprintf(buf, "%d\n", auth_cycle_count);
}
static struct device_attribute auth_dev_batt_cycle_attr =
	__ATTR(auth_dev_batt_cycle, 0644,auth_dev_batt_cycle_show, auth_dev_batt_cycle_store);

static ssize_t product_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	hq_info("%s board_version = %d\n", __func__, manager->board_version);
	return sprintf(buf, "%d\n", manager->board_version);
}

static ssize_t product_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	char str_buf[64] = {0};

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (count >= 64) {
		hq_err("set product name error\n");
		strlcpy(str_buf, "unknown", 8);
		manager->board_version = OTHER_VERSION;
		return -EINVAL;
	}

	strlcpy(str_buf, buf, 64);

	manager->board_version = OTHER_VERSION;
	if (strstr(str_buf, "eea")) {
		manager->board_version = EEA_VERSION;
	}

	hq_err("product name: %s board_version = %d\n", str_buf, manager->board_version);

	return count;
}
static struct device_attribute product_name_attr =
	__ATTR(product_name, 0644,product_name_show, product_name_store);


static ssize_t typec_cc_orientation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	bool usb_online = false;
	bool otg_value = false;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	charger_get_otg_status(manager->charger, &otg_value);
	charger_get_online(manager->charger, &usb_online);

	if (usb_online == false && otg_value == false)
		return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
	else if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->tcpc->typec_polarity + 1);
}
static struct device_attribute typec_cc_orientation_attr =
	__ATTR(typec_cc_orientation, 0644, typec_cc_orientation_show, NULL);

static ssize_t usb_otg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	bool otg_value = false;
	int ret;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	ret = charger_get_otg_status(manager->charger, &otg_value);
	if (ret < 0)
		hq_err("can not get otg status\n");

	hq_info("is otg mode:%d", otg_value);
	return scnprintf(buf, PAGE_SIZE, "%d\n", otg_value);
}
static struct device_attribute usb_otg_attr =
	__ATTR(usb_otg, 0644, usb_otg_show, NULL);

static int get_apdo_max(struct charger_manager *manager) {
	int apdo_max = 0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	if (manager->pd_active != CHARGE_PD_PPS_ACTIVE) {
		goto done;
	}

	apdo_max = g_policy->cap.volt_max[g_policy->cap_nr] *
		g_policy->cap.curr_max[g_policy->cap_nr] / 1000000;

done:
	return apdo_max;

}

static ssize_t power_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	manager->apdo_max = get_apdo_max(manager);

	if (manager->apdo_max >= manager->charge_power_max)
		manager->apdo_max = manager->charge_power_max;

	hq_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);

}
static struct device_attribute power_max_attr =
	__ATTR(power_max, 0644, power_max_show, NULL);

static int quick_charge_type(struct charger_manager *manager)
{
	enum quick_charge_type quick_charge_type = QUICK_CHARGE_NORMAL;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	union power_supply_propval pval = {0, };

	bool usbpd_verifed = false;
	int ret = 0;
	int i = 0;

	if (IS_ERR_OR_NULL(manager->charger)) {
		hq_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		hq_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->batt_psy);
	}

	if (IS_ERR_OR_NULL(manager->usb_psy)) {
		hq_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->usb_psy);
	}

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	if (IS_ERR_OR_NULL(manager->pd_adapter)) {
		hq_err("manager->pd_adapter is_err_or_null\n");
		return PTR_ERR(manager->pd_adapter);
	}

	ret = adapter_get_usbpd_verifed(manager->pd_adapter, &usbpd_verifed);
	if (ret < 0){
		hq_err("Couldn't get usbpd verifed ret=%d\n", ret);
		return ret;
	}
#endif

	ret = power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		hq_err("Couldn't get usb online ret=%d\n", ret);
		return -EINVAL;
	}

	if (!(pval.intval))
		return -EINVAL;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("Couldn't get bat temp ret=%d\n", ret);
		return -EINVAL;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		hq_err("Couldn't get usb type ret=%d\n", ret);
		return ret;
	}

	while (quick_charge_map[i].adap_type != 0) {
		if (vbus_type == quick_charge_map[i].adap_type) {
			quick_charge_type = quick_charge_map[i].adap_cap;
		}
		i++;
	}

	if(manager->pd_active)
		quick_charge_type = QUICK_CHARGE_FAST;
	if(usbpd_verifed && manager->pd_active == CHARGE_PD_PPS_ACTIVE) {
		if (get_apdo_max(manager) >= SUPER_CHARGE_POWER)
			quick_charge_type = QUICK_CHARGE_TURBE;
		else
			quick_charge_type = QUICK_CHARGE_TURBE;
	}
	if(pval.intval >= BATTERY_WARM_TEMP){
		quick_charge_type = QUICK_CHARGE_NORMAL;
	}

	hq_debug("quick_charge_type = %d\n", quick_charge_type);

	return quick_charge_type;
}

static ssize_t quick_charge_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return scnprintf(buf, PAGE_SIZE, "%d\n", quick_charge_type(manager));
}

static struct device_attribute quick_charge_type_attr =
	__ATTR(quick_charge_type, 0644, quick_charge_type_show, NULL);

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};
static ssize_t typec_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n",usb_typec_mode_text[manager->tcpc->typec_mode]);
}

static struct device_attribute typec_mode_attr = __ATTR(typec_mode,0644,typec_mode_show,NULL);

static ssize_t mtbf_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val)) {
		hq_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val != 0) {
		manager->mtbf_mode = val;
	} else {
		manager->mtbf_mode = 0;
	}

	hq_info("set mtbf_mode = %d\n", manager->mtbf_mode);

	return count;
}

static ssize_t mtbf_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	hq_info("get mtbf_mode = %d\n", manager->mtbf_mode);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->mtbf_mode);
}

static struct device_attribute mtbf_mode_attr = __ATTR(mtbf_mode,0644,mtbf_mode_show,mtbf_mode_store);

static struct attribute *usb_psy_attrs[] = {
	&real_type_attr.attr,
	&typec_cc_orientation_attr.attr,
	&usb_otg_attr.attr,
	&power_max_attr.attr,
	&quick_charge_type_attr.attr,
	&typec_mode_attr.attr,
	&mtbf_mode_attr.attr,
	#if IS_ENABLED(CONFIG_CHARGE_ARCH_CHARGEPUMP)
	&cp_icl_attr.attr,
	#endif
	NULL,
};

static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};
int hq_usb_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->usb_psy->dev.kobj,
								&usb_psy_attrs_group);
}
EXPORT_SYMBOL(hq_usb_sysfs_create_group);

#if IS_ENABLED(CONFIG_RUST_DETECTION)
static ssize_t lpd_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->lpd_charging_limit);
}

static ssize_t lpd_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->lpd_charging_limit = val;
	g_policy->lpd_detected = val;
	hq_info("manager->lpd_charging_limit = %d", manager->lpd_charging_limit);
	return count;
}
static struct device_attribute lpd_charging_attr =
	__ATTR(lpd_charging, 0644, lpd_charging_show, lpd_charging_store);
#endif

static ssize_t input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->input_suspend);
}
static ssize_t input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	hq_info("input_suspend_store start\n");
	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
		hq_info("main_chg_disable_votable not found\n");
		return PTR_ERR(manager->main_chg_disable_votable);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->input_suspend = val;
	if (manager->input_suspend) {
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, true, 1);
	} else {
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, false, 0);
	}
	manager->charger->input_suspend = manager->input_suspend;
	hq_info("manager->input_suspend = %d\n", manager->input_suspend);
	return count;
}
static struct device_attribute input_suspend_attr =
	__ATTR(input_suspend, 0644, input_suspend_show, input_suspend_store);

static ssize_t shipmode_count_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->shipmode_flag);
}

static ssize_t shipmode_count_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->shipmode_flag = !!val;
	manager->charger->shipmode_flag = manager->shipmode_flag;

	hq_info("set shipmode_count_reset = %d\n", manager->shipmode_flag);

	return count;
}
static struct device_attribute shipmode_count_reset_attr =
	__ATTR(shipmode_count_reset, 0644, shipmode_count_reset_show, shipmode_count_reset_store);

static ssize_t soc_decimal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	int soc_decimal = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", soc_decimal);

}
static struct device_attribute soc_decimal_attr =
	__ATTR(soc_decimal, 0644, soc_decimal_show, NULL);

static ssize_t soc_decimal_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct charger_manager *manager = NULL;
	int soc_decimal_rate = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(manager->fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", soc_decimal_rate);
}
static struct device_attribute soc_decimal_rate_attr =
	__ATTR(soc_decimal_rate, 0644, soc_decimal_rate_show, NULL);

void xm_uevent_report(struct charger_manager *manager)
{
	int soc_decimal_rate = 0;
	int soc_decimal = 0;

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			return;
		}
	}

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;


	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(manager->fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;

	xm_charge_uevents_bundle_report(CHG_UEVENT_BUNDLE_CHG_ANIMATION, 
	                            quick_charge_type(manager), soc_decimal, soc_decimal_rate);

}
EXPORT_SYMBOL(xm_uevent_report);

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
static ssize_t smart_chg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 smart_chg_node;
	struct charger_manager *manager = NULL;
	bool func_on;
	int func_val;
	unsigned long func_type = 0;
	unsigned long func_type_bitmap[1] = {0};

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->smart_chg))
		return PTR_ERR(manager->smart_chg);

	if (kstrtou32(buf, 0, &smart_chg_node))
		return -EINVAL;

	hq_info("smart_chg_node = 0x%08X\n", smart_chg_node);

	func_on = !!(smart_chg_node & 0x1);
	func_type_bitmap[0] = (smart_chg_node & 0xFFFE) >> 1;
	func_val = (smart_chg_node & 0xFFFF0000) >> 16;

	if (bitmap_weight(func_type_bitmap, SMART_CHG_FUNC_MAX) != 1) {
		hq_err("none or more than one function type bit set\n");
		manager->smart_chg->status = SMART_CHG_ERROR;
		return -EINVAL;
	}

	func_type = find_first_bit(func_type_bitmap, SMART_CHG_FUNC_MAX);
	if (func_type >= SMART_CHG_FUNC_MAX) {
		hq_err("failed to find function type bit\n");
		manager->smart_chg->status = SMART_CHG_ERROR;
		return -EINVAL;
	}

	manager->smart_chg->funcs[func_type].func_on = func_on;
	manager->smart_chg->funcs[func_type].func_val = func_val;
	manager->smart_chg->status = SMART_CHG_SUCCESS;

	hq_info("set smart_chg = 0x%08X, func_type = %d, func_on = %d, func_value = %d\n",
		smart_chg_node, func_type, func_on, func_val);

	return count;
}

static ssize_t smart_chg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	int i = 0;
	u32 smart_chg_node = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->smart_chg))
		return PTR_ERR(manager->smart_chg);

	/* fill functions on/off bit */
	for (i = SMART_CHG_FUNC_MIN; i < SMART_CHG_FUNC_MAX; i++) {
		if (manager->smart_chg->funcs[i].func_on) {
			smart_chg_node |= BIT_MASK(i);
		} else {
			smart_chg_node &= ~BIT_MASK(i);
		}
	}

	/* fill smart charge status bit: 0 -> success 1 -> error */
	smart_chg_node = ((smart_chg_node < 1) | !!manager->smart_chg->status);

	hq_info("get smart_chg = 0x%08X", smart_chg_node);

	return sprintf(buf, "%u\n", smart_chg_node);
}

static struct device_attribute smart_chg_attr =
		__ATTR(smart_chg, 0644, smart_chg_show, smart_chg_store);
#endif /* CONFIG_XM_SMART_CHG */

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
static ssize_t smart_batt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->batt_health))
		return PTR_ERR(manager->batt_health);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->batt_health->smart_batt = val;

	hq_info("set smart_batt =%d\n", manager->batt_health->smart_batt);

	return count;
}

static ssize_t smart_batt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->batt_health))
		return PTR_ERR(manager->batt_health);

	return sprintf(buf, "%d\n", manager->batt_health->smart_batt);
}

static struct device_attribute smart_batt_attr =
		__ATTR(smart_batt, 0644, smart_batt_show, smart_batt_store);

static ssize_t smart_fv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->batt_health))
		return PTR_ERR(manager->batt_health);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->batt_health->smart_fv = val;

	hq_info("set smart_fv = %d\n", manager->batt_health->smart_fv);

	return count;
}

static ssize_t smart_fv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->batt_health))
		return PTR_ERR(manager->batt_health);

	return sprintf(buf, "%d\n", manager->batt_health->smart_fv);
}

static struct device_attribute smart_fv_attr =
		__ATTR(smart_fv, 0644, smart_fv_show, smart_fv_store);

static ssize_t night_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	bool val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->batt_health))
		return PTR_ERR(manager->batt_health);

	if (kstrtobool(buf, &val))
		return -EINVAL;

	manager->batt_health->night_smart_charge_on = val;

	return count;
}

static ssize_t night_charging_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->batt_health))
		return PTR_ERR(manager->batt_health);

	return sprintf(buf, "%d\n", manager->batt_health->night_smart_charge_on);
}

static struct device_attribute night_charging_attr =
		__ATTR(night_charging, 0644, night_charging_show, night_charging_store);
#endif /* CONFIG_XM_BATTERY_HEALTH */

#if IS_ENABLED(CONFIG_BATT_VERIFY)
static ssize_t chip_ok_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool chip_ok_status = false;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			hq_err("manager->auth_dev is_err_or_null\n");
			goto out;
		}
	}
	chip_ok_status = !!manager->auth_dev->secret_ic;
out:
	return sprintf(buf, "%d\n", chip_ok_status);
}
static struct device_attribute chip_ok_attr =
	__ATTR(chip_ok, 0644, chip_ok_show, NULL);
#endif

static ssize_t batt_id_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int id = 0xFF;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}
	id = fuel_gauge_get_batt_id(manager->fuel_gauge);
out:
	return sprintf(buf, "%d\n", id);
}
static struct device_attribute batt_id_attr =
__ATTR(batt_id, 0444, batt_id_show, NULL);

static ssize_t batt_id_voltage_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int voltage_value = 0;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

	if (manager->board_version != EEA_VERSION) {
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
			if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
				hq_err("manager->fuel_gauge is_err_or_null\n");
				goto out;
			}
		}
		voltage_value = fuel_gauge_get_batt_id_voltage(manager->fuel_gauge);
	} else {
		voltage_value = -1;
	}
out:
	return sprintf(buf, "%d\n", voltage_value);
}
static struct device_attribute batt_id_voltage_attr =
__ATTR(batt_id_voltage, 0444, batt_id_voltage_show, NULL);

static ssize_t soh_sn_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	char *batt_sn = "UNKNOWN";
	char fg_batt_sn[64] = {0};
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			hq_err("manager->auth_dev is_err_or_null\n");
			goto out;
		}
	}

	batt_sn = manager->auth_dev->batt_sn;
#endif
#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	fuel_gauge_get_soh_sn(manager->fuel_gauge, fg_batt_sn);
	return sprintf(buf, "%s\n", fg_batt_sn);
#endif
out:
	return sprintf(buf, "%s\n", batt_sn);
}
static struct device_attribute soh_sn_attr =
__ATTR(soh_sn, 0444, soh_sn_show, NULL);

static ssize_t calc_rvalue_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	unsigned long calc_rvalue = 0;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;
#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}
	calc_rvalue = fuel_gauge_get_calc_rvalue(manager->fuel_gauge);
#endif
out:
	return sprintf(buf, "%d\n", calc_rvalue);
}
static struct device_attribute calc_rvalue_attr =
__ATTR(calc_rvalue, 0444, calc_rvalue_show, NULL);

#if IS_ENABLED(CONFIG_BATT_VERIFY)
static ssize_t soh20_aging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	bool val = false;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (val)
		schedule_delayed_work(&manager->batt_soh20_aging_test, 0);
	else
		cancel_delayed_work(&manager->batt_soh20_aging_test);

	return count;
}
static struct device_attribute soh20_aging_attr =
		__ATTR(soh20_aging, 0644, NULL, soh20_aging_store);
#endif

static ssize_t fg1_cycle_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	u32 cycle_count = 0;
	#if !IS_ENABLED(CONFIG_CHARGE_ARCH_BATT_AUTH)
	int ret = 0;
	union power_supply_propval pval;
	#endif

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

	if (manager->fake_batt_cycle != 0xFFFF) {
		cycle_count = manager->fake_batt_cycle;
		goto out;
	}

	#if IS_ENABLED(CONFIG_CHARGE_ARCH_BATT_AUTH)
	cycle_count = manager->batt_cycle;
	#else
	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		hq_err("failed to get cycle_count prop\n");
		goto out;
	}
	cycle_count = pval.intval;
	#endif

out:
	return sprintf(buf, "%d\n", cycle_count);
}
static struct device_attribute fg1_cycle_attr =
__ATTR(fg1_cycle, 0444, fg1_cycle_show, NULL);

static ssize_t manufacturing_date_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	char manufacturing_date[9] = {'2','0','2','0','0','1','0','1','\0'};

	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	charger_get_batt_manufacturing_date(manager, manufacturing_date);
#endif
#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
	charger_get_fg_manufacturing_date(manager, manufacturing_date);
#endif

out:
	return sprintf(buf, "%s\n", manufacturing_date);
}
static struct device_attribute manufacturing_date_attr =
__ATTR(manufacturing_date, 0444, manufacturing_date_show, NULL);

static ssize_t first_usage_date_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	int ret = 0;
	static char first_usage_date[9] = {'0','0','0','0','0','0','0','0','\0'};

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager)) {
		memcpy(first_usage_date,"99999999",8);
		goto out;
	}

	if (manager->is_usage_update) {
		hq_info("first_usage_date has update: %s\n", first_usage_date);
		return sprintf(buf, "%s\n", first_usage_date);
	}

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	ret = charger_get_batt_first_usage_date(manager, &first_usage_date[2]);
	if (ret != 0) {
		memcpy(first_usage_date,"99999999", 8);
		goto out;
	}
#endif
#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
	ret = charger_get_fg_first_usage_date(manager, first_usage_date);
#endif

	if (strncmp(&first_usage_date[2], "000000", 6)) //read date != 00000000, show date
		first_usage_date[0] = '2';

out:

	if ((strncmp(&first_usage_date[2], "000000", 6) != 0) && (strncmp(&first_usage_date[2], "999999", 6) != 0)) {
		manager->is_usage_update = true;
		hq_info("is_usage_update: %d\n", manager->is_usage_update);
	}

	return sprintf(buf, "%s\n", first_usage_date);
}

static ssize_t first_usage_date_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	char first_usage_date[6] = {0};

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	hq_info("first_usage_date:%s\n", buf);
	memcpy(first_usage_date, &buf[2], 6);

#if IS_ENABLED(CONFIG_BATT_VERIFY)
	charger_set_batt_first_usage_date(manager, first_usage_date);
#endif
#if IS_ENABLED(CONFIG_BQ_FUEL_GAUGE)
	charger_set_fg_first_usage_date(manager, buf, count);
#endif
	manager->is_usage_update = false;
	hq_info("is_usage_update: %d\n", manager->is_usage_update);
	return count;
}
static struct device_attribute first_usage_date_attr =
		__ATTR(first_usage_date, 0644, first_usage_date_show, first_usage_date_store);

#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
static ssize_t reverse_quick_charge_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	bool value = false;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtobool(buf, &value))
		return -EINVAL;

	manager->reverse_charge_policy->reverse_quick_charge_flag = value;

	hq_info("reverse_quick_charge:%d\n",
		manager->reverse_charge_policy->reverse_quick_charge_flag);

	return count;
}
static struct device_attribute reverse_quick_charge_attr =
		__ATTR(reverse_quick_charge, 0644, NULL, reverse_quick_charge_store);

static ssize_t revchg_bcl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	bool value = false;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtobool(buf, &value))
		return -EINVAL;

	manager->reverse_charge_policy->revchg_bcl_flag = value;

	hq_info("revchg_bcl_set:%d\n",
		manager->reverse_charge_policy->revchg_bcl_flag);

	reverse_charge_policy_run(manager);

	return count;
}
static struct device_attribute revchg_bcl_attr =
		__ATTR(revchg_bcl, 0644, NULL, revchg_bcl_store);

static ssize_t revchg_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	bool value = false;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtobool(buf, &value))
		return -EINVAL;

	hq_info("revchg_test_store:%d\n", value);

	if (!manager->charger || !manager->master_cp_chg) {
		hq_info("charger or master_cp_chg is null \n");
		return -EINVAL;
	}

	if (value) {
		hq_info("enable buck otg\n");
		charger_set_otg(manager->charger, true);
		mdelay(2000);
		hq_info("enable revchg\n");
		chargerpump_set_reverse_charge(manager->master_cp_chg, true);
		mdelay(2000);
		hq_info("disable buck otg\n");
		charger_set_otg(manager->charger, false);
	} else {
		hq_info("disable revchg\n");
		chargerpump_set_reverse_charge(manager->master_cp_chg, false);
	}

	return count;
}
static struct device_attribute revchg_test_attr =
		__ATTR(revchg_test, 0644, NULL, revchg_test_store);
#endif

static struct attribute *batt_psy_attrs[] = {
	&input_suspend_attr.attr,
	&shipmode_count_reset_attr.attr,
	&batt_id_attr.attr,
	&batt_id_voltage_attr.attr,
	&soh_sn_attr.attr,
	&soc_decimal_attr.attr,
	&soc_decimal_rate_attr.attr,
#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	&smart_chg_attr.attr,
#endif /* CONFIG_XM_SMART_CHG */
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	&smart_batt_attr.attr,
	&smart_fv_attr.attr,
	&night_charging_attr.attr,
#endif /* CONFIG_XM_BATTERY_HEALTH */
	&calc_rvalue_attr.attr,
#if IS_ENABLED(CONFIG_BATT_VERIFY)
	&soh20_aging_attr.attr,
	&chip_ok_attr.attr,
#endif
	&fg1_cycle_attr.attr,
	&manufacturing_date_attr.attr,
	&first_usage_date_attr.attr,
	&cc_toggle_attr.attr,
	&cid_status_attr.attr,
	&otg_ui_support_attr.attr,
	&det_cell_attr.attr,
	&auth_dev_batt_cycle_attr.attr,
	&product_name_attr.attr,
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	&lpd_charging_attr.attr,
#endif
#if IS_ENABLED(CONFIG_HQ_REVERSE_CHARGE_POLICY)
	&revchg_bcl_attr.attr,
	&reverse_quick_charge_attr.attr,
	&revchg_test_attr.attr,
#endif
	NULL,
};

static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};

int hq_batt_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->batt_psy->dev.kobj,
								&batt_psy_attrs_group);
}
EXPORT_SYMBOL(hq_batt_sysfs_create_group);

MODULE_DESCRIPTION("Huaqin Charger sysfs");
MODULE_LICENSE("GPL v2");
