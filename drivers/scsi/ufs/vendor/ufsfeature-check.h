#ifndef _UFS_WB_CHECK_
#define _UFS_WB_CHECK_
#include <linux/types.h>
struct check_wb_t {
	int total_gb;
	int wb_gb;
};
struct ufstw_check_info {
	u32 seg_size;
	u8 unit_size;
};
#endif
