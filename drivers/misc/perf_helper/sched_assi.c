//MIUI ADD: Performance_BoostFramework
#include "perfhelper.h"
#include <linux/sched/walt.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/cputime.h>
#include <trace/hooks/sched.h>
#include "../../../kernel/sched/sched.h"
#include "../../../kernel/sched/walt/walt.h"

#define TASK_LONG_RUNNABLE_DEBUG_DISABLE	60000
#define SCHEDULER_TICK_PRINT_INTERVAL	30000

struct kobject *sched_assi_kobj;

static u64 sched_long_runnable_check = TASK_LONG_RUNNABLE_DEBUG_DISABLE;

static int is_cfs_task(struct task_struct *task)
{
	if (task && task->prio >= MAX_RT_PRIO && !is_idle_task(task))
		return 1;

	return 0;
}

void mi_cfs_enqueue_runnable_task(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (list_empty(&wts->runnable_list) && wts->runnable_start == 0) {
		wts->runnable_start = sched_clock();
		list_add_tail(&wts->runnable_list, &wrq->runnable_tasks);
	}
}

void mi_cfs_dequeue_runnable_task(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (!list_empty(&wts->runnable_list) && wts->runnable_list.next) {
		wts->runnable_start = 0;
		list_del_init(&wts->runnable_list);
	}
}

static void android_rvh_tick_entry(void *unused, struct rq *rq)
{
	struct walt_rq *wrq = &per_cpu(walt_rq, cpu_of(rq));
	struct walt_task_struct *wts = NULL;
	u64 task_runnable_thres = sched_long_runnable_check * 1000 * 1000;
	u64 delta = 0;
	u64 now = 0;

	if (!list_empty(&wrq->runnable_tasks) && sched_long_runnable_check < TASK_LONG_RUNNABLE_DEBUG_DISABLE) {
		wts = list_first_entry(&wrq->runnable_tasks, struct walt_task_struct, runnable_list);
		now = sched_clock();
		delta = now - wts->runnable_start;
		if (delta > task_runnable_thres && wts->runnable_start != 0) {
			wrq->privilege_disable = true;
		} else {
			wrq->privilege_disable = false;
		}
	} else {
		wrq->privilege_disable = false;
	}
}

static void android_rvh_schedule(void *unused, struct task_struct *prev,
		struct task_struct *next, struct rq *rq)
{
	if (likely(prev != next)) {
		if (prev->on_rq == TASK_ON_RQ_QUEUED && is_cfs_task(prev))
			mi_cfs_enqueue_runnable_task(rq, prev);
		if (is_cfs_task(next))
			mi_cfs_dequeue_runnable_task(next);
	}
}

static void android_rvh_enqueue_task(void *unused, struct rq *rq,
		struct task_struct *p, int flags)
{
	if (is_cfs_task(p))
		mi_cfs_enqueue_runnable_task(rq, p);
}

static void android_rvh_dequeue_task(void *unused, struct rq *rq,
		struct task_struct *p, int flags)
{
	if (is_cfs_task(p))
		mi_cfs_dequeue_runnable_task(p);
}

static ssize_t sched_long_runnable_check_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t res = 0;

	res = snprintf(buf, BUFF_SIZE, "%llu\n", sched_long_runnable_check);

	return res;
}

static ssize_t sched_long_runnable_check_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u64 val = 0;
	int err = 0;

	err = kstrtoull(buf, 10, &val);
	if (err != 0)
		return err;

	sched_long_runnable_check = val;

	return count;
}

static struct kobj_attribute sched_long_runnable_attr = __ATTR(sched_long_runnable, 0664, sched_long_runnable_check_show, sched_long_runnable_check_store);

static struct attribute *sched_assi_attrs[] = {
	&sched_long_runnable_attr.attr,
	NULL,
};

static const struct attribute_group sched_assi_attr_group = {
	.attrs = sched_assi_attrs,
};


void sched_assi_init(void)
{
	register_trace_android_rvh_tick_entry(android_rvh_tick_entry, NULL);
	register_trace_android_rvh_schedule(android_rvh_schedule, NULL);
	register_trace_android_rvh_enqueue_task(android_rvh_enqueue_task, NULL);
	register_trace_android_rvh_dequeue_task(android_rvh_dequeue_task, NULL);

	sched_assi_kobj = kobject_create_and_add("sched_assi", &THIS_MODULE->mkobj.kobj);
	if (sched_assi_kobj) {
		if (sysfs_create_group(sched_assi_kobj, &sched_assi_attr_group))
			printk(KERN_ERR "%s create sched_assi sysfs nodes group failed\n", __func__);
	}
}

void sched_assi_exit(void)
{
	if (sched_assi_kobj) {
		sysfs_remove_group(sched_assi_kobj, &sched_assi_attr_group);
		kobject_put(sched_assi_kobj);
	}
}
//END Performance_BoostFramework
