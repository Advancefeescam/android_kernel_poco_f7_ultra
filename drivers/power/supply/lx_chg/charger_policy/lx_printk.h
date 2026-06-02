/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LX_PRINTK__
#define __LX_PRINTK__

#include <linux/printk.h>

#ifdef KLOG_MODNAME
#undef KLOG_MODNAME
#define KLOG_MODNAME ""
#endif

#define TAG                     "[LX_CHG_DEFAULT]" // [VENDOR_MODULE_SUBMODULE]
#define lx_err(fmt, ...)        pr_err(TAG "[%s]: " fmt, __func__, ##__VA_ARGS__)
#define lx_warn(fmt, ...)       pr_warn(TAG "[%s]: " fmt, __func__, ##__VA_ARGS__)
#define lx_notice(fmt, ...)     pr_notice(TAG "[%s]: " fmt, __func__, ##__VA_ARGS__)
#define lx_info(fmt, ...)       pr_info(TAG "[%s]: " fmt, __func__, ##__VA_ARGS__)
#define lx_debug(fmt, ...)      pr_debug(TAG "[%s]: " fmt, __func__, ##__VA_ARGS__)

#endif
