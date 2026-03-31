// SPDX-License-Identifier: GPL-2.0+
/*
 * Module interface to control MPAM policy.
 *
 * Copyright (C) 2023 Xiaomi Ltd.
 */

#define DEBUG

#define pr_fmt(fmt) "MPAM_policy: " fmt

#include <linux/module.h>
#include <linux/cgroup.h>
#include <linux/sched/cputime.h>
#include <trace/hooks/fpsimd.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>

#include "../../../kernel/sched/sched.h"
#include "mpam_arch.h"

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("mpam_policy pre: mpam_arch");


static void mpam_hook_fork(void __always_unused *data,
			   struct task_struct *p)
{
	if (p->thread.android_vendor_data1 == 0) {
		WARN_ON(1);
		mpam_set_task_partid(p, MPAM_PARTID_DEFAULT);
	}
}

static void mpam_hook_switch(void __always_unused *data,
			     struct task_struct *prev, struct task_struct *next)
{
	mpam_sync_task(next);
}

#ifdef CONFIG_MPAM_ENABLE_TOPAPP
static int cpuqos_subsys_id = cpu_cgrp_id;

static void mpam_hook_attach(void __always_unused *data,
			     struct cgroup_subsys *ss, struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct task_struct *p;
	char buf[50];

	if (ss->id != cpuqos_subsys_id)
		return;

	cgroup_taskset_first(tset, &css);
	cgroup_path(css->cgroup, buf, 50);

	if (!strcmp(buf, "/top-app")) {
		cgroup_taskset_for_each(p, css, tset){
			mpam_set_task_partid(p, 17);
		}
	} else {
		cgroup_taskset_for_each(p, css, tset) {
			mpam_set_task_partid(p, MPAM_PARTID_DEFAULT);
		}
	}
}
#endif
/*
 * Default-16 is a sensible thing, and it avoids us having to do anything
 * to setup the task_struct vendor data field that serves as partid.
 * If it becomes different than zero, we need the following after registering
 * the sched_switch hook:
 * - a for_each_process_thread() loop, to initialize existing tasks
 * - a trace_task_newtask hook, to initialize tasks that are being
 *   forked and may not be covered by the above loop
 */
static_assert(MPAM_PARTID_DEFAULT == 16);
static int mpam_policy_hooks_init(void)
{
	int ret = 0;
	struct task_struct *p, *t;

	rcu_read_lock();
	for_each_process_thread(p, t) {
		mpam_set_task_partid(t, MPAM_PARTID_DEFAULT);
	}
	rcu_read_unlock();

#ifdef CONFIG_MPAM_ENABLE_TOPAPP
	ret = register_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
	if(ret)
		return ret;
#endif

	ret = register_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
	if (ret)
		goto out_attach;

	ret = register_trace_android_rvh_sched_fork(mpam_hook_fork, NULL);
	if (ret)
		goto out_attach_switch;

	return ret;

out_attach_switch:
	unregister_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
out_attach:
#ifdef CONFIG_MPAM_ENABLE_TOPAPP
	unregister_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
#endif
	return ret;
}

static int __init mpam_policy_init(void)
{
	int ret = 0;

	if (!mpam_arch_initd)
		return ret;
	ret = mpam_policy_hooks_init();

	return ret;
}

module_init(mpam_policy_init);