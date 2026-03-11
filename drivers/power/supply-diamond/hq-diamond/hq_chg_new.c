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
#include "hq_voter.h"

#define DEBUG

#ifdef pr_debug
#undef pr_debug
#endif

#ifdef DEBUG
#define pr_debug pr_info
#else
#define pr_debug do {} while (0)
#endif


#define  BATT_MAIN_WORK_PERIOD 5000
#define  BATT_INIT_WORK_PERIOD 1000

struct batt_chg *g_batt_chg;

int pps_change_to_stop(void)
{
	int val = 0;

	return val;
}
EXPORT_SYMBOL(pps_change_to_stop);

int set_jeita_lcd_on_off(bool lcdon)
{
	int val = 0;

	return val;
}
EXPORT_SYMBOL(set_jeita_lcd_on_off);

static int batt_get_charger_real_type(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to get chg\n");
		return 0;
	}

	return 0;
}

static void batt_update_chg_fg_info(struct batt_chg *chg)
{
	//struct sw_chg_parametes	*sw_info = &chg->batt_chg_fg.sw_chg_paras;
	//struct cp_chg_parametes	*cp_info = &chg->batt_chg_fg.cp_chg_paras;
	struct fg_chg_parametes	 *fg_info = &chg->batt_chg_fg.fg_paras;
	//struct other_parametes	*other_info = &chg->batt_chg_fg.other_paras;

	int rc = 0;

	/*update sw and cp ic paras*/

	/*update fg ic paras*/
	rc = batt_read_iio(chg, FG_TEMP, &fg_info->temp);
	if (rc < 0) {
		pr_err("read fuel gauge temp error\n");
	}

	rc = batt_read_iio(chg, FG_VOLTAGE_NOW, &fg_info->vbat);
	if (rc < 0) {
		pr_err("read fuel gauge vbat error\n");
	}

	rc = batt_read_iio(chg, FG_CAPACITY, &fg_info->soc);
	if (rc < 0) {
		pr_err("read fuel gauge vbat error\n");
	}

	/*update other paras*/
}

static void battery_charger_type_current_limit(struct batt_chg *chg)
{
	int real_type;
	int charger_type_ibat;
	int charger_type_ibus;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to get chg\n");
		return;
	}

	real_type = batt_get_charger_real_type(chg);

	switch (real_type)
	{
		case POWER_SUPPLY_TYPE_USB:
				charger_type_ibat = 500;
				charger_type_ibus = 500;
			break;
		case POWER_SUPPLY_TYPE_USB_FLOAT:
				charger_type_ibat = 1000;
				charger_type_ibus = 1000;
			break;
		case POWER_SUPPLY_TYPE_USB_CDP:
				charger_type_ibat = 1500;
				charger_type_ibus = 1500;
			break;
		case POWER_SUPPLY_TYPE_USB_DCP:
				charger_type_ibat = 2000;
				charger_type_ibus = 2000;
			break;
		case POWER_SUPPLY_TYPE_USB_HVDCP:
				charger_type_ibat = 3200;
				charger_type_ibus = 3200;
			break;
		case POWER_SUPPLY_TYPE_USB_PD:
				charger_type_ibat = 3000;
				charger_type_ibus = 3000;
			break;
		case POWER_SUPPLY_TYPE_UNKNOWN:
		default:
				charger_type_ibat = 0;
				charger_type_ibus = 0;
			break;
	}

	vote(chg->hq_ibus_votable, HQ_IBUS_CHARGER_TYPE, true, charger_type_ibus);
	vote(chg->hq_ibat_votable, HQ_IBAT_CHARGER_TYPE, true, charger_type_ibat);
}

static void batt_init_ic_info(struct work_struct *work)
{
	int rc = 0, val = 0;
	struct batt_chg *chg = container_of(work, struct batt_chg, batt_init_ic_work.work);

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to get chg\n");
		return;
	}

	/* update ic info by iio channel start */
	if (!chg->main_charge_info.init) {
		rc = batt_read_iio_force(chg, MAIN_IIO_ENUM_START, &val);
		if (rc >= 0) {
			pr_info("get main charge ic first vendor\n");
			chg->main_charge_info.vendor = VENDOR_FIRST;
			chg->main_charge_info.offset = 0;
			chg->main_charge_info.init = true;
		} else {
			rc = batt_read_iio_force(chg, MAIN_SECOND_IIO_ENUM_START, &val);
			if (rc >= 0) {
				pr_info("get main charge ic second vendor\n");
				chg->main_charge_info.vendor = VENDOR_SECOND;
				chg->main_charge_info.offset = MAIN_SECOND_IIO_ENUM_START - MAIN_IIO_ENUM_START;
				chg->main_charge_info.init = true;
			} else {
				pr_err("Failed to get main charge ic\n");
			}
		}
	}

	if (!chg->fg_info.init) {
		rc = batt_read_iio_force(chg, FG_IIO_ENUM_START, &val);
		if (rc >= 0) {
			pr_info("get fuel gauge ic first vendor\n");
			chg->fg_info.vendor = VENDOR_FIRST;
			chg->fg_info.offset = 0;
			chg->fg_info.init = true;
		} else {
			pr_err("Dont suppoert sencond fg ic\n");
		}
	}

	if (!chg->cp_master_info.init) {
		rc = batt_read_iio_force(chg, CP_MASTER_IIO_ENUM_START, &val);
		if (rc >= 0) {
			pr_info("get cp master ic first vendor\n");
			chg->cp_master_info.vendor = VENDOR_FIRST;
			chg->cp_master_info.offset = 0;
			chg->cp_master_info.init = true;
		}  else {
			rc = batt_read_iio_force(chg, CP_MASTER_SECOND_IIO_ENUM_START, &val);
			if (rc >= 0) {
				pr_info("get cp master ic second vendor\n");
				chg->cp_master_info.vendor = VENDOR_SECOND;
				chg->cp_master_info.offset = CP_MASTER_SECOND_IIO_ENUM_START - CP_MASTER_IIO_ENUM_START;
				chg->cp_master_info.init = true;
			} else {
				pr_err("Failed to get cp master ic\n");
			}
		}
	}

	if (!chg->cp_slave_info.init) {
		rc = batt_read_iio_force(chg, CP_SLAVE_IIO_ENUM_START, &val);
		if (rc >= 0) {
			pr_info("get cp slave ic first vendor\n");
			chg->cp_slave_info.vendor = VENDOR_FIRST;
			chg->cp_slave_info.offset = 0;
			chg->cp_slave_info.init = true;
		}  else {
			rc = batt_read_iio_force(chg, CP_SLAVE_SECOND_IIO_ENUM_START, &val);
			if (rc >= 0) {
				pr_info("get cp slave ic second vendor\n");
				chg->cp_slave_info.vendor = VENDOR_SECOND;
				chg->cp_slave_info.offset = CP_SLAVE_SECOND_IIO_ENUM_START - CP_SLAVE_IIO_ENUM_START;
				chg->cp_slave_info.init = true;
			} else {
				pr_err("Failed to get cp slave ic\n");
			}
		}
	}

	if (!(chg->main_charge_info.init && chg->fg_info.init &&
						chg->cp_master_info.init && chg->cp_slave_info.init)) {
		pr_info("All ic init failed, try again after %d:\n", BATT_INIT_WORK_PERIOD / 1000);
		schedule_delayed_work(&chg->batt_init_ic_work, msecs_to_jiffies(BATT_INIT_WORK_PERIOD));
	} else {
		pr_info("All ic init successful\n");
	}

	/* config ic info by iio channel end */
}

static int batt_get_iio_channel_byname(struct batt_chg *chg, const char *propname,
									struct iio_channel **chan)
{
	int rc = 0;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to get chg\n");
		return -ENODEV;
	}

	rc = of_property_match_string(chg->dev->of_node,
									"io-channel-names", propname);
	if (rc < 0) {
		pr_err("Cannot find prop:%s in device node\n", propname);
		return rc;
	}

	*chan = iio_channel_get(chg->dev, propname);
	if (IS_ERR_OR_NULL(*chan)) {
			rc = PTR_ERR(*chan);
			if (rc != -EPROBE_DEFER)
				pr_err("Channel:%s unavailable, rc = %d\n", propname, rc);
			else
				pr_err("Channel:%s is not ready, rc = %d\n", propname, rc);
			*chan = NULL;
		return rc;
	}

	pr_debug("Get channel : %s successful\n", propname);
	return rc;
}

static int batt_rw_value_by_iio(struct batt_chg *chg, int channel, int *val, enum batt_iio_option opt)
{
	int rc = 0;

	if (IS_ERR_OR_NULL(chg) || IS_ERR_OR_NULL(val)) {
		pr_err("Chg or val is NULL!!\n");
		return -EINVAL;
	}

	if ((channel < BATT_IIO_CHANNEL_START) ||
		(channel > BATT_IIO_CHANNEL_END)) {
		pr_err("Channel index:%d is over the range [%d-%d]\n",
						channel,
						BATT_IIO_CHANNEL_START,
						BATT_IIO_CHANNEL_END);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(batt_ext_iio_chan_name[channel])) {
		pr_err("Channel index:%d is not regsiter\n", channel);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg->batt_ext_iio_chans[channel])) {
		rc = batt_get_iio_channel_byname(chg, batt_ext_iio_chan_name[channel],
											&chg->batt_ext_iio_chans[channel]);
	}

	if (rc < 0 || IS_ERR_OR_NULL(chg->batt_ext_iio_chans[channel])) {
		pr_err("Failed to get iio channel index = %d, name = %s\n",
					channel, batt_ext_iio_chan_name[channel]);
		return -EINVAL;
	}

	switch(opt) {
		case BATT_IIO_OPT_READ:
			rc = iio_read_channel_processed(chg->batt_ext_iio_chans[channel], val);
			if (rc < 0) {
				pr_err("Failed to read iio channel index = %d, name = %s\n",
							channel, batt_ext_iio_chan_name[channel]);
				return rc;
			}

			pr_debug("Read iio channel index = %d, name = %s, value = %d\n",
				channel, batt_ext_iio_chan_name[channel], *val);
			break;

		case BATT_IIO_OPT_WRITE:
			rc = iio_write_channel_raw(chg->batt_ext_iio_chans[channel], *val);
			if (rc < 0) {
				pr_err("Failed to write iio channel index = %d, name = %s\n",
							channel, batt_ext_iio_chan_name[channel]);
				return rc;
			}

			pr_debug("Write iio channel index = %d, name = %s, value = %d\n",
				channel, batt_ext_iio_chan_name[channel], *val);
			break;

		default:
			pr_err("Unsupport opt:%d!\n", opt);
	}

	return rc;
}

struct batt_iio_category iio_cate[] = {
	{FG_IIO_ENUM_START,               FG_IIO_ENUM_END,               IC_FUEL_GAUGE},
	{MAIN_IIO_ENUM_START,             MAIN_IIO_ENUM_END,             IC_MAIN_CHARGE},
	{MAIN_SECOND_IIO_ENUM_START,      MAIN_SECOND_IIO_ENUM_END,      IC_MAIN_CHARGE},
	{CP_MASTER_IIO_ENUM_START,        CP_MASTER_IIO_ENUM_END,        IC_CHARGE_PUMP_MASTER},
	{CP_MASTER_SECOND_IIO_ENUM_START, CP_MASTER_SECOND_IIO_ENUM_END, IC_CHARGE_PUMP_MASTER},
	{CP_SLAVE_IIO_ENUM_START,         CP_SLAVE_IIO_ENUM_END,         IC_CHARGE_PUMP_SLAVE},
	{CP_SLAVE_SECOND_IIO_ENUM_START,  CP_SLAVE_SECOND_IIO_ENUM_END,  IC_CHARGE_PUMP_SLAVE},
};

enum batt_ic_type batt_get_ic_type(int channel)
{
	int i = 0;
	if ((channel < BATT_IIO_CHANNEL_START) ||
		(channel > BATT_IIO_CHANNEL_END)) {
		pr_err("Channel index:%d is over the range [%d-%d]\n",
						channel,
						BATT_IIO_CHANNEL_START,
						BATT_IIO_CHANNEL_END);
		return IC_UNKNOW;
	}

	for (i = 0; i < ARRAY_SIZE(iio_cate);i++) {
		if (channel >= iio_cate[i].start && channel <= iio_cate[i].end) {
			return iio_cate[i].type;
		}
	}

	pr_err("Request unknow iio\n");
	return IC_UNKNOW;
}


static int batt_get_iio_offset(struct batt_chg *chg, int channel)
{
	int offset = -1;
	enum batt_ic_type type = IC_UNKNOW;

	type = batt_get_ic_type(channel);

	switch (type) {
		case IC_MAIN_CHARGE:
			if (chg->main_charge_info.init) {
				offset = chg->main_charge_info.offset;
			}
			break;
		case IC_FUEL_GAUGE:
			if (chg->fg_info.init) {
				offset = chg->fg_info.offset;
			}
			break;
		case IC_CHARGE_PUMP_MASTER:
			if (chg->cp_master_info.init) {
				offset = chg->cp_master_info.offset;
			}
			break;
		case IC_CHARGE_PUMP_SLAVE:
			if (chg->cp_slave_info.init) {
				offset = chg->cp_slave_info.offset;
			}
			break;
		case IC_UNKNOW:
			pr_err("Unknow ic type, do nothing\n");
		default:
			break;
	}

	if (offset < 0) {
		pr_err("Failed to get iio offset\n");
		return offset;
	}

	if (type != batt_get_ic_type(channel + offset)) {
		pr_err("Type changed, set offset to 0\n");
		offset = 0;
	}

	pr_debug("iio channel = %d, offset = %d\n", channel, offset);
	return offset;
}

int batt_read_iio(struct batt_chg *chg, int channel, int *val)
{
	int offset = 0;

	offset = batt_get_iio_offset(chg, channel);

	if (offset < 0 ) {
		pr_err("Failed to get iio or ic is not ready\n");
		return -EINVAL;
	}

	return batt_rw_value_by_iio(chg, channel + offset, val, BATT_IIO_OPT_READ);
}

int batt_write_iio(struct batt_chg *chg, int channel, int *val)
{
	int offset = 0;

	offset = batt_get_iio_offset(chg, channel);

	if (offset < 0 ) {
		pr_err("Failed to get iio or ic is not ready\n");
		return -EINVAL;
	}

	return batt_rw_value_by_iio(chg, channel + offset, val, BATT_IIO_OPT_WRITE);
}

int batt_read_iio_force(struct batt_chg *chg, int channel, int *val)
{
	return batt_rw_value_by_iio(chg, channel, val, BATT_IIO_OPT_READ);
}

int batt_write_iio_force(struct batt_chg *chg, int channel, int *val)
{
	return batt_rw_value_by_iio(chg, channel, val, BATT_IIO_OPT_WRITE);
}

static int batt_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct batt_chg *chg = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;
	*val2 = 0;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to get chg\n");
		return -ENODEV;
	}

	switch (chan->channel) {
		default:
			rc = -EINVAL;
	}

	if (rc < 0) {
		pr_err("Unsupported read IIO[%d]:%s, rc = %d\n",
			chan->channel, chan->extend_name, chan->channel, rc);
		return rc;
	}

	pr_debug("Read IIO[%d]:%s, value = %d\n",
					chan->channel, chan->extend_name, val1);
	return IIO_VAL_INT;
}

static int batt_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct batt_chg *chg = iio_priv(indio_dev);
	int rc = 0;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to get chg\n");
		return -ENODEV;
	}

	switch (chan->channel) {
		default:
			rc = -EINVAL;
		}

	if (rc < 0) {
		pr_err("Unsupported write IIO[%d]:%s, val1 = %d, val2 = %d,"
				" rc = %d\n", chan->channel,
				chan->extend_name, val1, val2, rc);
		return rc;
	}

	pr_debug("Write IIO[%d]:%s, val1 = %d, val2 = %d\n",
				chan->channel, chan->extend_name, val1, val2);
	return rc;
}

static int batt_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct batt_chg *chg = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chg->batt_iio_chan_spec;
	int i = 0;

	if (IS_ERR_OR_NULL(iio_chan)) {
		pr_err("Failed to get iio chan\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(battery_iio_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0]) {
			pr_debug("Get successful index = %d\n", i);
			return i;
		}

	pr_err("Failed to find iio chan:%s\n", iio_chan->extend_name);
	return -EINVAL;

}

static const struct iio_info battery_iio_info = {
	.read_raw	= batt_iio_read_raw,
	.write_raw	= batt_iio_write_raw,
	.of_xlate	= batt_iio_of_xlate,
};

static int batt_init_chg_iio(struct batt_chg *chg)
{
	struct iio_dev *indio_dev = chg->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int batt_chan_num = ARRAY_SIZE(battery_iio_channels);
	int rc = -1, i = 0;

	pr_info("Start init chg iio\n");

	chg->batt_iio_chan_spec = devm_kcalloc(chg->dev, batt_chan_num,
				sizeof(*chg->batt_iio_chan_spec), GFP_KERNEL);
	if (IS_ERR_OR_NULL(chg->batt_iio_chan_spec)) {
		pr_err("Failed to alloc iio chan buffer\n");
		rc = -ENOMEM;
		goto err_1;
	}

	pr_debug("Batt iio spec ptr = %p, array size: %d",
								chg->batt_iio_chan_spec,
								batt_chan_num);

	chg->batt_iio_chans = devm_kcalloc(chg->dev,
				batt_chan_num,
				sizeof(*chg->batt_iio_chans),
				GFP_KERNEL);

	if (IS_ERR_OR_NULL(chg->batt_iio_chans)) {
		pr_err("Failed to alloc init iio chan buffer\n");
		rc = -ENOMEM;
		goto err_2;
	}

	pr_debug("Batt iio ptr = %p, array size: %d",
								chg->batt_iio_chans,
								batt_chan_num);

	indio_dev->info = &battery_iio_info;
	indio_dev->dev.parent = chg->dev;
	indio_dev->dev.of_node = chg->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chg->batt_iio_chan_spec;
	indio_dev->num_channels = batt_chan_num;
	indio_dev->name = "batt_chg";

	for (i = 0; i < batt_chan_num; i++) {
		chan = &chg->batt_iio_chan_spec[i];
		chan->address            = i;
		chan->channel            = battery_iio_channels[i].channel_num;
		chan->type               = battery_iio_channels[i].type;
		chan->datasheet_name     = battery_iio_channels[i].datasheet_name;
		chan->extend_name        = battery_iio_channels[i].datasheet_name;
		chan->info_mask_separate = battery_iio_channels[i].info_mask;

		chg->batt_iio_chans[i].channel = chan;
		chg->batt_iio_chans[i].indio_dev = indio_dev;

		pr_debug("Regsiter iio,index[%d], channel = %d, type = %d, "
									"name = %s, info_mask = %ld\n",
									i,
									chan->channel,
									chan->type,
									chan->datasheet_name,
									chan->info_mask_separate);
	}

	rc = devm_iio_device_register(chg->dev, indio_dev);
	if (rc) {
		goto err_3;
		pr_err("Failed to register battery IIO device, rc=%d\n", rc);
	}
	return rc;

err_3:
	if (chg->dev && chg->batt_iio_chans)
		devm_kfree(chg->dev, chg->batt_iio_chans);
err_2:
	if (chg->dev && chg->batt_iio_chan_spec)
		devm_kfree(chg->dev, chg->batt_iio_chan_spec);
err_1:
	pr_err("Failed init chg iio, rc=%d\n", rc);
	return rc;
}

static int batt_release_chg_iio(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg) || IS_ERR_OR_NULL(chg->dev)) {
		pr_err("Failed to release chg iio\n");
		return -ENODEV;
	}

	if (chg->batt_iio_chans)
		devm_kfree(chg->dev, chg->batt_iio_chans);

	if (chg->batt_iio_chan_spec)
		devm_kfree(chg->dev, chg->batt_iio_chan_spec);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 4, 0))
	if (chg->indio_dev)
		devm_iio_device_unregister(chg->dev, chg->indio_dev);
#endif

	pr_info("Release chg iio succeed\n");
	return 0;
}

static int batt_init_chg_ext_iio(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg) ) {
		pr_err("Failed to get batt chg\n");
		return -ENODEV;
	}

	chg->batt_ext_iio_chans = devm_kcalloc(chg->dev,
				ARRAY_SIZE(batt_ext_iio_chan_name),
				sizeof(*chg->batt_ext_iio_chans),
				GFP_KERNEL);

	if (!chg->batt_ext_iio_chans) {
		pr_err("Failed to alloc extern iio_chan\n");
		return -ENOMEM;
	}
	pr_debug("extern iio ptr = %p, array size: %d",
								chg->batt_ext_iio_chans,
								ARRAY_SIZE(batt_ext_iio_chan_name));

	pr_info("init succeed\n");
	return 0;
}

static int batt_release_chg_ext_iio(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg) || IS_ERR_OR_NULL(chg->dev)) {
		pr_err("Failed to release chg extern iio\n");
		return -ENODEV;
	}
	if (chg->batt_ext_iio_chans)
		devm_kfree(chg->dev, chg->batt_ext_iio_chans);

	pr_info("Release chg ext iio succeed\n");
	return 0;
}

static void batt_release_iio(struct batt_chg *chg)
{
	pr_info("start release iio\n");
	batt_release_chg_iio(chg);
	batt_release_chg_ext_iio(chg);

}

static int batt_init_iio(struct batt_chg *chg)
{
	int rc = 0;
	// register batt iio channel for other charge ic driver
	rc = batt_init_chg_iio(chg);
	if (rc) {
		pr_err("Failed to init chg iio\n");
		return rc;
	}

	// alloc buffer for iio device registed by other driver
	rc = batt_init_chg_ext_iio(chg);
	if (rc) {
		pr_err("Failed to init chg extern iio\n");
		return rc;
	}

	return rc;
}

static int hq_ibat_vote_callback(struct votable *votable, void *data,
			int value, const char *client)
{
	struct batt_chg *chg = (struct batt_chg *)data;
	int ret;

	pr_info("select ichg = %d\n",value);
	ret = batt_write_iio(chg, MAIN_CHAGER_CURRENT, &value);
	if(ret < 0) {
		pr_err("set main chg ichg error\n");
		return -1;
	}

	return 0;
}

static int hq_ibus_vote_callback(struct votable *votable, void *data,
			int value, const char *client)
{
	struct batt_chg *chg = (struct batt_chg *)data;
	int ret;

	pr_info("select icl = %d\n",value);
	ret = batt_write_iio(chg, MAIN_INPUT_CURRENT_SETTLED, &value);
	if(ret < 0) {
		pr_err("set main chg icl error\n");
		return -1;
	}

	return 0;
}

static int hq_cv_vote_callback(struct votable *votable, void *data,
			int value, const char *client)
{
	struct batt_chg *chg = (struct batt_chg *)data;
	int ret;

	pr_info("select cv = %d\n",value);
	ret = batt_write_iio(chg, MAIN_CHARGER_VOLTAGE_TERM, &value);
	if(ret < 0) {
		pr_err("set main chg cv error\n");
		return -1;
	}

	return 0;
}

static int hq_iterm_vote_callback(struct votable *votable, void *data,
			int value, const char *client)
{
	struct batt_chg *chg = (struct batt_chg *)data;
	int ret;

	pr_info("select iterm = %d\n",value);
	ret = batt_write_iio(chg, MAIN_CHAGER_TERM, &value);
	if(ret < 0) {
		pr_err("set main chg iterm error\n");
		return -1;
	}

	return 0;
}

static int batt_init_voter(struct batt_chg *chg)
{
	int rc = 0;

	chg->hq_ibat_votable = kzalloc(sizeof(struct votable), GFP_KERNEL);
	if(!chg->hq_ibat_votable)
		return -ENOMEM;

	chg->hq_ibus_votable = kzalloc(sizeof(struct votable), GFP_KERNEL);
	if(!chg->hq_ibus_votable)
		return -ENOMEM;

	chg->hq_cv_votable = kzalloc(sizeof(struct votable), GFP_KERNEL);
	if(!chg->hq_cv_votable)
		return -ENOMEM;

	chg->hq_iterm_votable = kzalloc(sizeof(struct votable), GFP_KERNEL);
	if(!chg->hq_iterm_votable)
		return -ENOMEM;

	chg->hq_ibat_votable  = create_votable(HQ_IBAT_VOTER, VOTE_MIN,
						hq_ibat_vote_callback, chg);
	if (IS_ERR(chg->hq_ibat_votable)) {
		rc = PTR_ERR(chg->hq_ibat_votable);
		chg->hq_ibat_votable = NULL;
		goto err;
	}

	chg->hq_ibus_votable = create_votable(HQ_IBUS_VOTER, VOTE_MIN,
						hq_ibus_vote_callback, chg);
	if (IS_ERR(chg->hq_ibus_votable)) {
		rc = PTR_ERR(chg->hq_ibus_votable);
		chg->hq_ibus_votable = NULL;
		goto err;
	}

	chg->hq_cv_votable  = create_votable(HQ_CV_VOTER, VOTE_MIN,
						hq_cv_vote_callback, chg);
	if (IS_ERR(chg->hq_cv_votable)) {
		rc = PTR_ERR(chg->hq_cv_votable);
		chg->hq_cv_votable = NULL;
		goto err;
	}

	chg->hq_iterm_votable = create_votable(HQ_ITERM_VOTER, VOTE_MIN,
						hq_iterm_vote_callback, chg);
	if (IS_ERR(chg->hq_ibat_votable)) {
		rc = PTR_ERR(chg->hq_ibat_votable);
		chg->hq_ibat_votable = NULL;
		goto err;
	}

	return rc;

err:
	destroy_votable(chg->hq_ibat_votable);
	destroy_votable(chg->hq_ibus_votable);
	destroy_votable(chg->hq_cv_votable);
	destroy_votable(chg->hq_iterm_votable);

	return -ENOMEM;
}

static int batt_init_config(struct batt_chg *chg)
{
	int rc = 0;

	chg->is_bringup         = true;
	chg->batt_dt.jeita_para.jeita_enable       = false;
	chg->batt_dt.thermal_table_paras.thermal_enable = false;

	chg->main_charge_info.type   = IC_MAIN_CHARGE;
	chg->main_charge_info.vendor = VENDOR_UNKNOW;
	chg->main_charge_info.offset = -1;
	chg->main_charge_info.init   = false;

	chg->cp_master_info.type   = IC_CHARGE_PUMP_MASTER;
	chg->cp_master_info.vendor = VENDOR_UNKNOW;
	chg->cp_master_info.offset = -1;
	chg->cp_master_info.init   = false;

	chg->cp_slave_info.type   = IC_CHARGE_PUMP_SLAVE;
	chg->cp_slave_info.vendor = VENDOR_UNKNOW;
	chg->cp_slave_info.offset = -1;
	chg->cp_slave_info.init   = false;

	chg->fg_info.type   = IC_FUEL_GAUGE;
	chg->fg_info.vendor = VENDOR_UNKNOW;
	chg->fg_info.offset = -1;
	chg->fg_info.init   = false;

	return rc;
}

static int batt_parse_jeita_dt(struct batt_chg *chg)
{
	struct device_node *node = chg->dev->of_node;
	struct jeita_parametes*hq_jeita = &chg->batt_dt.jeita_para;
	int val;
	int rc = 0;

	hq_jeita->jeita_enable = of_property_read_bool(node, "hq_chg,jeita_enable");
	if (!hq_jeita->jeita_enable) {
		pr_err("jeita is disabed.");
		return EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t0", &val) >= 0)
		hq_jeita->jeita_temp_t0 = val;
	else {
		pr_err("use default jeita_temp_t0 :%d\n", JEITA_TEMP_T0);
		hq_jeita->jeita_temp_t0 = JEITA_TEMP_T0;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t1", &val) >= 0)
		hq_jeita->jeita_temp_t1 = val;
	else {
		pr_err("use default jeita_temp_t1 :%d\n", JEITA_TEMP_T1);
		hq_jeita->jeita_temp_t1 = JEITA_TEMP_T1;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t2", &val) >= 0)
		hq_jeita->jeita_temp_t2 = val;
	else {
		pr_err("use default jeita_temp_t2 :%d\n", JEITA_TEMP_T2);
		hq_jeita->jeita_temp_t2 = JEITA_TEMP_T2;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t3", &val) >= 0)
		hq_jeita->jeita_temp_t3 = val;
	else {
		pr_err("use default jeita_temp_t3 :%d\n", JEITA_TEMP_T3);
		hq_jeita->jeita_temp_t3 = JEITA_TEMP_T3;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t4", &val) >= 0)
		hq_jeita->jeita_temp_t4 = val;
	else {
		pr_err("use default jeita_temp_t4 :%d\n", JEITA_TEMP_T4);
		hq_jeita->jeita_temp_t4 = JEITA_TEMP_T4;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t5", &val) >= 0)
		hq_jeita->jeita_temp_t5 = val;
	else {
		pr_err("use default jeita_temp_t5 :%d\n", JEITA_TEMP_T5);
		hq_jeita->jeita_temp_t5 = JEITA_TEMP_T5;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t6", &val) >= 0)
		hq_jeita->jeita_temp_t6 = val;
	else {
		pr_err("use default jeita_temp_t6 :%d\n", JEITA_TEMP_T6);
		hq_jeita->jeita_temp_t6 = JEITA_TEMP_T6;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t7", &val) >= 0)
		hq_jeita->jeita_temp_t7 = val;
	else {
		pr_err("use default jeita_temp_t7 :%d\n", JEITA_TEMP_T7);
		hq_jeita->jeita_temp_t7 = JEITA_TEMP_T7;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_below_t1_cc", &val) >= 0)
		hq_jeita->jeita_temp_below_t1_cc = val;
	else {
		pr_err("use default jeita_temp_below_t1_cc :%d\n", JEITA_TEMP_BELOW_T1_CC);
		hq_jeita->jeita_temp_below_t1_cc = JEITA_TEMP_BELOW_T1_CC;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t1_to_t2_cc", &val) >= 0)
		hq_jeita->jeita_temp_t1_to_t2_cc = val;
	else {
		pr_err("use default jeita_temp_t1_to_t2_cc :%d\n", JEITA_TEMP_T1_TO_T2_CC);
		hq_jeita->jeita_temp_t1_to_t2_cc = JEITA_TEMP_T1_TO_T2_CC;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t2_to_t3_cc", &val) >= 0)
		hq_jeita->jeita_temp_t2_to_t3_cc = val;
	else {
		pr_err("use default jeita_temp_t2_to_t3_cc :%d\n", JEITA_TEMP_T2_TO_T3_CC);
		hq_jeita->jeita_temp_t2_to_t3_cc = JEITA_TEMP_T2_TO_T3_CC;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t3_to_t4_cc", &val) >= 0)
		hq_jeita->jeita_temp_t3_to_t4_cc = val;
	else {
		pr_err("use default jeita_temp_t3_to_t4_cc :%d\n", JEITA_TEMP_T3_TO_T4_CC);
		hq_jeita->jeita_temp_t3_to_t4_cc = JEITA_TEMP_T3_TO_T4_CC;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t4_to_t5_cc", &val) >= 0)
		hq_jeita->jeita_temp_t4_to_t5_cc = val;
	else {
		pr_err("use default jeita_temp_t4_to_t5_cc :%d\n", JEITA_TEMP_T4_TO_T5_CC);
		hq_jeita->jeita_temp_t4_to_t5_cc = JEITA_TEMP_T4_TO_T5_CC;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_t5_to_t6_cc", &val) >= 0)
		hq_jeita->jeita_temp_t5_to_t6_cc = val;
	else {
		pr_err("use default jeita_temp_t5_to_t6_cc :%d\n", JEITA_TEMP_T5_TO_T6_CC);
		hq_jeita->jeita_temp_t5_to_t6_cc = JEITA_TEMP_T5_TO_T6_CC;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,jeita_temp_above_t6_cc", &val) >= 0)
		hq_jeita->jeita_temp_above_t6_cc = val;
	else {
		pr_err("use default jeita_temp_above_t6_cc :%d\n", JEITA_TEMP_ABOVE_T6_CC);
		hq_jeita->jeita_temp_above_t6_cc = JEITA_TEMP_ABOVE_T6_CC;
		rc = EINVAL;
	}

	return rc;
}

static int batt_parse_chg_type_curr_limit_dt(struct batt_chg *chg)
{
	struct device_node *node = chg->dev->of_node;
	struct chg_type_current_limit *chg_type_curr_limit = &chg->batt_dt.chg_type_curr_limit;
	int val;
	int rc = 0;

	if (of_property_read_u32(node, "hq_chg,sdp_input_charge_limit", &val) >= 0)
		chg_type_curr_limit->sdp_input_charge_limit = val;
	else {
		pr_err("use default sdp_input_charge_limit :%d\n", SDP_INPUT_CHARGE_LIMIT);
		chg_type_curr_limit->sdp_input_charge_limit = SDP_INPUT_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,sdp_battery_charge_limit", &val) >= 0)
		chg_type_curr_limit->sdp_battery_charge_limit = val;
	else {
		pr_err("use default sdp_input_charge_limit :%d\n", SDP_BATTERY_CHARGE_LIMIT);
		chg_type_curr_limit->sdp_battery_charge_limit = SDP_BATTERY_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,float_input_charge_limit", &val) >= 0)
		chg_type_curr_limit->float_input_charge_limit = val;
	else {
		pr_err("use default float_input_charge_limit :%d\n", FLOAT_INPUT_CHARGE_LIMIT);
		chg_type_curr_limit->float_input_charge_limit = FLOAT_INPUT_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,float_battery_charge_limit", &val) >= 0)
		chg_type_curr_limit->float_battery_charge_limit = val;
	else {
		pr_err("use default float_battery_charge_limit :%d\n", FLOAT_BATTERY_CHARGE_LIMIT);
		chg_type_curr_limit->float_battery_charge_limit = FLOAT_BATTERY_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,cdp_input_charge_limit", &val) >= 0)
		chg_type_curr_limit->cdp_input_charge_limit = val;
	else {
		pr_err("use default cdp_input_charge_limit :%d\n", CDP_INPUT_CHARGE_LIMIT);
		chg_type_curr_limit->cdp_input_charge_limit = CDP_INPUT_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,cdp_battery_charge_limit", &val) >= 0)
		chg_type_curr_limit->cdp_battery_charge_limit = val;
	else {
		pr_err("use default cdp_battery_charge_limit :%d\n", CDP_BATTERY_CHARGE_LIMIT);
		chg_type_curr_limit->cdp_battery_charge_limit = CDP_BATTERY_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,dcp_input_charge_limit", &val) >= 0)
		chg_type_curr_limit->dcp_input_charge_limit = val;
	else {
		pr_err("use default dcp_input_charge_limit :%d\n", DCP_INPUT_CHARGE_LIMIT);
		chg_type_curr_limit->dcp_input_charge_limit = DCP_INPUT_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,dcp_battery_charge_limit", &val) >= 0)
		chg_type_curr_limit->dcp_battery_charge_limit = val;
	else {
		pr_err("use default dcp_battery_charge_limit :%d\n", DCP_BATTERY_CHARGE_LIMIT);
		chg_type_curr_limit->dcp_battery_charge_limit = DCP_BATTERY_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,qc_input_charge_limit", &val) >= 0)
		chg_type_curr_limit->qc_input_charge_limit = val;
	else {
		pr_err("use default qc_input_charge_limit :%d\n", QC_INPUT_CHARGE_LIMIT);
		chg_type_curr_limit->qc_input_charge_limit = QC_INPUT_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,qc_battery_charge_limit", &val) >= 0)
		chg_type_curr_limit->qc_battery_charge_limit = val;
	else {
		pr_err("use default qc_battery_charge_limit :%d\n", QC_BATTERY_CHARGE_LIMIT);
		chg_type_curr_limit->qc_battery_charge_limit = QC_BATTERY_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,pd_input_charge_limit", &val) >= 0)
		chg_type_curr_limit->pd_input_charge_limit = val;
	else {
		pr_err("use default pd_input_charge_limit :%d\n", PD_INPUT_CHARGE_LIMIT);
		chg_type_curr_limit->pd_input_charge_limit = PD_INPUT_CHARGE_LIMIT;
		rc = EINVAL;
	}

	if (of_property_read_u32(node, "hq_chg,pd_battery_charge_limit", &val) >= 0)
		chg_type_curr_limit->pd_battery_charge_limit = val;
	else {
		pr_err("use default pd_battery_charge_limit :%d\n", PD_BATTERY_CHARGE_LIMIT);
		chg_type_curr_limit->pd_battery_charge_limit = PD_BATTERY_CHARGE_LIMIT;
		rc = EINVAL;
	}

	return rc;
}

static int batt_parse_thermal_dt(struct batt_chg *chg)
{
	struct device_node *node = chg->dev->of_node;
	struct thermal_parametes *hq_thermal_paras = &chg->batt_dt.thermal_table_paras;
	int ret = 0;
	int rc = 0;

	hq_thermal_paras->thermal_enable = of_property_read_bool(node, "hq_chg,thermal_enable");
	if (!hq_thermal_paras->thermal_enable) {
		pr_err("thermal is disabed.");
		return EINVAL;
	}

	ret = of_property_read_u32_array(node, "hq_chg,normal_charger_thermal_table",
			hq_thermal_paras->normal_charger_thermal_table, THERMAL_LEVEL_MAX);
	if (ret < 0) {
		pr_err("use default normal_charger_thermal_table.\n");
		memcpy(hq_thermal_paras->normal_charger_thermal_table, normal_charger_thermal_table, sizeof(normal_charger_thermal_table));
		rc = EINVAL;
	}

	ret = of_property_read_u32_array(node, "hq_chg,fast_charger_thermal_table",
			hq_thermal_paras->fast_charger_thermal_table, THERMAL_LEVEL_MAX);
	if (ret < 0) {
		pr_err("use default fast_charger_thermal_table.\n");
		memcpy(hq_thermal_paras->fast_charger_thermal_table, fast_charger_thermal_table, sizeof(fast_charger_thermal_table));
		rc = EINVAL;
	}

	ret = of_property_read_u32_array(node, "hq_chg,super_charger_thermal_table",
			hq_thermal_paras->super_charger_thermal_table, THERMAL_LEVEL_MAX);
	if (ret < 0) {
		pr_err("use default super_charger_thermal_table.\n");
		memcpy(hq_thermal_paras->super_charger_thermal_table, super_charger_thermal_table, sizeof(super_charger_thermal_table));
		rc = EINVAL;
	}

	return rc;
}

static int batt_parse_dt(struct batt_chg *chg)
{
	struct device_node *node = chg->dev->of_node;
	int rc;

	if (IS_ERR_OR_NULL(node)) {
		pr_err("Failed to get device tree node\n");
		return -ENODEV;
	}

	chg->is_bringup = of_property_read_bool(node, "hq_chg,is_bringup");
	pr_info("Bringup flag: %d\n", chg->is_bringup);

	rc = batt_parse_jeita_dt(chg);
	if (rc == EINVAL)
		pr_err("use default jeita arguments.");

	rc = batt_parse_chg_type_curr_limit_dt(chg);
	if (rc == EINVAL)
		pr_err("use default charger type current limit arguments.");

	rc = batt_parse_thermal_dt(chg);
	if (rc == EINVAL)
		pr_err("use default thermal arguments.");

	return 0;
}

/****************************************************
 ********* USB PSY REGISTRATION START ***************
 ****************************************************/
static enum power_supply_property usb_psy_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	//POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
};

static enum power_supply_usb_type chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};

static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *val)
{
	int rc = 0;
	struct batt_chg *chg = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(val) || IS_ERR_OR_NULL(chg)) {
		pr_err("val or chg is NULL!\n");
		return -ENODEV;
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_ONLINE:
			break;
		case POWER_SUPPLY_PROP_TYPE:
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			break;

		default:
			rc = -EINVAL;
			pr_err("unsuppoert prop %d rc = %d\n", prop, rc);
	}
	return rc;

}
static int usb_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct batt_chg *chg = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(val) || IS_ERR_OR_NULL(chg)) {
		pr_err("val or chg is NULL!\n");
		return -ENODEV;
	}

	switch (prop) {
		default:
			rc = -EINVAL;
			pr_err("Unsuppoert prop %d rc = %d\n", prop, rc);
	}
	return rc;
}

static int usb_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{

	switch (prop) {

		default:
			pr_debug("prop[%d] is  unwriteable\n", prop);
			return 0;
	}
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB_PD,
	.usb_types = chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(chg_psy_usb_types),
	.properties = usb_psy_props,
	.num_properties = ARRAY_SIZE(usb_psy_props),
	.get_property = usb_psy_get_prop,
	.set_property = usb_psy_set_prop,
	.property_is_writeable = usb_psy_prop_is_writeable,
};

static int batt_init_usb_psy(struct batt_chg *chg)
{
	struct power_supply_config usb_cfg = {0};

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return -ENODEV;
	}

	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR_OR_NULL(chg->batt_psy)) {
		pr_err("Failed to register battery power supply\n");
		return -ENODEV;
	}

	return 0;
}
/****************************************************
 ********* USB PSY REGISTRATION END ***************
 ****************************************************/


/****************************************************
 ********* BATT PSY REGISTRATION START **************
 ****************************************************/
static enum power_supply_property batt_psy_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};


static int batt_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
			return 1;
		default:
			pr_debug("prop[%d] is  unwriteable\n", prop);
		return 0;
	}
}

static int batt_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *val)
{
	struct batt_chg *chg = power_supply_get_drvdata(psy);
	int rc = 0;

	if (IS_ERR_OR_NULL(val) || IS_ERR_OR_NULL(chg)) {
		pr_err("val or chg is NULL!\n");
		return -ENODEV;
	}
	switch (prop) {
		case POWER_SUPPLY_PROP_STATUS:

			break;

		case POWER_SUPPLY_PROP_HEALTH:

			break;

		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = 1;
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = 49;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:

			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:

			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:

			break;

		case POWER_SUPPLY_PROP_CURRENT_NOW:

			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:

			break;

		case POWER_SUPPLY_PROP_TEMP:
			val->intval = 250;
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:

			break;

		case POWER_SUPPLY_PROP_CHARGE_COUNTER:

			break;

		case POWER_SUPPLY_PROP_CYCLE_COUNT:

			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL:

			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:

			break;

		default:
			rc = -EINVAL;
			pr_err("Unsuppoert prop %d rc = %d\n", prop, rc);
	}

	pr_debug("prop:%d, value = %d\n", prop, val->intval);
	return rc;
}

static int batt_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct batt_chg *chg = power_supply_get_drvdata(psy);
	int rc = 0;

	if (IS_ERR_OR_NULL(val) || IS_ERR_OR_NULL(chg)) {
		pr_err("val or chg is NULL!\n");
		return -ENODEV;
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
			rc = -EINVAL;
			break;
		default:
			rc = -EINVAL;
			pr_err("Unsuppoert prop %d rc = %d\n", prop, rc);
	}
	return rc;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_psy_props,
	.num_properties = ARRAY_SIZE(batt_psy_props),
	.get_property = batt_psy_get_prop,
	.set_property = batt_psy_set_prop,
	.property_is_writeable = batt_psy_prop_is_writeable,
};

static int batt_init_batt_psy(struct batt_chg *chg)
{
	struct power_supply_config batt_cfg = {0};

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return -ENODEV;
	}

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;

	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR_OR_NULL(chg->batt_psy)) {
		pr_err("Failed to register battery power supply\n");
		return -ENODEV;
	}

	return 0;
}

/****************************************************
 ********** BATT PSY REGISTRATION END ***************
 ****************************************************/

static int batt_init_wakeup_source(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return -ENODEV;
	}

	chg->batt_ws = wakeup_source_register(chg->dev, "charge_wakeup");
	if (IS_ERR_OR_NULL(chg->batt_ws)) {
		pr_err("Failed to register wakeup source fail!\n");
		return -ENODEV;
	}
	return 0;
}

static void batt_release_wakeup_source(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return;
	}

	if (IS_ERR_OR_NULL(chg->batt_ws)) {
		pr_err("Failed: batt wakeup source is NULL\n");
		return;
	}

	wakeup_source_unregister(chg->batt_ws);
	pr_info("Wakeup source unregister succeed\n");
	return;
}

static void swchg_jeita_iterm_select(struct batt_chg *chg)
{
	int iterm = 200;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return;
	}

	//TODO: add jeita iterm

	vote(chg->hq_iterm_votable, HQ_ITERM_JEITA, true, iterm);
}

static void swchg_jeita_cv_select(struct batt_chg *chg)
{
	int cv = 4450;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return;
	}

	//TODO: add jeita cv

	vote(chg->hq_cv_votable, HQ_CV_JEITA, true, cv);
}

static void swchg_jeita_cc_select(struct batt_chg *chg)
{
	int jeita_ibat_curr = 500;
	int jeita_ibus_curr= 500;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return;
	}

	//TODO: add jeita cc

	vote(chg->hq_ibus_votable, HQ_IBUS_JEITA, true, jeita_ibus_curr);
	vote(chg->hq_ibat_votable, HQ_IBAT_JEITA, true, jeita_ibat_curr);
}

static void battery_jeita_main(struct batt_chg *chg)
{
	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return;
	}

	swchg_jeita_iterm_select(chg);
	swchg_jeita_cv_select(chg);
	swchg_jeita_cc_select(chg);
	return;
}

static void battery_thermal_main(struct batt_chg *chg)
{
//	int thermal_level = 0;   //todo: need get value from charger_control_limit prop
	int thermal_ibat_curr  = 0;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed: chg is NULL\n");
		return;
	}

	//TODO: need select normal/fast/super thermal tables with different charger type

	vote(chg->hq_ibat_votable, HQ_IBAT_THERMAL, true, thermal_ibat_curr);
	return;
}

static void battery_low_cap_main(struct batt_chg *chg)
{
	//TODO: low capacity logic
	return;
}

static void batt_test_iio_func(struct batt_chg *chg)
{
	int rc = 0, val = 0, i = 0;

	for (i = FG_IIO_ENUM_START; i <= BATT_IIO_CHANNEL_END; i++) {
		rc = batt_read_iio(chg, i, &val);
		if (rc < 0)
			pr_err("Failed to get channel = %d, value = %d, rc = %d\n",
											i, val, rc);
		else
			pr_info("Get iio successful, channel = %d, value = %d\n",
											i, val);
		val = 0;
	}
}

static void batt_chg_main(struct work_struct *work)
{
	int next_time = BATT_MAIN_WORK_PERIOD;    // default time to start next work

	struct batt_chg *chg = container_of(work, struct batt_chg, batt_chg_work.work);
	struct thermal_parametes *hq_thermal_paras = &chg->batt_dt.thermal_table_paras;
	struct jeita_parametes*hq_jeita = &chg->batt_dt.jeita_para;

	if (IS_ERR_OR_NULL(chg)) {
		pr_err("Failed to get chg driver! Start main work again after %ds\n", BATT_MAIN_WORK_PERIOD / 1000);
		goto out;
	}

	batt_update_chg_fg_info(chg);

	battery_charger_type_current_limit(chg);

	if (!chg->is_bringup && hq_jeita->jeita_enable) {
		battery_jeita_main(chg);
	} else {
		pr_debug("Skip jeita function\n");
	}

	if (!chg->is_bringup && hq_thermal_paras->thermal_enable) {
		battery_thermal_main(chg);
	} else {
		pr_debug("Skip thermal function\n");
	}

	battery_low_cap_main(chg);

	batt_test_iio_func(chg);
	//TODO: report psy

out:
	schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(next_time));
}

static int batt_chg_probe(struct platform_device *pdev)
{
	struct batt_chg *batt_chg = NULL;
	struct iio_dev *indio_dev = NULL;
	int rc = -1;

	pr_info("Enter\n");

	if (IS_ERR_OR_NULL(pdev->dev.of_node)) {
		pr_err("Fail to get node:%d \n", __LINE__);
		rc = -ENODEV;
		goto err_1;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct batt_chg));
	if (IS_ERR_OR_NULL(indio_dev)) {
		pr_err("Fail to alloc iio device memory:%d \n", __LINE__);
		rc = -ENOMEM;
		goto err_1;
	}

	batt_chg = iio_priv(indio_dev);
	batt_chg->indio_dev = indio_dev;
	batt_chg->dev = &pdev->dev;
	batt_chg->pdev = pdev;

	rc = batt_init_iio(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize iio, rc=%d\n", rc);
		goto err_2;
	}

	rc = batt_init_voter(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize voter, rc=%d\n", rc);
		goto err_2;
	}

	rc = batt_init_config(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize config, rc=%d\n", rc);
		goto err_3;
	}

	rc = batt_parse_dt(batt_chg);
	if (rc < 0) {
		pr_err("Failed to parse device tree, rc=%d\n", rc);
		goto err_3;
	}

	rc = batt_init_batt_psy(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize batt psy, rc=%d\n", rc);
		goto err_3;
	}

	rc = batt_init_usb_psy(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize usb psy, rc=%d\n", rc);
		goto err_4;
	}

	rc = batt_init_other_class(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize other class, rc=%d\n", rc);
		goto err_5;
	}

	rc = batt_init_wakeup_source(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize wakeup source, rc=%d\n", rc);
		goto err_6;
	}

	INIT_DELAYED_WORK(&batt_chg->batt_init_ic_work, batt_init_ic_info);
	schedule_delayed_work(&batt_chg->batt_init_ic_work, msecs_to_jiffies(BATT_INIT_WORK_PERIOD));

	INIT_DELAYED_WORK(&batt_chg->batt_chg_work, batt_chg_main);
	// Wait 5s for other charge ic probe done
	schedule_delayed_work(&batt_chg->batt_chg_work, msecs_to_jiffies(BATT_MAIN_WORK_PERIOD));

	g_batt_chg = batt_chg;
	platform_set_drvdata(pdev, batt_chg);
	pr_info("batt_chg probe success\n");
	return 0;

err_6:
	batt_release_other_class(batt_chg);

err_5:
	if (!IS_ERR_OR_NULL(batt_chg->usb_psy))
		power_supply_unregister(batt_chg->usb_psy);

err_4:
	if (!IS_ERR_OR_NULL(batt_chg->batt_psy))
		power_supply_unregister(batt_chg->batt_psy);

err_3:
	batt_release_iio(batt_chg);

err_2:
	platform_set_drvdata(pdev, NULL);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 4, 0))
	if (batt_chg && batt_chg->indio_dev)
		devm_iio_device_free(&pdev->dev, batt_chg->indio_dev);
#endif

err_1:
	pr_err("Probe fail\n");
	return rc;
}

static int batt_chg_remove(struct platform_device *pdev)
{
	return 0;
}

static void batt_chg_shutdown(struct platform_device *pdev)
{
	batt_release_wakeup_source(g_batt_chg);

	batt_release_other_class(g_batt_chg);

	if (!IS_ERR_OR_NULL(g_batt_chg->usb_psy))
		power_supply_unregister(g_batt_chg->usb_psy);


	if (!IS_ERR_OR_NULL(g_batt_chg->batt_psy))
		power_supply_unregister(g_batt_chg->batt_psy);

	batt_release_iio(g_batt_chg);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 4, 0))
	if (g_batt_chg && g_batt_chg->indio_dev)
		devm_iio_device_free(&pdev->dev, g_batt_chg->indio_dev);
#endif


	return;
}

static const struct of_device_id batt_chg_dt_match[] = {
	{.compatible = "hq,batt_chg"},
	{},
};
MODULE_DEVICE_TABLE(of, batt_chg_dt_match);

static struct platform_driver batt_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "HQ_CHG",
		.of_match_table = of_match_ptr(batt_chg_dt_match),
	},
	.probe = batt_chg_probe,
	.remove = batt_chg_remove,
	.shutdown = batt_chg_shutdown,
};

static int __init batt_chg_init(void)
{
	int rc = 0;
	g_batt_chg = NULL;

	rc = platform_driver_register(&batt_chg_driver);
	if (rc) {
		pr_err("Failed to register platform driver\n");
		return rc;
	}

	pr_info("Batt_chg_init end\n");
	return rc;
}

static void __exit batt_chg_exit(void)
{
	pr_info("Batt_chg_exit start\n");
	platform_driver_unregister(&batt_chg_driver);
}

module_init(batt_chg_init);
module_exit(batt_chg_exit);

MODULE_AUTHOR("HuaQinTech Inc.");
MODULE_DESCRIPTION("Charge mamnager driver");
MODULE_LICENSE("GPL");
