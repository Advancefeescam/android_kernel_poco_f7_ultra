#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
//#include <linux/uidgid.h>
#include <linux/slab.h>
#include <linux/iio/consumer.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>

//static kuid_t uid = KUIDT_INIT(0);
//static kgid_t gid = KGIDT_INIT(1000);

struct blk_thermal_info {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct iio_channel *channel;
	s32 *lookup_table;
	int nlookup_table;
};
/*
static DEFINE_MUTEX(MTM_DRV_THERM_PROC_DIR_LOCK);
static struct proc_dir_entry *proc_drv_therm_dir_entry;

struct proc_dir_entry *mtk_thermal_get_proc_drv_therm_dir_entry(void)
{
	mutex_lock(&MTM_DRV_THERM_PROC_DIR_LOCK);
	if (proc_drv_therm_dir_entry == NULL) {
		proc_drv_therm_dir_entry = proc_mkdir("driver/thermal", NULL);
		if (proc_drv_therm_dir_entry == NULL)
			printk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	}
	mutex_unlock(&MTM_DRV_THERM_PROC_DIR_LOCK);
	return proc_drv_therm_dir_entry;
}*/

static int blk_thermal_adc_to_temp(struct blk_thermal_info *bti, int val)
{
	int temp, temp_hi, temp_lo, adc_hi, adc_lo;
	int i;

	if (!bti->lookup_table)
		return val;

	for (i = 0; i < bti->nlookup_table; i++) {
		if (val >= bti->lookup_table[2 * i + 1])
			break;
	}

	if (i == 0) {
		temp = bti->lookup_table[0];
	} else if (i >= bti->nlookup_table) {
		temp = bti->lookup_table[2 * (bti->nlookup_table - 1)];
	} else {
		adc_hi = bti->lookup_table[2 * i - 1];
		adc_lo = bti->lookup_table[2 * i + 1];

		temp_hi = bti->lookup_table[2 * i - 2];
		temp_lo = bti->lookup_table[2 * i];

		temp = temp_hi + mult_frac(temp_lo - temp_hi, val - adc_hi,
					   adc_lo - adc_hi);
	}

	return temp;
}

static int blk_thermal_get_temp(void *data, int *temp)
{
	struct blk_thermal_info *bti = data;
	int val;
	int ret;

	ret = iio_read_channel_processed(bti->channel, &val);
	if (ret < 0) {
		dev_err(bti->dev, "IIO channel read failed %d\n", ret);
		return ret;
	}
	*temp = blk_thermal_adc_to_temp(bti, val);

	return 0;
}

static const struct thermal_zone_of_device_ops blk_thermal_ops = {
	.get_temp = blk_thermal_get_temp,
};

static int blk_thermal_read_linear_lookup_table(struct device *dev,
						 struct blk_thermal_info *bti)
{
	struct device_node *np = dev->of_node;
	int ntable;
	int ret;

	ntable = of_property_count_elems_of_size(np, "temperature-lookup-table",
						 sizeof(u32));
	if (ntable <= 0) {
		dev_err(dev, "lookup table is not found\n");
		return 0;
	}

	if (ntable % 2) {
		dev_err(dev, "Pair of temperature vs ADC read value missing\n");
		return -EINVAL;
	}

	bti->lookup_table = devm_kcalloc(dev,
					 ntable, sizeof(*bti->lookup_table),
					 GFP_KERNEL);
	if (!bti->lookup_table)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "temperature-lookup-table",
					 (u32 *)bti->lookup_table, ntable);
	if (ret < 0) {
		dev_err(dev, "Failed to read temperature lookup table: %d\n",
			ret);
		return ret;
	}

	bti->nlookup_table = ntable / 2;

	return 0;
}

static int blk_thermal_probe(struct platform_device *pdev)
{
	struct blk_thermal_info *bti;
	int ret;
	/*
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_blkntc_dir = NULL;

	mtkts_blkntc_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_blkntc_dir) {
		printk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	} else {
		entry = proc_create("backlight_therm_ntc", 0664, mtkts_blkntc_dir,
							&mtkts_blkntc_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("backlight_therm_ntc_param", 0664, mtkts_blkntc_dir,
							&mtkts_blkntc_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}*/

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	bti = devm_kzalloc(&pdev->dev, sizeof(*bti), GFP_KERNEL);
	if (!bti)
		return -ENOMEM;

	bti->channel = devm_iio_channel_get(&pdev->dev, "blk-adc-channel");
	if (IS_ERR(bti->channel)) {
		ret = PTR_ERR(bti->channel);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "IIO channel not found: %d\n", ret);
		return ret;
	}

	ret = blk_thermal_read_linear_lookup_table(&pdev->dev, bti);
	if (ret < 0)
		return ret;

	bti->dev = &pdev->dev;
	platform_set_drvdata(pdev, bti);

	bti->tz_dev = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, bti,
							   &blk_thermal_ops);
	if (IS_ERR(bti->tz_dev)) {
		ret = PTR_ERR(bti->tz_dev);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Thermal zone sensor register failed: %d\n",
				ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id of_adc_thermal_match[] = {
	{ .compatible = "blk-adc-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, of_adc_thermal_match);

static struct platform_driver blk_thermal_driver = {
	.driver = {
		.name = "blk-adc-thermal",
		.of_match_table = of_adc_thermal_match,
	},
	.probe = blk_thermal_probe,
};

module_platform_driver(blk_thermal_driver);

MODULE_LICENSE("GPL v2");
