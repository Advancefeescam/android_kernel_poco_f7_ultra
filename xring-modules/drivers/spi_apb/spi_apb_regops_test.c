// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#define pr_fmt(fmt) "spi_apb regops test: " fmt

#define DEBUG

#include <linux/printk.h>
#include <linux/spi/spi.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include "spi_apb_regops.h"
#include "spi_apb_interface.h"
#include <linux/delay.h>
#include <linux/workqueue.h>

static u32 debug_reg_addr;
static struct dentry *regops_debugfs;

#define CREATE_ST_FILE(p, x) debugfs_create_u32(x##_name, 0644, p, &x)

int reg_read_show(struct seq_file *m, void *unused)
{
	u32 val = 0;
	int ret = 0;

	ret = spi_apb_regops_read(debug_reg_addr, &val);
	if (ret == 0)
		seq_printf(m, "0x%08x = 0x%08x\n", debug_reg_addr, val);
	else
		seq_printf(m, "Read 0x%08x error %d\n", debug_reg_addr, ret);

	return ret;
}
DEFINE_SHOW_ATTRIBUTE(reg_read);

static ssize_t reg_write(struct file *file, const char __user *user_buf,
			 size_t count, loff_t *ppos)
{
	u32 val = 0;
	int ret = 0;

	(void)kstrtouint_from_user(user_buf, count, 16, &val);
	pr_info("will write val = 0x%x\n", val);
	ret = spi_apb_regops_write(debug_reg_addr, val);
	if (ret != 0)
		pr_err("debugfs write 0x%x failed %d\n", debug_reg_addr, ret);

	return ret == 0 ? count : ret;
}

static const struct file_operations reg_write_fops = {
	.open = simple_open,
	.write = reg_write,
};

static int regops_speed_get(void *data, u64 *val)
{
	(void)data;
	*val = spi_apb_regops_get_speed();
	return 0;
}

static int regops_speed_set(void *data, u64 val)
{
	spi_apb_regops_set_speed(val);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(regops_speed_fops, regops_speed_get, regops_speed_set,
			 "%llu\n");

int spi_apb_regops_test_init(struct dentry *debugfs_dir)
{
	struct dentry *debugfs_file;

	pr_info("Into %s\n", __func__);

	regops_debugfs = debugfs_create_dir("spi_apb_regops", debugfs_dir);
	if (!IS_ERR_OR_NULL(regops_debugfs)) {
		debugfs_create_u32("addr", 0644, regops_debugfs,
				   &debug_reg_addr);

		debugfs_file = debugfs_create_file(
			"write", 0200, regops_debugfs, NULL, &reg_write_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("write debugfs init failed %ld\n",
				PTR_ERR(debugfs_file));
			return -ENODEV;
		}

		debugfs_file = debugfs_create_file("read", 0444, regops_debugfs,
						   NULL, &reg_read_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("read debugfs init failed %ld\n",
				PTR_ERR(debugfs_file));
			return -ENODEV;
		}

		debugfs_file =
			debugfs_create_file("speed", 0644, regops_debugfs, NULL,
					    &regops_speed_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("speed debugfs init failed %ld\n",
				PTR_ERR(debugfs_file));
			return -ENODEV;
		}
	}

	pr_info("regops test probe finish!\n");

	return 0;
}

void spi_apb_regops_test_exit(void)
{
	if (!IS_ERR_OR_NULL(regops_debugfs))
		debugfs_remove(regops_debugfs);
}
