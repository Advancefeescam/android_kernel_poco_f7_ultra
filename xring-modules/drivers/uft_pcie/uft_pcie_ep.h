/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#ifndef UFT_PCIE_EP_H__
#define UFT_PCIE_EP_H__

#include "../pci/pcie-xring-interface.h"
#include "../pci/pcie-xring.h"
#include <dt-bindings/xring/platform-specific/pcie_ctrl.h>
#include <dt-bindings/xring/platform-specific/hss2_top.h>
#include "../../pci.h"

#define ATU_ENABLED_MASK (1 << IATU_REGION_CTRL_2_OFF_INBOUND_0_REGION_EN_BitAddressOffset)
#define ATU_BAR_MATCH_MODE_MASK (1 << IATU_REGION_CTRL_2_OFF_INBOUND_0_MATCH_MODE_BitAddressOffset)
#define ATU_BAR_NUM(reg) ((reg >> IATU_REGION_CTRL_2_OFF_INBOUND_0_BAR_NUM_BitAddressOffset) & (0x7))
#define UFT_ATU_NUM 8

#define PCI_VENDER_ID_UFT		0x16c3
#define PCI_DEVICE_ID_UFT		0xaaaa
#define TEST_MEMSIZE		0x38a0
#define ATU_OFFSET			0x4000

#define DDR_ACCESS_BAR		0
#define SLIDE_WINDOW_BAR	1
#define REG_ACCESS_BAR		2
#define HDMA_ACCESS_BAR		5

struct uft_pcie_bar_ctrl {
	void __iomem	*bar_virt;
	u32				bar_phys;
	u32				ib_target_addr_phys;
	u32				bar_size;
	u16				ib_atu_idx;
	bool			ib_atu_enabled;
};

struct uft_pcie_ob_ctrl {
	u32				ep_addr;
	u32				rc_addr;
	u32				size;
	bool			ob_atu_enabled;
};

struct uft_sysfs_ctrl {
	u32				uft_sysfs_value;
	int				retval;
	u32				uft_sysfs_size;
	u32				uft_sysfs_address;
};

struct uft_pcie_ep {
	struct pci_dev	*pdev;
	int		num_irq;
	struct uft_pcie_bar_ctrl	*uft_bars[PCI_STD_NUM_BARS];
	struct uft_pcie_ob_ctrl		*uft_ob_res[UFT_ATU_NUM];
	u32				ib_atu_map;
	u32				ob_atu_map;
	void __iomem			*atu_base;
	struct uft_sysfs_ctrl		*sysfs_ctl;
	int				pci_irq_type;
	struct pcie_hdma	*pdma;
};

int uft_pcie_add_debugfs(struct uft_pcie_ep *ep);
void uft_pcie_remove_debugfs(void);

int xring_prog_ib_atu(struct uft_pcie_ep *ep, int bar, u32 target_addr);
int mem_update_by_sliding_win(struct uft_pcie_ep *ep,
					char *data, u32 size, u32 target_addr);
uint32_t uft_pcie_mem_read(struct uft_pcie_ep *ep, int bar_id,
					uint32_t offset);
void uft_pcie_mem_write(struct uft_pcie_ep *ep, int bar_id,
					uint32_t offset, uint32_t val);
void uft_pcie_write(struct uft_pcie_ep *ep, u32 reg, u32 val, int bar_id);
u32 uft_pcie_read(struct uft_pcie_ep *ep, u32 reg, int bar_id);
void uft_pcie_ep_reg_write(struct uft_pcie_ep *ep, u32 reg, u32 val);
u32 uft_pcie_ep_reg_read(struct uft_pcie_ep *ep, u32 reg);
void uft_pcie_ep_ddr_write(struct uft_pcie_ep *ep, u32 addr, u32 val);
u32 uft_pcie_ep_ddr_read(struct uft_pcie_ep *ep, u32 addr);
#endif
