// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_dsi_panel.h"
#ifdef CONFIG_MI_DISP_DFS_EVENT
#include "../mediatek/mediatek_v2/mi_disp/mi_disp_event.h"
#endif
#include "include/panel_alpha_data.h"
#include "linux/hardware_info.h"

#define REGFLAG_CMD             0xFFFA
#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

#define DATA_RATE                   1100

#define FRAME_WIDTH                 (1080)
#define FRAME_HEIGHT                (2392)

#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 40
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10
#define DSC_DSC_LINE_BUF_DEPTH      11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
#define DSC_SLICE_HEIGHT            8
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               279
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      357
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         1
#define DSC_NFL_BPG_OFFSET          293
#define DSC_SLICE_BPG_OFFSET        3255
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          7
#define DSC_FLATNESS_MAXQP          16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15
#define DSC_RC_QUANT_INCR_LIMIT1    15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3
#define PHYSICAL_WIDTH              70805
#define PHYSICAL_HEIGHT             156819
#define CMD_NUM_MAX                 20
#define REFRESH_AOD 30
#define FACTORY_MAX_BRIGHTNESS      2047

static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16};
static unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};

extern int get_panel_dead_flag(void);
extern bool long_press_pwrkey;
static struct drm_panel *this_panel;
static last_bl_level;
static last_non_zero_bl_level = 511;

static int doze_lbm_bl = 29;
static int doze_hbm_bl = 248;

static bool lhbm_flag = false;
static bool fod_in_calibration = false;
static int bl_level_for_fod = 0;
//static int hbm_level =2047;
static int bl_dc_pwm_threshold =277;
static bool aod_backlight_flag = false;

enum lhbm_cmd_type {
	TYPE_WHITE_1200 = 0,
	TYPE_WHITE_750,
	TYPE_WHITE_500,
	TYPE_WHITE_200,
	TYPE_GREEN_500,
	TYPE_LHBM_OFF
};

static const char *panel_name = "panel_name=dsi_p7_36_0d_0a_dsc_vdo";

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static struct LCM_setting_table local_hbm_normal_white[] = {
	{0xf0, 02, {0xaa, 0x1c}},
	{0xc0, 12, {0x02, 0x54, 0x02, 0x21, 0x02, 0xA1, 0x01, 0xCD, 0x01, 0xAC, 0x02, 0x05}},
	{0x63, 05, {0x10, 0x00, 0x00, 0x00, 0x00}},/*DBV>>8, DBV&0XFF*/
	{0x62, 01, {0x03}},
	{0xf0, 02, {0xaa, 0x00}},
};

static struct LCM_setting_table local_hbm_normal_off[] = {
	{0x62, 01, {0x00}},
	{0x51, 02, {0x00, 0x00}},
};

static struct LCM_setting_table local_hbm_gir_on[] = {
	{0xf0, 02, {0xaa, 0x11}},
	{0xd5, 01, {0x12}},
	{0xf0, 02, {0xaa, 0x00}},
};

static struct LCM_setting_table local_hbm_gir_off[] = {
	{0xf0, 02, {0xaa, 0x11}},
	{0xd5, 01, {0x13}},
	{0xf0, 02, {0xaa, 0x00}},
};

static struct LCM_setting_table lcm_round_on[] = {
	{0xF0, 02, {0xAA, 0x18}},
	{0xB0, 01, {0x13}},
	{0xF0, 02, {0xAA, 0x00}},
};
static struct LCM_setting_table lcm_round_off[] = {
	{0xF0, 02, {0xAA, 0x18}},
	{0xB0, 01, {0x10}},
	{0xF0, 02, {0xAA, 0x00}},
};

static struct LCM_setting_table lcm_aod_hbm[] = {
	{0x6d, 01, {0x01}},
};
static struct LCM_setting_table lcm_aod_lbm[] = {
	{0x6d, 01, {0x00}},
};
static struct LCM_setting_table lcm_fod_aod_dbv[] = {
	{0x51, 02, {0x00, 0x00}},
};

static struct LCM_setting_table lcm_backlight_off[] = {
	{0x51, 02, {0x00, 0x00}},
};
//static struct LCM_setting_table lcm_aod_off[] = {
//	{0x38, 01, {0x00}},
//	{0x51, 02, {0x07, 0xff}},
//};
#if 0
static struct LCM_setting_table exit_hbm_to_normal[] = {
	{0x51, 02, {0x07,0xff}},
};
#endif
static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;
	if (ctx->error < 0)
		return;
	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void panel_get_lhbm_info(void)
{
	struct device_node *chosen;
	char *tmp_buf = NULL;
	unsigned long tmp_size = 0;
	char lhbm_info[12] = {0};
	int byte_count = 0;
	int i = 0;
	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tmp_buf = (char *)of_get_property(chosen, "lcm_lhbm_gamma", (int *)&tmp_size);
		if (tmp_size > 0) {
			pr_info("[%s]: lcm_lhbm_gamma = %s, size = %d\n", __func__, tmp_buf, tmp_size);
			if (((tmp_size-1) % 2) != 0) {
				pr_info("lcm_lhbm_gamma format error: %d\n", tmp_size);
			} else {
				byte_count = ((tmp_size -1) / 2);
				for (i = 0; i < byte_count; i++) {
					sscanf(tmp_buf + 2 * i, "%02hhx", &lhbm_info[i]);
				}
				for (i = 0; i < byte_count; i++) {
					local_hbm_normal_white[1].para_list[i] = lhbm_info[i];
					pr_info("lcm_lhbm_gamma para_list[%d]: 0x%02x\n", i, local_hbm_normal_white[1].para_list[i]);
				}
			}
		}
	} else {
		pr_info("[%s]:find chosen failed\n", __func__);
	}
}

static struct regulator *lcm_dvdd;
static int regulator_inited;
static int lcm_panel_dvdd_regulator_init(struct device *dev)
{
	int ret = 0;

	if (regulator_inited)
		return 0;

	/* please only get regulator once in a driver */
	lcm_dvdd = devm_regulator_get(dev, "dvdd");
	if (IS_ERR(lcm_dvdd)) {
		ret = PTR_ERR(lcm_dvdd);
		pr_info("get disp dvdd fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_dvdd_regulator_config(struct device *dev, bool enable)
{
	int ret = 0;
	ret = lcm_panel_dvdd_regulator_init(dev);
	pr_info("%s: enable = %d, lcm_dvdd = %d\n", __func__, enable, IS_ERR(lcm_dvdd));
	if (ret == 0) {
		if (IS_ERR_OR_NULL(lcm_dvdd)) {
			pr_err("Invalid regulator pointer: %ld\n", PTR_ERR(lcm_dvdd));
			return PTR_ERR(lcm_dvdd);
		}
		if (enable) {
			/* set voltage with min & max*/
			ret = regulator_set_voltage(lcm_dvdd, 1200000, 1200000);
			if (ret < 0)
				pr_info("set voltage disp dvdd fail, ret = %d\n", ret);
			/* enable regulator */
			ret = regulator_enable(lcm_dvdd);
			if (ret < 0)
				pr_info("enable regulator disp dvdd fail, ret = %d\n", ret);
		} else {
			ret = regulator_disable(lcm_dvdd);
			if (ret < 0)
				pr_info("disable regulator disp dvdd fail, ret = %d\n", ret);
		}
	}
	return ret;	
}

int panel_ddic_send_cmd(struct LCM_setting_table *table,
	unsigned int count, bool block)
{
	int i = 0, j = 0, k = 0;
	int ret = 0;
	unsigned char temp[25][255] = {0};
	unsigned char cmd = {0};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 0,
		//.flags = MIPI_DSI_MSG_USE_LPM,
		.tx_cmd_num = count,
	};

	if (table == NULL) {
		pr_err("invalid ddic cmd \n");
		return ret;
	}
	if (count == 0 || count > CMD_NUM_MAX) {
		pr_err("cmd count invalid, value:%d \n", count);
		return ret;
	}

	for (i = 0;i < count; i++) {
		memset(temp[i], 0, sizeof(temp[i]));
		/* LCM_setting_table format: {cmd, count, {para_list[]}} */
		cmd = (u8)table[i].cmd;
		temp[i][0] = cmd;
		for (j = 0; j < table[i].count; j++) {
			temp[i][j+1] = table[i].para_list[j];
		}

		cmd_msg.type[i] = table[i].count > 1 ? 0x39 : 0x15;
		cmd_msg.tx_buf[i] = temp[i];
		cmd_msg.tx_len[i] = table[i].count + 1;

		for (k = 0; k < cmd_msg.tx_len[i]; k++) {
			pr_info("%s cmd_msg.tx_buf:0x%02x\n", __func__, temp[i][k]);
		}

		pr_info("%s cmd_msg.tx_len:%d\n", __func__, cmd_msg.tx_len[i]);
	}
	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, block, false);
	if (ret != 0) {
		pr_err("%s: failed to send ddic cmd\n", __func__);
	}
	return ret;
}
static void lcm_panel_init(struct lcm *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	mutex_lock(&ctx->panel_lock);
	pr_info(" %s start \n", __func__);
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(30);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0x03, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x09);
	if (ctx->dynamic_fps == 120) {
		lcm_dcs_write_seq_static(ctx, 0x6c, 0x00);
	} else if (ctx->dynamic_fps == 90) {
		lcm_dcs_write_seq_static(ctx, 0x6c, 0x01);
	} else {
		lcm_dcs_write_seq_static(ctx, 0x6c, 0x02);
	}
	lcm_dcs_write_seq_static(ctx, 0x6d, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x70, 0x11, 0x00, 0x00, 0xab, 0x30, 0x80, 0x09, 0x58, 0x04,
			0x38, 0x00, 0x08, 0x02, 0x1c, 0x02, 0x1c, 0x02, 0x00, 0x01,
			0x17, 0x00, 0x20, 0x01, 0x65, 0x00, 0x07, 0x00, 0x01, 0x01,
			0x25, 0x0c, 0xb7, 0x18, 0x00, 0x10, 0xf0, 0x07, 0x10, 0x20,
			0x00, 0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46,
			0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x02,
			0x02, 0x22, 0x00, 0x2a, 0x40, 0x2A, 0xBE, 0x3A, 0xFC, 0x3A,
			0xFA, 0x3A, 0xF8, 0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B,
			0xB6, 0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xf0, 0xaa, 0x14);
	lcm_dcs_write_seq_static(ctx, 0xc1, 0x0b);
	lcm_dcs_write_seq_static(ctx, 0xc3, 0x01,0x21,0x20,0x04,0x86,0x04,0x86,0x04,0x86,0x04,0x86,
		0x03,0xEF,0x03,0xEF,0x03,0xEF,0x00,0xE5,0x00,0xE5,0x00,0xE5,
		0x00,0xE5,0x00,0x31,0x00,0x31,0x00,0x31);
	lcm_dcs_write_seq_static(ctx, 0xf0, 0xaa, 0x16);
	lcm_dcs_write_seq_static(ctx, 0xd1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xf0, 0xaa, 0x17);
	lcm_dcs_write_seq_static(ctx, 0xb2, 0x44, 0x10, 0x28, 0x10, 0xd0, 0xf8, 0xd0);
	lcm_dcs_write_seq_static(ctx, 0xff, 0x5a, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xfd, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x0a);
	lcm_dcs_write_seq_static(ctx, 0xf9, 0xa8);
	lcm_dcs_write_seq_static(ctx, 0xff, 0x5a, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xf3, 0x61);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x0b);
	lcm_dcs_write_seq_static(ctx, 0xf3, 0x78);
	lcm_dcs_write_seq_static(ctx, 0xff, 0x5a, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xfb, 0x73, 0x73);
	lcm_dcs_write_seq_static(ctx, 0xff, 0x5a, 0x83);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x0b);
	lcm_dcs_write_seq_static(ctx, 0xf7, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x09);
	lcm_dcs_write_seq_static(ctx, 0xf7, 0x10, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xf0, 0xaa, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xcf, 0x87);
	//lcm_dcs_write_seq_static(ctx, 0x65, 0x08);
	//lcm_dcs_write_seq_static(ctx, 0xcf, 0x0f);
	lcm_dcs_write_seq_static(ctx, 0xf0, 0xaa, 0x1c);
	lcm_dcs_write_seq_static(ctx, 0xb0, 0x03, 0x00, 0x0B, 0x1A, 0xFF, 0x54, 0x76, 0x24, 0x25, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xb1, 0x04, 0x40, 0x04, 0x40, 0x04, 0x20, 0x04, 0x60, 0x04, 0x6A, 0x04, 0x40, 0x04, 0xA0, 0x04, 0xB0, 0x04, 0x80, 0x02, 0x50, 0x02, 0x6A, 0x02, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xb2, 0x04, 0x40, 0x04, 0x40, 0x04, 0x20, 0x04, 0x60, 0x04, 0x6A, 0x04, 0x40, 0x04, 0xA0, 0x04, 0xB0, 0x04, 0x80, 0x02, 0x50, 0x02, 0x6A, 0x02, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xb3, 0x04, 0x40, 0x04, 0x40, 0x04, 0x20, 0x04, 0x60, 0x04, 0x6A, 0x04, 0x40, 0x04, 0xA0, 0x04, 0xB0, 0x04, 0x80, 0x02, 0x50, 0x02, 0x6A, 0x02, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xf0, 0xaa, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xff, 0x5a, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xf9, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x0a);
	lcm_dcs_write_seq_static(ctx, 0xf9, 0x1e);
	lcm_dcs_write_seq_static(ctx, 0xff, 0x5a, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	mdelay(10);
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	mutex_unlock(&ctx->panel_lock);
	pr_info(" %s end !\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	if (!ctx->prepared)
		return 0;

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);
	mutex_unlock(&ctx->panel_lock);
	udelay(2000);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	//devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	//udelay(1000);
	//pr_err(" lcm_prepare 3 %s\n", __func__);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

	lhbm_flag = false;
	fod_in_calibration = false;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HSA (14)
#define HBP (75)

#define VSA (2)
#define VBP (18)

#define VAC (2392)
#define HAC (1080)

#define MODE_0_FPS 60
#define MODE_0_VFP 2580
#define MODE_0_HFP 78
#define MODE_0_DATA_RATE DATA_RATE

#define MODE_1_FPS 90
#define MODE_1_VFP 916
#define MODE_1_HFP 78
#define MODE_1_DATA_RATE DATA_RATE

#define MODE_2_FPS 120
#define MODE_2_VFP 84
#define MODE_2_HFP 78
#define MODE_2_DATA_RATE DATA_RATE

#define MODE_3_FPS 30
#define MODE_3_VFP 84
#define MODE_3_HFP 1900
#define MODE_3_DATA_RATE DATA_RATE

static struct drm_display_mode default_mode = {/* 60hz */
	.clock = (FRAME_WIDTH + MODE_0_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};

static struct drm_display_mode performance_mode_1 = { /* 90hz */
	.clock = (FRAME_WIDTH + MODE_1_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};

static struct drm_display_mode performance_mode_2 = { /* 120HZ */
	.clock = (FRAME_WIDTH + MODE_2_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_2_VFP + VSA + VBP) * MODE_2_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};

static struct drm_display_mode performance_mode_3 = { /* 30HZ */
	.clock = (FRAME_WIDTH + MODE_3_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_3_VFP + VSA + VBP) * MODE_3_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_3_HFP,
	.hsync_end = FRAME_WIDTH + MODE_3_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_3_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_3_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_3_VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x42, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0xDA, data, 3);
	pr_info(" %s start \n", __func__);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static void lcm_panel_backlight_timer(struct timer_list *t)
{
	struct lcm *ctx = from_timer(ctx, t, backlight_timer);
	u8 bl_level[] = {0x51, 0x00, 0x00};
	pr_err("[%s] +.\n", __func__);

	aod_backlight_flag = false;

	if (!ctx) {
		pr_err("[%s]: ctx is NULL.\n", __func__);
		return;
	}

	if (!ctx->prepared)
	{
		pr_err("[%s]: prepared = %d", __func__, ctx->prepared);
		return;
	}

	bl_level[1] = (last_bl_level >> 8) & 0xFF;
	bl_level[2] = last_bl_level & 0xFF;

	lcm_dcs_write(ctx, bl_level, ARRAY_SIZE(bl_level)); 
	pr_info("[%s] -.  last_bl_level = %d\n", __func__, last_bl_level);
}

#define BL_OD_THRESHOLD 47

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	static int apl_on = 0;
	static int od_bl_on = 0;
	int left = 0, mid = 0;
	int right = sizeof(tianma_apl_set) / sizeof(tianma_apl_set[0]) - 1;
	char bl_tb0[] = {0x51, 0x00, 0x00};
	char apl_bl_cmd[] = {0xF0, 0xAA, 0x1C};
	char apl_bl_reg[] = {0xB3, 0xB2, 0xB1};

	char od_bl_tb0[] = {0xa4, 0x01};
	char od_bl_tb1[] = {0xf0, 0xaa, 0x19};
	char od_bl_tb2[] = {0xc0, 0x80};
	char od_bl_tb3[] = {0xf0, 0xaa, 0x00};
	char od_bl_tb4[] = {0xa4, 0x00};
	char od_bl_tb5[] = {0xa6, 0x01};
	
	char esd_restore_reg[] = {0x64, 0x00};

	int i;

	struct lcm *ctx = panel_to_lcm(this_panel);
	pr_info(" %s start. level = %d \n", __func__, level);

	if (long_press_pwrkey) {
		pr_info("[%s]: display off prepared %d.\n", __func__, ctx->prepared);
		return 0;
	}

	if (true == ctx->esd_flag) {
		ctx->esd_flag = false;
		mutex_lock(&ctx->panel_lock);
		cb(dsi, handle, esd_restore_reg, ARRAY_SIZE(esd_restore_reg));
		mutex_unlock(&ctx->panel_lock);
	}

	if ((RECOVERY_BOOT == ctx->boot_mode) && (level != 0)) {
		pr_info("[%s]: boot_mode is recovery mode.\n", __func__);
		level = 307;
	}

	if (aod_backlight_flag && level != 0) {
		last_bl_level = level;
		pr_info("[%s] current level = %d skip backlight.\n", __func__, level);
		return 0;
	}

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}

	if (!cb)
		return -1;

	pr_info("%s: last_bl_level = %d,level = %d\n", __func__, last_bl_level, level);

	if (fod_in_calibration || lhbm_flag || ctx->dynamic_fps == 30) {
		pr_info("panel skip set backlight %d due to fod hbm "
				"or fod calibration\n", level);
	} else {
		if (level < tianma_apl_set[0].bl) {
			if (apl_on) {
				mutex_lock(&ctx->panel_lock);
				cb(dsi, handle, apl_bl_cmd, ARRAY_SIZE(apl_bl_cmd));
				for (i = 0; i < sizeof(apl_bl_reg); i++) {
					pr_info("%s: apl_on[%d],level = %d,apl_bl_reg[%d]\n", __func__, apl_on, level, apl_bl_reg[i]);
					tianma_apl_set[0].para_list[0] = apl_bl_reg[i];
					cb(dsi, handle, tianma_apl_set[0].para_list, ARRAY_SIZE(tianma_apl_set[0].para_list));
				}
				mutex_unlock(&ctx->panel_lock);
				apl_on = 0;
			}
		} else {
			while (left <= right) {
				mid = left + (right - left) / 2;
				if (tianma_apl_set[mid].bl == level) {
					mutex_lock(&ctx->panel_lock);
					cb(dsi, handle, apl_bl_cmd, ARRAY_SIZE(apl_bl_cmd));
					for (i = 0; i < sizeof(apl_bl_reg); i++) {
						pr_info("%s: apl_on[%d],level = %d,apl_bl_reg[%d]\n", __func__, apl_on, level, apl_bl_reg[i]);
						tianma_apl_set[mid].para_list[0] = apl_bl_reg[i];
						cb(dsi, handle, tianma_apl_set[mid].para_list, ARRAY_SIZE(tianma_apl_set[mid].para_list));
					}
					mutex_unlock(&ctx->panel_lock);
					apl_on = 1;
					break;
				} else if (tianma_apl_set[mid].bl < level) {
					left = mid + 1;
				} else {
					right = mid - 1;
				}
			}
		}
		mutex_lock(&ctx->panel_lock);
		if (level <= BL_OD_THRESHOLD && level != 0 && od_bl_on == 0) {
			od_bl_tb2[1]= 0x80;
			pr_info("%s: bl 0d level = %d,od_bl_tb2[1]=[0x%x]\n", __func__, level, od_bl_tb2[1]);
			cb(dsi, handle, od_bl_tb0, ARRAY_SIZE(od_bl_tb0));
			cb(dsi, handle, od_bl_tb1, ARRAY_SIZE(od_bl_tb1));
			cb(dsi, handle, od_bl_tb2, ARRAY_SIZE(od_bl_tb2));
			cb(dsi, handle, od_bl_tb3, ARRAY_SIZE(od_bl_tb3));
			cb(dsi, handle, od_bl_tb4, ARRAY_SIZE(od_bl_tb4));
			cb(dsi, handle, od_bl_tb5, ARRAY_SIZE(od_bl_tb5));
			od_bl_on = 1;
		} else if (od_bl_on == 1 && level > BL_OD_THRESHOLD) {
			od_bl_tb2[1] = 0x00;
			pr_info("%s: bl 0d level = %d,od_bl_tb2[1]=[0x%x]\n", __func__, level, od_bl_tb2[1]);
			cb(dsi, handle, od_bl_tb0, ARRAY_SIZE(od_bl_tb0));
			cb(dsi, handle, od_bl_tb1, ARRAY_SIZE(od_bl_tb1));
			cb(dsi, handle, od_bl_tb2, ARRAY_SIZE(od_bl_tb2));
			cb(dsi, handle, od_bl_tb3, ARRAY_SIZE(od_bl_tb3));
			cb(dsi, handle, od_bl_tb4, ARRAY_SIZE(od_bl_tb4));
			cb(dsi, handle, od_bl_tb5, ARRAY_SIZE(od_bl_tb5));
			od_bl_on = 0;
		}

		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		mutex_unlock(&ctx->panel_lock);
	}

	if (level != 0)
		last_non_zero_bl_level = level;
	last_bl_level = level;

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = { /* 60HZ */
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,

	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},

	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,

#ifdef CONFIG_MI_DISP_FOD_SYNC
 	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif

	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.dfps_cmd_table[0] = {0, 2, {0x6c, 0x02} },
		.dfps_cmd_table[1] = {0, 2, {0x38, 0x00} },
	},

	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
	{
	int count = 0;
	struct lcm *ctx;
	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);
	return count;
}

static int panel_get_panel_soft_id(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct lcm *ctx;
	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "panel_soft_id:0x%02x\n", ctx->panel_soft_id);
	return count;
}

static int panel_normal_hbm_control(struct drm_panel *panel, uint32_t level)
{
//	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	pr_info(" lcm %s, level = %d\n", __func__, level);
#if 0
	ctx = panel_to_lcm(panel);
	mutex_lock(&ctx->panel_lock);
	if (level == 1) {
		//lcm_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF);
	} else if (level == 0) {
		//lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
	}
	mutex_unlock(&ctx->panel_lock);
#endif
	return 0;
}

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (level > 2047) {
		ctx->hbm_enabled = true;
	}
	pr_err("lcm_setbacklight_control backlight %d\n", level);
	return 0;
}

static struct LCM_setting_table fod_calibration[] ={
	{0x51, 02, {0x07, 0xff}},
};

static int backlight_for_calibration(struct drm_panel *panel, unsigned int level)
{
	u8 bl_tb[2] = {0};
	unsigned int bl_level = -1;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	if (level == -1) {
		bl_level = last_bl_level;
		pr_err("FOD calibration brightness restore last_bl_level = %d\n", last_bl_level);
		fod_in_calibration = false;
	} else {
		bl_level = level;
		fod_in_calibration = true;
	}
	bl_level_for_fod = bl_level;
	bl_tb[0] = (bl_level >> 8) & 0xFF;
	bl_tb[1] = bl_level & 0xFF;
	fod_calibration[0].para_list[0] = bl_tb[0];
	fod_calibration[0].para_list[1] = bl_tb[1];
	panel_ddic_send_cmd(fod_calibration, ARRAY_SIZE(fod_calibration), true);
	pr_info("backlight_for_calibration backlight %d\n", bl_level);
	return 0;
}

static bool get_lcm_initialized(struct drm_panel *panel)
{
	bool ret = false;
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	ret = ctx->prepared;
	return ret;
}

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness) {
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		ret = -1;
		goto err;
	}
	/*
		DOZE_TO_NORMAL = 0,
		DOZE_BRIGHTNESS_HBM = 1,
		DOZE_BRIGHTNESS_LBM = 2,
	*/
	pr_info(" lcm %s, doze_brightness = %d, lhbm_flag = %d, dynamic_fps = %d\n", __func__, doze_brightness, lhbm_flag, ctx->dynamic_fps);
	switch (doze_brightness) {
		case DOZE_BRIGHTNESS_HBM:
			if (lhbm_flag == false && ctx->dynamic_fps == REFRESH_AOD) {
				panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), true);
			} else {
				lcm_fod_aod_dbv[0].para_list[0] = (doze_hbm_bl >> 8) & 0xFF;
				lcm_fod_aod_dbv[0].para_list[1] = doze_hbm_bl & 0xFF;
				panel_ddic_send_cmd(lcm_fod_aod_dbv, ARRAY_SIZE(lcm_fod_aod_dbv), true);
			}
			break;
		case DOZE_BRIGHTNESS_LBM:
			if (lhbm_flag == false && ctx->dynamic_fps == REFRESH_AOD) {
				panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), true);
			} else {
				lcm_fod_aod_dbv[0].para_list[0] = (doze_lbm_bl >> 8) & 0xFF;
				lcm_fod_aod_dbv[0].para_list[1] = doze_lbm_bl & 0xFF;
				panel_ddic_send_cmd(lcm_fod_aod_dbv, ARRAY_SIZE(lcm_fod_aod_dbv), true);
			}
			break;
		case DOZE_TO_NORMAL:
			//if (ctx->dynamic_fps == REFRESH_AOD) {
				aod_backlight_flag = true;
				panel_ddic_send_cmd(lcm_backlight_off, ARRAY_SIZE(lcm_backlight_off), true);
				mod_timer(&ctx->backlight_timer, jiffies + msecs_to_jiffies(300));
			//}
			break;
		default:
			pr_err("%s: doze_brightness is invalid\n", __func__);
			ret = -1;
			goto err;
	}
	pr_info("doze_brightness = %d\n", doze_brightness);
	ctx->doze_brightness = doze_brightness;
err:
	return ret;
}

static int panel_set_only_aod_backlight(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s set_doze_brightness state = %d, lhbm_flag = %d, fps = %d, last_bl_level = %d",__func__, doze_brightness, lhbm_flag, ctx->dynamic_fps, last_bl_level);

	if ((DOZE_BRIGHTNESS_LBM  == doze_brightness || DOZE_BRIGHTNESS_HBM  == doze_brightness || DOZE_TO_NORMAL == doze_brightness) && lhbm_flag == false) {
		if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
			panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), true);
		} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
			panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), true);
		} else if (DOZE_TO_NORMAL == doze_brightness) {
			lcm_fod_aod_dbv[0].para_list[0] = (last_bl_level >> 8) & 0xFF;
			lcm_fod_aod_dbv[0].para_list[1] = last_bl_level & 0xFF;
			panel_ddic_send_cmd(lcm_fod_aod_dbv, ARRAY_SIZE(lcm_fod_aod_dbv), true);
		}
	}

	ctx->doze_brightness = doze_brightness;
	pr_info("%s set doze_brightness %d end -\n", __func__, doze_brightness);
	return 0;
}

int panel_get_doze_brightness(struct drm_panel *panel, u32 *brightness) {
	struct lcm *ctx = NULL;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}
	*brightness = ctx->doze_brightness;
	return 0;
}

static int panel_set_gir_on(struct drm_panel *panel)
{
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s: + ctx->gir_status = %d  \n", __func__, ctx->gir_status);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		panel_ddic_send_cmd(local_hbm_gir_on, ARRAY_SIZE(local_hbm_gir_on), true);
		ctx->gir_status = 1;
	}
err:
	return ret;
}
static int panel_set_gir_off(struct drm_panel *panel)
{
	struct lcm *ctx = NULL;
	int ret = -1;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s: + ctx->gir_status = %d \n", __func__, ctx->gir_status);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		panel_ddic_send_cmd(local_hbm_gir_off, ARRAY_SIZE(local_hbm_gir_off), true);
		ctx->gir_status = 0;
	}
err:
	return ret;
}

static int panel_get_gir_status(struct drm_panel *panel)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);
	return ctx->gir_status;
}

static int mi_disp_panel_update_lhbm_reg(struct mtk_dsi * dsi, enum lhbm_cmd_type type, int bl_level)
{
	u8 alpha_buf[2] = {0};
	u8 bl_tb[2] = {0};
	if(!dsi) {
		pr_err("%s:panel is NULL\n", __func__);
		return -1;
	}
	pr_err("lhbm update reg, lhbm_cmd_type:%d, bl_level = %d\n", type, bl_level);
	if (bl_level >= 0) {
		bl_tb[0] = (bl_level >> 8) & 0xFF;
		bl_tb[1] = bl_level & 0xFF;
		if (bl_level < bl_dc_pwm_threshold) {
			alpha_buf[0] = (tianma_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = tianma_alpha_set[bl_level] & 0xff;
			local_hbm_normal_white[2].para_list[0] = alpha_buf[0];
			local_hbm_normal_white[2].para_list[1] = alpha_buf[1];
			local_hbm_normal_white[2].para_list[2] = 0x01;
			local_hbm_normal_white[2].para_list[3] = 0x15;
		} else {
			local_hbm_normal_white[2].para_list[0] = 0x10;
			local_hbm_normal_white[2].para_list[1] = 0x00;
			local_hbm_normal_white[2].para_list[2] = bl_tb[0];
			local_hbm_normal_white[2].para_list[3] = bl_tb[1];
		}
		pr_err("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
	} else {
		pr_err("The backlight level wrong!\n");
		return -1;
	}

	switch (type) {
	case TYPE_WHITE_1200:
		local_hbm_normal_white[2].para_list[4] = 0x00;
		break;
	case TYPE_WHITE_200:
		local_hbm_normal_white[2].para_list[4] = 0x01;
		break;
	case TYPE_GREEN_500:
		break;
	case TYPE_LHBM_OFF:
		local_hbm_normal_off[1].para_list[0] = bl_tb[0];
		local_hbm_normal_off[1].para_list[1] = bl_tb[1];
		break;
	default:
		pr_err("unsuppport cmd \n");
		return -1;
	}

	return 0;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi,  enum local_hbm_state lhbm_state)
{
	struct lcm *ctx = NULL;
	int bl_level = 0;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if(!dsi || !dsi->panel) {
		pr_err("%s:panel is NULL\n", __func__);
		return -1;
	}
	mi_cfg = &dsi->mi_cfg;
	ctx = panel_to_lcm(dsi->panel);
	if (!ctx || !ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}
	pr_err("[%s]:[%d]:[fod_in_calibration:%d]:[last_bl_level:%d]\n", __func__, __LINE__, fod_in_calibration, last_bl_level);
	if (fod_in_calibration) {
		bl_level = bl_level_for_fod;
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
		bl_level = doze_lbm_bl;
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
		bl_level = doze_hbm_bl;
	} else {
		bl_level = last_bl_level;
	}
	pr_err("[%s]:[%d]:[bl_level:%d]:[lhbm_state:%d]\n", __func__, __LINE__, bl_level, lhbm_state);
#if 0
	if (bl_level > hbm_level) {
		if((lhbm_state == LOCAL_HBM_OFF_TO_NORMAL) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE)) {
			pr_info("exit lhbm to hbm!\n");
		} else {
			pr_info("The bl_level is %d,exit hbm mode to normal!\n", bl_level);
			bl_level = hbm_level;
			panel_ddic_send_cmd(exit_hbm_to_normal, ARRAY_SIZE(exit_hbm_to_normal), true);
		}
	}
#endif
	pr_err("%s local fps:%d hbm_state :%d \n", __func__, ctx->dynamic_fps, lhbm_state);

	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL://0
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT://10
		pr_info("LOCAL_HBM_NORMAL off\n");
		lhbm_flag = false;

		msleep(5);
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_LHBM_OFF, bl_level);
		panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		mutex_unlock(&ctx->lhbm_lock);

		mi_cfg->lhbm_en = false;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE://11
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE\n");
		lhbm_flag = false;
		mutex_lock(&ctx->lhbm_lock);

		if ((ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) || (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM)) {
			mi_disp_panel_update_lhbm_reg(dsi, TYPE_LHBM_OFF, bl_level);
			panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		} else {
			mi_disp_panel_update_lhbm_reg(dsi, TYPE_LHBM_OFF, last_bl_level);
			panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		}

		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = false;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT://6
	case LOCAL_HBM_NORMAL_WHITE_1000NIT://1
		pr_err(" LOCAL_HBM_NORMAL_WHITE_1000NIT \n");
		lhbm_flag = true;

		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_WHITE_1200, bl_level);
		panel_ddic_send_cmd(local_hbm_normal_white, ARRAY_SIZE(local_hbm_normal_white), true);
		mutex_unlock(&ctx->lhbm_lock);

		mi_cfg->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT://7
	case LOCAL_HBM_NORMAL_WHITE_110NIT://4
		pr_info("LOCAL_HBM_NORMAL_WHITE_110NIT \n");
		lhbm_flag = true;

		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_WHITE_200, bl_level);
		panel_ddic_send_cmd(local_hbm_normal_white, ARRAY_SIZE(local_hbm_normal_white), true);
		mutex_unlock(&ctx->lhbm_lock);

		mi_cfg->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT://5
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIT \n");
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}

	return 0;

}

static struct mtk_panel_params ext_params_mode_1 = { /* 90HZ */
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},

	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,

#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif

	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0x6c, 0x01} },
		.dfps_cmd_table[1] = {0, 2, {0x38, 0x00} },
	},

	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_mode_2 = { /* 120HZ */
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},

	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,

#ifdef CONFIG_MI_DISP_FOD_SYNC
 	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif

	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0x6c, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x38, 0x00} },
	},

	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_mode_3 = { /* 30HZ */
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,

	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},

	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,

#ifdef CONFIG_MI_DISP_FOD_SYNC
 	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif

	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 30,
		.dfps_cmd_table[0] = {0, 2, {0x6d, 0x01} },
		.dfps_cmd_table[1] = {0, 2, {0x39, 0x00} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
					       unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry (m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
				   struct drm_connector *connector,
				   unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct lcm *ctx = panel_to_lcm(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m == NULL) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}
	if (drm_mode_vrefresh(m) == MODE_0_FPS) {
		ext->params = &ext_params; /* 60hz */
	} else if (drm_mode_vrefresh(m) == MODE_1_FPS) {
		ext->params = &ext_params_mode_1; /* 90hz */
	} else if (drm_mode_vrefresh(m) == MODE_2_FPS){
		ext->params = &ext_params_mode_2; /* 120hz */
	} else if (drm_mode_vrefresh(m) == MODE_3_FPS) { /* 30hz */
		if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
			ext_params_mode_3.dyn_fps.dfps_cmd_table[0].para_list[1] = 0x01;
		} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
			ext_params_mode_3.dyn_fps.dfps_cmd_table[0].para_list[1] = 0x00;
		}
		ext->params = &ext_params_mode_3; /* 30hz */
	} else{
		ret = 1;
	}
	pr_info(" lcm %s, fps:%d, ret:%d\n", __func__, drm_mode_vrefresh(m), ret);
	if (!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	return ret;
}

static int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	struct device_node *chosen;
	char *tmp_buf = NULL;
	unsigned long tmp_size = 0;
	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tmp_buf = (char *)of_get_property(chosen, "lcm_white_point", (int *)&tmp_size);
		if (tmp_size > 0) {
			strncpy(buf, tmp_buf, tmp_size);
			pr_info("[%s]: white_point = %s, size = %d\n", __func__, buf, tmp_size);
		} else {
			pr_info("[%s]:get white point failed\n", __func__);
#ifdef CONFIG_MI_DISP_DFS_EVENT
			mi_disp_mievent_str(MI_EVENT_PANEL_WP_READ_FAILED);
#endif
		}
	} else {
		pr_info("[%s]:find chosen failed\n", __func__);
#ifdef CONFIG_MI_DISP_DFS_EVENT
		mi_disp_mievent_str(MI_EVENT_PANEL_WP_READ_FAILED);
#endif
	}

	return tmp_size;
}

static int panel_init_power(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	//VCI 3.0V
	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	udelay(10000);

	//VDD 1.2V
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n",
			__func__, PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	udelay(1000);
	lcm_panel_dvdd_regulator_config(ctx->dev, true);
	
	pr_info(" lcm %s\n", __func__);
	return 0;
}

static int panel_power_down(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	udelay(2000);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	udelay(1000);

	//VDD 1.2V -> 0
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n",
			__func__, PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	lcm_panel_dvdd_regulator_config(ctx->dev, false);
	udelay(2000);

	//VCI 3.0V -> 0
	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	udelay(1000);

	return 0;
}

#ifdef CONFIG_MI_ESD_SUPPORT
static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	unsigned char bl_tb0[] = {0x51, 0x00, 0x00};
	struct lcm *ctx = panel_to_lcm(panel);
	bl_tb0[1] = (last_non_zero_bl_level >> 8) & 0xFF;
	bl_tb0[2] = last_non_zero_bl_level & 0xFF;
	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);
	pr_info("lcm_esd_restore_backlight , last_bl_level[%d]\n", last_non_zero_bl_level);
}
#endif

static int panel_fod_lhbm_init (struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_err("invalid dsi point\n");
		return -1;
	}
	pr_info("panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 3;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 3;
	dsi->mi_cfg.local_hbm_enabled = 1;
	return 0;
}

static int panel_get_dynamic_fps(struct drm_panel *panel, u32 *fps)
{
	int ret = 0;
	struct lcm *ctx;

	if (!panel || !fps) {
	pr_err("%s: panel or fps is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	*fps = ctx->dynamic_fps;
err:
	return ret;
}

static int panel_get_factory_max_brightness(struct drm_panel *panel, u32 *max_brightness_clone)
{
	struct lcm *ctx;

	pr_info("%s +\n", __func__);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->factory_max_brightness;

	return 0;
}

static void panel_set_round_enable(struct drm_panel *panel, bool mode)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s set round state = %d",__func__, mode);

	if (mode) {
		panel_ddic_send_cmd(lcm_round_on, ARRAY_SIZE(lcm_round_on), true);
	} else {
		panel_ddic_send_cmd(lcm_round_off, ARRAY_SIZE(lcm_round_off), true);
	}
	ctx->round_status = mode;
	pr_info("%s set round %d end -\n", __func__, mode);
	return;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
	.get_panel_info = panel_get_panel_info,
	.get_panel_soft_id = panel_get_panel_soft_id,
	.normal_hbm_control = panel_normal_hbm_control,
	.setbacklight_control = lcm_setbacklight_control,
	.get_panel_initialized = get_lcm_initialized,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_wp_info = panel_get_wp_info,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.init_power = panel_init_power,
	.power_down = panel_power_down,
#ifdef CONFIG_MI_ESD_SUPPORT
	.esd_restore_backlight = lcm_esd_restore_backlight,
#endif
	.backlight_for_calibration = backlight_for_calibration,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.set_only_aod_backlight = panel_set_only_aod_backlight,
	.get_panel_dynamic_fps = panel_get_dynamic_fps,
	.get_panel_factory_max_brightness = panel_get_factory_max_brightness,
	.panel_set_round_enable = panel_set_round_enable,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	unsigned int bpc;
	struct {
		unsigned int width;
		unsigned int height;
	} size;
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	struct drm_display_mode *mode_3;
	pr_info(" %s lcm_get_modes start \n", __func__);

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_1.hdisplay,
			 performance_mode_1.vdisplay,
			 drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	mode_2 = drm_mode_duplicate(connector->dev, &performance_mode_2);
	if (!mode_2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_2.hdisplay,
			 performance_mode_2.vdisplay,
			 drm_mode_vrefresh(&performance_mode_2));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_2);

	mode_3 = drm_mode_duplicate(connector->dev, &performance_mode_3);
	if (!mode_3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_3.hdisplay, performance_mode_3.vdisplay,
			drm_mode_vrefresh(&performance_mode_3));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_3);

	connector->display_info.width_mm = PHYSICAL_WIDTH / 1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT / 1000;
	pr_info(" %s lcm_get_modes end \n", __func__);
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void panel_soft_id_init(struct lcm *ctx)
{
	struct device_node *chosen;
	char *tmp_buf = NULL;
	unsigned long tmp_size = 0;
	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tmp_buf = (char *)of_get_property(chosen, "lcm_soft_id", (int *)&tmp_size);
		if (tmp_size > 0) {
			sscanf(tmp_buf, "%02hhx", &(ctx->panel_soft_id));
			pr_info("[%s]: lcm_soft_id = %s, size = %d\n", __func__, tmp_buf, tmp_size);
		} else {
			pr_info("[%s]:get lcm_soft_id failed\n", __func__);
		}
	} else {
		pr_info("[%s]:find chosen failed\n", __func__);
	}
}

static void panel_get_boot_mode(struct lcm *ctx)
{
	struct device_node *chosen;
	unsigned long tmp_size = 0;
	const struct {
		u32 size;
		u32 tag;
		u32 boot_mode;
		u32 boot_type;
	} *tag;

	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tag = of_get_property(chosen, "atag,boot", (int *)&tmp_size);
		if (tag) {
			pr_info("size = %d, tmp_size = %d, tag = %d, boot_mode = %d, boot_type = %d", 
					tag->size, tmp_size, tag->tag, tag->boot_mode, tag->boot_type);
			ctx->boot_mode = tag->boot_mode;
		} else {
			pr_info("[%s]:get recovery_mode failed\n", __func__);
		}
	} else {
		pr_info("[%s]:find chosen failed\n", __func__);
	}
}

extern void get_panel_name(const char *flag);
static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	pr_info(" %s+\n", __func__);
	dsi_node = of_get_parent(dev->of_node);
	pr_info("lcm: %d\n", dsi_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);

	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
#ifdef CONFIG_MI_ESD_SUPPORT
	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
			dev->of_node, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));
	ext_params_mode_1.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_mode_1.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_mode_2.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_mode_2.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_mode_3.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_mode_3.err_flag_irq_flags = ext_params.err_flag_irq_flags;
#endif
	devm_gpiod_put(dev, ctx->reset_gpio);

	ret = lcm_panel_dvdd_regulator_init(dev);
	if(ret) {
		if(ret == -EPROBE_DEFER) {
			pr_info("%s+ skip probe, delay regulator ready coll probe\n", __func__);
			return ret;
		}
		pr_info("LCM regulator init failed: %d\n", ret);
	}

	lcm_panel_dvdd_regulator_config(dev, true);
	ctx->esd_flag = true;
	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel_info = panel_name;
	ctx->gir_status = 1;
	ctx->dynamic_fps = 120;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	ctx->hbm_enabled = false;
	this_panel = &ctx->panel;
	panel_soft_id_init(ctx);
	panel_get_boot_mode(ctx);
	panel_get_lhbm_info();
	pr_info("%s-\n", __func__);
	get_panel_name(panel_name);
	timer_setup(&ctx->backlight_timer, lcm_panel_backlight_timer, 0);
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	pr_info(" %s start \n", __func__);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	if (ext_ctx != NULL) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "p7_36_0d_0a_dsc_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-p7-36-0d-0a-dsc-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);


MODULE_DESCRIPTION("panel-p7-36-0d-0a-dsc-vdo Panel Driver");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: et5904-regulator");
