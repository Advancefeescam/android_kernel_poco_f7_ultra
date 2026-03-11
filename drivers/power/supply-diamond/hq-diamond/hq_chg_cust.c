#define pr_fmt(fmt) "[HQ_CHG][%s] %s %d: " fmt, __FILE_NAME__, __func__, __LINE__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pm_wakeup.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/version.h>

#include <dt-bindings/iio/qti_power_supply_iio.h>

#include "hq_chg_new.h"
#include "hq_chg_cust.h"
#include "hq_power_supply.h"

#define DEBUG

#ifdef pr_debug
#undef pr_debug
#endif

#ifdef DEBUG
#define pr_debug pr_info
#else
#define pr_debug do {} while (0)
#endif

#define EXTERN_CLASS_NAME "qcom-battery"

static ssize_t ibat_now_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						extern_class);
	int val = 0;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to release chg iio\n");
		return -ENODEV;
	}


	//batt_get_iio_channel(chg, BMS, BATT_QG_CURRENT_NOW, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(ibat_now);

static struct attribute *extern_class_attrs[] = {
#if 0
	&class_attr_soc_decimal.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_real_type.attr,
	&class_attr_pd_auth.attr,
	&class_attr_cell_voltage.attr,
	&class_attr_vbus_voltage.attr,
	&class_attr_cp_vbus_voltage.attr,
	&class_attr_input_current.attr,
	&class_attr_cp_ibus_slave.attr,
	&class_attr_cp_ibus_master.attr,
	&class_attr_cp_present_slave.attr,
	&class_attr_cp_present_master.attr,
	&class_attr_cp_temp_slave.attr,
	&class_attr_cp_temp_master.attr,
	&class_attr_charging_call_state.attr,
	&class_attr_verify_digest.attr,
	&class_attr_authentic.attr,
	&class_attr_battery_name.attr,
	&class_attr_chip_ok.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_stopcharging_test.attr,
	&class_attr_startcharging_test.attr,
	&class_attr_input_suspend.attr,
	//&class_attr_quick_charge_type.attr,
	&class_attr_apdo_max.attr,
	&class_attr_typec_mode.attr,
	&class_attr_mtbf_current.attr,
	&class_attr_fastcharge_mode.attr,
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_connector_temp.attr,
	&class_attr_enable_otg.attr,
	&class_attr_main_ibus.attr,
#endif
	&class_attr_ibat_now.attr,
	NULL,
};
ATTRIBUTE_GROUPS(extern_class);



int batt_init_other_class(struct batt_chg *chg)
{
	int rc = 0;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return -ENODEV;
	}

	chg->extern_class.name = EXTERN_CLASS_NAME;
	chg->extern_class.class_groups = extern_class_groups;
	rc = class_register(&chg->extern_class);
	if (rc < 0) {
		pr_err("Failed to create extern_class rc=%d\n", rc);
		return rc;
	}
	pr_info("Extern class register successful\n");
	return rc;
}

void batt_release_other_class(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return;
	}

	class_unregister(&chg->extern_class);
	pr_info("Release extern class succeed\n");
	return;
};