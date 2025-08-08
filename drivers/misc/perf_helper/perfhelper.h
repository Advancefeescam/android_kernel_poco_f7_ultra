//MIUI ADD: Performance_BoostFramework
#ifndef __PERF_HELPER_H__
#define __PERF_HELPER_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/swap.h>
#include <linux/gfp.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/mmzone.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/mmzone.h>
#include <linux/limits.h>
#include <linux/kthread.h>
#include <linux/pm.h>
#include <linux/cgroup.h>
#include <linux/memcontrol.h>
#include <trace/hooks/vmscan.h>

#define MAX_RECORD_NUM		2048
#define MAX_ONE_RECORD_SIZE	128
#define BUFF_SIZE           64

extern void perflock_init(void);
extern void perflock_exit(void);
extern void mimd_init(void);
extern void mimd_exit(void);
extern void sched_assi_init(void);
extern void sched_assi_exit(void);
#endif
//END Performance_BoostFramework
