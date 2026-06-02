/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LX_PRINTK__
#define __LX_PRINTK__

#include <linux/printk.h>

#ifdef KLOG_MODNAME
#undef KLOG_MODNAME
#define KLOG_MODNAME ""
#endif

#define LXLOG_ERROR_LEVEL   3
#define LXLOG_INFO_LEVEL    6

#define LXLOG_DEFAULT_LEVEL		8

#define TAG                     "[LX_CHG_DEFAULT]" // [VENDOR_MODULE_SUBMODULE]

#define lx_err(fmt, ...)   \
do {\
	if (LXLOG_DEFAULT_LEVEL >= LXLOG_ERROR_LEVEL) {\
		pr_err(TAG "[%s]: " fmt, __func__, ##__VA_ARGS__); \
	} \
} while (0)

#define lx_info(fmt, ...)   \
	do {\
		if (LXLOG_DEFAULT_LEVEL >= LXLOG_INFO_LEVEL) {\
			pr_info(TAG "[%s]: " fmt, __func__, ##__VA_ARGS__); \
		} \
	} while (0)


#endif
