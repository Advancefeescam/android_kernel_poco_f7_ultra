// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023, X-Ring technologies Inc., All rights reserved.
 */
#include "mdr_print.h"
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include "mdr_field_core.h"
#include "dt-bindings/xring/platform-specific/common/mdr/include/mdr_pub.h"
#include "dt-bindings/xring/platform-specific/dfx_memory_layout.h"

#define MODID_XR_AP_PANIC	0x80000001
#define MODID_NPU_EXCEPTION	0xc0000fff
#define MODID_TEST1_EXCEPTION     0x81fff100
#define MODID_TEST2_EXCEPTION     0x81fff101
#define MODID_TEST3_EXCEPTION     0x81fff102
#define MODID_TEST1_CORE1_EXCEPTION     0x81fff103
#define MODID_TEST3_CORE3_EXCEPTION     0x81fff104
#define MODID_TEST4_EXCEPTION     0x81fff105

#define XR_AP_NOC	0
#define NPU_S_NOC	1
#define MDR_TEST1	0x40000000
#define MDR_TEST2	0x80000000
#define MDR_TEST3	0x100000000
#define MDR_TEST4	0x200000000
#define XR_TEST1	1
#define	XR_TEST2	1
#define	XR_TEST3	1
#define	XR_TEST4	1
#define XR_TEST1_EXCEPTION	0xd1
#define XR_TEST2_EXCEPTION	0xd2
#define XR_TEST3_EXCEPTION	0xd3
#define XR_TEST4_EXCEPTION	0xd4

struct mdr_exception_info_s ap_exception = {
		.e_modid            = (u32)MODID_XR_AP_PANIC,
		.e_modid_end        = (u32)MODID_XR_AP_PANIC,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_NOW,
		.e_notify_core_mask = MDR_AP,
		.e_reset_core_mask  = MDR_AP,
		.e_from_core        = MDR_AP,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_AP_PANIC,
		.e_exce_subtype     = XR_AP_NOC,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "ap",
		.e_desc             = "ap",
};

struct mdr_exception_info_s ap_exception_mid_error = {
		.e_modid            = (u32)MODID_XR_AP_PANIC,
		.e_modid_end        = (u32)(MODID_XR_AP_PANIC - 1),
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_NOW,
		.e_notify_core_mask = MDR_AP,
		.e_reset_core_mask  = MDR_AP,
		.e_from_core        = MDR_AP,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_AP_PANIC,
		.e_exce_subtype     = XR_AP_NOC,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "ap",
		.e_desc             = "ap",
};

struct mdr_exception_info_s test1_exception[2] = {
	[0] = {
		.e_modid            = (u32)MODID_TEST1_EXCEPTION,
		.e_modid_end        = (u32)MODID_TEST1_EXCEPTION,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_WAIT,
		.e_notify_core_mask = MDR_TEST1,
		.e_reset_core_mask  = MDR_TEST1,
		.e_from_core        = MDR_TEST1,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_TEST1_EXCEPTION,
		.e_exce_subtype     = XR_TEST1,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "test_core",
		.e_desc             = "test_core",
	},
	[1] = {
		.e_modid            = (u32)MODID_TEST1_CORE1_EXCEPTION,
		.e_modid_end        = (u32)MODID_TEST1_CORE1_EXCEPTION,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_WAIT,
		.e_notify_core_mask = MDR_TEST1|MDR_TEST2|MDR_TEST3,
		.e_reset_core_mask  = MDR_TEST1,
		.e_from_core        = MDR_TEST1,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_TEST1_EXCEPTION,
		.e_exce_subtype     = XR_TEST1,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "test_core",
		.e_desc             = "test_core",
	},
};

struct mdr_exception_info_s test2_exception = {
		.e_modid            = (u32)MODID_TEST2_EXCEPTION,
		.e_modid_end        = (u32)MODID_TEST2_EXCEPTION,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_WAIT,
		.e_notify_core_mask = MDR_TEST2|MDR_NPU|MDR_TEST1|MDR_TEST3,
		.e_reset_core_mask  = MDR_TEST2,
		.e_from_core        = MDR_TEST2,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_TEST2_EXCEPTION,
		.e_exce_subtype     = XR_TEST2,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "test_core",
		.e_desc             = "test_core",
};

struct mdr_exception_info_s test3_exception[2] = {
	[0] = {
		.e_modid            = (u32)MODID_TEST3_EXCEPTION,
		.e_modid_end        = (u32)MODID_TEST3_EXCEPTION,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_WAIT,
		.e_notify_core_mask = MDR_TEST3|MDR_NPU|MDR_TEST1,
		.e_reset_core_mask  = MDR_TEST3,
		.e_from_core        = MDR_TEST3,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_TEST3_EXCEPTION,
		.e_exce_subtype     = XR_TEST3,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "test_core",
		.e_desc             = "test_core",
	},
	[1] = {
		.e_modid            = (u32)MODID_TEST3_CORE3_EXCEPTION,
		.e_modid_end        = (u32)MODID_TEST3_CORE3_EXCEPTION,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_WAIT,
		.e_notify_core_mask = MDR_TEST3|MDR_NPU|MDR_TEST1|MDR_TEST2,
		.e_reset_core_mask  = MDR_TEST3,
		.e_from_core        = MDR_TEST3,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_TEST3_EXCEPTION,
		.e_exce_subtype     = XR_TEST3,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "test_core",
		.e_desc             = "test_core",
	},
};

struct mdr_exception_info_s test4_exception = {
		.e_modid            = (u32)MODID_TEST4_EXCEPTION,
		.e_modid_end        = (u32)MODID_TEST4_EXCEPTION,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_WAIT,
		.e_notify_core_mask = MDR_TEST2|MDR_TEST4|MDR_TEST1|MDR_TEST3,
		.e_reset_core_mask  = MDR_TEST4,
		.e_from_core        = MDR_TEST4,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = XR_TEST4_EXCEPTION,
		.e_exce_subtype     = XR_TEST4,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "test_core",
		.e_desc             = "test_core",
};

void mdr_ap_dump_example(u32 modid, u32 etype, u64 coreid, char *log_path,
			pfn_cb_dump_done pfn_cb, void *data)
{
	pr_info("mdr_ap_dump\n");
}

void mdr_ap_reset_example(u32 modid, u32 etype, u64 coreid, void *data)
{
	pr_info("mdr_ap_reset\n");
}

struct mdr_module_ops s_soc_ops = {
		.ops_dump   = mdr_ap_dump_example,
		.ops_reset  = mdr_ap_reset_example,
};

struct mdr_exception_info_s npu_exception = {
		.e_modid            = (u32)MODID_NPU_EXCEPTION,
		.e_modid_end        = (u32)MODID_NPU_EXCEPTION,
		.e_process_priority = MDR_ERR,
		.e_reboot_priority  = MDR_REBOOT_WAIT,
		.e_notify_core_mask = MDR_NPU|MDR_AP|MDR_LPM3,
		.e_reset_core_mask  = MDR_NPU|MDR_AP,
		.e_from_core        = MDR_NPU,
		.e_reentrant        = (u32)MDR_REENTRANT_DISALLOW,
		.e_exce_type        = NPU_S_EXCEPTION,
		.e_exce_subtype     = NPU_S_NOC,
		.e_upload_flag      = (u32)MDR_UPLOAD_YES,
		.e_from_module      = "NPU",
		.e_desc             = "NPU",
};

void mdr_npu_dump(u32 modid, u32 etype, u64 coreid, char *log_path,
			pfn_cb_dump_done pfn_cb, void *data)
{
	pr_info("dump\n");
}

void mdr_npu_reset(u32 modid, u32 etype, u64 coreid, void *data)
{
	pr_info("reset\n");
}

struct mdr_module_ops s_npu_ops = {
		.ops_dump  = mdr_npu_dump,
		.ops_reset = mdr_npu_reset,
};

void mdr_test1_dump(u32 modid, u32 etype, u64 coreid, char *log_path,
			pfn_cb_dump_done pfn_cb, void *data)
{
	pr_info("\n");
}

void mdr_test1_reset(u32 modid, u32 etype, u64 coreid, void *data)
{
	pr_info("\n");
}

struct mdr_module_ops s_test1_ops = {
		.ops_dump  = mdr_test1_dump,
		.ops_reset = mdr_test1_reset,
};

void mdr_test2_dump(u32 modid, u32 etype, u64 coreid, char *log_path,
			pfn_cb_dump_done pfn_cb, void *data)
{
	pr_info("\n");
}

void mdr_test2_reset(u32 modid, u32 etype, u64 coreid, void *data)
{
	pr_info("\n");
}

struct mdr_module_ops s_test2_ops = {
		.ops_dump  = mdr_test2_dump,
		.ops_reset = mdr_test2_reset,
};

void mdr_test3_dump(u32 modid, u32 etype, u64 coreid, char *log_path,
			pfn_cb_dump_done pfn_cb, void *data)
{
	pr_info("\n");
}

void mdr_test3_reset(u32 modid, u32 etype, u64 coreid, void *data)
{
	pr_info("\n");
}

struct mdr_module_ops s_test3_ops = {
		.ops_dump  = mdr_test3_dump,
		.ops_reset = mdr_test3_reset,
};

static int proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int test1_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static ssize_t test1_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	void *log_addr = ioremap_wc(DFX_MEM_FASTBOOTLOG_ADDR, DFX_MEM_FASTBOOTLOG_SIZE);

	if (ppos == NULL) {
		pr_err("ppos is null\n");
		return 0;
	}

	if (buf == NULL) {
		pr_err("buf is null\n");
		return 0;
	}

	if (copy_to_user(buf, log_addr, DFX_MEM_FASTBOOTLOG_SIZE)) {
		pr_err("%s copy to user failed\n", __func__);
		return -EFAULT;
	}

	return size;
}

static const struct proc_ops test1_proc_fops = {
	.proc_open      = test1_open,
	.proc_read      = test1_read,
	.proc_release   = single_release,
};

static int test2_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static ssize_t test2_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	void *log_addr = ioremap_wc(DFX_MEM_FASTBOOTLOG_ADDR, DFX_MEM_FASTBOOTLOG_SIZE);

	if (ppos == NULL) {
		pr_err("ppos is null\n");
		return 0;
	}

	if (buf == NULL) {
		pr_err("buf is null\n");
		return 0;
	}

	if (copy_to_user(buf, log_addr, DFX_MEM_FASTBOOTLOG_SIZE)) {
		pr_err("%s copy to user failed\n", __func__);
		return -EFAULT;
	}

	return size;
}

static const struct proc_ops test2_proc_fops = {
	.proc_open      = test2_open,
	.proc_read      = test2_read,
	.proc_release   = single_release,
};

static int test3_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static ssize_t test3_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	void *log_addr = ioremap_wc(DFX_MEM_FASTBOOTLOG_ADDR, DFX_MEM_FASTBOOTLOG_SIZE);

	if (ppos == NULL) {
		pr_err("ppos is null\n");
		return 0;
	}

	if (buf == NULL) {
		pr_err("buf is null\n");
		return 0;
	}

	if (copy_to_user(buf, log_addr, DFX_MEM_FASTBOOTLOG_SIZE)) {
		pr_err("%s copy to user failed\n", __func__);
		return -EFAULT;
	}

	return size;
}

static const struct proc_ops test3_proc_fops = {
	.proc_open      = test3_open,
	.proc_read      = test3_read,
	.proc_release   = single_release,
};

static int test4_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static ssize_t test4_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	void *log_addr = ioremap_wc(DFX_MEM_FASTBOOTLOG_ADDR, DFX_MEM_FASTBOOTLOG_SIZE);

	pr_info("start\n");

	int *a = NULL;

	pr_info("%d\n", a[10]);

	if (ppos == NULL) {
		pr_err("ppos is null\n");
		return 0;
	}

	if (buf == NULL) {
		pr_err("buf is null\n");
		return 0;
	}

	if (copy_to_user(buf, log_addr, DFX_MEM_FASTBOOTLOG_SIZE)) {
		pr_err("%s copy to user failed\n", __func__);
		return -EFAULT;
	}

	return size;
}

static const struct proc_ops test4_proc_fops = {
	.proc_open      = test4_open,
	.proc_read      = test4_read,
	.proc_release   = single_release,
};

static int testlog_node_init(void)
{
	struct proc_dir_entry *de;

	de = proc_create("test1", 0440, NULL, &test1_proc_fops);
	if (!de) {
		pr_err("test1 proc created failed\n");
		return -ENOENT;
	}

	de = proc_create("test2", 0440, NULL, &test2_proc_fops);
	if (!de) {
		pr_err("test2 proc created failed\n");
		return -ENOENT;
	}

	de = proc_create("test3", 0440, NULL, &test3_proc_fops);
	if (!de) {
		pr_err("test3 proc created failed\n");
		return -ENOENT;
	}

	de = proc_create("test4", 0440, NULL, &test4_proc_fops);
	if (!de) {
		pr_err("test4 proc created failed\n");
		return -ENOENT;
	}

	return 0;
}

void mdr_exception_register_success(void)
{
	int ret = 0;

	ret = mdr_register_exception(&ap_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}
}

void mdr_exception_register_multi_success(void)
{
	int ret = 0;

	ret = mdr_register_exception(&ap_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}

	ret = mdr_register_exception(&npu_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}
}

void mdr_exception_register_mid_fail(void)
{
	int ret = 0;

	ret = mdr_register_exception(&ap_exception_mid_error);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}
}

void mdr_exception_register_mid_duplicate_fail(void)
{
	int ret = 0;

	ret = mdr_register_exception(&ap_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}

	ret = mdr_register_exception(&ap_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}
}

void mdr_module_register_resetops_null(void)
{
	int ret = 0;
	struct mdr_register_module_result retinfo;
	struct mdr_module_ops s_soc_ops = {
			.ops_dump			= NULL,
			.ops_reset			= NULL,
	};

	ret = mdr_register_module_ops(MDR_AP, &s_soc_ops, &retinfo);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}
}

void mdr_module_register_dumpops_null(void)
{
	int ret = 0;
	struct mdr_register_module_result retinfo;
	struct mdr_module_ops s_soc_ops = {
			.ops_dump			= NULL,
			.ops_reset			= NULL,
	};

	ret = mdr_register_module_ops(MDR_AP, &s_soc_ops, &retinfo);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}
}

void mdr_module_register_coreid_duplicate(void)
{
	int ret = 0;
	struct mdr_register_module_result retinfo;

	ret = mdr_register_module_ops(MDR_AP, &s_soc_ops, &retinfo);
	if (ret < 0) {
		pr_err("module register fail\n");
		return;
	}

	ret = mdr_register_module_ops(MDR_AP, &s_soc_ops, &retinfo);
	if (ret < 0) {
		pr_err("module register fail\n");
		return;
	}
}

void mdr_module_register_success(void)
{
	int ret = 0;
	struct mdr_register_module_result retinfo;

	ret = mdr_register_module_ops(MDR_AP, &s_soc_ops, &retinfo);
	if (ret < 0) {
		pr_err("module register ap fail\n");
		return;
	}
}

void mdr_module_register_multi_success(void)
{
	int ret = 0;
	struct mdr_register_module_result retinfo;

	ret = mdr_register_module_ops(MDR_AP, &s_soc_ops, &retinfo);
	if (ret < 0) {
		pr_err("module register ap fail\n");
		return;
	}

	ret = mdr_register_module_ops(MDR_NPU, &s_npu_ops, &retinfo);
	if (ret < 0) {
		pr_err("module register npu fail\n");
		return;
	}
}

void mdr_singlemodule_test(void)
{
	int ret = 0;
	struct mdr_register_module_result retinfo;

	ret = mdr_register_exception(&ap_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}

	ret = mdr_register_module_ops(MDR_AP, &s_soc_ops, &retinfo);
	if (ret < 0) {
		pr_err("module register fail\n");
		return;
	}
}

void mdr_multimodule_test(void)
{
	int ret = 0;
	u32 i;

	testlog_node_init();

	ret = mdr_register_exception(&npu_exception);
	if (ret == 0)
		pr_err("exception register fail\n");

	for (i = 0; i < ARRAY_SIZE(test1_exception); i++) {
		ret = mdr_register_exception(&test1_exception[i]);
		if (ret == 0) {
			pr_err("exception register fail\n");
			return;
		}
	}

	ret = mdr_filesys_write_log(MDR_TEST1, "/proc/test1", "test1.log",
			DFX_MEM_FASTBOOTLOG_SIZE);
	if (ret < 0) {
		pr_err("mdr_filesys_write_log fail, ret %d.\n", ret);
		return;
	}

	ret = mdr_register_exception(&test2_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}

	ret = mdr_filesys_write_log(MDR_TEST2, "/proc/test2", "test2.log",
			DFX_MEM_FASTBOOTLOG_SIZE);
	if (ret < 0) {
		pr_err("mdr_filesys_write_log fail, ret %d.\n", ret);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(test3_exception); i++) {
		ret = mdr_register_exception(&test3_exception[i]);
		if (ret == 0) {
			pr_err("exception register fail\n");
			return;
		}
	}

	ret = mdr_filesys_write_log(MDR_TEST3, "/proc/test3", "test3.log",
			DFX_MEM_FASTBOOTLOG_SIZE);
	if (ret < 0) {
		pr_err("mdr_filesys_write_log fail, ret %d.\n", ret);
		return;
	}

	ret = mdr_register_exception(&test4_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}

	ret = mdr_filesys_write_log(MDR_TEST4, "/proc/test4", "test4.log",
			DFX_MEM_FASTBOOTLOG_SIZE);
	if (ret < 0) {
		pr_err("mdr_filesys_write_log fail, ret %d.\n", ret);
		ret = 0;
	}
}

void mdr_wlog_nodename_size_error(void)
{
	int ret;

	ret = mdr_filesys_write_log(MDR_TEST3, "/proc/test3testtesttesttesttesttest", "test3.log",
			DFX_MEM_FASTBOOTLOG_SIZE);
	if (ret < 0)
		pr_err("mdr_filesys_write_log fail, ret %d.\n", ret);
}

void mdr_wlog_logname_size_error(void)
{
	int ret;

	ret = mdr_filesys_write_log(MDR_TEST3, "/proc/test3", "test3.logtesttesttesttesttesttesttesttest",
			DFX_MEM_FASTBOOTLOG_SIZE);
	if (ret < 0)
		pr_err("mdr_filesys_write_log fail, ret %d.\n", ret);
}

void mdr_wlog_log_size_error(void)
{
	int ret;

	ret = mdr_filesys_write_log(MDR_TEST3, "/proc/test3", "test3.log",
			100 * DFX_MEM_FASTBOOTLOG_SIZE);
	if (ret < 0)
		pr_err("mdr_filesys_write_log fail, ret %d.\n", ret);
}

void mdr_lpctrl_test(void)
{
	int ret = 0;
	struct mdr_register_module_result retinfo;

	ret = mdr_register_exception(&npu_exception);
	if (ret == 0) {
		pr_err("exception register fail\n");
		return;
	}

	ret = mdr_register_module_ops(MDR_NPU, &s_npu_ops, &retinfo);
	if (ret < 0)
		pr_err("module register fail\n");
}

void ap_die_test(void)
{
	u32 val;
	u32 shift = 32;

	val = 0x1 << shift;
}
