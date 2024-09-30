/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_PCIE_MEDIATEK_GEN3_H__
#define __MTK_PCIE_MEDIATEK_GEN3_H__

#include <linux/pci.h>

u32 mtk_pcie_dump_link_info(int port);
int mtk_pcie_disable_data_trans(int port);

#endif
