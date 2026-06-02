// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019 The Linux Foundation. All rights reserved.
 */

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "../../../misc/lx_typec/tcpc/inc/tcpm.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_core.h"
#include "../../../misc/lx_typec/tcpc/inc/tcpci_typec.h"
#endif
#include "lxchg_class.h"
#include "lxbat_auth_class.h"

#include "lxchg_voter.h"
#include "lxchg_jeita.h"
#include "lx_cp_policy.h"

#include "lxchg_manager.h"
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "lxchg_voter.h"
#endif
#include "lxchg_printk.h"
#include "lxchg_notify.h"

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
#ifdef FACTORY_BUILD
	"USB_PD",
	"USB_PD_PPS",
	"USB_MI_PPS",
#else
	"USB_PD",
	"USB_PD",
	"USB_PD",
#endif
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

static struct quick_charge quick_charge_map[] = {
	{ VBUS_TYPE_SDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_DCP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_CDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_NON_STAND, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_PD, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_HVDCP_3, QUICK_CHARGE_FLASH },
	{ VBUS_TYPE_HVDCP_3P5, QUICK_CHARGE_FLASH },
	{ VBUS_TYPE_PD_PPS, QUICK_CHARGE_FAST },
	{ VBUS_TYPE_MI_PPS, QUICK_CHARGE_TURBE },
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

static int lxchg_usb_get_property(struct power_supply *psy,
						enum power_supply_property psp,
						union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	int ret = 0;

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
		charger_get_vbus_type(manager->charger, &manager->charger->bc12_type);

		if (manager->charger->bc12_type == VBUS_TYPE_SDP || manager->charger->bc12_type == VBUS_TYPE_CDP)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;

		if (manager->pd_active == CHARGE_PD_INVALID) {
			manager->real_type = manager->charger->bc12_type;
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = manager->charger_online;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = VOLTAGE_MAX;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = charger_get_adc(manager->charger, CHG_ADC_VBUS, &manager->vbus);
		if (ret < 0) {
			lx_err("Couldn't read input volt ret=%d\n", ret);
			return ret;
		}
		val->intval = manager->vbus;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = charger_manager_get_current(manager, &manager->ibus);
		if (ret < 0) {
			lx_err("Couldn't read input curr ret=%d\n", ret);
			return ret;
		}
		val->intval = manager->ibus;
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

static int lxchg_usb_set_property(struct power_supply *psy,
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
	.get_property = lxchg_usb_get_property,
	.set_property = lxchg_usb_set_property,
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

static int lxchg_battery_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	union power_supply_propval pval;
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
			val->intval = manager->chg_status;
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

		case POWER_SUPPLY_PROP_CAPACITY:
			if (manager->fake_soc != 0xFFFF) {
				val->intval = manager->fake_soc;
				manager->uisoc = val->intval;
				break;
			}
			val->intval = manager->uisoc;
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			charger_get_adc(manager->charger, CHG_ADC_VBAT, &bat_volt);
			if (bat_volt < 2000) {
				lx_err("get vbat err from charger, try get from fg\n");
				if (!IS_ERR_OR_NULL(manager->fg_psy)) {
					ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
					if (ret < 0) {
						lx_err("failed to get voltage-now prop\n");
						val->intval = 0;
					} else
						val->intval = pval.intval;
				}
			} else {
				val->intval = bat_volt * 1000;
			}
			manager->vbat = val->intval / 1000;
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
			val->intval = manager->ffc_thermal_levels;
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
			manager->ibat = val->intval;
			break;

		case POWER_SUPPLY_PROP_TEMP:
			if (manager->fake_tbat != 0xFFFF) {
				val->intval = manager->fake_tbat;
				manager->tbat = val->intval;
				break;
			}
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
			if (ret < 0) {
				lx_err("failed to get temp prop\n");
				val->intval = 250;
				break;
			}
			val->intval = pval.intval;
			val->intval -= manager->temp_compensate*10;
			manager->tbat = val->intval;
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
			break;

		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			if (manager->fake_batt_cycle != 0xFFFF) {
				val->intval = manager->fake_batt_cycle;
				manager->batt_cycle = val->intval;
				break;
			}
			val->intval = manager->batt_cycle;
			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &pval);
			if (ret < 0) {
				lx_err("failed to get charge_full prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
			if (ret < 0) {
				lx_err("failed to get charge_full prop\n");
				break;
			}
			val->intval = pval.intval;
			manager->charge_full_car = val->intval;
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
		default:
			break;
	}
	return 0;
}

static int lxchg_battery_set_property(struct power_supply *psy,
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
			manager->batt_cycle = manager->fake_batt_cycle = val->intval;
			lx_info("set fake battery cycle count = %d\n", manager->fake_batt_cycle);
			break;
		case POWER_SUPPLY_PROP_TEMP:
			manager->tbat = manager->fake_tbat = val->intval;
			lx_info("set fake tbat = %d\n", manager->fake_tbat);
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			manager->uisoc = manager->fake_soc = val->intval;
			lx_info("set fake soc = %d\n", manager->fake_soc);
			break;
		default:
			break;
	}
	lxchg_psy_updata(POWER_SUPPLY_CHANGED);
	return 0;
}

static int batt_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		case POWER_SUPPLY_PROP_CYCLE_COUNT:
		case POWER_SUPPLY_PROP_TEMP:
		case POWER_SUPPLY_PROP_CAPACITY:
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
	.get_property = lxchg_battery_get_property,
	.set_property = lxchg_battery_set_property,
	.property_is_writeable = batt_prop_is_writeable,
};

static int lxchg_charger_get_property(struct power_supply *psy,
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
			ret = power_supply_get_property(manager->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
			if (ret < 0) {
				lx_err("get online status form batt psy fail\n");
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
				break;
			}
			val->intval = pval.intval;
			break;
		case POWER_SUPPLY_PROP_USB_TYPE:
			if (ret < 0)
				lx_err("Couldn't get usb type ret=%d\n", ret);
			if (manager->charger->bc12_type == VBUS_TYPE_NONE)
				val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			else
				val->intval = POWER_SUPPLY_USB_TYPE_PD;
			break;
		case POWER_SUPPLY_PROP_MODEL_NAME:
			lx_info("vendor_supply_id = %d\n", manager->charger->vendor_supply_id);
			if (manager->charger->vendor_supply_id == FIRST_SUPPLY)
				val->strval = "sc6601";
			else if (manager->charger->vendor_supply_id == SECOND_SUPPLY)
				val->strval = "nu6601";
			break;
		default:
			break;
	}
	return 0;
}

static int lxchg_charger_set_property(struct power_supply *psy,
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
	.get_property = lxchg_charger_get_property,
	.set_property = lxchg_charger_set_property,
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
void xm_uevent_report(struct charger_manager *manager);

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
	xm_uevent_report(manager);
	return sprintf(buf, "%s\n", real_type_txt[manager->real_type]);
}

static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

static ssize_t pd_authentication_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->pd_adapter->verifed);
}

static struct device_attribute pd_authentication_attr =
	__ATTR(pd_authentication, 0644, pd_authentication_show, NULL);

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

	if (!manager->typec_attach) {
		lx_info("typec is not attached,%s cc toggle\n", en ? "set" : "not set");
		charger_soft_cid_set_toggle(manager->charger, en ? true : false);
	} else {
		lx_info("typec is attached, not set cc\n");
	}

	if(en && !manager->typec_attach)
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
	} else {
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

	lx_info("otg_ui_support : %d\n",manager->support_ui_otg);

	return sprintf(buf, "%d\n", manager->support_ui_otg);
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

	lx_info("cid_status : %d\n",manager->typec_attach);

	return sprintf(buf, "%d\n", manager->typec_attach);
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
	lxchg_psy_updata(BATTERY_EVENT_CYCLE_CHANGED);
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

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (!manager->typec_attach)
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
		if (manager->real_type == quick_charge_map[i].adap_type) {
			quick_charge_type = quick_charge_map[i].adap_cap;
			break;
		}
	}

	if (tbat >= BATTERY_WARM_TEMP) {
		quick_charge_type = QUICK_CHARGE_NORMAL;
	}
out:
	lx_info("quick_charge_type = %d\n", quick_charge_type);
	return quick_charge_type;
}

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

	lx_info("envp[0]:%s envp[1]:%s envp[2]:%s",envp[0],envp[1],envp[2]);
}

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
	int apdo = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	apdo = get_apdo_max(manager);

	if (manager->apdo_max < apdo)
		manager->apdo_max = apdo;

	if (manager->apdo_max >= manager->input_power_over)
		manager->apdo_max = manager->input_power_over;

	lx_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);

}

static ssize_t apdo_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	int apdo = 0;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	apdo = get_apdo_max(manager);

	if (manager->apdo_max < apdo)
		manager->apdo_max = apdo;

	if (manager->apdo_max >= manager->input_power_over)
		manager->apdo_max = manager->input_power_over;

	lx_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);
}

static struct device_attribute power_max_attr =
	__ATTR(power_max, 0644, power_max_show, NULL);
static struct device_attribute apdo_max_attr =
	__ATTR(apdo_max, 0644, apdo_max_show, NULL);

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

static const char *vbus_type_txt[] = {
	"None", "SDP", "CDP", "DCP", "FLOAT",
	"Non-Stand", "QC", "QC3", "QC3+",
	"PD", "PPS", "MI_PPS",
};

static ssize_t misc_string_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%s,%s,%s,%s,%s,%s\n", vbus_type_txt[manager->real_type], manager->total_effect_voter,
		manager->icharge_effect_voter, manager->input_effect_voter, manager->fv_effect_voter, manager->iterm_effect_voter);
}

static struct device_attribute misc_string_attr = __ATTR(misc_string,0644,misc_string_show,NULL);

static ssize_t misc_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		manager->bootmode,manager->fv, manager->ichg, manager->thermal_board_temp,
		manager->ilimit, manager->hiz_en, manager->vote_fv, manager->vote_icharge,
		manager->vote_input_limit, manager->vote_iterm, manager->vote_total_fcc, manager->uisoc,
		manager->rsoc, manager->apdo_max, manager->smart_batt, manager->charge_full_car,
		manager->auth_dev->battery_id);
}

static struct device_attribute misc_data_attr = __ATTR(misc_data,0644,misc_data_show,NULL);

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
	&pd_authentication_attr.attr,
	&misc_string_attr.attr,
	&misc_data_attr.attr,
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

	if (IS_ERR_OR_NULL(manager->input_limit_votable)) {
		lx_info("input_limit_votable not found\n");
		return PTR_ERR(manager->input_limit_votable);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val) {
		vote(manager->input_limit_votable, USER_SUSPEND_VOTER, true, 0);
	} else {
		vote(manager->input_limit_votable, USER_SUSPEND_VOTER, false, 0);
	}
	lxchg_psy_updata(POWER_SUPPLY_CHANGED);
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
	fv_votable = find_votable("FV_VOTE");
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

static ssize_t board_thermal_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->fake_board_thermal = val;
	manager->thermal_board_temp = manager->fake_board_thermal;

	return count;
}

static ssize_t board_thermal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->thermal_board_temp);
}

static struct device_attribute board_thermal_attr =
		__ATTR(board_thermal, 0644, board_thermal_show, board_thermal_store);

static ssize_t is_eea_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->is_eea);
}

static struct device_attribute is_eea_attr =
	__ATTR(is_eea, 0444, is_eea_show, NULL);


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

static ssize_t auth_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	char *auth_name = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		goto out;
	auth_name = auth_get_name(manager->auth_dev);
	return sprintf(buf, "%s\n", auth_name);

out:
	return sprintf(buf, "unknow\n");
}
static struct device_attribute auth_name_attr =
__ATTR(auth_name, 0444, auth_name_show, NULL);

static ssize_t first_usage_date_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	int ret = 0;
	char first_usage_date[9] = {'0','0','0','0','0','0','0','0','\0'};

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager)) {
		memcpy(first_usage_date,"00000000",8);
		goto out;
	}

	ret = charger_get_batt_first_usage_date(manager, &first_usage_date[2]);
	if (ret != 0) {
		memcpy(first_usage_date,"00000000", 8);
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

	lx_info("first_usage_date:%.*s\n", 8, buf);
	memcpy(first_usage_date, &buf[2], 6);

	charger_set_batt_first_usage_date(manager, first_usage_date);

	return count;
}

static struct device_attribute first_usage_date_attr =
		__ATTR(first_usage_date, 0644, first_usage_date_show, first_usage_date_store);

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
		xm_smart_stop_charge_ctrl(manager, DISABLE_CHARGE_VOTER, true);
	} else {
		xm_smart_stop_charge_ctrl(manager, DISABLE_CHARGE_VOTER, false);
	}
	lxchg_psy_updata(POWER_SUPPLY_CHANGED);
	lx_info("charge_disable = %d\n", val);
	return count;
}
static struct device_attribute disable_chg_attr =
	__ATTR(disable_chg, 0644, disable_chg_show, disable_chg_store);

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
	lxchg_psy_updata(POWER_SUPPLY_CHANGED);
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

	lx_info("reverse_quick_charge_enabled = %d\n", val);
	manager->reverse_quick_charge_enabled = val;
	if (val)
		schedule_delayed_work(&manager->reverse_quick_charge_work, 0);

	return count;
}
static struct device_attribute reverse_quick_charge_attr =
	__ATTR(reverse_quick_charge, 0644, reverse_quick_charge_show, reverse_quick_charge_store);

static ssize_t aicl_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->aicl_test);
}

static ssize_t aicl_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->aicl_test = val;
	lx_info("aicl_test = %d\n", manager->aicl_test);

	return count;
}
static struct device_attribute aicl_test_attr =
	__ATTR(aicl_test, 0644, aicl_test_show, aicl_test_store);


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
	&fg1_cycle_attr.attr,
	&manufacturing_date_attr.attr,
	&first_usage_date_attr.attr,
	&cc_toggle_attr.attr,
	&otg_ui_support_attr.attr,
	&cid_status_attr.attr,
	&auth_dev_batt_cycle_attr.attr,
	&disable_chg_attr.attr,
#ifdef FACTORY_BUILD
	&ato_soc_user_control_attr.attr,
#endif
	&reverse_quick_charge_attr.attr,
	&aicl_test_attr.attr,
	&auth_name_attr.attr,
	&board_thermal_attr.attr,
	&is_eea_attr.attr,
	NULL,
};

static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};

int lxchg_usb_psy_register(struct charger_manager *manager)
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
EXPORT_SYMBOL(lxchg_usb_psy_register);

int lxchg_battery_psy_register(struct charger_manager *manager)
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
EXPORT_SYMBOL(lxchg_battery_psy_register);

int lxchg_charger_psy_register(struct charger_manager *manager)
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
EXPORT_SYMBOL(lxchg_charger_psy_register);


MODULE_DESCRIPTION("LiXun Charger sysfs");
MODULE_LICENSE("GPL v2");
