//MIUI ADD: Performance_BoostFramework
#include "perfhelper.h"

static int __init perf_helper_init(void)
{
	perflock_init();
	mimd_init();
	sched_assi_init();

	return 0;
}

static void __exit perf_helper_exit(void)
{
	perflock_exit();
	mimd_exit();
	sched_assi_exit();
}

module_init(perf_helper_init);
module_exit(perf_helper_exit);

MODULE_LICENSE("GPL");
//END Performance_BoostFramework
