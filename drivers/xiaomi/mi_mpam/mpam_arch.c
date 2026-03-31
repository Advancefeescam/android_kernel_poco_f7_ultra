// SPDX-License-Identifier: GPL-2.0+
/*
 * Module-based hack to drive MPAM functionality
 *
 * NOTICE: This circumvents existing infrastructure to discover and enable CPU
 * features and attempts to contain everything within a loadable module. This is
 * *not* the right way to do things, but it is one way to start testing MPAM on
 * real hardware.
 *
 * Copyright (C) 2023 Xiaomi Ltd.
 */

#define DEBUG

#define pr_fmt(fmt) "MPAM_arch: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <soc/qcom/mpam.h>
#include "../../../kernel/sched/walt/walt.h"
#include <asm/sysreg.h>

#include "mpam_arch.h"

MODULE_LICENSE("GPL");

struct msc_part_kobj {
	struct mpam_msc *msc;
	unsigned int partid;
	struct kobject kobj;
};

struct mpam_msc {
	spinlock_t lock;
	unsigned int partid_count;

	struct kobject ko_root;
	struct kobject ko_part_dir;
	struct msc_part_kobj *ko_parts;
};

struct mpam_validation_masks {
	cpumask_var_t visited_cpus;
	cpumask_var_t supported_cpus;
	spinlock_t lock;
};

static __read_mostly unsigned int mpam_partid_count = UINT_MAX;
static u32 cpbm_local[64] = {100};

int mpam_arch_initd = 0;
EXPORT_SYMBOL_GPL(mpam_arch_initd);
/* The MPAM0_EL1.PARTID_D in use by a given CPU */
static DEFINE_PER_CPU(unsigned int, mpam_local_partid) = MPAM_PARTID_DEFAULT;

static void mpam_set_el0_partid(unsigned int inst_id, unsigned int data_id)
{
	u64 reg;

	cant_migrate();

	reg = read_sysreg_s(SYS_MPAM0_EL1);

	FIELD_SET(reg, MPAM0_EL1_PARTID_I, inst_id);
	FIELD_SET(reg, MPAM0_EL1_PARTID_D, data_id);

	write_sysreg_s(reg, SYS_MPAM0_EL1);
	write_sysreg_s(reg, SYS_MPAM1_EL1);
	/*
	 * Note: if the scope is limited to userspace, we'll get an EL switch
	 * before getting back to US which will be our context synchronization
	 * event, so this won't be necessary.
	 */
	isb();
}

/*
 * Write the PARTID to use on the local CPU.
 */
void mpam_write_partid(unsigned int partid)
{
	WARN_ON_ONCE(preemptible());
	WARN_ON_ONCE(partid >= mpam_partid_count);

	if (partid == this_cpu_read(mpam_local_partid))
		return;

	this_cpu_write(mpam_local_partid, partid);
	mpam_set_el0_partid(partid, partid);
}
EXPORT_SYMBOL_GPL(mpam_write_partid);


/*
 * Same as mpam_sync_task(), with a pre-filter for the current task.
 */
static void mpam_sync_current(void *task)
{
	if (task && task != current)
		return;

	mpam_sync_task(current);
}

static bool __task_curr(struct task_struct *p)
{
	return cpu_curr(task_cpu(p)) == p;
}

static void mpam_kick_task(struct task_struct *p)
{
	/*
	 * If @p is no longer on the task_cpu(p) we see here when the smp_call
	 * actually runs, then it had a context switch, so it doesn't need the
	 * explicit update - no need to chase after it.
	 */
	if (__task_curr(p))
		smp_call_function_single(task_cpu(p), mpam_sync_current, p, 1);
}

unsigned int mpam_get_task_partid(struct task_struct *p)
{
	return READ_ONCE(p->thread.android_vendor_data1);
}
EXPORT_SYMBOL_GPL(mpam_get_task_partid);

void mpam_set_task_partid(struct task_struct *p, unsigned int partid)
{
	WRITE_ONCE(p->thread.android_vendor_data1, partid);
	mpam_kick_task(p);
}
EXPORT_SYMBOL_GPL(mpam_set_task_partid);

/*
 * Sync @p's associated PARTID with this CPU's register.
 */
void mpam_sync_task(struct task_struct *p)
{
	mpam_write_partid(mpam_get_task_partid(p));
}
EXPORT_SYMBOL_GPL(mpam_sync_task);


static int mpam_msc_set_cpbm(unsigned int id,
			const unsigned int percent, u64 config_ctrl)
{
	int ret;

	if (id < MPAM_PARTID_DEFAULT || id > mpam_partid_count)
		goto err;
	if (percent < MPAM_PORTION_MIN || percent > MPAM_PORTION_MAX)
		goto err;
	if (config_ctrl < MPAM_ZONE_FIRST || config_ctrl > MPAM_ZONE_LAST)
		goto err;

	ret = qcom_mpam_set_cache_portion(id, percent, config_ctrl);
	if(ret) {
		pr_err("set cache portion failed ret %d\n", ret);
		return -EINVAL;
	}

	cpbm_local[id] = percent;
	return 0;

err:
	pr_err("invald val of partid and percent\n");
	return -EINVAL;
}

static ssize_t mpam_msc_cpbm_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	int ret;
	u64 zone;
	DECLARE_BITMAP(cache_mpam, BITS_PER_TYPE(u64));
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);

	ret = qcom_mpam_get_cache_portion(mpk->partid, &zone);
	if (ret)
		pr_err("get mpam error zone valve\n");

	bitmap_from_u64(cache_mpam, zone);

	return scnprintf(buf, PAGE_SIZE, "percent:%u zone:%*pbl\n",
			cpbm_local[mpk->partid], 64, cache_mpam);
}

static ssize_t mpam_msc_cpbm_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t size)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	int ret;
	u32 cpbm_val, zone;
	char *c;
	char kbuf[7];

	if (size > sizeof(kbuf) - 1)
		goto err;

	memcpy(kbuf, buf, size);

	kbuf[size] = 0;

	c = strchr(kbuf, ' ');
	if (!c)
		goto err;
	*c = '\0';

	ret = kstrtouint(kbuf, 0, &cpbm_val);
	ret = kstrtouint(c+1, 0, &zone);
	if (ret)
		goto err;

	ret = mpam_msc_set_cpbm(mpk->partid, cpbm_val, zone);
	if (ret)
		goto err;

	return ret ?: size;

err:
	pr_err("usage: echo <percentage> <config_ctrl> > cpbm\n");
	return -EINVAL;
}

static ssize_t mpam_msc_tasks_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	struct task_struct *p, *t;
	ssize_t len = 0;

	rcu_read_lock();
	for_each_process_thread(p, t) {
		if (mpam_get_task_partid(t) == mpk->partid)
			len += scnprintf(buf + len, PAGE_SIZE - len, "%d\n", t->pid);
	}
	rcu_read_unlock();

	return len;
}

static ssize_t mpam_msc_tasks_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t size)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	struct task_struct *p;

	/* PID limit is the millions, 7 chars + newline + \0 */
	int ret = 0;
	pid_t pid;

	if (kstrtouint(buf, 0, &pid))
		return -EINVAL;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		ret = -EINVAL;
		rcu_read_unlock();
		goto out;
	}

	get_task_struct(p);
	rcu_read_unlock();

	mpam_set_task_partid(p, mpk->partid);

	put_task_struct(p);

out:
	return ret ?: size;
}

static ssize_t mpam_msc_procs_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	struct task_struct *p;
	ssize_t len = 0;

	rcu_read_lock();
	for_each_process(p) {
		if (mpam_get_task_partid(p) == mpk->partid)
			len += scnprintf(buf + len, PAGE_SIZE - len, "%d\n", p->pid);
	}
	rcu_read_unlock();

	return len;
}

static ssize_t mpam_msc_procs_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t size)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	struct task_struct *tsk, *leader,* task;
	int ret = 0;
	pid_t pid;

	if (kstrtouint(buf, 0, &pid))
		return -EINVAL;

	rcu_read_lock();
	if (pid) {
		tsk = find_task_by_vpid(pid);
		if (!tsk) {
			tsk = ERR_PTR(-ESRCH);
			rcu_read_unlock();
			goto out;
		}
	} else {
		tsk = current;
	}

	leader = tsk->group_leader;
	get_task_struct(leader);
	rcu_read_unlock();

	task = leader;
	do {
		mpam_set_task_partid(task, mpk->partid);
	} while_each_thread(leader, task);

	put_task_struct(leader);

out:
	return ret ?: size;
}

static struct kobj_attribute mpam_msc_cpbm_attr =
	__ATTR(cpbm, 0644, mpam_msc_cpbm_show, mpam_msc_cpbm_store);

static struct kobj_attribute mpam_msc_tasks_attr =
	__ATTR(tasks, 0644, mpam_msc_tasks_show, mpam_msc_tasks_store);

static struct kobj_attribute mpam_msc_procs_attr =
	__ATTR(procs, 0644, mpam_msc_procs_show, mpam_msc_procs_store);

static struct attribute *mpam_msc_ctrl_attrs[] = {
	&mpam_msc_cpbm_attr.attr,
	&mpam_msc_tasks_attr.attr,
	&mpam_msc_procs_attr.attr,
	NULL,
};

static umode_t mpam_msc_ctrl_attr_visible(struct kobject *kobj,
				     struct attribute *attr,
				     int n)
{
	struct msc_part_kobj *mpk;

	mpk = container_of(kobj, struct msc_part_kobj, kobj);

	if (attr == &mpam_msc_cpbm_attr.attr)
		goto visible;

	if (attr == &mpam_msc_tasks_attr.attr)
		goto visible;

	if (attr == &mpam_msc_procs_attr.attr)
		goto visible;

	return 0;

visible:
	return attr->mode;
}

static struct attribute_group mpam_msc_ctrl_attr_group = {
	.attrs = mpam_msc_ctrl_attrs,
	.is_visible = mpam_msc_ctrl_attr_visible,
};


static struct kobj_type mpam_kobj_ktype = {
	.sysfs_ops	= &kobj_sysfs_ops,
};

/*
 * sysfs/
 *   mpam/
 *     partitions/
 *       0/cpbm, tasks, procs
 *       1/cpbm, tasks, procs
 *       ...
 */
static int mpam_msc_create_sysfs(struct mpam_msc *msc)
{
	unsigned int partid_count = min(mpam_partid_count, msc->partid_count);
	unsigned int part, tmp;
	int ret;

	kobject_init(&msc->ko_root, &mpam_kobj_ktype);
	ret = kobject_add(&msc->ko_root, fs_kobj, "mpam");
	if (ret)
		goto err_root;

	kobject_init(&msc->ko_part_dir, &mpam_kobj_ktype);
	ret = kobject_add(&msc->ko_part_dir, &msc->ko_root, "partitions");
	if (ret)
		goto err_part_dir;

	msc->ko_parts = kzalloc(sizeof(*msc->ko_parts) * partid_count,
				     GFP_KERNEL);
	if (!msc->ko_parts) {
		ret = -ENOMEM;
		goto err_part_dir;
	}

	for (part = MPAM_PARTID_DEFAULT; part < partid_count; part++) {
		kobject_init(&msc->ko_parts[part].kobj, &mpam_kobj_ktype);
		msc->ko_parts[part].msc = msc;
		msc->ko_parts[part].partid = part;
		ret = kobject_add(&msc->ko_parts[part].kobj, &msc->ko_part_dir, "%d", part);
		if (ret)
			goto err_parts_add;
	}

	for (part = MPAM_PARTID_DEFAULT; part < partid_count; part++) {
		ret = sysfs_create_group(&msc->ko_parts[part].kobj, &mpam_msc_ctrl_attr_group);
		if (ret)
			goto err_parts_grp;
	}

	mpam_arch_initd = 1;
	return 0;

err_parts_grp:
	for (tmp = MPAM_PARTID_DEFAULT; tmp < part; tmp++)
		sysfs_remove_group(&msc->ko_parts[tmp ].kobj, &mpam_msc_ctrl_attr_group);
	part = partid_count - 1;

err_parts_add:
	for (tmp = MPAM_PARTID_DEFAULT; tmp < part; tmp++)
		kobject_put(&msc->ko_parts[tmp ].kobj);

	kfree(msc->ko_parts);
err_part_dir:
	kobject_put(&msc->ko_part_dir);
err_root:
	kobject_put(&msc->ko_root);
	return ret;
}


static int mpam_msc_initialize(struct mpam_msc *msc)
{
	int partid;

	/*
	 * We're using helpers that expect the lock to be held, but we're
	 * setting things up and there is no interface yet, so nothing can
	 * race with us. Make lockdep happy, and save ourselves from a couple
	 * of lock/unlock.
	 */
	spin_acquire(&msc->lock.dep_map, 0, 0, _THIS_IP_);

	msc->partid_count = mpam_partid_count;
	for (partid = MPAM_PARTID_DEFAULT; partid < mpam_partid_count; partid++) {
		mpam_msc_set_cpbm(partid, MPAM_PORTION_MAX, MPAM_ZONE_FIRST);
	}

	spin_release(&msc->lock.dep_map, _THIS_IP_);

	return mpam_msc_create_sysfs(msc);
}

static int mpam_init_arch(void)
{
	struct mpam_msc *msc;
	int ret;

	msc = kzalloc(sizeof(*msc), GFP_KERNEL);
	if (!msc)
		return -ENOMEM;

	spin_lock_init(&msc->lock);
	ret = mpam_msc_initialize(msc);

	return ret;
}


static void mpam_validate_cpu(void *info)
{
	struct mpam_validation_masks *masks = (struct mpam_validation_masks *)info;
	unsigned int partid_count;
	bool valid = true;

	if (!FIELD_GET(ID_AA64PFR0_MPAM, read_sysreg_s(SYS_ID_AA64PFR0_EL1))) {
		valid = false;
		goto out;
	}

	if (!FIELD_GET(MPAM1_EL1_MPAMEN, read_sysreg_s(SYS_MPAM1_EL1))) {
		valid = false;
		goto out;
	}

	partid_count = FIELD_GET(MPAMIDR_EL1_PARTID_MAX, read_sysreg_s(SYS_MPAMIDR_EL1)) + 1;

	spin_lock(&masks->lock);
	mpam_partid_count = min(partid_count, mpam_partid_count);
	spin_unlock(&masks->lock);

out:
	cpumask_set_cpu(smp_processor_id(), masks->visited_cpus);
	if (valid)
		cpumask_set_cpu(smp_processor_id(), masks->supported_cpus);
}

/*
 * Does the system support MPAM, and if so is it actually usable?
 */
static int mpam_validate_sys(void)
{
	struct mpam_validation_masks masks;
	int ret = 0;

	if (!zalloc_cpumask_var(&masks.visited_cpus, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&masks.supported_cpus, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out_free_visited;
	}
	spin_lock_init(&masks.lock);

	on_each_cpu_cond_mask(NULL, mpam_validate_cpu, &masks, true, cpu_present_mask);

	if (!cpumask_equal(masks.visited_cpus, cpu_present_mask)) {
		pr_warn("Could not check all CPUs for MPAM settings (visited %*pbl)\n",
			cpumask_pr_args(masks.visited_cpus));
		ret = -ENODATA;
		goto out;
	}

	if (!cpumask_equal(masks.visited_cpus, masks.supported_cpus)) {
		pr_warn("MPAM only supported on CPUs [%*pbl]\n",
			cpumask_pr_args(masks.supported_cpus));
		ret = -EOPNOTSUPP;
	}
out:
	free_cpumask_var(masks.supported_cpus);
out_free_visited:
	free_cpumask_var(masks.visited_cpus);

	return ret;
}

static int __init mpam_arch_driver_init(void)
{
	int ret;

	/* Does the system support MPAM at all? */
	ret = mpam_validate_sys();
	if (ret)
		return 0;

	return mpam_init_arch();
}

module_init(mpam_arch_driver_init);
