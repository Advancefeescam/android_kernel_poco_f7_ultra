/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
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

extern int spi_apb_regops_write(u32 addr, u32 data);
extern int spi_apb_regops_read(u32 addr, u32 *valp);
extern bool spi_apb_ready(void);

int register_ep_driver(void);
void unregister_ep_driver(void);

#define PCI_VENDER_ID_XRING			0x16c3
#define PCI_DEVICE_ID_XRING			0xaaaa
#define EP_RESET_OFFSET				0x314
#define EP_MISC_CTL_1_OFF			0x8bc
#define DEV_VEN_ID				0xaaaa16c3
#define BAR0_OFFSET				0x10
#define BAR0_MASK_OFFSET			0x40010
#define MSI_CAP_REG				0x50
#define MEM_BAR_TYPE				0
#define IO_BAR_TYPE				2
#define INBOUND1_OFFSET				0x200
#define INBOUND2_OFFSET				0x400
#define ELBI_OFFSET				0x20000
#define HDMA_TEST_DATA_5A			0x5a5a5a5a
#define HDMA_TEST_DATA_A5			0xa5a5a5a5
#define MSI_VEC_MIN				1
#define MSI_VEC_MAX				32
#define MSIX_VEC_MAX				2048

#define BAR_SIZE				SZ_4K
#define BAR_SIZE_64K				0xffff
#define BAR_SIZE_16M				0xffffff
#define OFFSET_04				(4)
#define OFFSET_08				(8)
#define OFFSET_12				(12)
#define OFFSET_16				(16)
#define DEV_VEN_ID_OFFSET			0x00
#define PCIE_CAP_OFF				0x70
#define LINK_CONTROL2_LINK_STATUS2_REG		(PCIE_CAP_OFF + 0x30)
#define GEN_MASK				0xf

#define put_pcie_reg32(val, address)                                                  \
	({                                                                      \
		u32 addr = address;                                             \
		do {                                                            \
			int ret;                                                \
			ret = spi_apb_regops_write(addr, val);                    \
			if (ret != 0) {                                         \
				pr_err("Regops write 0x%x failed(%d)! %s:%d\n", \
				       addr, ret, __func__, __LINE__);      \
			}                                                       \
		} while (0);                                                    \
	})


#define get_pcie_reg32(address)                                                      \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 addr = address;                                            \
		do {                                                           \
			int ret;                                               \
			ret = spi_apb_regops_read(addr, &val);                   \
			if (ret != 0) {                                        \
				pr_err("Regops read 0x%x failed(%d)! %s:%d\n",    \
				       addr, ret, __func__, __LINE__);     \
			}                                                      \
		} while (0);                                                   \
		val;                                                           \
	})


#define pcie_clear_set_reg32(value, mask, address)                                  \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 ad = address;                                              \
		do {                                                           \
			val = get_pcie_reg32(ad);                                    \
			val &= ~mask;                                          \
			val |= (value & mask);                                 \
			put_pcie_reg32(val, ad);                                     \
		} while (0);                                                   \
	})

#define pcie_polling_reg32(value, mask, address)                                    \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 ad = address;                                              \
		u32 ret = -1;                                                  \
		u32 timeout = 1000;                                            \
		do {                                                           \
			val = get_pcie_reg32(ad);                                    \
			if ((val & mask) == value) {                           \
				ret = 0;                                       \
				pr_info("Regops polling 0x%x done! %s:%d\n",   \
				       ad,  __func__, __LINE__);           \
				break;                                         \
			}                                                      \
		} while (timeout--);                                           \
		if (ret) {                                                     \
			pr_err("Regops polling 0x%x failed! %s:%d\n",          \
				ad,  __func__, __LINE__);                  \
		}                                                              \
		ret;                                                           \
	})
