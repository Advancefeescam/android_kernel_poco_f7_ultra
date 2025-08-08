//MIUI ADD: Performance_BoostFramework
#include "perfhelper.h"

#define MAX_EXCEPTION_NUM 128
#define MAX_ONE_EXCEPTION_SIZE 1024

static DEFINE_SPINLOCK(plr_lock);
static DEFINE_SPINLOCK(ple_lock);

struct perflock_records_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec64 key_time;
};

struct perflock_exception_buff {
	char msg[MAX_ONE_EXCEPTION_SIZE];
	struct timespec64 key_time;
	int msg_count;
};

struct perflock_records_buff plr_buff[MAX_RECORD_NUM];
static u32 plr_num;
static u32 index_head;
static u32 index_tail;
static int kdamond_pid = -1;

struct perflock_exception_buff ple_buff[MAX_EXCEPTION_NUM];
static u32 ple_num;
static u32 ple_write_index;

static char cpu_set[BUFF_SIZE];

static void perflock_record(const char *perflock_msg)
{
	static int m;

	if (!perflock_msg)
		return;

	if (!spin_trylock(&plr_lock))
		return;

	index_tail = m;
	ktime_get_real_ts64(&plr_buff[m].key_time);
	snprintf(plr_buff[m++].msg, MAX_ONE_RECORD_SIZE, "%s", perflock_msg);

	if (m >= MAX_RECORD_NUM)
		m = 0;

	plr_num++;
	if (plr_num >= MAX_RECORD_NUM) {
		plr_num = MAX_RECORD_NUM;
		index_head = index_tail + 1;
		if (index_head >= MAX_RECORD_NUM)
			index_head = 0;
	}

	spin_unlock(&plr_lock);
}

static int perflock_records_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&plr_lock);
	if (plr_num < MAX_RECORD_NUM) {
		for (i = 0; i < plr_num; i++) {
			rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_RECORD_NUM; i++) {
			rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						plr_buff[i].msg);
			}
		}
	}
	spin_unlock(&plr_lock);

	return 0;
}

static int perflock_records_open(struct inode *inode, struct file *file)
{
	return single_open(file, perflock_records_show, NULL);
}

static ssize_t perflock_records_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[MAX_ONE_RECORD_SIZE] = {0};

	if (count > MAX_ONE_RECORD_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	perflock_record(buf);

	return count;
}

static const struct proc_ops perflock_records_ops = {
	.proc_open           = perflock_records_open,
	.proc_read           = seq_read,
	.proc_write		= perflock_records_write,
	.proc_lseek         = seq_lseek,
	.proc_release        = single_release,
};

static int kswapd_pid_show(struct seq_file *seq, void *v)
{
	struct task_struct *p = NULL;
	pid_t pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (!p->mm && !strncmp(p->comm, "kswapd0", strlen("kswapd0"))) {
			pid = p->pid;
			break;
		}
	}
	read_unlock(&tasklist_lock);

	seq_printf(seq, "%d", pid);

	return 0;
}

static int kswapd_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, kswapd_pid_show, NULL);
}

static const struct proc_ops kswapd_pid_ops = {
	.proc_open           = kswapd_pid_open,
	.proc_read           = seq_read,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int kcompactd_pid_show(struct seq_file *seq, void *v)
{
	struct task_struct *p = NULL;
	pid_t pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (!p->mm && !strncmp(p->comm, "kcompactd0", strlen("kcompactd0"))) {
			pid = p->pid;
			break;
		}
	}
	read_unlock(&tasklist_lock);

	seq_printf(seq, "%d", pid);

	return 0;
}

static int kcompactd_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, kcompactd_pid_show, NULL);
}

static const struct proc_ops kcompactd_pid_ops = {
	.proc_open	= kcompactd_pid_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static void perflock_exception(const char *perflock_msg)
{
	if (!perflock_msg)
		return;
	if (!spin_trylock(&ple_lock))
		return;
	// find same request change msgcount
	for (int i = 0; i < ple_num; i++) {
		if (strcmp(perflock_msg, ple_buff[i].msg) == 0 && ple_buff[i].msg_count < INT_MAX) {
			ple_buff[i].msg_count++;
			spin_unlock(&ple_lock);
			return;
		}
	}
	// cant find same request
	ktime_get_real_ts64(&ple_buff[ple_write_index].key_time);
	snprintf(ple_buff[ple_write_index].msg, MAX_ONE_EXCEPTION_SIZE, "%s", perflock_msg);
	// renew write index
	ple_write_index = (ple_write_index + 1) % MAX_EXCEPTION_NUM;
	ple_num++;

	if (ple_num >= MAX_EXCEPTION_NUM) {
		ple_num = MAX_EXCEPTION_NUM;
	}

	spin_unlock(&ple_lock);
}

static int perflock_exception_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&ple_lock);

	if (ple_num < MAX_EXCEPTION_NUM) {
		for (i = 0; i < ple_num; i++) {
			rtc_time64_to_tm(ple_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s count = %d }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ple_buff[i].msg, ple_buff[i].msg_count + 1);
		}
	} else {
		for (i = ple_write_index; i < MAX_EXCEPTION_NUM; i++) {
			rtc_time64_to_tm(ple_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s count = %d }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ple_buff[i].msg, ple_buff[i].msg_count + 1);
		}

		for (i = 0; i < ple_write_index; i++) {
			rtc_time64_to_tm(ple_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s count = %d }\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
				ple_buff[i].msg, ple_buff[i].msg_count + 1);
		}
	}
	spin_unlock(&ple_lock);

	return 0;
}
static int perflock_exception_open(struct inode *inode, struct file *file)
{
	return single_open(file, perflock_exception_show, NULL);
}

static ssize_t perflock_exception_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[MAX_ONE_EXCEPTION_SIZE] = {0};

	// printk(KERN_ERR "userbuf size is %d",count);
	if (count > MAX_ONE_EXCEPTION_SIZE) {
		printk(KERN_ERR "perflock_exception userbuf size longer than 1024");
		return -EINVAL;
	}

	if (copy_from_user(buf, userbuf, count)) {
		printk(KERN_ERR "perflock_exception cant copy_from_userbuf");
		return -EFAULT;
	}

	perflock_exception(buf);

	return count;
}

static const struct proc_ops perflock_exception_ops = {
	.proc_open           = perflock_exception_open,
	.proc_read           = seq_read,
	.proc_write		= perflock_exception_write,
	.proc_lseek         = seq_lseek,
	.proc_release        = single_release,
};

static bool kdamond_input_parse(const char *buf, int *cpu_str, int *kdamond_pid)
{
	char  *args, *arg;
	const char *arg_cpu;
	int count = 0, value = 0;

	args = kstrndup(buf, 32, GFP_KERNEL);
	if (!args)
		return false;

	arg = strsep(&args, ";");
	if (!arg) {
		goto err;
	}
	arg_cpu = strsep(&arg, ",");
	while (arg_cpu != NULL) {
		if (kstrtoint(arg_cpu, 10, &value))
			goto err;

		if (value < 0) {
			printk("Failed to convert string to integer\n");
			goto err;
		}

		cpu_str[count] = value;
		count++;
		arg_cpu = strsep(&arg, ",");
	}

	arg = strsep(&args, ";");
	if (!arg)
		goto err;
	if (kstrtoint(arg, 10, kdamond_pid))
		goto err;
	kfree(args);
	return true;

err:
	kfree(args);
	return false;

}

static int kdamond_cpuset_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "kdamond pid:%d cpu_set:%s\n", kdamond_pid, cpu_set);
	return 0;
}

static int kdamond_cpuset_open(struct inode *inode, struct file *file)
{
	return single_open(file, kdamond_cpuset_show, NULL);
}

static ssize_t kdamond_cpuset_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	cpumask_t kdamond_cpumask;
	struct task_struct *kdamond_task = NULL;
	char buf[BUFF_SIZE];
	int  cpumask_str[BUFF_SIZE];

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	strncpy(cpu_set, buf, BUFF_SIZE-1);
	for (int i = 0; i < BUFF_SIZE; i++)
		cpumask_str[i] = -1;

	if (!kdamond_input_parse(buf,cpumask_str, &kdamond_pid)) {
		pr_err("kdamond_cpuset_input_parse: param err(%s)\n", buf);
		return -EINVAL;
	}

	if (kdamond_pid <= 0)
		return -EINVAL;
	cpumask_clear(&kdamond_cpumask);

	for (int i = 0; i < BUFF_SIZE; i++) {
		if (cpumask_str[i] >= 0) {
			cpumask_set_cpu(cpumask_str[i], &kdamond_cpumask);
		} else {
			break;
		}
	}

	rcu_read_lock();
	kdamond_task = find_task_by_vpid(kdamond_pid);
	if (kdamond_task == NULL) {
		rcu_read_unlock();
		return -EINVAL;
	}
	rcu_read_unlock();
	set_cpus_allowed_ptr(kdamond_task, &kdamond_cpumask);
	return count;
}

static const struct proc_ops kdamond_cpuset_ops = {
	.proc_open	= kdamond_cpuset_open,
	.proc_read	= seq_read,
	.proc_write	= kdamond_cpuset_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

void perflock_init(void)
{
	struct proc_dir_entry *perflock_record_entry;
	struct proc_dir_entry *kswapd_pid_entry;
	struct proc_dir_entry *kcompactd_pid;
	struct proc_dir_entry *perflock_exception_entry;
	struct proc_dir_entry *kdamond_entry;

	perflock_record_entry = proc_create("perflock_records", 0664, NULL, &perflock_records_ops);
	if (!perflock_record_entry)
		printk(KERN_ERR "%s: create perflock_records node failed\n", __func__);

	kswapd_pid_entry = proc_create("kswapd_pid", 0440, NULL, &kswapd_pid_ops);
	if (!kswapd_pid_entry)
		printk(KERN_ERR "%s: create kswapd_pid node failed\n", __func__);

	kcompactd_pid = proc_create("kcompactd_pid", 0664, NULL, &kcompactd_pid_ops);
	if(!kcompactd_pid)
		printk(KERN_ERR "%s: create kcompactd_pid node failed\n", __func__);

	perflock_exception_entry = proc_create("perflock_exception", 0664, NULL, &perflock_exception_ops);
	if (!perflock_exception_entry)
		printk(KERN_ERR "%s: create perflock_exception node failed\n", __func__);

	kdamond_entry = proc_create("kdamond_cpuset", 0664, NULL, &kdamond_cpuset_ops);
	if (!kdamond_entry)
		printk(KERN_ERR "%s: create kdamond_cpuset node failed\n", __func__);
}

void perflock_exit(void)
{
	remove_proc_entry("perflock_records", NULL);
	remove_proc_entry("kswapd_pid", NULL);
	remove_proc_entry("kcompactd_pid", NULL);
	remove_proc_entry("perflock_exception", NULL);
	remove_proc_entry("kdamond_cpuset", NULL);
}
//END Performance_BoostFramework
