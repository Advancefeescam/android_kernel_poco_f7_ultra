// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_JEITA_PARA_H__
#define __HQ_JEITA_PARA_H__

#define JEITA_TEMP_RANGE_NUM (7)

static struct jeita_range jeita_temp_range[JEITA_TEMP_RANGE_NUM] = {
	[0] = {.low = -10, .high = 0},
	[1] = {.low = 0, .high = 5},
	[2] = {.low = 5, .high = 10},
	[3] = {.low = 10, .high = 15},
	[4] = {.low = 15, .high = 35},
	[5] = {.low = 35, .high = 45},
	[6] = {.low = 45, .high = 58}
};

/* charge parameter for t0 index 0 */
static struct chg_parameter chg_para_t0_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 538},
	},
	.fv = 4500,
	.iterm = {350, 400, -1, -1, -1},
};

/* charge parameter for t0 index 0 */
static struct chg_parameter chg_para_t1_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 1074},
	},
	.fv = 4500,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t2 index 0 */
static struct chg_parameter chg_para_t2_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 2686},
	},
	.fv = 4500,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t3 index 0 */
static struct chg_parameter chg_para_t3_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 4296},
	},
	.fv = 4500,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t4 index 0 */
static struct chg_parameter chg_para_t4_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, 4480}, .ichg = 5400},
	},
	.fv = 4480,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t4 index 1 */
static struct chg_parameter chg_para_t4_1 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4070}, .ichg = 8000},
		[1] = {.vbat_range = {4070, 4470}, .ichg = 5400},
	},
	.fv = 4470,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t4 index 2 */
static struct chg_parameter chg_para_t4_2 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4070}, .ichg = 8000},
		[1] = {.vbat_range = {4070, 4460}, .ichg = 5400},
	},
	.fv = 4460,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t4 index 3 */
static struct chg_parameter chg_para_t4_3 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4070}, .ichg = 5400},
		[1] = {.vbat_range = {4070, 4440}, .ichg = 4320},
	},
	.fv = 4440,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t4 index 4 */
static struct chg_parameter chg_para_t4_4 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, 4300}, .ichg = 6000},
		[2] = {.vbat_range = {4300, 4500}, .ichg = 5400},
		[3] = {.vbat_range = {4500, 4530}, .ichg = 4296},
	},
	.fv = 4530,
	.iterm = {914, 1074, -1, -1, -1},
};

/* charge parameter for t4 index 5 */
static struct chg_parameter chg_para_t4_5 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, 4250}, .ichg = 6000},
		[2] = {.vbat_range = {4250, 4450}, .ichg = 5400},
		[3] = {.vbat_range = {4450, 4510}, .ichg = 4296},
	},
	.fv = 4510,
	.iterm = {914, 1074, -1, -1, -1},
};

/* charge parameter for t4 index 6 */
static struct chg_parameter chg_para_t4_6 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, 4220}, .ichg = 6000},
		[2] = {.vbat_range = {4220, 4420}, .ichg = 5400},
		[3] = {.vbat_range = {4420, 4490}, .ichg = 4296},
	},
	.fv = 4490,
	.iterm = {914, 1074, -1, -1, -1},
};

/* charge parameter for t4 index 7 */
static struct chg_parameter chg_para_t4_7 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 5400},
		[1] = {.vbat_range = {4000, 4200}, .ichg = 4800},
		[2] = {.vbat_range = {4200, 4400}, .ichg = 4320},
		[3] = {.vbat_range = {4400, 4480}, .ichg = 1721},
	},
	.fv = 4480,
	.iterm = {914, 1074, -1, -1, -1},
};

/* charge parameter for t5 index 0 */
static struct chg_parameter chg_para_t5_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, 4480}, .ichg = 5400},
	},
	.fv = 4480,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t5 index 1 */
static struct chg_parameter chg_para_t5_1 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4070}, .ichg = 8000},
		[1] = {.vbat_range = {4070, 4470}, .ichg = 5400},
	},
	.fv = 4470,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t5 index 2 */
static struct chg_parameter chg_para_t5_2 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4070}, .ichg = 8000},
		[1] = {.vbat_range = {4070, 4460}, .ichg = 5400},
	},
	.fv = 4460,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t5 index 3 */
static struct chg_parameter chg_para_t5_3 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4070}, .ichg = 5400},
		[1] = {.vbat_range = {4070, 4440}, .ichg = 4320},
	},
	.fv = 4440,
	.iterm = {376, 430, -1, -1, -1},
};

/* charge parameter for t5 index 4 */
static struct chg_parameter chg_para_t5_4 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, 4300}, .ichg = 6000},
		[2] = {.vbat_range = {4300, 4500}, .ichg = 5400},
		[3] = {.vbat_range = {4500, 4530}, .ichg = 4296},
	},
	.fv = 4530,
	.iterm = {1182, 1396, -1, -1, -1},
};

/* charge parameter for t5 index 5 */
static struct chg_parameter chg_para_t5_5 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, 4250}, .ichg = 6000},
		[2] = {.vbat_range = {4250, 4450}, .ichg = 5400},
		[3] = {.vbat_range = {4450, 4510}, .ichg = 4296},
	},
	.fv = 4510,
	.iterm = {1182, 1396, -1, -1, -1},
};

/* charge parameter for t5 index 6 */
static struct chg_parameter chg_para_t5_6 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, 4220}, .ichg = 6000},
		[2] = {.vbat_range = {4220, 4420}, .ichg = 5400},
		[3] = {.vbat_range = {4420, 4490}, .ichg = 4296},
	},
	.fv = 4490,
	.iterm = {1182, 1396, -1, -1, -1},
};

/* charge parameter for t5 index 7 */
static struct chg_parameter chg_para_t5_7 = {
	.ichg_para_size = 4,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 5400},
		[1] = {.vbat_range = {4000, 4200}, .ichg = 4800},
		[2] = {.vbat_range = {4200, 4400}, .ichg = 4320},
		[3] = {.vbat_range = {4400, 4480}, .ichg = 1721},
	},
	.fv = 4480,
	.iterm = {1182, 1396, -1, -1, -1},
};

/* charge parameter for t6 index 0 */
static struct chg_parameter chg_para_t6_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 2686},
	},
	.fv = 4100,
	.iterm = {376, 430, -1, -1, -1},
};


/* jeita parameter table of t0 [-10 degree ~ 0 degree) */
static struct jeita_parameter jeita_para_t0[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t0_0,
	},
};

/* jeita parameter table of t1 [0 degree ~ 5 degree) */
static struct jeita_parameter jeita_para_t1[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t1_0,
	},
};

/* jeita parameter table of t2 [5 degree ~ 10 degree) */
static struct jeita_parameter jeita_para_t2[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t2_0,
	},
};

/* jeita parameter table of t3 [10 degree ~ 15 degree) */
static struct jeita_parameter jeita_para_t3[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t3_0,
	},
};

/* jeita parameter table of t4 [15 degree ~ 35 degree) */
static struct jeita_parameter jeita_para_t4[] = {
	[0] = {
		.cycle_range = {0, 100},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t4_0,
	},

	[1] = {
		.cycle_range = {100, 300},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t4_1,
	},

	[2] = {
		.cycle_range = {300, 800},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t4_2,
	},

	[3] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t4_3,
	},

	[4] = {
		.cycle_range = {0, 100},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t4_4,
	},

	[5] = {
		.cycle_range = {100, 300},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t4_5,
	},

	[6] = {
		.cycle_range = {300, 800},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t4_6,
	},

	[7] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t4_7,
	},
};

/* jeita parameter table of t5 [35 degree ~ 45 degree) */
static struct jeita_parameter jeita_para_t5[] = {
	[0] = {
		.cycle_range = {0, 100},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t5_0,
	},

	[1] = {
		.cycle_range = {100, 300},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t5_1,
	},

	[2] = {
		.cycle_range = {300, 800},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t5_2,
	},

	[3] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t5_3,
	},

	[4] = {
		.cycle_range = {0, 100},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t5_4,
	},

	[5] = {
		.cycle_range = {100, 300},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t5_5,
	},

	[6] = {
		.cycle_range = {300, 800},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t5_6,
	},

	[7] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t5_7,
	},
};

/* jeita parameter table of t6 [45 degree ~ 58 degree) */
static struct jeita_parameter jeita_para_t6[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t6_0,
	},
};

#endif /* __HQ_JEITA_PARA_H__ */
