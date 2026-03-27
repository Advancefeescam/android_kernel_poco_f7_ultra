// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023, X-Ring technologies Inc., All rights reserved.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci_regs.h>
#include <linux/types.h>
#include <dt-bindings/xring/platform-specific/pcie_phy.h>
#include "../pci/pcie-xring-interface.h"
#include "uft_pcie_boot.h"
#include "../pci/dwc_c20pcie4_phy_x2_ns_pcs_raw_ref_100m_ext_rom.h"
#include "uft_pcie_ep.h"
#include <dt-bindings/xring/platform-specific/DWC_pcie_unroll_dbi_cpcie_usp_DWC_pcie_ctl_header.h>
#include <linux/string.h>
#include "xring_pcie_hdma.h"

void memcpy_to_ep(void __iomem *to, const void *from, u32 count)
{
	u32 __iomem *dst = to;
	const u32 *src = from;
	const u32 *end = src + count / 4;

	while (src < end)
		writel(*src++, dst++);
}

void xring_pcie_writel_atu_ib(struct uft_pcie_ep *ep, u32 index, u32 reg, u32 val)
{
	void __iomem *base;

	base = ep->atu_base + PCIE_ATU_UNROLL_BASE(PCIE_ATU_REGION_DIR_IB, index);
	writel(val, base + reg);
}

u32 xring_pcie_readl_atu_ib(struct uft_pcie_ep *ep, u32 index, u32 reg)
{
	void __iomem *base;

	base = ep->atu_base + PCIE_ATU_UNROLL_BASE(PCIE_ATU_REGION_DIR_IB, index);
	return readl(base + reg);
}

void xring_pcie_writel_atu_ob(struct uft_pcie_ep *ep, u32 index, u32 reg, u32 val)
{
	void __iomem *base;

	base = ep->atu_base + PCIE_ATU_UNROLL_BASE(PCIE_ATU_REGION_DIR_OB, index);
	writel(val, base + reg);
}

u32 xring_pcie_readl_atu_ob(struct uft_pcie_ep *ep, u32 index, u32 reg)
{
	void __iomem *base;

	base = ep->atu_base + PCIE_ATU_UNROLL_BASE(PCIE_ATU_REGION_DIR_OB, index);
	return readl(base + reg);
}

uint32_t uft_pcie_mem_read(struct uft_pcie_ep *ep, int bar_id,
					uint32_t offset)
{
	struct uft_pcie_bar_ctrl *uft_bar = ep->uft_bars[bar_id];

	return readl(uft_bar->bar_virt + offset);
}

void uft_pcie_mem_write(struct uft_pcie_ep *ep, int bar_id,
					uint32_t offset, uint32_t val)
{
	struct uft_pcie_bar_ctrl *uft_bar = ep->uft_bars[bar_id];

	writel(val, uft_bar->bar_virt + offset);
}

int xring_pcie_alloc_ib_atu(struct uft_pcie_ep *ep, int bar, u32 target_addr)
{
	struct pci_dev *pdev = ep->pdev;
	struct device *dev = &pdev->dev;
	struct uft_pcie_bar_ctrl *uft_bar = ep->uft_bars[bar];
	u16 idx;
	u32 ctrl_2_reg = 0, val;
	int retries;

	idx = find_first_zero_bit((const unsigned long *)&ep->ib_atu_map, UFT_ATU_NUM);

	/*target address*/
	xring_pcie_writel_atu_ib(ep, idx, PCIE_ATU_LOWER_TARGET, target_addr);

	/*check type*/
	if (pci_resource_flags(ep->pdev, bar) & (IORESOURCE_IO))
		xring_pcie_writel_atu_ib(ep, idx, PCIE_ATU_REGION_CTRL1, PCIE_ATU_TYPE_IO);
	else if (pci_resource_flags(ep->pdev, bar) & (IORESOURCE_MEM))
		xring_pcie_writel_atu_ib(ep, idx, PCIE_ATU_REGION_CTRL1, PCIE_ATU_TYPE_MEM);
	else {
		dev_err(dev, "bar num invalid");
		return -1;
	}

	/*bar match mode/enable atu*/
	ctrl_2_reg |= PCIE_ATU_BAR_MODE_ENABLE | PCIE_ATU_ENABLE;
	/*bar num*/
	ctrl_2_reg |= bar << IATU_REGION_CTRL_2_OFF_INBOUND_0_BAR_NUM_BitAddressOffset;

	xring_pcie_writel_atu_ib(ep, idx, PCIE_ATU_REGION_CTRL2, ctrl_2_reg);

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = xring_pcie_readl_atu_ib(ep, idx, PCIE_ATU_REGION_CTRL2);

		if (val & PCIE_ATU_ENABLE) {
			uft_bar->ib_target_addr_phys = target_addr;
			ep->ib_atu_map |= BIT(idx);
			uft_bar->ib_atu_enabled = true;
			uft_bar->ib_atu_idx = idx;

			return 0;
		}
		mdelay(LINK_WAIT_IATU);
	}

	dev_err(dev, "Inbound iATU is not being enabled\n");

	return -1;
}

int xring_prog_ib_atu(struct uft_pcie_ep *ep, int bar, u32 target_addr)
{
	struct pci_dev *pdev = ep->pdev;
	struct device *dev = &pdev->dev;
	struct uft_pcie_bar_ctrl *uft_bar = ep->uft_bars[bar];
	u16 idx;

	/*NOTICE!!! target address must align by bar_size*/
	if (!IS_ALIGNED(target_addr, uft_bar->bar_size)) {
		dev_err(dev, "Error:target address must align by bar_size!");
		return -1;
	}

	if (uft_bar->ib_atu_enabled) {
		idx = uft_bar->ib_atu_idx;

		xring_pcie_writel_atu_ib(ep, idx, PCIE_ATU_LOWER_TARGET, target_addr);
		uft_bar->ib_target_addr_phys = target_addr;

		return 0;
	}

	/*no atu found*/
	dev_info(dev, "alloc new ib atu for bar %d", bar);
	return xring_pcie_alloc_ib_atu(ep, bar, target_addr);
}

static int xring_get_inbound_atu_info(struct uft_pcie_ep *ep, int bar)
{
	u32 ctrl_2_reg;
	u32 target_addr;
	struct uft_pcie_bar_ctrl *uft_bar = ep->uft_bars[bar];

	for (int idx = 0; idx < UFT_ATU_NUM; idx++) {
		ctrl_2_reg = xring_pcie_readl_atu_ib(ep, idx, PCIE_ATU_REGION_CTRL2);

		if ((ctrl_2_reg & PCIE_ATU_ENABLE) &&
			(ctrl_2_reg & ATU_BAR_MATCH_MODE_MASK) &&
			(bar == ATU_BAR_NUM(ctrl_2_reg))) {
			target_addr = xring_pcie_readl_atu_ib(ep, idx,  PCIE_ATU_LOWER_TARGET);

			uft_bar->ib_target_addr_phys = target_addr;
			uft_bar->ib_atu_enabled = true;
			uft_bar->ib_atu_idx = idx;

			ep->ib_atu_map |= BIT(idx);

			return 0;
		}
	}
	return -1;
}

int xring_update_ob_atu_info(struct uft_pcie_ep *ep, int atu_idx)
{
	struct pci_dev *pdev = ep->pdev;
	struct device *dev = &pdev->dev;
	struct uft_pcie_ob_ctrl *ob_ctrl = ep->uft_ob_res[atu_idx];
	u32 ctrl_2_reg;

	ctrl_2_reg = xring_pcie_readl_atu_ob(ep, atu_idx, PCIE_ATU_REGION_CTRL2);
	ob_ctrl->ob_atu_enabled = (ctrl_2_reg & PCIE_ATU_ENABLE) ? true : false;
	ob_ctrl->rc_addr = xring_pcie_readl_atu_ob(ep, atu_idx, PCIE_ATU_LOWER_TARGET);
	ob_ctrl->ep_addr = xring_pcie_readl_atu_ob(ep, atu_idx, PCIE_ATU_LOWER_BASE);
	ob_ctrl->size = xring_pcie_readl_atu_ob(ep, atu_idx, PCIE_ATU_LIMIT) - ob_ctrl->ep_addr;

	dev_info(dev, "%s:idx:%d,rc_addr:%x,ep addr:%x,size:%x",
				__func__,
				atu_idx,
				ob_ctrl->rc_addr,
				ob_ctrl->ep_addr,
				ob_ctrl->size);

	return 0;
}

void xring_prog_ob_atu(struct uft_pcie_ep *ep, int idx, u32 ep_addr, u32 rc_addr, u32 size)
{
	xring_pcie_writel_atu_ob(ep, idx, PCIE_ATU_LOWER_BASE, ep_addr);
	xring_pcie_writel_atu_ob(ep, idx, PCIE_ATU_LIMIT, ep_addr + size);
	xring_pcie_writel_atu_ob(ep, idx, PCIE_ATU_LOWER_TARGET, rc_addr);
	xring_pcie_writel_atu_ob(ep, idx, PCIE_ATU_REGION_CTRL2, PCIE_ATU_ENABLE);

	xring_update_ob_atu_info(ep, idx);
}
EXPORT_SYMBOL_GPL(xring_prog_ob_atu);

int xring_pcie_alloc_ob_atu(struct uft_pcie_ep *ep, u32 ep_addr, u32 rc_addr, u32 size)
{
	struct uft_pcie_ob_ctrl *ob_ctrl;
	int atu_idx;
	struct pci_dev *pdev = ep->pdev;
	struct device *dev = &pdev->dev;

	for (atu_idx = 0; atu_idx < UFT_ATU_NUM; atu_idx++) {
		ob_ctrl = ep->uft_ob_res[atu_idx];
		if (!ob_ctrl->ob_atu_enabled) {
			xring_prog_ob_atu(ep, atu_idx, ep_addr, rc_addr, size);
			return atu_idx;
		}
	}

	dev_err(dev, "no outbound atu found!");

	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(xring_pcie_alloc_ob_atu);

int xring_pcie_ob_atu_init(struct uft_pcie_ep *ep)
{
	int atu_idx;
	struct pci_dev *pdev = ep->pdev;
	struct device *dev = &pdev->dev;

	for (atu_idx = 0; atu_idx < UFT_ATU_NUM; atu_idx++) {
		ep->uft_ob_res[atu_idx] = devm_kzalloc(dev, sizeof(struct uft_pcie_ob_ctrl), GFP_KERNEL);

		xring_update_ob_atu_info(ep, atu_idx);
	}

	return 0;
}

int mem_update_by_sliding_win(struct uft_pcie_ep *ep, char *data, u32 size, u32 target_addr)
{
	struct uft_pcie_bar_ctrl *uft_bar = ep->uft_bars[SLIDE_WINDOW_BAR];
	u32 bar_size = uft_bar->bar_size;
	int count, i;
	int idx = 0;
	u32 remainder, bar_mem_left, offset, bar_base;
	struct pci_dev *pdev = ep->pdev;

	if (!IS_ALIGNED(size, 4)) {
		dev_err(&pdev->dev, "unaligned size: %x", size);
		return -1;
	}

	if (!IS_ALIGNED(target_addr, 4)) {
		dev_err(&pdev->dev, "unaligned target_addr: %x", target_addr);
		return -1;
	}

	/* first: update unaligned part */
	bar_base = ALIGN_DOWN(target_addr, bar_size);

	xring_prog_ib_atu(ep, SLIDE_WINDOW_BAR, bar_base);
	offset = target_addr - bar_base;
	bar_mem_left = bar_size - offset;

	if (size <= bar_mem_left)
		memcpy_to_ep(uft_bar->bar_virt + offset, &data[idx], size);
	else {
		memcpy_to_ep(uft_bar->bar_virt + offset, &data[idx], bar_mem_left);

		/* update aligned part */
		idx += bar_mem_left;
		size = size - bar_mem_left;
		count = size / bar_size;
		remainder = size % bar_size;

		for (i = 0; i < count; i++) {
			bar_base += bar_size;
			xring_prog_ib_atu(ep, SLIDE_WINDOW_BAR, bar_base);
			memcpy_to_ep(uft_bar->bar_virt, &data[idx], bar_size);
			idx += bar_size;
		}

		if (remainder) {
			/* update left part */
			bar_base += bar_size;
			xring_prog_ib_atu(ep, SLIDE_WINDOW_BAR, bar_base);

			memcpy_to_ep(uft_bar->bar_virt, &data[idx], remainder);
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mem_update_by_sliding_win);

void uft_pcie_write(struct uft_pcie_ep *ep, u32 reg, u32 val, int bar_id)
{
	struct pci_dev *pdev = ep->pdev;
	struct uft_pcie_bar_ctrl *access_bar = ep->uft_bars[bar_id];
	u32 bar_size = access_bar->bar_size;
	u32 bar_base = ALIGN_DOWN(reg, bar_size);

	if (!IS_ALIGNED(reg, 4)) {
		dev_err(&pdev->dev, "unaligned address: %x", reg);
		return;
	}

	if (access_bar->ib_target_addr_phys != bar_base)
		xring_prog_ib_atu(ep, bar_id, bar_base);

	uft_pcie_mem_write(ep, bar_id, reg - bar_base, val);
}
EXPORT_SYMBOL_GPL(uft_pcie_write);

u32 uft_pcie_read(struct uft_pcie_ep *ep, u32 reg, int bar_id)
{
	struct pci_dev *pdev = ep->pdev;
	struct uft_pcie_bar_ctrl *access_bar = ep->uft_bars[bar_id];
	u32 bar_size = access_bar->bar_size;
	u32 bar_base = ALIGN_DOWN(reg, bar_size);

	if (!IS_ALIGNED(reg, 4)) {
		dev_err(&pdev->dev, "unaligned address: %x", reg);
		return 0xffffffff;
	}

	if (access_bar->ib_target_addr_phys != bar_base)
		xring_prog_ib_atu(ep, bar_id, bar_base);

	return uft_pcie_mem_read(ep, bar_id, reg - bar_base);
}
EXPORT_SYMBOL_GPL(uft_pcie_read);

void uft_pcie_ep_reg_write(struct uft_pcie_ep *ep, u32 reg, u32 val)
{
	uft_pcie_write(ep, reg, val, REG_ACCESS_BAR);
}
EXPORT_SYMBOL_GPL(uft_pcie_ep_reg_write);

u32 uft_pcie_ep_reg_read(struct uft_pcie_ep *ep, u32 reg)
{
	return uft_pcie_read(ep, reg, REG_ACCESS_BAR);
}
EXPORT_SYMBOL_GPL(uft_pcie_ep_reg_read);

void uft_pcie_ep_ddr_write(struct uft_pcie_ep *ep, u32 addr, u32 val)
{
	uft_pcie_write(ep, addr, val, DDR_ACCESS_BAR);
}
EXPORT_SYMBOL_GPL(uft_pcie_ep_ddr_write);

u32 uft_pcie_ep_ddr_read(struct uft_pcie_ep *ep, u32 addr)
{
	return uft_pcie_read(ep, addr, DDR_ACCESS_BAR);
}
EXPORT_SYMBOL_GPL(uft_pcie_ep_ddr_read);

static void show_4k_bar(struct pci_dev *pdev, int bar)
{
	struct device *dev = &pdev->dev;
	struct uft_pcie_ep *uft_pcie_ep = pci_get_drvdata(pdev);
	int offset = 0;

	dev_info(dev, "\nbar%d:\n", bar);

	while (offset < BAR_SIZE) {
		dev_info(dev, "0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", offset,
			ioread32(uft_pcie_ep->uft_bars[bar]->bar_virt + offset),
			ioread32(uft_pcie_ep->uft_bars[bar]->bar_virt + offset + OFFSET_04),
			ioread32(uft_pcie_ep->uft_bars[bar]->bar_virt + offset + OFFSET_08),
			ioread32(uft_pcie_ep->uft_bars[bar]->bar_virt + offset + OFFSET_12));

		offset = offset + OFFSET_16;
	}
}

static ssize_t read_bar_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct uft_pcie_ep *uft_pcie_ep = pci_get_drvdata(pdev);
	int bar = 0;

	for (bar = 0; bar < BAR_4; bar++) {
		if (uft_pcie_ep->uft_bars[bar]->bar_virt)
			show_4k_bar(pdev, bar);
	}

	return 0;
}

static DEVICE_ATTR_RO(read_bar);

static bool uft_alloc_irq_vectors(struct pci_dev *pdev,
				struct uft_pcie_ep *priv)
{
	struct device *dev = &pdev->dev;
	int irq = -1;

	irq = pci_alloc_irq_vectors(pdev, 1, 32, PCI_IRQ_MSI);
	if (irq < 0) {
		dev_err(dev, "Failed to get MSI interrupts!\n");
		return false;
	}

	dev_info(dev, "the num of msi(x) irqs is %d\n", irq);
	priv->num_irq = irq;

	return true;
}

static int uft_pcie_ep_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct uft_pcie_ep *uft_pcie_ep;
	struct device *dev = &pdev->dev;
	enum pci_barno bar;
	struct uft_pcie_bar_ctrl *uft_bar;
	void __iomem *base;
	int ret;

	dev_info(dev, "in uft pcie ep probe!");

	uft_pcie_ep = devm_kzalloc(dev, sizeof(*uft_pcie_ep), GFP_KERNEL);
	if (!uft_pcie_ep) {
		ret = -ENOMEM;
		goto out;
	}

	uft_pcie_ep->pdev = pdev;

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(dev, "Cannot enable PCI device\n");
		goto out;
	}

	pci_set_master(pdev);

	/*get atu address first*/
	uft_pcie_ep->atu_base = pci_iomap(pdev, HDMA_ACCESS_BAR, 0) + ATU_OFFSET;

	for (bar = 0; bar < PCI_STD_NUM_BARS; bar++) {
		uft_pcie_ep->uft_bars[bar] = devm_kzalloc(dev, sizeof(struct uft_pcie_bar_ctrl), GFP_KERNEL);

		if (!uft_pcie_ep->uft_bars[bar]) {
			ret = -ENOMEM;
			goto out;
		}

		uft_bar = uft_pcie_ep->uft_bars[bar];

		if (pci_resource_flags(pdev, bar) & (IORESOURCE_MEM | IORESOURCE_IO)) {
			/*set bar info*/
			uft_bar->bar_phys = pci_resource_start(pdev, bar);
			uft_bar->bar_size = pci_resource_len(pdev, bar);

			base = pci_iomap(pdev, bar, 0);
			if (!base)
				dev_err(dev, "Failed to read BAR%d\n", bar);

			uft_bar->bar_virt = base;

			/*get ib_atu info */
			ret = xring_get_inbound_atu_info(uft_pcie_ep, bar);
			if (ret)
				dev_info(dev, "Warning:bar%d has no inbound atu yet\n", bar);

			dev_info(dev, "bar_info:bar_num%d, phys:%x, size:%x, atu: %x",
						bar,
						uft_bar->bar_phys,
						uft_bar->bar_size,
						uft_bar->ib_target_addr_phys);
		}
	}

	xring_pcie_ob_atu_init(uft_pcie_ep);

	/*alloc msi irq*/
	ret = uft_alloc_irq_vectors(pdev, uft_pcie_ep);
	if (!ret) {
		dev_err(dev, "msi irq alloc failed %d\n", ret);
		goto err_disable_irq;
	}

	/*pcie hdma init*/
	uft_pcie_ep->pdma = xring_pcie_hdma_init(uft_pcie_ep);
	if (IS_ERR_OR_NULL(uft_pcie_ep->pdma)) {
		ret = PTR_ERR(uft_pcie_ep->pdma);
		dev_err(dev, "pcie hdma init failed: %d\n", ret);
		goto err_disable_irq;
	}

	uft_pcie_ep->sysfs_ctl = devm_kzalloc(dev, sizeof(struct uft_sysfs_ctrl), GFP_KERNEL);
	if (!uft_pcie_ep->sysfs_ctl) {
		ret = -ENOMEM;
		goto out;
	}

	ret = uft_pcie_add_debugfs(uft_pcie_ep);
	if (ret) {
		dev_err(dev, "Failed to add debugfs:%d\n", ret);
		goto out;
	}

	if (device_create_file(dev, &dev_attr_read_bar) < 0)
		dev_err(dev, "Failed to create file read_bar\n");

	pci_set_drvdata(pdev, uft_pcie_ep);

	return 0;

err_disable_irq:
	pci_free_irq_vectors(pdev);

out:
	return ret;

}

static void uft_pcie_ep_remove(struct pci_dev *pdev)
{
	struct uft_pcie_ep *uft_pcie_ep = pci_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	struct uft_pcie_bar_ctrl *uft_bar;
	int bar;

	for (bar = 0; bar < PCI_STD_NUM_BARS; bar++) {
		uft_bar = uft_pcie_ep->uft_bars[bar];

		if (uft_bar->bar_virt)
			pci_iounmap(pdev, uft_bar->bar_virt);
	}

	device_remove_file(dev, &dev_attr_read_bar);
	pci_free_irq_vectors(pdev);
	uft_pcie_remove_debugfs();

	uft_pcie_ep->num_irq = 0;

	pci_disable_device(pdev);
}

static const struct pci_device_id uft_pcie_ep_tbl[] = {
	{ PCI_DEVICE(PCI_VENDER_ID_UFT, PCI_DEVICE_ID_UFT) },
	{ 0, }	/* end of table */
};
MODULE_DEVICE_TABLE(pci, uft_pcie_ep_tbl);

static struct pci_driver uft_pcie_ep_driver = {
	.name		= "uft-pcie-ep",
	.id_table	= uft_pcie_ep_tbl,
	.probe		= uft_pcie_ep_probe,
	.remove		= uft_pcie_ep_remove,
};

int register_ep_driver(void)
{
	return pci_register_driver(&uft_pcie_ep_driver);
};

void unregister_ep_driver(void)
{
		pci_unregister_driver(&uft_pcie_ep_driver);
};

MODULE_LICENSE("GPL v2");
