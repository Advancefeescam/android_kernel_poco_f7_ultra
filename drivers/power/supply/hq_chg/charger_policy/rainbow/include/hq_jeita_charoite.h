// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_JEITA_PARA_H__
#define __HQ_JEITA_PARA_H__

#define DEFAULT_FV (4100)
#define DEFAULT_ITERM (319)
#define DEFAULT_ICHG (637)

#define JEITA_TEMP_RANGE_NUM (10)

static struct jeita_range jeita_temp_range[JEITA_TEMP_RANGE_NUM] = {
	[0] = {.low = -100, .high = 0},
	[1] = {.low = 0, .high = 50},
	[2] = {.low = 50, .high = 100},
	[3] = {.low = 100, .high = 150},
	[4] = {.low = 150, .high = 200},
	[5] = {.low = 200, .high = 350},
	[6] = {.low = 350, .high = 400},
	[7] = {.low = 400, .high = 450},
	[8] = {.low = 450, .high = 600}
};

/* charge parameter for t0 index 0 */
static struct chg_parameter chg_para_t0_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 637},
	},
	.fv = 4530,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t1 index 0 */
static struct chg_parameter chg_para_t1_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4200}, .ichg = 1274},
		[1] = {.vbat_range = {4200, INT_MAX}, .ichg = 956},
	},
	.fv = 4530,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t2 index 0 */
static struct chg_parameter chg_para_t2_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4200}, .ichg = 1911},
		[1] = {.vbat_range = {4200, INT_MAX}, .ichg = 1274},
	},
	.fv = 4530,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t3 index 0 */
static struct chg_parameter chg_para_t3_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4200}, .ichg = 4459},
		[1] = {.vbat_range = {4200, INT_MAX}, .ichg = 3185},
	},
	.fv = 4530,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t4 index 0 */
static struct chg_parameter chg_para_t4_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 5096},
	},
	.fv = 4530,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t5 index 0 */
static struct chg_parameter chg_para_t5_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, INT_MAX}, .ichg = 5096},
	},
	.fv = 4490,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t5 index 1 */
static struct chg_parameter chg_para_t5_1 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, INT_MAX}, .ichg = 5096},
	},
	.fv = 4480,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t5 index 2 */
static struct chg_parameter chg_para_t5_2 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, INT_MAX}, .ichg = 5096},
	},
	.fv = 4470,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t5 index 3 */
static struct chg_parameter chg_para_t5_3 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 6400},
		[1] = {.vbat_range = {4000, INT_MAX}, .ichg = 4077},
	},
	.fv = 4450,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t5 index 4 */
static struct chg_parameter chg_para_t5_4 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, 4200}, .ichg = 6000},
		[2] = {.vbat_range = {4200, INT_MAX}, .ichg = 5096},
	},
	.fv = 4560,
	.iterm = {1210, 1210, 1210, 1147, 1210},
};

/* charge parameter for t5 index 5 */
static struct chg_parameter chg_para_t5_5 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, 4150}, .ichg = 6000},
		[2] = {.vbat_range = {4150, INT_MAX}, .ichg = 5096},
	},
	.fv = 4540,
	.iterm = {1210, 1210, 1210, 1147, 1210},
};

/* charge parameter for t5 index 6 */
static struct chg_parameter chg_para_t5_6 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, 4120}, .ichg = 6000},
		[2] = {.vbat_range = {4120, INT_MAX}, .ichg = 5096},
	},
	.fv = 4520,
	.iterm = {1210, 1210, 1210, 1147, 1210},
};

/* charge parameter for t5 index 7 */
static struct chg_parameter chg_para_t5_7 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 6400},
		[1] = {.vbat_range = {4000, 4100}, .ichg = 4800},
		[2] = {.vbat_range = {4100, INT_MAX}, .ichg = 4077},
	},
	.fv = 4510,
	.iterm = {1210, 1210, 1210, 1147, 1210},
};

/* charge parameter for t6 index 0 */
static struct chg_parameter chg_para_t6_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, INT_MAX}, .ichg = 5096},
	},
	.fv = 4490,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t6 index 1 */
static struct chg_parameter chg_para_t6_1 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, INT_MAX}, .ichg = 5096},
	},
	.fv = 4480,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t6 index 2 */
static struct chg_parameter chg_para_t6_2 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, INT_MAX}, .ichg = 5096},
	},
	.fv = 4470,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t6 index 3 */
static struct chg_parameter chg_para_t6_3 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 6400},
		[1] = {.vbat_range = {4000, INT_MAX}, .ichg = 4077},
	},
	.fv = 4450,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t6 index 4 */
static struct chg_parameter chg_para_t6_4 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, 4200}, .ichg = 6000},
		[2] = {.vbat_range = {4200, INT_MAX}, .ichg = 5096},
	},
	.fv = 4560,
	.iterm = {1911, 1784, 1911, 1847, 1911},
};

/* charge parameter for t6 index 5 */
static struct chg_parameter chg_para_t6_5 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, 4150}, .ichg = 6000},
		[2] = {.vbat_range = {4150, INT_MAX}, .ichg = 5096},
	},
	.fv = 4540,
	.iterm = {1911, 1784, 1911, 1847, 1911},
};

/* charge parameter for t6 index 6 */
static struct chg_parameter chg_para_t6_6 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, 4120}, .ichg = 6000},
		[2] = {.vbat_range = {4120, INT_MAX}, .ichg = 5096},
	},
	.fv = 4520,
	.iterm = {1911, 1784, 1911, 1847, 1911},
};

/* charge parameter for t6 index 7 */
static struct chg_parameter chg_para_t6_7 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 6400},
		[1] = {.vbat_range = {4000, 4100}, .ichg = 4800},
		[2] = {.vbat_range = {4100, INT_MAX}, .ichg = 4077},
	},
	.fv = 4510,
	.iterm = {1911, 1784, 1911, 1847, 1911},
};

/* charge parameter for t7 index 0 */
static struct chg_parameter chg_para_t7_0 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, INT_MAX}, .ichg = 5096},
	},
	.fv = 4490,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t7 index 1 */
static struct chg_parameter chg_para_t7_1 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, INT_MAX}, .ichg = 5096},
	},
	.fv = 4480,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t7 index 2 */
static struct chg_parameter chg_para_t7_2 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, INT_MAX}, .ichg = 5096},
	},
	.fv = 4470,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t7 index 3 */
static struct chg_parameter chg_para_t7_3 = {
	.ichg_para_size = 2,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 6400},
		[1] = {.vbat_range = {4000, INT_MAX}, .ichg = 4077},
	},
	.fv = 4450,
	.iterm = {446, 446, 446, 446, 446},
};

/* charge parameter for t7 index 4 */
static struct chg_parameter chg_para_t7_4 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4100}, .ichg = 8000},
		[1] = {.vbat_range = {4100, 4200}, .ichg = 6000},
		[2] = {.vbat_range = {4200, INT_MAX}, .ichg = 5096},
	},
	.fv = 4560,
	.iterm = {2675, 2421, 2675, 2548, 2675},
};

/* charge parameter for t7 index 5 */
static struct chg_parameter chg_para_t7_5 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4050}, .ichg = 8000},
		[1] = {.vbat_range = {4050, 4150}, .ichg = 6000},
		[2] = {.vbat_range = {4150, INT_MAX}, .ichg = 5096},
	},
	.fv = 4540,
	.iterm = {2675, 2421, 2675, 2548, 2675},
};

/* charge parameter for t7 index 6 */
static struct chg_parameter chg_para_t7_6 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4020}, .ichg = 8000},
		[1] = {.vbat_range = {4020, 4120}, .ichg = 6000},
		[2] = {.vbat_range = {4120, INT_MAX}, .ichg = 5096},
	},
	.fv = 4520,
	.iterm = {2675, 2421, 2675, 2548, 2675},
};

/* charge parameter for t7 index 7 */
static struct chg_parameter chg_para_t7_7 = {
	.ichg_para_size = 3,
	.ichg_para = {
		[0] = {.vbat_range = {0, 4000}, .ichg = 6400},
		[1] = {.vbat_range = {4000, 4100}, .ichg = 4800},
		[2] = {.vbat_range = {4100, INT_MAX}, .ichg = 4077},
	},
	.fv = 4510,
	.iterm = {2675, 2421, 2675, 2548, 2675},
};

/* charge parameter for t8 index 0 */
static struct chg_parameter chg_para_t8_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3185},
	},
	.fv = 4100,
	.iterm = {319, 319, 319, 319, 319},
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

/* jeita parameter table of t4 [15 degree ~ 20 degree) */
static struct jeita_parameter jeita_para_t4[] = {
	[0] = {
		.cycle_range = {0, 100},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t4_0,
	},
};

/* jeita parameter table of t5 [20 degree ~ 35 degree) */
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

/* jeita parameter table of t6 [35 degree ~ 40 degree) */
static struct jeita_parameter jeita_para_t6[] = {
	[0] = {
		.cycle_range = {0, 100},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t6_0,
	},

	[1] = {
		.cycle_range = {100, 300},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t6_1,
	},

	[2] = {
		.cycle_range = {300, 800},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t6_2,
	},

	[3] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t6_3,
	},

	[4] = {
		.cycle_range = {0, 100},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t6_4,
	},

	[5] = {
		.cycle_range = {100, 300},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t6_5,
	},

	[6] = {
		.cycle_range = {300, 800},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t6_6,
	},

	[7] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t6_7,
	},

};

/* jeita parameter table of t7 [40 degree ~ 45 degree) */
static struct jeita_parameter jeita_para_t7[] = {
	[0] = {
		.cycle_range = {0, 100},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t7_0,
	},

	[1] = {
		.cycle_range = {100, 300},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t7_1,
	},

	[2] = {
		.cycle_range = {300, 800},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t7_2,
	},

	[3] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t7_3,
	},

	[4] = {
		.cycle_range = {0, 100},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t7_4,
	},

	[5] = {
		.cycle_range = {100, 300},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t7_5,
	},

	[6] = {
		.cycle_range = {300, 800},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t7_6,
	},

	[7] = {
		.cycle_range = {800, INT_MAX},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t7_7,
	},

};


/* jeita parameter table of t8 [45 degree ~ 60 degree) */
static struct jeita_parameter jeita_para_t8[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t8_0,
	},

};
#endif /* __HQ_JEITA_PARA_H__ */
