#include <linux/swap.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/sched/task.h>
#include <linux/sched/clock.h>
#include <linux/sched/cputime.h>
#include <../../../kernel/sched/sched.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/dtask.h>

/* |63-------->8|7-------->0| */
/* |speedui flag|setaffinity| */
#define MI_SPEED_UI_FLAG		0x5350454544554100
#define MI_SPEED_UI_AFFINITY_MASK 	0xFF
#define AFFINITY_ENABLE			(1 << 0)
#define Android_OEM_DATA_VALUE		4

static const char *speeduiThread = "SchedBoostServi";
static unsigned int speedui_enable = 1;
module_param(speedui_enable, uint, 0644);

struct speedui_task_struct {
	unsigned long flag;
	unsigned int affinity;
	spinlock_t lock_affinity;
};

static struct kmem_cache *speedui_node_cache;

static bool init_speedui_cachep(void)
{
	speedui_node_cache = kmem_cache_create("speedui_node", sizeof(struct speedui_task_struct),
		0, SLAB_HWCACHE_ALIGN | SLAB_PANIC | SLAB_ACCOUNT, NULL);
	return speedui_node_cache != NULL;
}

static inline struct speedui_task_struct *speedui_node_alloc(gfp_t gfp_flag)
{
	return speedui_node_cache ? kmem_cache_alloc(speedui_node_cache, gfp_flag) : NULL;
}

static inline void speedui_node_free(struct speedui_task_struct *speedui_task)
{
	if (speedui_task) {
		kmem_cache_free(speedui_node_cache, speedui_task);
		speedui_task = NULL;
	}
}

static void speedui_hook_fork(void *ignore, struct task_struct *p)
{
	struct speedui_task_struct *speedui_task;
	if (p) {
		get_task_struct(p);
		p->android_oem_data1[Android_OEM_DATA_VALUE] = 0;
		speedui_task = speedui_node_alloc(GFP_KERNEL);
		if (!speedui_task) {
			pr_err("speed_ui: tsk %s %d failed to allocate speedui_task\n",
				p->comm, p->pid);
			put_task_struct(p);
			return;
		}
		speedui_task->flag = MI_SPEED_UI_FLAG;
		speedui_task->affinity = 0;
		spin_lock_init(&speedui_task->lock_affinity);
		p->android_oem_data1[Android_OEM_DATA_VALUE] = (unsigned long)speedui_task;
		put_task_struct(p);
	}
}

static struct speedui_task_struct *get_speedui_task_struct(struct task_struct *p)
{
	struct speedui_task_struct *speedui_task =
		(struct speedui_task_struct *)p->android_oem_data1[Android_OEM_DATA_VALUE];
	if (speedui_task && speedui_task->flag == MI_SPEED_UI_FLAG) {
		return speedui_task;
	}
	return NULL;
}

static inline bool check_speedui_data(struct task_struct *p)
{
	struct speedui_task_struct *speedui_task = get_speedui_task_struct(p);
	if (!speedui_task || speedui_task->flag != MI_SPEED_UI_FLAG)
		return false;
	return true;
}

static inline void set_speedui_affinity_data(struct task_struct *p, bool flag, int value)
{
	struct speedui_task_struct *speedui_task = get_speedui_task_struct(p);
	if (!speedui_task)
		return;
	spin_lock(&speedui_task->lock_affinity);
	if (flag) {
		speedui_task->affinity |= value;
	} else {
		speedui_task->affinity &= ~value;
	}
	spin_unlock(&speedui_task->lock_affinity);
}

static inline unsigned long get_speedui_affinity_data(struct task_struct *p)
{
	unsigned long affinity;
	struct speedui_task_struct *speedui_task = get_speedui_task_struct(p);
	if (!speedui_task)
		return 0xff;
	spin_lock(&speedui_task->lock_affinity);
	affinity = speedui_task->affinity;
	spin_unlock(&speedui_task->lock_affinity);
	return affinity;
}

static void android_rvh_sched_setaffinity(void *unused, struct task_struct *p,
						const struct cpumask *in_mask, int *retval)
{
	struct cpumask cpus_requested;
	if (*retval || !p || !check_speedui_data(p))
		return;
	get_task_struct(p);
	if (!strncmp(current->comm, speeduiThread, strlen(speeduiThread))) {
		if (!(p->flags & PF_KTHREAD)) {
			cpumask_and(&cpus_requested, in_mask, cpu_possible_mask);
			set_speedui_affinity_data(p, false, MI_SPEED_UI_AFFINITY_MASK);
			set_speedui_affinity_data(p, true, cpumask_bits(&cpus_requested)[0]);
		}
	}
	put_task_struct(p);
}

static void android_rvh_update_cpus_allowed(void *unused, struct task_struct *p,
						cpumask_var_t cpus_requested,
						const struct cpumask *new_mask, int *ret)
{
	unsigned long affinity;
	const unsigned long *affinityp = &affinity;
	struct cpumask speedui_cpus_requested;
	DECLARE_BITMAP(affinity_bitmap, 8);
	if (!p || !(speedui_enable & AFFINITY_ENABLE) || !check_speedui_data(p))
		return;
	get_task_struct(p);
	affinity = get_speedui_affinity_data(p);
	bitmap_copy(affinity_bitmap, affinityp, 8);
	cpumask_clear(&speedui_cpus_requested);
	cpumask_copy(&speedui_cpus_requested, to_cpumask(affinity_bitmap));
	if (affinity && cpumask_subset(&speedui_cpus_requested, cpus_requested)) {
		*ret = set_cpus_allowed_ptr(p, &speedui_cpus_requested);
	}
	put_task_struct(p);
}

static void speedui_hook_exit(void *ignore, struct task_struct *p)
{
	if (p && p->android_oem_data1[Android_OEM_DATA_VALUE] && check_speedui_data(p)) {
		struct speedui_task_struct *speedui_task;
		get_task_struct(p);
		speedui_task = get_speedui_task_struct(p);
		speedui_node_free(speedui_task);
		put_task_struct(p);
	}
}

int __init speed_ui_init(void)
{
	pr_info("speed_ui:module init!");
	if (!init_speedui_cachep()) {
		pr_err("speed_ui: module init error!");
		return 0;
	}
	register_trace_android_rvh_sched_fork(speedui_hook_fork, NULL);
	register_trace_android_vh_free_task(speedui_hook_exit, NULL);
	register_trace_android_rvh_sched_setaffinity(android_rvh_sched_setaffinity, NULL);
	register_trace_android_rvh_update_cpus_allowed(android_rvh_update_cpus_allowed, NULL);
	return 0;
}

void __exit speed_ui_exit(void)
{
	pr_info("speed_ui:module exit!");
}

module_init(speed_ui_init);
module_exit(speed_ui_exit);
MODULE_LICENSE("GPL");
