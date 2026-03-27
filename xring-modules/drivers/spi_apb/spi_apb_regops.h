/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#ifndef _LINUX_XM_REGOPS_H
#define _LINUX_XM_REGOPS_H

#include <linux/types.h>
#include <linux/printk.h>
#include "spi_apb_interface.h"

int spi_apb_regops_init(struct spi_device *sdev);
int spi_apb_regops_deinit(struct spi_device *sdev);
int spi_apb_regops_test_init(struct dentry *uft_debugfs);
void spi_apb_regops_test_exit(void);

#define SPI_APB_SPI_LOW_SP 1000000
#define SPI_APB_SPI_HIGH_SP 32000000

#define spi_apb_set_lowsp() spi_apb_regops_set_speed(SPI_APB_SPI_LOW_SP)
#define spi_apb_set_highsp() spi_apb_regops_set_speed(SPI_APB_SPI_HIGH_SP)

#define BITS_PER_WORD_10 10
#define BITS_PER_WORD_8 8
/* IOC magic */
#define SPI_APB_IOC_MAGIC 'A'
/* Retry times for checking after read-request/write. */
#define RW_CHECK_TIMES 3

/* spi2ahb operation command */
#define REGOPS_READ_CMD 0x10
#define REGOPS_READ_REQ_CMD 0x20
#define REGOPS_READ_STATUS_CMD 0x50
#define REGOPS_DREAD_CMD 0x70
#define REGOPS_WRITE_CMD 0x80
#define REGOPS_WRITE_STATUS_CMD 0xD0

#define REGOPS_STATUS_ERR 0x40
#define REGOPS_STATUS_OK 0x80

#define BURST_DATA_BUF_SIZE (64 * 4 + 4)
#define DATA_BUF_SIZE (16)

#define CMD_SPI_APB_ADDR_READ _IOR(SPI_APB_IOC_MAGIC, 0, struct spi_apb_ioc_args)
#define CMD_SPI_APB_ADDR_WRITE _IOW(SPI_APB_IOC_MAGIC, 1, struct spi_apb_ioc_args)

#define GET_BYTE_0(addr)  ((uint8_t)((addr) & 0xFF))
#define GET_BYTE_1(addr)  ((uint8_t)(((addr) >> 8) & 0xFF))
#define GET_BYTE_2(addr)  ((uint8_t)(((addr) >> 16) & 0xFF))
#define GET_BYTE_3(addr)  ((uint8_t)(((addr) >> 24) & 0xFF))

#define SET_BYTE_0(addr, val)  (addr = (addr & 0xFFFFFF00) | ((val) & 0xFF))
#define SET_BYTE_1(addr, val)  (addr = (addr & 0xFFFF00FF) | (((val) & 0xFF) << 8))
#define SET_BYTE_2(addr, val)  (addr = (addr & 0xFF00FFFF) | (((val) & 0xFF) << 16))
#define SET_BYTE_3(addr, val)  (addr = (addr & 0x00FFFFFF) | (((val) & 0xFF) << 24))

struct spi_apb_ioc_args {
	u32    addr;
	u32    value;
};

struct spi_apb_dev {
	struct spi_device *spi_dev;
	struct dentry *debugfs_dir;
	struct mutex *base_lock;
};

#define putreg32(val, address)                                                  \
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

#define getreg32(address)                                                      \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 addr = address;                                            \
		do {                                                           \
			int ret;                                               \
			ret = spi_apb_regops_read(addr, &val);                   \
			if (ret != 0) {                                        \
				pr_err("Regops read 0x%x failed(%d)! %s:%d\n", \
				       addr, ret, __func__, __LINE__);     \
			}                                                      \
		} while (0);                                                   \
		val;                                                           \
	})

#define clear_set_reg32(value, mask, address)                                  \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 ad = address;                                              \
		do {                                                           \
			val = getreg32(ad);                                    \
			val &= ~mask;                                          \
			val |= (value & mask);                                 \
			putreg32(val, ad);                                     \
		} while (0);                                                   \
	})

#define polling_reg32(value, mask, address)                                    \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 ad = address;                                              \
		u32 ret = -1;                                                  \
		u32 timeout = 1000;                                            \
		do {                                                           \
			val = getreg32(ad);                                    \
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
#endif
