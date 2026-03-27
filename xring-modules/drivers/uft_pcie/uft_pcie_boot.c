// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/pci.h>
#include <linux/resource.h>
#include <linux/types.h>
#include "pcie-designware.h"
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/sizes.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include "../pci/pcie-xring-interface.h"
#include "../pci/pcie-xring.h"
#include <dt-bindings/xring/platform-specific/pcie_ctrl.h>
#include <dt-bindings/xring/platform-specific/hss2_top.h>
#include "../../pci.h"
#include "../pci/dwc_c20pcie4_phy_x2_ns_pcs_raw_ref_100m_ext_rom.h"
#include <dt-bindings/xring/platform-specific/DWC_pcie_unroll_dbi_cpcie_usp_DWC_pcie_ctl_header.h>
#include "uft_pcie_boot.h"
#include <dt-bindings/xring/platform-specific/pcie_phy.h>

u32 ep_ctrl_base;
u32 ep_dbi_base;
u32 ep_atu_base;


static struct delayed_work mod_init_work;
static struct dentry *dfile_uft_pcie_link;
struct device *dev;
static struct dentry *uft_debugfs;
int perstn_gpio;

#define BAR_REG(idx) (ep_dbi_base + BAR0_OFFSET + (idx * 4))
#define BAR_MASK_REG(idx) (ep_dbi_base + BAR0_MASK_OFFSET + (idx * 4))

void uft_pcie_ep_prst_assert(void)
{
	dev_info(dev, "uft ep prst asserted");
	gpio_set_value(perstn_gpio, 0);
	msleep(500);
}

void uft_pcie_ep_prst_deassert(void)
{
	dev_info(dev, "uft ep prst deasserted");
	gpio_set_value(perstn_gpio, 1);
	msleep(150);
}

void pcie_ep_rdonly_wr_switch(bool is_enable)
{
	u32 reg;

	reg = get_pcie_reg32(ep_dbi_base + EP_MISC_CTL_1_OFF);

	if (is_enable)
		reg |= 0x1;
	else
		reg &= (~0x1);

	put_pcie_reg32(reg, ep_dbi_base + EP_MISC_CTL_1_OFF);
}


void uft_pcie_ep_init(void)
{
	u32 reg;

	dev_info(dev, "uft_pcie_ep_init");

	pcie_ep_rdonly_wr_switch(true);

	/* change ep device id and vendor id */
	put_pcie_reg32(DEV_VEN_ID, ep_dbi_base + DEV_VEN_ID_OFFSET);

	/* change max gen speed*/
	reg = get_pcie_reg32(ep_dbi_base + LINK_CONTROL2_LINK_STATUS2_REG);
	reg &= ~GENMASK(3, 0);
	reg |= 1;
	put_pcie_reg32(reg, ep_dbi_base + LINK_CONTROL2_LINK_STATUS2_REG);

	pcie_ep_rdonly_wr_switch(false);
}

void pcie_ep_config_bar(void)
{
	dev_info(dev, "uft_pcie_boot:config uft ep bars");

	/*bar0 size 16M,type mem/32bit/non prefetchable */
	put_pcie_reg32(BAR_SIZE_16M, BAR_MASK_REG(0));
	put_pcie_reg32(MEM_BAR_TYPE, BAR_REG(0));

	/*bar1 size 64k,type mem/32bit/non prefetchable */
	put_pcie_reg32(BAR_SIZE_64K, BAR_MASK_REG(1));
	put_pcie_reg32(MEM_BAR_TYPE, BAR_REG(1));

	/*bar2 size 64k,type mem/32bit/non prefetchable*/
	put_pcie_reg32(BAR_SIZE_64K, BAR_MASK_REG(2));
	put_pcie_reg32(MEM_BAR_TYPE, BAR_REG(2));

	/*bar3 size 64k,type mem/32bit/non prefetchable*/
	put_pcie_reg32(BAR_SIZE_64K, BAR_MASK_REG(3));
	put_pcie_reg32(MEM_BAR_TYPE, BAR_REG(3));

	/*bar4 size 64k,type mem/32bit/non prefetchable*/
	put_pcie_reg32(BAR_SIZE_64K, BAR_MASK_REG(4));
	put_pcie_reg32(MEM_BAR_TYPE, BAR_REG(4));

	/*bar5 size 64k,type mem/32bit/non prefetchable*/
	put_pcie_reg32(BAR_SIZE_64K, BAR_MASK_REG(5));
	put_pcie_reg32(MEM_BAR_TYPE, BAR_REG(5));
}

void pcie_ep_config_iatu(void)
{
	uint32_t ctrl_2_reg = 0;

	dev_info(dev, "uft_pcie_boot:config ep iatu");

	/*iatu0----bar0, target address= 0x80000000, for ddr access*/
	/*target address*/
	put_pcie_reg32(0x80000000, ep_atu_base + IATU_LWR_TARGET_ADDR_OFF_INBOUND_1);
	/*mem type*/
	put_pcie_reg32(0x0, ep_atu_base + IATU_REGION_CTRL_1_OFF_INBOUND_1);
	/*bar match mode*/
	ctrl_2_reg |= 1 << IATU_REGION_CTRL_2_OFF_INBOUND_0_MATCH_MODE_BitAddressOffset;
	/*enable atu*/
	ctrl_2_reg |= 1 << IATU_REGION_CTRL_2_OFF_INBOUND_0_REGION_EN_BitAddressOffset;
	/*bar 0*/
	ctrl_2_reg |= 0 << IATU_REGION_CTRL_2_OFF_INBOUND_0_BAR_NUM_BitAddressOffset;

	put_pcie_reg32(ctrl_2_reg, ep_atu_base + IATU_REGION_CTRL_2_OFF_INBOUND_1);

	ctrl_2_reg = 0;
	/*iatu3----bar2, target address= 0x20000000, for reg access*/
	/*target address*/
	put_pcie_reg32(0x20000000, ep_atu_base + IATU_LWR_TARGET_ADDR_OFF_INBOUND_3);
	/*mem type*/
	put_pcie_reg32(0x0, ep_atu_base + IATU_REGION_CTRL_1_OFF_INBOUND_3);
	/*bar match mode*/
	ctrl_2_reg |= 1 << IATU_REGION_CTRL_2_OFF_INBOUND_2_MATCH_MODE_BitAddressOffset;
	/*enable atu*/
	ctrl_2_reg |= 1 << IATU_REGION_CTRL_2_OFF_INBOUND_2_REGION_EN_BitAddressOffset;
	/*bar 2*/
	ctrl_2_reg |= 2 << IATU_REGION_CTRL_2_OFF_INBOUND_2_BAR_NUM_BitAddressOffset;

	put_pcie_reg32(ctrl_2_reg, ep_atu_base + IATU_REGION_CTRL_2_OFF_INBOUND_3);
}

void ep_config_addr_trans(void)
{
	dev_info(dev, "start: config ep address translation");

	pcie_ep_rdonly_wr_switch(true);

	pcie_ep_config_bar();
	pcie_ep_config_iatu();

	pcie_ep_rdonly_wr_switch(false);
}

static void uft_pcie_start_link(void)
{
	uft_pcie_ep_prst_assert();
	uft_pcie_ep_prst_deassert();

	uft_pcie_ep_init();

	ep_config_addr_trans();

	xring_pcie_probe_port_by_port(1);
}

static void uft_pcie_stop_link(void)
{
	xring_pcie_remove_port_by_port(1);
	uft_pcie_ep_prst_assert();
}

static ssize_t  uft_debugfs_pcie_linkops(struct file *file,
				const char __user *buf, size_t count, loff_t *ppos)
{
	char input_parm[10];
	int ret;
	char *p;
	int cmd = 0;

	ret = copy_from_user(input_parm, buf, count);
	input_parm[count] = '\0';
	p = input_parm;
	ret = kstrtoint(p, 10, &cmd);

	if (cmd)
		uft_pcie_start_link();
	else
		uft_pcie_stop_link();

	return count;
}

static const struct file_operations uft_debugfs_pcie_link = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = uft_debugfs_pcie_linkops,
};

static int uft_pcie_boot_probe(struct platform_device *pdev)
{
	struct device_node *np;
	int ret;

	dev = &pdev->dev;
	np = dev->of_node;

	ret = of_property_read_u32(np, "ep-ctrl-addr", &ep_ctrl_base);
	if (ret)
		dev_err(dev, "failed to get ep_ctrl_base\n");
	else
		dev_info(dev, "ep_ctrl_base:0x%x\n", ep_ctrl_base);

	ret = of_property_read_u32(np, "ep-dbi-addr", &ep_dbi_base);
	if (ret)
		dev_err(dev, "failed to get ep_dbi_base\n");
	else
		dev_info(dev, "ep_dbi_base:0x%x\n", ep_dbi_base);

	ret = of_property_read_u32(np, "ep-atu-addr", &ep_atu_base);
	if (ret)
		dev_err(dev, "failed to get ep_atu_base\n");
	else
		dev_info(dev, "ep_atu_base:0x%x\n", ep_atu_base);


	perstn_gpio = of_get_named_gpio(np, "perstn-gpios", 0);

	if (perstn_gpio < 0) {
		dev_err(dev, "Failed to request GPIO perst_n\n");
		return perstn_gpio;
	}

	ret = gpio_direction_output(perstn_gpio, 0);
	if (ret < 0)
		dev_err(dev, "gpio_direction_output failed:%d\n", ret);

	uft_debugfs = debugfs_lookup("uft_fpga", NULL);
	if (IS_ERR_OR_NULL(uft_debugfs)) {
		pr_err("fail to lookup the uft_fpga dir for debug_fs.\n");
		return -ENODEV;
	}

	dfile_uft_pcie_link = debugfs_lookup("pcie_link", uft_debugfs);
	if (IS_ERR_OR_NULL(dfile_uft_pcie_link)) {
		dfile_uft_pcie_link = debugfs_create_file("pcie_link", 0664,
								uft_debugfs, NULL,
								&uft_debugfs_pcie_link);
		if (IS_ERR_OR_NULL(dfile_uft_pcie_link)) {
			pr_err("fail to create the pcie_link file for debug_fs.\n");
			return -ENODEV;
		}
	}

	xring_pcie_remove_port_by_port(1);

	uft_pcie_start_link();

	ret = register_ep_driver();

	return ret;
}

static int uft_pcie_boot_remove(struct platform_device *pdev)
{
	uft_pcie_stop_link();
	unregister_ep_driver();

	return 0;
}

static const struct of_device_id uft_pcie_ep_boot_of_match[] = {
	{ .compatible = "xring,f3-uft-ep-boot" },
	{},
};

static struct platform_driver uft_ep_boot_driver = {
	.driver = {
		.name	= "uft_pcie_ep_boot",
		.of_match_table = uft_pcie_ep_boot_of_match,
	},
	.probe = uft_pcie_boot_probe,
	.remove = uft_pcie_boot_remove,
};

__maybe_unused static void uft_pcie_boot_init_work(struct work_struct *w)
{
	pr_info("Into %s\n", __func__);

	if (!spi_apb_ready()) {
		pr_info("%s waiting spi_apb_ready\n", __func__);

		mod_delayed_work(system_wq, &mod_init_work,
				 msecs_to_jiffies(3000));

		return;
	}

	pr_info("%s: spi_apb_ready\n", __func__);

	platform_driver_register(&uft_ep_boot_driver);
}

static int __init uft_pcie_boot_init(void)
{

	pr_info("Into %s\n", __func__);

	INIT_DELAYED_WORK(&mod_init_work, uft_pcie_boot_init_work);

	schedule_delayed_work(&mod_init_work, 0);

	return 0;
}
module_init(uft_pcie_boot_init);

static void __exit uft_pcie_boot_exit(void)
{
	platform_driver_unregister(&uft_ep_boot_driver);
}
module_exit(uft_pcie_boot_exit);

MODULE_LICENSE("GPL v2");
