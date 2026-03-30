// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_JEITA_PARA_KOTO_H__
#define __HQ_JEITA_PARA_KOTO_H__

#define JEITA_TEMP_RANGE_NUM (5)

static struct jeita_range jeita_temp_range[JEITA_TEMP_RANGE_NUM] = {
	[0] = {.low = 0, .high = 9},        /* T0 */
	[1] = {.low = 10, .high = 14},      /* T1 */
	[2] = {.low = 15, .high = 19},      /* T2 */
	[3] = {.low = 20, .high = 44},      /* T3 */
	[4] = {.low = 45, .high = 60}       /* T4 */
};

/* charge parameter for t0 index 0 */
static struct chg_parameter chg_para_t0_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 882},
	},
	.fv = 4456,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t1 index 0 */
static struct chg_parameter chg_para_t1_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 2646},
	},
	.fv = 4456,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t2 index 0 */
static struct chg_parameter chg_para_t2_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3600},
	},
	.fv = 4456,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t3 index 0 */
static struct chg_parameter chg_para_t3_ffc_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3600},
	},
	.fv = 4456,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t3 ffc index 1 */
static struct chg_parameter chg_para_t3_ffc_1 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3600},
	},
	.fv = 4448,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t3 ffc index 2 */
static struct chg_parameter chg_para_t3_ffc_2 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3600},
	},
	.fv = 4440,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t3 ffc index 3 */
static struct chg_parameter chg_para_t3_ffc_3 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3600},
	},
	.fv = 4424,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t3 ffc index 4 */
static struct chg_parameter chg_para_t3_ffc_4 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 2880},
	},
	.fv = 4424,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t3 normal index 0 */
static struct chg_parameter chg_para_t3_normal_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3600},
	},
	.fv = 4456,
	.iterm = {450, 450, -1, -1, -1},
};

/* charge parameter for t4 index 0 */
static struct chg_parameter chg_para_t4_0 = {
	.ichg_para_size = 1,
	.ichg_para = {
		[0] = {.vbat_range = {0, INT_MAX}, .ichg = 3600},
	},
	/* 
	 * NOTE: to fix plug in high soc machine will full immediately,
	 * set fv to max and stop charge protect will disable charge for safety
	 */
	.fv = 4456,
	.iterm = {800, 800, -1, -1, -1},
};

/* jeita parameter table of t0 [0 degree ~ 9 degree] */
static struct jeita_parameter jeita_para_t0[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t0_0,
	},
};

/* jeita parameter table of t1 [10 degree ~ 14 degree] */
static struct jeita_parameter jeita_para_t1[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t1_0,
	},
};

/* jeita parameter table of t2 [15 degree ~ 19 degree] */
static struct jeita_parameter jeita_para_t2[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t2_0,
	},
};

/* jeita parameter table of t3 [20 degree ~ 44 degree] */
static struct jeita_parameter jeita_para_t3[] = {
	[0] = {
		.cycle_range = {0, 100},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t3_ffc_0,
	},

	[1] = {
		.cycle_range = {101, 200},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t3_ffc_1,
	},

	[2] = {
		.cycle_range = {201, 300},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t3_ffc_2,
	},

	[3] = {
		.cycle_range = {301, 800},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t3_ffc_3,
	},

	[4] = {
		.cycle_range = {801, INT_MAX},
		.charge_mode = FFC_CHARGE_MODE,

		.chg_para = &chg_para_t3_ffc_4,
	},

	[5] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = NORMAL_CHARGE_MODE,

		.chg_para = &chg_para_t3_normal_0,
	},
};

/* jeita parameter table of t4 [45 degree ~ 60 degree] */
static struct jeita_parameter jeita_para_t4[] = {
	[0] = {
		.cycle_range = {0, INT_MAX},
		.charge_mode = ALL_CHARGE_MODE,

		.chg_para = &chg_para_t4_0,
	},
};

#endif /* __HQ_JEITA_PARA_KOTO_H__ */
