/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include <linux/types.h>
#include <linux/printk.h>

int spi_apb_regops_read(u32 addr, u32 *valp);
int spi_apb_regops_dread(u32 addr, u32 *valp);
int spi_apb_regops_write(u32 addr, u32 data);
int spi_apb_regops_w8dw(u32 addr, u32 *data);
int spi_apb_regops_w64dw(u32 addr, u32 *data);

int spi_apb_regops_get_speed(void);
int spi_apb_regops_set_speed(u32 sp);
