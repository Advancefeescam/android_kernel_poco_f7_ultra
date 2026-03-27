// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023, X-Ring technologies Inc., All rights reserved.
 */

#define pr_fmt(fmt)     "[xr_dfx][noc]:%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/io.h>

#include "noc.h"
#include "dt-bindings/xring/platform-specific/common/mdr/include/mdr_pub.h"
#include "noc_exception.h"

#define MODID_SHUB_NOC_BUS_ERROR	0x890000fe
#define MODID_ISP_NOC_BUS_ERROR		0x81fffefe

struct mdr_exception_info_s sh_noc_exception = {
	.e_modid            = (uint32_t)MODID_SHUB_NOC_BUS_ERROR,
	.e_modid_end        = (uint32_t)MODID_SHUB_NOC_BUS_ERROR,
	.e_process_priority = MDR_ERR,
	.e_reboot_priority  = MDR_REBOOT_NO,
	.e_notify_core_mask = MDR_SHUB | MDR_AP,
	.e_reset_core_mask  = MDR_SHUB,
	.e_from_core        = MDR_SHUB,
	.e_reentrant        = (uint32_t)MDR_REENTRANT_DISALLOW,
	.e_exce_type        = SHUB_S_EXCEPTION,
	.e_exce_subtype     = SHUB_BUSFAULT,
	.e_upload_flag      = (uint32_t)MDR_UPLOAD_YES,
	.e_from_module      = "sensorhub",
	.e_desc             = "sensorhub core busfault",
};

struct mdr_exception_info_s isp_noc_exception = {
	.e_modid            = (u32)MODID_ISP_NOC_BUS_ERROR,
	.e_modid_end        = (u32)MODID_ISP_NOC_BUS_ERROR,
	.e_process_priority = MDR_ERR,
	.e_reboot_priority  = MDR_REBOOT_NO,
	.e_notify_core_mask = MDR_ISP,
	.e_reset_core_mask  = MDR_ISP,
	.e_from_core        = MDR_ISP,
	.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
	.e_exce_type        = ISP_S_EXCEPTION,
	.e_exce_subtype     = ISP_S_MIPI_WR_ERROR,
	.e_upload_flag      = (u32)MDR_UPLOAD_YES,
	.e_from_module      = "ISP",
	.e_desc             = "ISP MIPI BUS ERROR",
};

static struct work_struct work_noc_test;

static void tst_noc_mid(struct work_struct *work)
{
	void  __iomem *noc_addr = ioremap(RESERVE_ADD, RESERVE_ADD_SIZE);

	if (!noc_addr) {
		pr_err("Cannot map device memory\n");
		return;
	}

	readl(noc_addr);

	iounmap(noc_addr);

	cancel_work_sync(&work_noc_test);
}

void tst_noc(u32 cpu)
{
	bool ret;

	INIT_WORK(&work_noc_test, tst_noc_mid);
	ret = queue_work_on(cpu, system_wq, &work_noc_test);

	if (!ret) {
		pr_err("Unable to queue work\n");
		return;
	}
}

void noc_register_test(void)
{
	int ret;

	ret = noc_register_exception(&sh_noc_exception);
	if (ret) {
		pr_err("sh noc register test fail\n");
		return;
	}

	ret = noc_register_exception(&isp_noc_exception);
	if (ret) {
		pr_err("isp noc register test fail\n");
		return;
	}

	pr_info("noc register test success\n");
}

void noc_unregister_test(void)
{
	int ret;

	ret = noc_unregister_exception(&sh_noc_exception);
	if (ret) {
		pr_err("sh noc unregister test fail\n");
		return;
	}

	ret = noc_unregister_exception(&isp_noc_exception);
	if (ret) {
		pr_err("isp noc unregister test fail\n");
		return;
	}

	pr_info("noc unregister test success\n");
}

void noc_exception_test(u32 tst_id)
{
	switch (tst_id) {
	case 0:
		/* noc ap exception */
		set_noc_exception_list(77, 1);
		break;
	case 1:
		/* noc isp exception */
		set_noc_exception_list(63, 1);
		break;
	case 2:
		/* noc isp tmo exception */
		set_noc_exception_list(63, 6);
		break;
	case 3:
		/* noc isp exception */
		set_noc_exception_list(63, 1);
		/* noc sh exception */
		set_noc_exception_list(2, 0);
		break;
	default:
		pr_info("%s: invalid tst_id %d\n", __func__, tst_id);
		break;
	}

	noc_exception_process();
}

void noc_dma_test(u32 tst_id)
{
	struct noc_dma_exception_s *e = NULL;
	struct noc_dev *dev = get_noc();

	if (!dev) {
		pr_err("invalid noc dev\n");
		return;
	}

	e = &dev->noc_dma_s;
	if (!e) {
		pr_err("invalid noc dma exception info\n");
		return;
	}

	switch (tst_id) {
	case 0:
		noc_dma_exception(e, "dma_ns", 25, 0xec10000, 0);
		break;
	case 1:
		noc_dma_exception(e, "spi_dma", 7, 0x1c0000000, 1);
		break;
	case 2:
		noc_dma_exception(e, "layer rdma1", 40, 0x3ffffffff, 2);
		break;
	case 3:
		noc_dma_exception(e, "cmdlist_rdma", 58, 0x12000000, 3);
		break;
	default:
		pr_info("%s: invalid tst_id %d\n", __func__, tst_id);
		break;
	}
}

void noc_dma_process_test(void)
{
	struct noc_dma_exception_s *e = NULL;
	struct noc_dev *dev = get_noc();

	if (!dev) {
		pr_err("invalid noc dev\n");
		return;
	}

	e = &dev->noc_dma_s;
	if (!e) {
		pr_err("invalid noc dma exception info\n");
		return;
	}

	noc_dma_process(e);
}

void noc_dma_process_exit_test(void)
{
	struct noc_dma_exception_s *e = NULL;
	struct noc_dev *dev = get_noc();

	if (!dev) {
		pr_err("invalid noc dev\n");
		return;
	}

	e = &dev->noc_dma_s;
	if (!e) {
		pr_err("invalid noc dma exception info\n");
		return;
	}

	noc_dma_process_exit(e);
}
