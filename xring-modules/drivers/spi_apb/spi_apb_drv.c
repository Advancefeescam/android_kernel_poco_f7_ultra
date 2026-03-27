// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mfd/core.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include "spi_apb_regops.h"
#include <linux/delay.h>
#include <linux/component.h>
#include <linux/debugfs.h>

static const struct of_device_id spi_apb_of_match[] = {
	{
		.compatible = "xring,spi_apb",
	},
	{},
};

static const struct spi_device_id spi_apb_device_id[] = {
	{ "spi_apb", 0 },
	{}
};

static int spi_apb_init(struct spi_device *spi)
{
	int ret = 0;
	struct spi_apb_dev *sdev;
	struct device *dev = &spi->dev;
	const char *debugfs_dir_name;

	spi->mode = SPI_MODE_0;

	sdev = kzalloc(sizeof(*sdev), GFP_KERNEL);
	if (sdev == NULL)
		return -ENOMEM;

	sdev->spi_dev = spi;
	spi_set_drvdata(spi, sdev);

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(dev, "spi_setup failed (%d)!\n", ret);
		goto out;
	}

	ret = spi_apb_regops_init(spi);
	if (ret != 0) {
		dev_err(dev, "spi_apb add regops failed (%d)!\n", ret);
		goto out;
	}

	if (!of_property_read_string(dev->of_node, "debugfs-dir", &debugfs_dir_name)) {
		sdev->debugfs_dir = debugfs_create_dir(debugfs_dir_name, NULL);
		if (IS_ERR_OR_NULL(sdev->debugfs_dir)) {
			pr_err("Fail to create the debugfs_dir dir for debug_fs.\n");
			return -ENODEV;
		}

		ret = spi_apb_regops_test_init(sdev->debugfs_dir);
		if (ret) {
			pr_err("Fail to init regops test\n");
			return -ENODEV;
		}
	}

	return 0;
out:
	return ret;
}

static int spi_apb_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	int ret = 0;

	dev_info(dev, "spi apb probe start");

	ret = spi_apb_init(spi);
	if (ret != 0)
		dev_err(dev, "Probe met error %d, probe not finish.\n", ret);

	return ret;
}

static void spi_apb_remove(struct spi_device *spi)
{
	struct spi_apb_dev *sdev;

	sdev = spi_get_drvdata(spi);

	debugfs_remove(sdev->debugfs_dir);
	sdev->debugfs_dir = NULL;
	spi_apb_regops_deinit(spi);
	kfree(sdev);
}

__maybe_unused static struct spi_driver spi_apb_drv = {
	.driver = {
		.name = "spi_apb",
		.owner = THIS_MODULE,
		.of_match_table = spi_apb_of_match,
	},
	.probe = spi_apb_probe,
	.remove = spi_apb_remove,
	.id_table = spi_apb_device_id,
};

int __init spi_apb_drv_init(void)
{
	return spi_register_driver(&spi_apb_drv);
}

void __exit spi_apb_drv_exit(void)
{
	spi_unregister_driver(&spi_apb_drv);
}

module_init(spi_apb_drv_init);
module_exit(spi_apb_drv_exit);

MODULE_DESCRIPTION("spi_2_apb driver");
MODULE_LICENSE("GPL v2");
