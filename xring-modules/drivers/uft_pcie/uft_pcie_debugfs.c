// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/random.h>
#include "uft_pcie_ep.h"
#include "../spi_apb/spi_apb_regops.h"
#include "xring_pcie_hdma.h"

static struct uft_sysfs_ctrl *sysfs_ctl;
static struct dentry *uft_pcie;
static struct dentry *dfile_uft_memops;
static struct dentry *dfile_set_bar_target;
static struct dentry *dfile_uft_slide_win;
static struct dentry *dfile_uft_memdump;
static struct dentry *dfile_uft_memwrite;
static struct dentry *dfile_uft_hdma_test;
static struct dentry *uft_debugfs;

#define MAX_DEBUG_BUF_LEN 50
#define MAX_DUMP_BUF_LEN 1000000

unsigned long custom_strtoul(const char *cp, char **endp, unsigned int base)
{
	unsigned long result = 0;
	unsigned int value;

	if (*cp == '0') {
		cp++;
		if ((base == 0 || base == 16) && (*cp == 'x' || *cp == 'X')) {
			cp++;
			base = 16;
		} else if (base == 0)
			base = 8;
	}

	if (base == 0)
		base = 10;

	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0' :
			islower(*cp) ? *cp - 'a' + 10 : *cp - 'A' + 10) < base) {
		result = result * base + value;
		cp++;
	}

	if (endp)
		*endp = (char *)cp;

	return result;
}

static ssize_t uft_debugfs_uft_memops_read(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	char output[MAX_DEBUG_BUF_LEN];

	if (sysfs_ctl->retval)
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "error occurred :%d\n", sysfs_ctl->retval);
	else
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "0x%08x\n", sysfs_ctl->uft_sysfs_value);


	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t  uft_debugfs_uft_memops_write(struct file *file,
				const char __user *buf, size_t count, loff_t *ppos)
{
	char *p;
	unsigned int bar_num, value, size, end;
	unsigned int offset = 0;
	int ret;
	char input_parm[MAX_DEBUG_BUF_LEN];
	struct uft_pcie_ep *priv;

	priv = file->private_data;
	if (!priv)
		return -EINVAL;

	struct pci_dev *pdev = priv->pdev;

	/* write format:[bar_num]:[offset]:[value] */
	/* read format:[bar_num]-[offset]-[size] */

	ret = copy_from_user(input_parm, buf, count);
	input_parm[count] = '\0';
	p = input_parm;
	bar_num = custom_strtoul(p, &p, 10);

	if (bar_num >= 0 && bar_num < PCI_STD_NUM_BARS) {
		if (*p == ':') {
			p++;
			/* get offset */
			offset = custom_strtoul(p, &p, 16);

			if (!IS_ALIGNED(offset, 4)) {
				dev_err(&pdev->dev, "unaligned address: %x", offset);
				sysfs_ctl->uft_sysfs_value = 0;
				sysfs_ctl->retval = -EFAULT;
				return count;
			}

			if (*p == ':') {
				p++;
				value = custom_strtoul(p, &p, 16);

				uft_pcie_write(priv, offset, value, bar_num);

				sysfs_ctl->retval = 0;
				sysfs_ctl->uft_sysfs_value = 0;
				sysfs_ctl->uft_sysfs_value = uft_pcie_read(priv, offset, bar_num);
			}
		} else if (*p == '-') {
			p++;
			/* get offset */
			offset = custom_strtoul(p, &p, 16);

			if (!IS_ALIGNED(offset, 4)) {
				dev_err(&pdev->dev, "unaligned address: %x", offset);
				sysfs_ctl->uft_sysfs_value = 0;
				sysfs_ctl->retval = -EFAULT;
				return count;
			}

			sysfs_ctl->retval = 0;
			sysfs_ctl->uft_sysfs_value = 0;
			sysfs_ctl->uft_sysfs_value = uft_pcie_read(priv, offset, bar_num);

			if (*p == '-') {
				p++;
				size = custom_strtoul(p, &p, 0);
				end = offset + size * 4;

				while (offset < end) {
					dev_info(&pdev->dev, "0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", offset,
						uft_pcie_read(priv, offset, bar_num),
						uft_pcie_read(priv, offset + OFFSET_04, bar_num),
						uft_pcie_read(priv, offset + OFFSET_08, bar_num),
						uft_pcie_read(priv, offset + OFFSET_12, bar_num));

					offset += OFFSET_16;
				}
			}
		}
	} else {
		sysfs_ctl->retval = -EINVAL;
	}
	return count;
}

static ssize_t uft_debugfs_get_target(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	char output[MAX_DEBUG_BUF_LEN];

	if (sysfs_ctl->retval)
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "error occurred :%d\n", sysfs_ctl->retval);
	else
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "target addr:0x%08x\n", sysfs_ctl->uft_sysfs_value);

	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t  uft_debugfs_set_target(struct file *file,
				const char __user *buf, size_t count, loff_t *ppos)
{
	char *p;
	unsigned int bar_num, value;
	int ret;
	char input_parm[MAX_DEBUG_BUF_LEN];
	struct uft_pcie_ep *priv;
	struct uft_pcie_bar_ctrl *uft_bar;

	priv = file->private_data;
	if (!priv)
		return -EINVAL;

	/* write format:[bar_num]:[value] */
	/* read format:[bar_num] */

	ret = copy_from_user(input_parm, buf, count);
	input_parm[count] = '\0';
	p = input_parm;
	bar_num = custom_strtoul(p, &p, 10);

	if (bar_num >= 0 && bar_num < PCI_STD_NUM_BARS) {
		uft_bar = priv->uft_bars[bar_num];

		if (*p == ':') {
			p++;
			/* get value */
			value = custom_strtoul(p, &p, 16);
			ret = xring_prog_ib_atu(priv, bar_num, value);

			if (ret) {
				sysfs_ctl->retval = ret;
				return count;
			}
		}

		sysfs_ctl->retval = 0;
		sysfs_ctl->uft_sysfs_value = 0;
		sysfs_ctl->uft_sysfs_value = uft_bar->ib_target_addr_phys;
	} else {
		sysfs_ctl->retval = -EINVAL;
	}

	return count;
}

static ssize_t uft_debugfs_get_base(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	char output[MAX_DEBUG_BUF_LEN];

	if (sysfs_ctl->retval)
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "error occurred :%d,offset = 0x%08x\n", sysfs_ctl->retval, sysfs_ctl->uft_sysfs_value);
	else
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "target addr:0x%08x\n", sysfs_ctl->uft_sysfs_value);


	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t uft_debugfs_slide_win(struct file *file,
				const char __user *buf, size_t count, loff_t *ppos)
{
	char *p;
	unsigned int size, target_addr, value;
	int ret, i;
	char input_parm[MAX_DEBUG_BUF_LEN];
	char *data;
	struct uft_pcie_ep *priv;

	priv = file->private_data;
	if (!priv)
		return -EINVAL;

	struct pci_dev *pdev = priv->pdev;
	struct device *dev = &pdev->dev;

	/* write format:[size]:[target] */

	ret = copy_from_user(input_parm, buf, count);
	input_parm[count] = '\0';
	p = input_parm;
	size = custom_strtoul(p, &p, 0);
	data = devm_kzalloc(dev, size, GFP_KERNEL);

	for (i = 0; i < size; i++)
		data[i] = (i >> 12) + 1;

	if (*p == ':') {
		p++;
		/* get size */
		target_addr = custom_strtoul(p, &p, 16);
		mem_update_by_sliding_win(priv, data, size, target_addr);

		for (i = 0; i < size; i += 4) {
			value = (data[i]) |
					(data[i] << 8) |
					(data[i] << 16) |
					(data[i] << 24);

			if (uft_pcie_read(priv, target_addr + i, SLIDE_WINDOW_BAR) != value) {
				sysfs_ctl->uft_sysfs_value = i;
				sysfs_ctl->retval = -1;
				dev_err(dev, "error check data:0x%x,", i);
				return count;
			}
		}

		sysfs_ctl->retval = 0;
		sysfs_ctl->uft_sysfs_value = 0;
		sysfs_ctl->uft_sysfs_value = target_addr;
		devm_kfree(dev, data);
	} else {
		sysfs_ctl->retval = -1;
		sysfs_ctl->uft_sysfs_value = 0;
	}

	return count;
}

static ssize_t uft_debugfs_memdump_result(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
	u32 remaining_count = min(sysfs_ctl->uft_sysfs_size - (u32)*ppos, (u32)count);
	struct uft_pcie_ep *priv = file->private_data;
	struct pci_dev *pdev = priv->pdev;
	char *output;
	u32 start_addr = sysfs_ctl->uft_sysfs_address;

	remaining_count = min(MAX_DUMP_BUF_LEN, remaining_count);
	output = devm_kzalloc(&pdev->dev, remaining_count, GFP_KERNEL);

	pr_info("memdump:start_addr:%x,remaining count:%x", start_addr, remaining_count);

	if (!remaining_count)
		return remaining_count;

	xring_hdma_single_trans_va(priv->pdma, HDMA_TO_DEVICE, output, remaining_count, start_addr + *ppos);

	if (copy_to_user(buf, output, remaining_count)) {
		devm_kfree(&pdev->dev, output);
		pr_info("memdump_err:start_addr:%x,ppos:%lld", start_addr, *ppos);
		return -EFAULT;
	}

	devm_kfree(&pdev->dev, output);

	*ppos += remaining_count;
	pr_info("memdump:start_addr:%x,ppos:%lld", start_addr, *ppos);
	return remaining_count;
}

static ssize_t uft_debugfs_memdump_get(struct file *file,
				const char __user *buf, size_t count, loff_t *ppos)
{
	char *p;
	int ret;
	char input_parm[MAX_DEBUG_BUF_LEN];
	struct uft_pcie_ep *priv;

	priv = file->private_data;
	if (!priv)
		return -EINVAL;

	/* write format:[address]-[size] > memdump */
	ret = copy_from_user(input_parm, buf, count);
	input_parm[count] = '\0';
	p = input_parm;
	sysfs_ctl->uft_sysfs_address = custom_strtoul(p, &p, 0);

	if (*p == '-') {
		p++;
		sysfs_ctl->uft_sysfs_size = custom_strtoul(p, &p, 0);
	}
	sysfs_ctl->uft_sysfs_value = 1;

	return count;
}

static ssize_t uft_debugfs_memwrite_get(struct file *file,
				const char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	struct uft_pcie_ep *priv = file->private_data;
	struct pci_dev *pdev = priv->pdev;
	u32 write_count = min(MAX_DUMP_BUF_LEN, count);
	char *input = devm_kzalloc(&pdev->dev, write_count + 5, GFP_KERNEL);
	u32 start_addr = sysfs_ctl->uft_sysfs_address;

	if (!priv)
		return -EINVAL;

	ret = copy_from_user(input, buf, write_count);

	xring_hdma_single_trans_va(priv->pdma, HDMA_FROM_DEVICE, input, write_count, start_addr + *ppos);

	pr_info("memwrite:start_addr:%x, count:%x,*ppos = %lld", start_addr, write_count, *ppos);

	*ppos += write_count;

	devm_kfree(&pdev->dev, input);

	return write_count;
}

static ssize_t uft_debugfs_hdma_result(struct file *file,
				char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	char output[MAX_DEBUG_BUF_LEN];

	if (sysfs_ctl->uft_sysfs_value)
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "uft_hdma_test pass\n");
	else
		ret = scnprintf(output, MAX_DEBUG_BUF_LEN, "uft_hdma_test failed\n");

	return simple_read_from_buffer(buf, count, ppos, output, ret);
}

static ssize_t uft_debugfs_hdma_test(struct file *file,
				const char __user *buf, size_t count, loff_t *ppos)
{
	char *p;
	unsigned int ep_addr, size, dir, mode;
	void *va;
	dma_addr_t pa;
	int ret, i;
	char input_parm[MAX_DEBUG_BUF_LEN];
	struct uft_pcie_ep *priv;

	priv = file->private_data;
	if (!priv)
		return -EINVAL;

	struct pci_dev *pdev = priv->pdev;
	struct device *dev = &pdev->dev;

	/* format:[size]:[ep_addr]:[dir]:[mode] */

	ret = copy_from_user(input_parm, buf, count);
	input_parm[count] = '\0';
	p = input_parm;

	size = custom_strtoul(p, &p, 0);
	va = dma_alloc_coherent(dev, size, &pa, GFP_KERNEL);

	pr_err("dma:pa = %llx", pa);

	p++;
	ep_addr = custom_strtoul(p, &p, 16);

	p++;
	dir = custom_strtoul(p, &p, 0);

	p++;
	mode = custom_strtoul(p, &p, 0);

	for (i = 0; i < size; i++)
		*((char *)va + i) = i % 0x100;

	if (mode == 0)
		xring_hdma_single_trans_pa(priv->pdma, dir, pa, size, ep_addr);
	else
		xring_hdma_single_trans_va(priv->pdma, dir, va, size, ep_addr);

	sysfs_ctl->uft_sysfs_value = 1;

	if (dir == HDMA_FROM_DEVICE)
		dma_free_coherent(dev, size, va, pa);

	return count;
}

static const struct file_operations uft_debugfs_uft_mem_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = uft_debugfs_uft_memops_read,
	.write = uft_debugfs_uft_memops_write,
};

static const struct file_operations uft_debugfs_set_bar_target = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = uft_debugfs_get_target,
	.write = uft_debugfs_set_target,
};

static const struct file_operations uft_debugfs_slide_win_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = uft_debugfs_get_base,
	.write = uft_debugfs_slide_win,
};

static const struct file_operations uft_debugfs_memdump = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = uft_debugfs_memdump_result,
	.write = uft_debugfs_memdump_get,
};

static const struct file_operations uft_debugfs_memwrite = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = uft_debugfs_memwrite_get,
};

static const struct file_operations uft_debugfs_hdma = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = uft_debugfs_hdma_result,
	.write = uft_debugfs_hdma_test,
};

void uft_debugfs_add_slide_win_test(struct uft_pcie_ep *ep)
{
	if (!ep)
		return;

	dfile_uft_slide_win = debugfs_lookup("slide_win", uft_pcie);
	if (IS_ERR_OR_NULL(dfile_uft_slide_win)) {
		dfile_uft_slide_win = debugfs_create_file("slide_win", 0664,
						uft_pcie, ep,
						&uft_debugfs_slide_win_ops);
		if (IS_ERR_OR_NULL(dfile_uft_slide_win)) {
			pr_err("uft PCIe: fail to create the slide_win file for debug_fs.\n");
			debugfs_remove_recursive(uft_pcie);
		}
	}
}

void uft_debugfs_add_pcie_uft_memops(struct uft_pcie_ep *ep)
{
	if (!ep)
		return;

	dfile_uft_memops = debugfs_lookup("uft_memops", uft_pcie);
	if (IS_ERR_OR_NULL(dfile_uft_memops)) {
		dfile_uft_memops = debugfs_create_file("uft_memops", 0664,
						uft_pcie, ep,
						&uft_debugfs_uft_mem_ops);
		if (IS_ERR_OR_NULL(dfile_uft_memops)) {
			pr_err("uft PCIe: fail to create the uft_memops file for debug_fs.\n");
			debugfs_remove_recursive(uft_pcie);
		}
	}
}

void uft_debugfs_add_pcie_uft_set_bar_target(struct uft_pcie_ep *ep)
{
	if (!ep)
		return;

	dfile_set_bar_target = debugfs_lookup("uft_set_bar_target", uft_pcie);
	if (IS_ERR_OR_NULL(dfile_set_bar_target)) {
		dfile_set_bar_target = debugfs_create_file("uft_set_bar_target", 0664,
						uft_pcie, ep,
						&uft_debugfs_set_bar_target);
		if (IS_ERR_OR_NULL(dfile_set_bar_target)) {
			pr_err("uft PCIe: fail to create the uft_set_bar_target file for debug_fs.\n");
			debugfs_remove_recursive(uft_pcie);
		}
	}
}

void uft_debugfs_add_memdump(struct uft_pcie_ep *ep)
{
	if (!ep)
		return;

	dfile_uft_memdump = debugfs_lookup("memdump", uft_pcie);
	if (IS_ERR_OR_NULL(dfile_uft_memdump)) {
		dfile_uft_memdump = debugfs_create_file("memdump", 0664,
						uft_pcie, ep,
						&uft_debugfs_memdump);
		if (IS_ERR_OR_NULL(dfile_uft_memdump)) {
			pr_err("uft PCIe: fail to create the memdump file for debug_fs.\n");
			debugfs_remove_recursive(uft_pcie);
		}
	}
}

void uft_debugfs_add_memwrite(struct uft_pcie_ep *ep)
{
	if (!ep)
		return;

	dfile_uft_memwrite = debugfs_lookup("memwrite", uft_pcie);
	if (IS_ERR_OR_NULL(dfile_uft_memwrite)) {
		dfile_uft_memwrite = debugfs_create_file("memwrite", 0664,
						uft_pcie, ep,
						&uft_debugfs_memwrite);
		if (IS_ERR_OR_NULL(dfile_uft_memwrite)) {
			pr_err("uft PCIe: fail to create the memwrite file for debug_fs.\n");
			debugfs_remove_recursive(uft_pcie);
		}
	}
}

void uft_debugfs_add_hdma_test(struct uft_pcie_ep *ep)
{
	if (!ep)
		return;

	dfile_uft_hdma_test = debugfs_lookup("hdma_test", uft_pcie);
	if (IS_ERR_OR_NULL(dfile_uft_hdma_test)) {
		dfile_uft_hdma_test = debugfs_create_file("hdma_test", 0664,
						uft_pcie, ep,
						&uft_debugfs_hdma);
		if (IS_ERR_OR_NULL(dfile_uft_hdma_test)) {
			pr_err("uft PCIe: fail to create the dfile_uft_hdma_test file for debug_fs.\n");
			debugfs_remove_recursive(uft_pcie);
		}
	}
}

int uft_pcie_add_debugfs(struct uft_pcie_ep *ep)
{

	uft_debugfs = debugfs_lookup("uft_fpga", NULL);
	if (IS_ERR_OR_NULL(uft_debugfs)) {
		pr_err("fail to lookup the uft_fpga dir for debug_fs.\n");
		return -ENODEV;
	}

	uft_pcie = debugfs_lookup("uft_pcie", uft_debugfs);
	if (IS_ERR_OR_NULL(uft_pcie)) {
		uft_pcie = debugfs_create_dir("uft_pcie", uft_debugfs);
		if (IS_ERR_OR_NULL(uft_pcie)) {
			pr_err("fail to create the uft_pcie folder for debug_fs.\n");
			return -ENODEV;
		}
	}

	sysfs_ctl = ep->sysfs_ctl;

	uft_debugfs_add_pcie_uft_memops(ep);
	uft_debugfs_add_pcie_uft_set_bar_target(ep);
	uft_debugfs_add_slide_win_test(ep);
	uft_debugfs_add_memdump(ep);
	uft_debugfs_add_hdma_test(ep);
	uft_debugfs_add_memwrite(ep);

	return 0;
}

void uft_pcie_remove_debugfs(void)
{
	debugfs_remove_recursive(uft_pcie);
}

MODULE_LICENSE("GPL v2");
