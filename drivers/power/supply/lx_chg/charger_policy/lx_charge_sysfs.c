// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019 The Linux Foundation. All rights reserved.
 */

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_core.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_typec.h"
#endif
#include "../charger_class/lx_charger_class.h"
#include "../charger_class/lx_cp_class.h"
#include "../charger_class/lx_fg_class.h"
#include "lx_voter.h"
#include "lx_jeita.h"
#include "lx_cp_policy.h"

#include "lx_charger_manager.h"
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "lx_voter.h"
#endif
#include "lx_printk.h"
#include "lx_notify.h"

#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG_SYSFS]"
#endif

static bool is_mtbf_mode = false;

static const char *real_type_txt[] = {
	"None",
	"USB",
	"USB_CDP",
	"DCP",
	"USB_FLOAT",
	"Non-Stand",
	"USB_HVDCP",
	"USB_HVDCP_3",
	"USB_HVDCP_3P5",
	"USB_PD",
	"USB_PD_PPS",
};

static int battery_temp[5] = {-100, 150, 480, 520, 600};

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,    /* USB、DCP、CDP、Float */
	QUICK_CHARGE_FAST,          /* PD、QC2、QC3 */
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,         /* verified PD(apdo_max < 50W)、QC3.5、QC3-27W */
	QUICK_CHARGE_SUPER,         /* verified PD(apdo_max >= 50W) */
};

struct quick_charge {
	enum chg_type adap_type;
	enum quick_charge_type adap_cap;
};

static struct quick_charge quick_charge_map[10] = {
	{ VBUS_TYPE_SDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_DCP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_CDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_NON_STAND, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_PD, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP_3, QUICK_CHARGE_FLASH },
	{ VBUS_TYPE_HVDCP_3P5, QUICK_CHARGE_FLASH },
	{ VBUS_TYPE_PD_PPS, QUICK_CHARGE_TURBE },
	{ 0, 0 },
};
#endif

#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
static struct product_name_stru product_name_map[PRODUCT_NAME_MAP_MAX_INDEX] = {
	{"unkown", UNKOWN},
	{"tanzanite_eea", EEA},
};

static struct blocking_notifier_head charge_current_nb;

int charge_current_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&charge_current_nb, nb);
}
EXPORT_SYMBOL(charge_current_register_notifier);

int charge_current_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&charge_current_nb, nb);
}
EXPORT_SYMBOL(charge_current_unregister_notifier);

int charge_current_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&charge_current_nb, val, v);
}
EXPORT_SYMBOL(charge_current_notifier_call_chain);
#endif

static int charger_usb_get_property(struct power_supply *psy,
						enum power_supply_property psp,
						union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	bool online = false;
	enum chg_type chg_type;
	int ret = 0;
	int volt = 0;
	int curr = 0;

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			lx_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager->tcpc);
		}
	}

	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->charger)) {
		lx_err("manager charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		chg_type = manager->charger->real_type;
		if (chg_type == VBUS_TYPE_SDP || chg_type == VBUS_TYPE_CDP ||
			((chg_type == VBUS_TYPE_FLOAT || chg_type == VBUS_TYPE_NON_STAND) && manager->is_dr_swap))
		/***** when dongle(support pd) plug in, it would send DR_SWAP,
		dongle would connect pd and southchip chg_type would become VBUS_TYPE_FLOAT,
		dongle would connect pd and nulta chg_type would become VBUS_TYPE_NON_STAND,
		so we would set type as usb so that extcon_usb set phone_role as devices *****/
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = charger_get_online(manager->charger, &online);

		if (ret < 0)
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
			lx_err("Couldn't read input volt ret=%d\n", ret);
			return ret;
		}
		val->intval = volt;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = charger_manager_get_current(manager, &curr);
		if (ret < 0) {
			lx_err("Couldn't read input curr ret=%d\n", ret);
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
		lx_err("manager is_err_or_null\n");
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

static int get_battery_health(struct charger_manager *manager)
{
	union power_supply_propval pval;
	int battery_health = POWER_SUPPLY_HEALTH_GOOD;
	int ret = 0;

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lx_err("failed to get temp prop\n");
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

#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
static void charger_get_batt_manufacturing_date(struct charger_manager *manager, char *manufacturing_date)
{
	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			lx_err("manager->auth_dev is_err_or_null\n");
			return ;
		}
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
	lx_info("manufacturing_date:%s\n", manufacturing_date);
}

static int charger_get_batt_first_usage_date(struct charger_manager *manager, char* first_usage_date)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			lx_err(" manager->auth_dev is_err_or_null\n");
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
			lx_err("manager->auth_dev is_err_or_null\n");
			return;
		}
	}

	lx_info("first_usage_date:%s\n", first_usage_date);

	auth_device_set_first_usage_date(manager->auth_dev, first_usage_date, 6);

}
#endif

static int charger_batt_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	union power_supply_propval pval;
	int state = 0, status = 0;
	int warm_stop_charge = 0;
	int __maybe_unused bat_volt = 0;
	int ret = 0;
#if !IS_ENABLED(CONFIG_BQ_FUELGAUGE)
	bool pd_verifed = false;
#endif

	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (manager->fg_psy == NULL)
		manager->fg_psy = power_supply_get_by_name("bms");

	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		lx_err("failed to get bms psy\n");
		return PTR_ERR(manager->fg_psy);
	}

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				lx_err("failed to get chg status prop\n");
				break;
			}

			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
				lx_err("failed to get main_icl_votable\n");
				break;
			}
			warm_stop_charge = get_warm_stop_charge_state();
			if ((status != POWER_SUPPLY_STATUS_DISCHARGING) &&
					(!is_input_suspend(manager))) {
				if (manager->tbat >= BATTERY_HOT_TEMP)
					status = POWER_SUPPLY_STATUS_NOT_CHARGING;
				else if ((manager->tbat >= BATTERY_WARM_TEMP || warm_stop_charge))
					status = POWER_SUPPLY_STATUS_CHARGING;
				else if (((manager->tbat < BATTERY_WARM_TEMP)
							&& (manager->vbat < 4100)))
					status = POWER_SUPPLY_STATUS_CHARGING;
			}

#if 0//IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			if (manager->cp_policy == NULL) {
				lx_err("manager->cp_policy is NULL\n");
			} else {
				if ((manager->cp_policy->switch1_1_enable || manager->cp_policy->switch1_1_single_enable) &&
					(manager->cp_policy->state == POLICY_RUNNING)) {
					charger_get_online(manager->charger, &online);
					if (online)
						status = POWER_SUPPLY_STATUS_CHARGING;
				}
			}

#ifdef KERNEL_FACTORY_BUILD
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			if (ret < 0)
					lx_err("get battery volt error.\n");
			else
					manager->vbat = pval.intval/1000;
#else
			charger_get_otg_status(manager->charger, &otg_value);

			if(manager->usb_online || otg_value){
				ret = charger_get_adc(manager->charger, CHG_ADC_VBAT, &bat_volt);
				if (!bat_volt) {
					charger_adc_enable(manager->charger, true);
					ret = charger_get_adc(manager->charger, CHG_ADC_VBAT, &bat_volt);
				}
				if (ret < 0)
					lx_err("get battery volt error1.\n");
				else
					manager->vbat = bat_volt;

			}else{
				charger_adc_enable(manager->charger, false);
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0)
					lx_err("get battery volt error.\n");
				else
					manager->vbat = pval.intval/1000;
			}
#endif //KERNEL_FACTORY_BUILD

			// if((manager->smart_charge[SMART_CHG_NAVIGATION].active_status) && (status != POWER_SUPPLY_STATUS_CHARGING)){
			// 	status = POWER_SUPPLY_STATUS_CHARGING;
			// }
			if((manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status) && (status != POWER_SUPPLY_STATUS_CHARGING)){
				lx_err("smart_chg: endurance is working, set charging.\n");
				status = POWER_SUPPLY_STATUS_CHARGING;
			}
			else if(status == POWER_SUPPLY_STATUS_CHARGING){
				ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
				if (ret < 0)
					lx_err("get battery soc error.\n");
				else
					manager->soc = pval.intval;
				if (manager->tbat <= BATTERY_COLD_TEMP)
					status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
#endif //CONFIG_XIAOMI_SMART_CHG

#if IS_ENABLED(CONFIG_LIXUN_SOFT_ITERM_SUPPORT)
			if(manager->soft_term_status == POWER_SUPPLY_STATUS_FULL)
				status = POWER_SUPPLY_STATUS_FULL;
#endif //CONFIG_LIXUN_SOFT_ITERM_SUPPORT

			if (is_input_suspend(manager))
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			else
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
				lx_err("failed to get online prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				lx_err("failed to get chg type prop\n");
				break;
			}
			val->intval = state;
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
			ret = power_supply_get_property(manager->fg_psy,
					POWER_SUPPLY_PROP_CAPACITY, &pval);
			if (ret < 0) {
				lx_err("failed to get capaticy prop\n");
				break;
			} else
				val->intval = pval.intval;

			if (val->intval <= 1)
				val->intval = 1;

#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
			if (manager->product_name_index == EEA) {
				if ((manager->soc == 100) && (pval.intval == 100)
					&& (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING)) {
					manager->plug_in_soc100_flag = true;
					lx_info("uisoc is 100, need disable charge\n");
				} else if (pval.intval >= 99) {
					if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING) {
						val->intval = 99;
						lx_info("hold uisoc 99 until charge status full\n");
					} else if (manager->chg_status
								== POWER_SUPPLY_STATUS_FULL) {
						val->intval = 100;
						lx_info("need report 100 when charge status full\n");
					}
				}
			}
#endif

#if IS_ENABLED(CONFIG_LIXUN_FUEL_ALGORITHM)
			if (!IS_ERR_OR_NULL(manager->fuel_algo)) {
				#if IS_ENABLED(CONFIG_LIXUN_SOC_SMOOTH_SUPPORT)
				if (manager->fuel_algo->OptimalSoc >= 0)
					val->intval = manager->fuel_algo->OptimalSoc;
				#endif
			}
#endif
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			if (!IS_ERR_OR_NULL(manager->fg_psy)) {
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0) {
					lx_err("failed to get voltage-now prop\n");
					val->intval = 0;
				} else
					val->intval = pval.intval;
			}

			if (val->intval < 2000000) {
				lx_err("get vbat from fg_psy is abnormal(%d), try get vbat from charger\n", val->intval);
				charger_adc_enable(manager->charger, true);
				charger_get_adc(manager->charger, CHG_ADC_VBAT, &bat_volt);
				if (bat_volt < 2000)
					val->intval = 0;
				else
					val->intval = bat_volt * 1000;
			}
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			if (!IS_ERR_OR_NULL(manager->fuel_gauge))
				ret = fuel_gauge_get_fastcharge_mode(manager->fuel_gauge);
			if (ret)
				val->intval = FAST_CHG_VOLTAGE_MAX;
			else
				val->intval = NORMAL_CHG_VOLTAGE_MAX;
			#else
			#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
			if (IS_ERR_OR_NULL(manager->pd_adapter)) {
				lx_err("manager->xm_adapter->adapter_dev is_err_or_null\n");
				return PTR_ERR(manager->pd_adapter);
			}

			ret = adapter_get_usbpd_verifed(manager->pd_adapter, &pd_verifed);
			if (ret < 0){
				lx_err("Couldn't get usbpd verifed ret=%d\n", ret);
				return ret;
			}
			#endif
			if (manager->cp_policy == NULL) {
				val->intval = NORMAL_CHG_VOLTAGE_MAX;
			} else {
				if (manager->tbat > TEMP_LEVEL_15 && manager->tbat <= TEMP_LEVEL_45 &&
					pd_verifed && (manager->cp_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || manager->cp_policy->cp_charge_done))
					val->intval = FAST_CHG_VOLTAGE_MAX;
				else
					val->intval = NORMAL_CHG_VOLTAGE_MAX;
			}
			#endif
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			val->intval = manager->system_temp_level;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
			val->intval = manager->charge_control_limit_max;
			break;

		case POWER_SUPPLY_PROP_CURRENT_NOW:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
			if (ret < 0) {
				lx_err("failed to get current_now prop\n");
				break;
			}
			//Negative value represents charging, unit ma
			val->intval = -pval.intval;
			#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
			charge_current_notifier_call_chain(val->intval, NULL);
			#endif
			break;

		case POWER_SUPPLY_PROP_TEMP:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
			if (ret < 0) {
				lx_err("failed to get temp prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
			break;

		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			if (manager->fake_batt_cycle != 0xFFFF) {
				val->intval = manager->fake_batt_cycle;
				break;
			}

#if IS_ENABLED(CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT)
			val->intval = manager->batt_cycle;
#else
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
			if (ret < 0) {
				lx_err("failed to get cycle_count prop\n");
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
				lx_err("failed to get charge_full prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_MODEL_NAME:
			#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			val->strval = manager->model_name;
			#else
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_MODEL_NAME, &pval);
			if (ret < 0) {
				lx_err("failed to get model_name prop\n");
				break;
			}
			val->strval = pval.strval;
			#endif
			break;

		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
			if (ret < 0) {
				lx_err("failed to get charge_counter prop\n");
				break;
			}
			val->intval = pval.intval / 1000;		//mAh
			break;
		#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
		case POWER_SUPPLY_PROP_AUTHENTIC:
			if (IS_ERR_OR_NULL(manager->auth_dev)) {
				manager->auth_dev = get_batt_auth_by_name("secret_ic");
				if (IS_ERR_OR_NULL(manager->auth_dev)) {
					lx_err("manager->auth_dev is_err_or_null\n");
					val->intval = 0;
				}
			} else {
				val->intval = manager->auth_dev->auth_result;
			}
			break;
		#endif
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
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
			manager->system_temp_level = val->intval;
			lx_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			manager->system_temp_level = val->intval;
			lx_set_prop_system_temp_level(manager, CALL_THERMAL_DAEMON_VOTER);
			break;
		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			manager->fake_batt_cycle = val->intval;
			lx_info("set fake battery cycle count = %d\n", manager->fake_batt_cycle);
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
	#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
	POWER_SUPPLY_PROP_AUTHENTIC,
	#endif
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

static int charger_chg_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	union power_supply_propval pval;
	int ret = 0;

	if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(manager->usb_psy) || IS_ERR_OR_NULL(manager->batt_psy)) {
		lx_err("manager or usb psy or batt psy is_err_or_null\n");
		return -ENOMEM;
	}
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			ret = power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			if (ret < 0) {
				lx_err("get online status form usb psy fail\n");
				val->intval = 0;
				break;
			}
			val->intval = pval.intval;
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
			if (IS_ERR_OR_NULL(manager->fv_votable)) {
				lx_err("fv votable is_err_or_null\n");
				return -ENOMEM;
			}
			val->intval = get_effective_result(manager->fv_votable);
			break;
		case POWER_SUPPLY_PROP_STATUS:
#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
			if (manager->product_name_index != EEA) {
				ret = power_supply_get_property(manager->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &pval);
				if (ret < 0) {
					lx_err("get online status form batt psy fail\n");
					val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
					break;
				}
				val->intval = pval.intval;
			}
#else
			ret = power_supply_get_property(manager->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
			if (ret < 0) {
				lx_err("get online status form batt psy fail\n");
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
				break;
			}
			val->intval = pval.intval;
#endif
			break;
		case POWER_SUPPLY_PROP_USB_TYPE:
			if (ret < 0)
				lx_err("Couldn't get usb type ret=%d\n", ret);
			if (manager->charger->real_type == VBUS_TYPE_NONE)
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			else
				val->intval = POWER_SUPPLY_USB_TYPE_PD;
			break;
		case POWER_SUPPLY_PROP_MODEL_NAME:
			lx_info("vendor_supply_id = %d\n", manager->charger->vendor_supply_id);
			if (manager->charger->vendor_supply_id == FIRST_SUPPLY)
				val->strval = "sc6601";
			else if (manager->charger->vendor_supply_id == SECOND_SUPPLY)
				val->strval = "sy6976";
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
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static enum power_supply_usb_type lx_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static char *lx_charger_supplied_to[] = {
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
	.usb_types = lx_charger_usb_types,
	.num_usb_types = ARRAY_SIZE(lx_charger_usb_types),
};

static void charger_manager_from_psy(struct device *dev, struct charger_manager **manager)
{
	struct power_supply *psy;
	if (IS_ERR_OR_NULL(dev)) {
		lx_err("dev is_err_or_null\n");
		return;
	}

	psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(psy)) {
		lx_err("psy is_err_or_null\n");
		return;
	}

	*manager = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(*manager)) {
		lx_err("manager is_err_or_null\n");
		return;
	}
}
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
void xm_uevent_report(struct charger_manager *manager);
#endif
static ssize_t real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->charger)) {
		lx_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager);
	}

	return sprintf(buf, "%s\n", real_type_txt[manager->charger->real_type]);
}

static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

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
		lx_err("manager->master_cp_chg is_err_or_null\n");
		return PTR_ERR(manager->master_cp_chg);
	}

	ret = chargerpump_get_adc_value(manager->master_cp_chg, CP_ADC_IBUS, &cp_icl);
	if (ret < 0){
		lx_err("Couldn't get cp_ibus ret=%d\n", ret);
		cp_icl = 0;
	}

	return sprintf(buf, "%d\n", cp_icl);
}

static struct device_attribute cp_icl_attr =
	__ATTR(cp_icl, 0644, cp_icl_show, NULL);

static ssize_t det_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev,&manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	lx_info("det_cell : %d\n",manager->en_single_cell);

	return sprintf(buf, "%d\n", manager->en_single_cell);
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
			lx_err("manager->tcpc is_err_or_null\n");
			return;
		}
	}

	manager->ui_cc_toggle = en;

	if(!manager->typec_attach && en)
	{
		lx_info("typec is not attached set cc toggle\n");
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_DRP);
	} else if (!manager->typec_attach && !en) {
		lx_info("set cc not toggle\n");
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_SNK);
	} else {
		lx_info("typec is attached, not set cc toggle\n");
	}

	if(en && !manager->cid_status)
	{
		ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
		if (ret < 0) {
			lx_err("callback was running, skip timer\n");
			return;
		}
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		end_time.tv_sec = time_now.tv_sec + 600;
		end_time.tv_nsec = time_now.tv_nsec + 0;
		ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

		lx_info("alarm timer start:%d, %lld %ld\n", ret,
			end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&manager->rust_det_work_timer, ktime);
	}else{
		ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
		if (ret < 0) {
			lx_err("callback was running, skip timer\n");
			return;
		}
		lx_info("ui disable cc toggle : stop hrtimer\n");
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

	lx_info("otg_ui_support : %d\n",manager->en_floatgnd);

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

	lx_info("cid_status : %d\n",manager->cid_status);

	return sprintf(buf, "%d\n", manager->cid_status);
}

static struct device_attribute cid_status_attr =
	__ATTR(cid_status, 0444, cid_status_show, NULL);


static ssize_t cc_toggle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val = 0;

	lx_info("cc_toggle_store start\n");
	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manual_set_cc_toggle(manager,!!val);

	lx_info("cc_toggle_store end\n");
	return count;
}

static ssize_t cc_toggle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	lx_info("cc_toggle: %d\n", !!manager->ui_cc_toggle);

	return sprintf(buf, "%d\n", (!!manager->ui_cc_toggle));
}
static struct device_attribute cc_toggle_attr =
	__ATTR(cc_toggle, 0644, cc_toggle_show, cc_toggle_store);


static ssize_t auth_dev_batt_cycle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val = 0;
	u32 auth_cycle_count = 0;
	int ret = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		lx_err("failed to get battery auth device\n");
		return PTR_ERR(manager->auth_dev);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	ret = auth_device_get_cycle_count(manager->auth_dev, &auth_cycle_count);
	if (ret != 0) {
		lx_err("read auth cycle count error\n");
		return -EINVAL;
	}
	mdelay(20);

	ret = auth_device_set_cycle_count(manager->auth_dev, val, auth_cycle_count);
	if (ret != 0) {
		lx_err("write auth cycle count error\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t auth_dev_batt_cycle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	u32 auth_cycle_count = 0;
	int ret = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		lx_err("failed to get battery auth device\n");
		return PTR_ERR(manager->auth_dev);
	}

	ret = auth_device_get_cycle_count(manager->auth_dev, &auth_cycle_count);
	if (ret != 0) {
		lx_err("read auth cycle count error\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", auth_cycle_count);
}
static struct device_attribute auth_dev_batt_cycle_attr =
	__ATTR(auth_dev_batt_cycle, 0644,auth_dev_batt_cycle_show, auth_dev_batt_cycle_store);


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
			lx_err("manager->tcpc is_err_or_null\n");
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
		lx_err("can not get otg status\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", otg_value);
}
static struct device_attribute usb_otg_attr =
	__ATTR(usb_otg, 0644, usb_otg_show, NULL);

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
static int get_apdo_max(struct charger_manager *manager) {
	int apdo_max = 0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	if (manager->pd_active != CHARGE_PD_PPS_ACTIVE) {
		goto done;
	}

	apdo_max = manager->cp_policy->cap.volt_max[manager->cp_policy->cap_nr] *
		manager->cp_policy->cap.curr_max[manager->cp_policy->cap_nr] / 1000000;

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

	if (manager->apdo_max >= manager->input_power_over)
		manager->apdo_max = manager->input_power_over;

	lx_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);

}
static struct device_attribute power_max_attr =
	__ATTR(power_max, 0644, power_max_show, NULL);
static struct device_attribute apdo_max_attr =
	__ATTR(apdo_max, 0644, power_max_show, NULL);

static int quick_charge_type(struct charger_manager *manager)
{
	enum quick_charge_type quick_charge_type = QUICK_CHARGE_NORMAL;
	union power_supply_propval pval = {0, };
	int ret = 0;
	int i = 0;
	int tbat = 250;

	if (IS_ERR_OR_NULL(manager->charger)) {
		lx_err("manager->charger is_err_or_null\n");
		goto out;
	}

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		lx_err("manager->charger is_err_or_null\n");
		goto out;
	}

	if (IS_ERR_OR_NULL(manager->usb_psy)) {
		lx_err("manager->charger is_err_or_null\n");
		goto out;
	}

	ret = power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		lx_err("Couldn't get usb online ret=%d\n", ret);
		goto out;
	}

	if (!(pval.intval))
		goto out;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lx_err("Couldn't get bat temp ret=%d\n", ret);
		goto out;
	}
	tbat = pval.intval;

	for (i=0; quick_charge_map[i].adap_type != 0; i++) {
		if (manager->charger->real_type == quick_charge_map[i].adap_type) {
			quick_charge_type = quick_charge_map[i].adap_cap;
			break;
		}
	}

	if(tbat >= BATTERY_WARM_TEMP){
		quick_charge_type = QUICK_CHARGE_NORMAL;
	}
out:
	lx_debug("quick_charge_type = %d\n", quick_charge_type);
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
#endif

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
			lx_err("manager->tcpc is_err_or_null\n");
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

	lx_info("mtbf_mode_store start\n");

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val != 0) {
		is_mtbf_mode = 1;
		lx_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		lx_info("is_mtbf_mode = 0\n");
	}
	return count;
}

static ssize_t mtbf_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_mtbf_mode);
}

static struct device_attribute mtbf_mode_attr = __ATTR(mtbf_mode,0644,mtbf_mode_show,mtbf_mode_store);

bool is_mtbf_mode_func(void)
{
	return is_mtbf_mode;
}
EXPORT_SYMBOL_GPL(is_mtbf_mode_func);

static struct attribute *usb_psy_attrs[] = {
	&real_type_attr.attr,
	&typec_cc_orientation_attr.attr,
	&usb_otg_attr.attr,
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&power_max_attr.attr,
	&apdo_max_attr.attr,
	&quick_charge_type_attr.attr,
#endif
	&typec_mode_attr.attr,
	&mtbf_mode_attr.attr,
	&cp_icl_attr.attr,
	NULL,
};

static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};


static ssize_t input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "Err\n");

	return sprintf(buf, "%d\n", is_input_suspend(manager));
}
static ssize_t input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
		lx_info("main_icl_votable not found\n");
		return PTR_ERR(manager->main_icl_votable);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val) {
		vote(manager->main_icl_votable, USER_SUSPEND_VOTER, true, 0);
	} else {
		vote(manager->main_icl_votable, USER_SUSPEND_VOTER, false, 0);
	}
	lxchg_psy_updata(LXCHG_DEFAULT_EVENT);
	lx_info("input_suspend = %d\n", val);
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

	return sprintf(buf, "%d\n", manager->shipmode);
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

	manager->shipmode = val;
	lx_info("shipmode = %d\n", manager->shipmode);

	return count;
}
static struct device_attribute shipmode_count_reset_attr =
	__ATTR(shipmode_count_reset, 0644, shipmode_count_reset_show, shipmode_count_reset_store);

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
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
			lx_err("manager->fuel_gauge is_err_or_null\n");
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
			lx_err("manager->fuel_gauge is_err_or_null\n");
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

	char quick_charge_string[64];
	char soc_decimal_string[64];
	char soc_decimal_string_rate[64];

	char *envp[] = {
		quick_charge_string,
		soc_decimal_string,
		soc_decimal_string_rate,
		NULL,
	};

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("manager->fuel_gauge is_err_or_null\n");
			return;
		}
	}

	sprintf(quick_charge_string, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", quick_charge_type(manager));

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;
	sprintf(soc_decimal_string, "POWER_SUPPLY_SOC_DECIMAL=%d", soc_decimal);

	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(manager->fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;
	sprintf(soc_decimal_string_rate, "POWER_SUPPLY_SOC_DECIMAL_RATE=%d", soc_decimal_rate);

	kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);

	lx_debug("envp[0]:%s envp[1]:%s envp[2]:%s",envp[0],envp[1],envp[2]);
}
EXPORT_SYMBOL(xm_uevent_report);
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static ssize_t smart_chg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	// DECLEAR_BITMAP(func_type, SMART_CHG_FEATURE_MAX_NUM);
	int val;
	bool en_ret;
	unsigned long func_type;
	int func_val;
	int bit_pos;
	int all_func_status;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	if (kstrtoint(buf, 0, &val))
			return -EINVAL;

	en_ret = val & 0x1;
	func_type = (val & 0xFFFE) >> 1;
	func_val = val >> 16;

	lx_info("get val:%#X, func_type:%#X, en_ret:%d, func_val:%d\n",
			val, func_type, en_ret, func_val);

	bit_pos = find_first_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM);

	if(bit_pos == SMART_CHG_FEATURE_MAX_NUM || find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1) != SMART_CHG_FEATURE_MAX_NUM){
		lx_info("ERROR: zero or more than one func type!\n");
		lx_info("find_next_bit = %d, bit_pos = %d\n",
		find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1), bit_pos);
		set_error(manager);
	} else
		set_success(manager);

	// if func_type bit0 is 1, bit_pos = 0, not 1. so ++bit_pos.
	if(!smart_chg_is_error(manager))
		handle_smart_chg_functype(manager, ++bit_pos, en_ret, func_val);

	/* update smart_chg[0] status */
	all_func_status = handle_smart_chg_functype_status(manager);
	manager->smart_chg_cmd = all_func_status;
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = all_func_status & 0x1;
	manager->smart_charge[SMART_CHG_STATUS_FLAG].active_status = (all_func_status & 0xFFFE) >> 1;

   return count;
}

static ssize_t smart_chg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_chg_cmd);
}

static struct device_attribute smart_chg_attr =
		__ATTR(smart_chg, 0644, smart_chg_show, smart_chg_store);

static ssize_t smart_batt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
        int val = 0;

        charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->smart_batt = val;
	lx_info("smart_batt = %d\n", manager->smart_batt);
	if (IS_ERR_OR_NULL(manager->fv_votable)) {
		lx_err("failed to get fv_votable\n");
	} else {
		rerun_election(manager->fv_votable);
	}
        return count;
}

static ssize_t smart_batt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_batt);
}

static struct device_attribute smart_batt_attr =
		__ATTR(smart_batt, 0644, smart_batt_show, smart_batt_store);

static ssize_t smart_fv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
        int val = 0;
        struct votable	*fv_votable = NULL;

        charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->smart_fv = val;
	lx_info("smart_fv = %d\n", manager->smart_fv);
	fv_votable = find_votable("MAIN_FV");
	if (!fv_votable) {
		lx_info("failed to get fv_votable\n");
	}else{
		rerun_election(fv_votable);
	}
        return count;
}

static ssize_t smart_fv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_fv);
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

	if (kstrtobool(buf, &val))
		return -EINVAL;

        manager->night_charging = val;

        return count;
}

static ssize_t night_charging_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->night_charging);
}

static struct device_attribute night_charging_attr =
		__ATTR(night_charging, 0644, night_charging_show, night_charging_store);
#endif

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
			lx_err("manager->auth_dev is_err_or_null\n");
			goto out;
		}
	}

	chip_ok_status = !!manager->auth_dev->secret_ic;
out:
	return sprintf(buf, "%d\n", chip_ok_status);
}
static struct device_attribute chip_ok_attr =
	__ATTR(chip_ok, 0644, chip_ok_show, NULL);

static ssize_t batt_id_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int id = 0xFF;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			lx_err("manager->auth_dev is_err_or_null\n");
			goto out;
		}
	}

	id = manager->auth_dev->battery_id;
out:
	return sprintf(buf, "%d\n", id);
}
static struct device_attribute batt_id_attr =
__ATTR(batt_id, 0444, batt_id_show, NULL);

static ssize_t soh_sn_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	char *batt_sn = "UNKNOWN";
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

	if (IS_ERR_OR_NULL(manager->auth_dev)) {
		manager->auth_dev = get_batt_auth_by_name("secret_ic");
		if (IS_ERR_OR_NULL(manager->auth_dev)) {
			lx_err("manager->auth_dev is_err_or_null\n");
			goto out;
		}
	}

	batt_sn = manager->auth_dev->batt_sn;
out:
	return sprintf(buf, "%s\n", batt_sn);
}
static struct device_attribute soh_sn_attr =
__ATTR(soh_sn, 0444, soh_sn_show, NULL);

#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
static ssize_t calc_rvalue_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int calc_rvalue = 0;
	return sprintf(buf, "%d\n", calc_rvalue);
}
static struct device_attribute calc_rvalue_attr =
__ATTR(calc_rvalue, 0444, calc_rvalue_show, NULL);

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

#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
static ssize_t fg1_cycle_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	u32 cycle_count = 0;
	#if !IS_ENABLED(CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT)
	int ret = 0;
	union power_supply_propval pval;
	#endif

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;

	#if IS_ENABLED(CONFIG_LIXUN_USE_AUTH_CYCLE_COUNT)
	cycle_count = manager->batt_cycle;
	#else
	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		lx_err("failed to get cycle_count prop\n");
		goto out;
	}
	cycle_count = pval->intval;
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

	charger_get_batt_manufacturing_date(manager, manufacturing_date);

out:
	return sprintf(buf, "%s\n", manufacturing_date);
}
static struct device_attribute manufacturing_date_attr =
__ATTR(manufacturing_date, 0444, manufacturing_date_show, NULL);

static ssize_t first_usage_date_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	int ret = 0;
	char first_usage_date[9] = {'0','0','0','0','0','0','0','0','\0'};

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager)) {
		memcpy(first_usage_date,"99999999",8);
		goto out;
	}

	ret = charger_get_batt_first_usage_date(manager, &first_usage_date[2]);
	if (ret != 0) {
		memcpy(first_usage_date,"99999999", 8);
		goto out;
	}

	if (strncmp(&first_usage_date[2], "000000", 6)) //read date != 00000000, show date
		first_usage_date[0] = '2';

out:
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

	lx_info("first_usage_date:%s\n", buf);
	memcpy(first_usage_date, &buf[2], 6);

	charger_set_batt_first_usage_date(manager, first_usage_date);

	return count;
}

static struct device_attribute first_usage_date_attr =
		__ATTR(first_usage_date, 0644, first_usage_date_show, first_usage_date_store);

static ssize_t product_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%s\n", "unkown");

	return sprintf(buf, "%s\n", manager->product_name);
}

static ssize_t product_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int i = 0;
	int ret;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (count >= 64) {
		lx_info("set product name error\n");
		memcpy(manager->product_name,"unkown\0", 7);
		return -1;
	}

	memcpy(manager->product_name, buf, count);
	manager->product_name[count] = '\0';

	for(i = 0 ; i < PRODUCT_NAME_MAP_MAX_INDEX; i++) {
		ret = strcmp(manager->product_name, product_name_map[i].product_name);
		if (!ret)
			break;
	}

	if (i == PRODUCT_NAME_MAP_MAX_INDEX)
		manager->product_name_index = product_name_map[0].index;
	else
		manager->product_name_index = product_name_map[i].index;

	lx_info("product name length=%d, product_name_index=%d\n",
		count, manager->product_name_index);

	return count;
}
static struct device_attribute product_name_attr =
		__ATTR(product_name, 0644, product_name_show, product_name_store);
#endif

static ssize_t disable_chg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "manager is err or null !!\n", 0);
	else
		return sprintf(buf, "%d\n", is_disable_chg_by_client(manager, DISABLE_CHARGE_VOTER));
}

static ssize_t disable_chg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val) {
		disable_chg_comm_ctrl(manager, DISABLE_CHARGE_VOTER, true);
	} else {
		disable_chg_comm_ctrl(manager, DISABLE_CHARGE_VOTER, false);
	}
	lxchg_psy_updata(LXCHG_DEFAULT_EVENT);
	lx_info("charge_disable = %d\n", val);
	return count;
}
static struct device_attribute disable_chg_attr =
	__ATTR(disable_chg, 0644, disable_chg_show, disable_chg_store);


static ssize_t authentic_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	manager->authentic = 1;

	return sprintf(buf, "%d\n", manager->authentic);
}

static ssize_t authentic_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->authentic = 1;
	lx_info("authentic = %d\n", manager->authentic);

	return count;
}
static struct device_attribute authentic_attr =
	__ATTR(authentic, 0644, authentic_show, authentic_store);

#ifdef FACTORY_BUILD
static ssize_t ato_soc_user_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->ato_soc_user_control);
}

static ssize_t ato_soc_user_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->ato_soc_user_control = val;
	lxchg_psy_updata(LXCHG_DEFAULT_EVENT);
	lx_info("ato_soc_user_control = %d\n", manager->ato_soc_user_control);

	return count;
}
static struct device_attribute ato_soc_user_control_attr =
	__ATTR(ato_soc_user_control, 0644, ato_soc_user_control_show, ato_soc_user_control_store);
#endif

static ssize_t reverse_quick_charge_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->reverse_quick_charge_enabled);
}

static ssize_t reverse_quick_charge_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	lx_info("ato_soc_user_control = %d\n", val);
	manager->reverse_quick_charge_enabled = val;
	if (val)
		schedule_delayed_work(&manager->reverse_quick_charge_work, 0);

	return count;
}
static struct device_attribute reverse_quick_charge_attr =
	__ATTR(reverse_quick_charge, 0644, reverse_quick_charge_show, reverse_quick_charge_store);


static struct attribute *batt_psy_attrs[] = {
	&input_suspend_attr.attr,
	&shipmode_count_reset_attr.attr,
	&chip_ok_attr.attr,
	&batt_id_attr.attr,
	&soh_sn_attr.attr,
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&soc_decimal_attr.attr,
	&soc_decimal_rate_attr.attr,
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	&smart_chg_attr.attr,
	&smart_batt_attr.attr,
	&night_charging_attr.attr,
	&smart_fv_attr.attr,
#endif
#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
	&calc_rvalue_attr.attr,
	&soh20_aging_attr.attr,
#endif
#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
	&fg1_cycle_attr.attr,
	&manufacturing_date_attr.attr,
	&first_usage_date_attr.attr,
	&product_name_attr.attr,
#endif
	&cc_toggle_attr.attr,
	&cid_status_attr.attr,
	&otg_ui_support_attr.attr,
	&det_cell_attr.attr,
	&auth_dev_batt_cycle_attr.attr,
	&disable_chg_attr.attr,
	&authentic_attr.attr,
#ifdef FACTORY_BUILD
	&ato_soc_user_control_attr.attr,
#endif
	&reverse_quick_charge_attr.attr,
	NULL,
};

static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};

int cm_usb_psy_register(struct charger_manager *manager)
{
	struct power_supply_config usb_psy_cfg = { .drv_data = manager,};

	memcpy(&manager->usb_psy_desc, &usb_psy_desc, sizeof(manager->usb_psy_desc));

	manager->usb_psy = devm_power_supply_register(manager->dev, &manager->usb_psy_desc,
							&usb_psy_cfg);
	if (IS_ERR(manager->usb_psy)) {
		lx_err("usb psy register failed\n");
		return PTR_ERR(manager->usb_psy);
	}
	sysfs_create_group(&manager->usb_psy->dev.kobj,	&usb_psy_attrs_group);
	lx_info("usb psy register success\n");
	return 0;
}
EXPORT_SYMBOL(cm_usb_psy_register);

int cm_battery_psy_register(struct charger_manager *manager)
{
	struct power_supply_config batt_psy_cfg = { .drv_data = manager,};
	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	manager->batt_psy = devm_power_supply_register(manager->dev, &batt_psy_desc,
							&batt_psy_cfg);
	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		lx_err("batt psy register failed\n");
		return PTR_ERR(manager->batt_psy);
	}
	sysfs_create_group(&manager->batt_psy->dev.kobj, &batt_psy_attrs_group);
	lx_info("batt psy register success\n");

	return 0;
}
EXPORT_SYMBOL(cm_battery_psy_register);

int cm_charger_psy_register(struct charger_manager *manager)
{
	struct power_supply_config chg_psy_cfg = { .drv_data = manager,};
	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}
	chg_psy_cfg.supplied_to = lx_charger_supplied_to;
	chg_psy_cfg.num_supplicants = ARRAY_SIZE(lx_charger_supplied_to);

	manager->chg_psy = devm_power_supply_register(manager->dev, &chg_psy_desc,
							&chg_psy_cfg);
	if (IS_ERR_OR_NULL(manager->chg_psy)) {
		lx_err("chg psy register failed\n");
		return PTR_ERR(manager->chg_psy);
	}
	lx_info("charger psy register success\n");
	return 0;
}
EXPORT_SYMBOL(cm_charger_psy_register);


MODULE_DESCRIPTION("LiXun Charger sysfs");
MODULE_LICENSE("GPL v2");
