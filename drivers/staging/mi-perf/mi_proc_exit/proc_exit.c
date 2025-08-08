#include <linux/module.h>
#include <trace/hooks/sched.h>
#include <linux/moduleparam.h>

int debug = 0;
module_param(debug, uint, 0644);

static const char *camera_name = ".android.camera";

static void extend_mmput_handler(void *data1, void *data2)
{
    int prev_nice = task_nice(current);

    if ((current->group_leader->flags & PF_EXITING) && prev_nice > 0) {
        if (strncmp(current->group_leader->comm, camera_name, strlen(camera_name)) == 0) {
            set_user_nice(current, -20);
        } else {
            set_user_nice(current, 0);
        }

        if (unlikely(debug)) {
            pr_info("proc_exit: set %d's from %d to %d", current->pid,
                    prev_nice, task_nice(current));
        }
    }
}

int __init proc_exit_init(void)
{
    register_trace_android_vh_mmput(extend_mmput_handler, NULL);
    pr_info("proc_exit: module init!");
    return 0;
}

void __exit proc_exit_exit(void)
{
    unregister_trace_android_vh_mmput(extend_mmput_handler, NULL);
    pr_info("proc_exit: module exit!");
}

module_init(proc_exit_init);
module_exit(proc_exit_exit);
MODULE_LICENSE("GPL");
