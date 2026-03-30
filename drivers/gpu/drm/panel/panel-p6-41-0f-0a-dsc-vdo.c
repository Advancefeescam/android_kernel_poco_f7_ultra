
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
/* P6 code for HQFEAT-126076 by p-chenchen79 at 2025/6/12 start */
#include <linux/hqsysfs.h>
/* P6 code for HQFEAT-126076 by p-chenchen79 at 2025/6/12 end */
/* P6 code for HQFEAT-190582 by p-liaoxianguo at 2025/9/26 start */
#include <linux/math64.h>
/* P6 code for HQFEAT-190582 by p-liaoxianguo at 2025/9/26 end */
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 start */
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_dsi_panel.h"
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 end */
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
#include "include/panel_alpha_data.h"
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
/*P6 code for HQFEAT-118719 by p-chenchen79 at 2025/07/02 start*/
#ifdef CONFIG_MI_DISP_DFS_EVENT
#include "../mediatek/mediatek_v2/mi_disp/mi_disp_event.h"
#endif
/*P6 code for HQFEAT-118719 by p-chenchen79 at 2025/07/02 end*/
#define REGFLAG_CMD                 0xFFFA
#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW           0xFFFE
#define REGFLAG_RESET_HIGH          0xFFFF
#define DATA_RATE                   1060
#define FRAME_WIDTH                 (1080)
#define FRAME_HEIGHT                (2392)
#define DSC_ENABLE                  1
#define DSC_VER                     18
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
#define DSC_DEC_DELAY               526
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      187
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          3511
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
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
#define FOD_DATA_LEN                20
#define CMD_NUM_MAX                 20
#define LHBM_DATA_LEN               18
#define REG_DATA_250nit             0xEA
#define REG_DATA_1200nit            0xEC
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
#define MAX_BRIGHTNESS_CLONE        16383
#define FACTORY_MAX_BRIGHTNESS      8191
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
#define REFRESH_AOD                 30
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16};
static unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};
#ifdef CONFIG_MI_DISP
extern void mi_dsi_panel_tigger_dimming_work(struct mtk_dsi *dsi);
#endif
extern int get_panel_dead_flag(void);
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
static struct LCM_setting_table lcm_aod_hbm[] = {
	{0x51, 02, {0x00, 0xD0}},
};
static struct LCM_setting_table lcm_aod_lbm[] = {
	{0x51, 02, {0x00, 0x19}},
};
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-149797 by p-chenchen79 at 2025/07/08 start*/
static struct LCM_setting_table lcm_exit_aod[] = {
	{0x9C, 2, {0xA5, 0xA5} },
	{0xFD, 2, {0x5A, 0x5A} },
	{0x9F, 1, {0x01} },
	{0xCD, 1, {0x0B} },
	{0x9C, 2, {0x5A, 0x5A} },
	{0xFD, 2, {0xA5, 0xA5} },
};
/* P6 code for HQFEAT-149797 by p-chenchen79 at 2025/07/08 end*/
static struct drm_panel *this_panel;
static last_bl_level;
static last_non_zero_bl_level = 511;
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
static struct mtk_ddic_dsi_msg *cmd_msg = NULL;
static bool lhbm_flag = false;
static bool fod_in_calibration = false;
static int bl_level_for_fod = 0;

//static u8 data_reg[FOD_DATA_LEN] = {0};

enum lhbm_cmd_type {
	TYPE_WHITE_1200 = 0,
	TYPE_WHITE_250,
	TYPE_LHBM_OFF
};
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
static int doze_lbm_bl = 25;
static int doze_hbm_bl = 208;
bool doze_brightness_flag;
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddi_gpio;
	struct gpio_desc *vdd_gpio;
	struct gpio_desc *vci_gpio;
	bool prepared;
	bool enabled;
	unsigned int gate_ic;
	int error;
	int dynamic_fps;
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 start */
	int gir_status;
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 start */
	bool hbm_enabled;
	const char *panel_info;
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 end */
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
	enum doze_brightness_state doze_brightness;
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
	struct mutex panel_lock;
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
	struct mutex lhbm_lock;
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
    int max_brightness_clone;
	int factory_max_brightness;
};
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 start */
static const char *panel_name = "panel_name=dsi_p6_41_0f_0a_dsc_vdo";
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 end */
#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})
#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
static u8 data_reg_250nit_default[LHBM_DATA_LEN] = {0x31,0x01,0x2A,0x30,0x16,0x8B,0x2A,0x33,0x18,0x8F,0x2A,0x41,0x26,0xA0,0x2A,0x41,0x26,0xA0};
static u8 data_reg_1200nit_default[LHBM_DATA_LEN] = {0x31,0x01,0x2B,0xD5,0xAA,0x59,0x2B,0xD7,0xAC,0x5B,0x2B,0xE6,0xBB,0x6C,0x2B,0xE6,0xBB,0x6C};
/*static struct LCM_setting_table local_hbm_first[] = {
	{0x9C, 02, {0xA5, 0xA5}},
	{0xFD, 02, {0x5A, 0x5A}},
	{0x9F, 01, {0x04}},
	{0x9B, 01, {0x80}},
};
static struct LCM_setting_table local_hbm_second[] = {
	{0x9F, 01, {0x04}},
	{0x9B, 01, {0x8A}},
};
static struct LCM_setting_table local_hbm_post_cmd[] = {
	{0x9F, 01, {0x04}},
	{0x9B, 01, {0x00}},
	{0x9C, 02, {0x5A, 0x5A}},
	{0xFD, 02, {0xA5, 0xA5}},
};*/

static struct LCM_setting_table local_hbm_normal_white[] = {
	{0x9C, 02, {0xA5, 0xA5}},
	{0xFD, 02, {0x5A, 0x5A}},
	{0x9F, 01, {0x04}},
	{0xBE, 18, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x97, 04, {0x11, 0xA0, 0x00, 0x00}},
	{0x9C, 02, {0x5A, 0x5A}},
	{0xFD, 02, {0xA5, 0xA5}},
};
static struct LCM_setting_table local_hbm_normal_off[] = {
	{0x97, 04, {0x00, 0x00, 0x00, 0x00}},
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 start */
	{0x51, 02, {0x07,0xff}},
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 end */
};
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 start */
static struct LCM_setting_table local_hbm_gir_on[] = {
	{0x9F, 01, {0x08}},
	{0xB2, 01, {0x12}},
};
#ifndef  KERNEL_FACTORY_BUILD
static struct LCM_setting_table local_hbm_gir_off[] = {
	{0x9F, 01, {0x08}},
	{0xB2, 01, {0x10}},
};
#endif
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 end */
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
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
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
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
static void lcm_panel_init(struct lcm *ctx)
{
	mutex_lock(&ctx->panel_lock);
	pr_info(" %s start \n", __func__);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(100);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(50);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xFD, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xD7, 0x11);
	if (ctx->dynamic_fps == 120) {
		lcm_dcs_write_seq_static(ctx, 0x48, 0x23);
	} else if (ctx->dynamic_fps == 90) {
		lcm_dcs_write_seq_static(ctx, 0x48, 0x03);
	} else {
		lcm_dcs_write_seq_static(ctx, 0x48, 0x13);
	}
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xFF, 0x87, 0x8F, 0xFF, 0x96, 0x9E, 0xFF, 0xA5, 0xAD, 0xFF, 0xB4, 0xBC, 0xFF, 0xC3, 0xCB, 0xFF, 0xD2, 0xDA, 0xFF, 0xE1, 0xE9, 0xFF, 0xF0, 0xF8, 0xF0, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xFF, 0x85, 0x8D, 0xFF, 0x94, 0x9C, 0xFF, 0xA4, 0xAB, 0xFF, 0xB3, 0xBA, 0xFF, 0xC2, 0xCA, 0xFF, 0xD1, 0xD9, 0xFF, 0xE1, 0xE8, 0xFF, 0xF0, 0xF7, 0xF0, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xFF, 0x8E, 0x95, 0xFF, 0x9C, 0xA3, 0xFF, 0xAA, 0xB1, 0xFF, 0xB8, 0xBF, 0xFF, 0xC7, 0xCE, 0xFF, 0xD5, 0xDC, 0xFF, 0xE3, 0xEA, 0xFF, 0xF1, 0xF8, 0xF0, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x03, 0xC9, 0x32, 0x91, 0x11, 0x11, 0x15, 0x12, 0x30, 0x18, 0x78, 0x21, 0x11, 0x06, 0x03, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x21, 0x21, 0x21, 0x21, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xC5, 0x08, 0x20, 0x88, 0x41, 0x99, 0x19, 0x19, 0x00, 0x1A, 0x3E, 0x39, 0x3A, 0x3A, 0x2A, 0x0A, 0x2F, 0x00, 0x20, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0xE1, 0xC0, 0x88, 0x1B, 0x1B, 0x95, 0x4E, 0x18);
#ifdef  KERNEL_FACTORY_BUILD
	// round off
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x00);
#else
	// round on
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01);
#endif
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x00, 0x01, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xEE, 0x04, 0x00, 0x00, 0x00, 0x08, 0x88, 0x08, 0x08, 0x00, 0xFF, 0x88, 0x88, 0x29, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x10, 0x00, 0xC0, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE2, 0x46, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x04, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x00, 0x80, 0x0C, 0xFF, 0x0F, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xCD, 0x0B, 0x1F, 0x1F, 0xF0, 0x03, 0xA8, 0x00, 0x96, 0xF0, 0x0A, 0x68, 0x00, 0x96, 0x1F, 0x00, 0x68, 0x00, 0x96, 0x1F, 0x0A, 0x68, 0x00, 0x96);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x07, 0x01, 0x00, 0x01, 0x01, 0x03, 0x4C, 0x02, 0x02, 0x03, 0x4C, 0x02, 0x02, 0x03, 0x4C, 0x02, 0x02, 0x88, 0xAA, 0x81);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x01, 0x01, 0x01, 0x03, 0x4C, 0x02, 0x02, 0x03, 0x4C, 0x02, 0x02, 0x03, 0x4C, 0x02, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD3, 0xA0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0xE0);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x29);
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
/* P6 code for BUGP6-3250 by p-liaoxianguo at 2025/8/11 start */
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xFD, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xC8, 0x1A, 0x1A, 0x00, 0x00);
	msleep(150);
/* P6 code for BUGP6-3250 by p-liaoxianguo at 2025/8/11 end */
	mutex_unlock(&ctx->panel_lock);
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	pr_err(" lcm_prepare 1 %s\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);
	pr_err(" lcm_prepare 4 %s\n", __func__);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	ctx->prepared = true;
/* P6 code for BUGP6-719 by p-liaoxianguo at 2025/07/14 start*/
	lhbm_flag = false;
	fod_in_calibration = false;
/* P6 code for BUGP6-719 by p-liaoxianguo at 2025/07/14 end*/
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_err(" lcm_prepare 5 %s\n", __func__);
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
/* P6 code for HQFEAT-126105 by p-chenchen79 at 2025/6/27 start */
static int panel_init_power(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	//VDDI 1.8V
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);
	//VCI 3.0V
	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	udelay(1000);
	//VDD 1.2V
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n",
			__func__, PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	udelay(10000);
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
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n",
			__func__, PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	udelay(1000);

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
	//VDDI 1.8V -> 0
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);
	return 0;
}
/* P6 code for HQFEAT-126105 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
/* P6 code for HQFEAT-118317 by p-liaoxianguo at 2025/8/1 start */
#define HFP (106)
#define HSA (16)
#define HBP (18)
#define VFP_120 (112)
#define VFP_90 (944)
#define VFP_60 (2672)
#define HFP_30 (1820)
#define VFP_30 (112)
#define VSA (1)
#define VBP (55)
#define VAC (2392)
#define HAC (1080)
#define MODE_120_FPS (120)
#define MODE_90_FPS (90)
#define MODE_60_FPS (60)
#define MODE_30_FPS (30)
/* P6 code for HQFEAT-118317 by p-liaoxianguo at 2025/8/1 end */
static u32 fake_heigh = 2392;
static u32 fake_width = 1080;
static bool need_fake_resolution;
static struct drm_display_mode mode_120 = {
	.clock = (FRAME_WIDTH + HFP + HSA + HBP) *
		 (FRAME_HEIGHT + VFP_120 + VSA + VBP) * MODE_120_FPS / 1000,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_120,
	.vsync_end = VAC + VFP_120 + VSA,
	.vtotal = VAC + VFP_120 + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
static struct drm_display_mode mode_90 = {
	.clock = (FRAME_WIDTH + HFP + HSA + HBP) *
		 (FRAME_HEIGHT + VFP_90 + VSA + VBP) * MODE_90_FPS / 1000,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90,
	.vsync_end = VAC + VFP_90 + VSA,
	.vtotal = VAC + VFP_90 + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
static struct drm_display_mode mode_60 = {
	.clock = (FRAME_WIDTH + HFP + HSA + HBP) *
		 (FRAME_HEIGHT + VFP_60 + VSA + VBP) * MODE_60_FPS / 1000,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60,
	.vsync_end = VAC + VFP_60 + VSA,
	.vtotal = VAC + VFP_60 + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
static struct drm_display_mode mode_30 = {
	.clock = (FRAME_WIDTH + HFP_30 + HSA + HBP) *
		 (FRAME_HEIGHT + VFP_30 + VSA + VBP) * MODE_30_FPS / 1000,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP_30,
	.hsync_end = HAC + HFP_30 + HSA,
	.htotal = HAC + HFP_30 + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_30,
	.vsync_end = VAC + VFP_30 + VSA,
	.vtotal = VAC + VFP_30 + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
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
	unsigned char id[3] = {0x40, 0x00, 0x00};
	ssize_t ret;
	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
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

static int panel_get_max_brightness_clone(struct drm_panel *panel, u32 *max_brightness_clone)
{
	struct lcm *ctx;
	pr_info("%s +\n", __func__);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->max_brightness_clone;
	return 0;
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
/* P6 code for HQFEAT-166864 by p-liaoxianguo at 2025/7/28 start */
#define APL_LEVEL_START 3407
#define APL_SCALE (-66)
#define APL_DIVISOR 688
#define APL_ADD 88
bool apl_mode;
void panel_normal_peak_control(void *dsi, dcs_write_gce cb, void *handle, unsigned int level, bool apl_enable)
{
	char peak_bl0[] = {0x51, 0x0D, 0x4F};//apl code
	char peak_bl1[] = {0x9C, 0xA5, 0xA5};
	char peak_bl2[] = {0xFD, 0x5A, 0x5A};
	char peak_bl3[] = {0x9F, 0x08};
	char peak_bl4[] = {0xB2, 0x12};
	char peak_bl5[] = {0xCC, 0x01, 0x00, 0x58};
	char peak_bl6[] = {0x9C, 0x5A, 0x5A};
	char peak_bl7[] = {0xFD, 0xA5, 0xA5};
	int32_t delta = 0;

	pr_info(" %s start \n", __func__);
	if (!cb)
		return;
	if (apl_enable == true) {
		if (level) {
			peak_bl0[1] = (level >> 8) & 0xFF;
			peak_bl0[2] = level & 0xFF;
			delta = level - APL_LEVEL_START;
		}
		peak_bl5[3] = (uint8_t)(APL_ADD + (delta * APL_SCALE) / APL_DIVISOR);
		cb(dsi, handle, peak_bl0, ARRAY_SIZE(peak_bl0));//ENTER APL CODE
		cb(dsi, handle, peak_bl1, ARRAY_SIZE(peak_bl1));
		cb(dsi, handle, peak_bl2, ARRAY_SIZE(peak_bl2));
		cb(dsi, handle, peak_bl3, ARRAY_SIZE(peak_bl3));
		cb(dsi, handle, peak_bl4, ARRAY_SIZE(peak_bl4));
		cb(dsi, handle, peak_bl5, ARRAY_SIZE(peak_bl5));
		cb(dsi, handle, peak_bl6, ARRAY_SIZE(peak_bl6));
		cb(dsi, handle, peak_bl7, ARRAY_SIZE(peak_bl7));
	} else if (apl_enable == false) {
		peak_bl0[1] = (last_bl_level >> 8) & 0xFF;
		peak_bl0[2] = last_bl_level & 0xFF;
		peak_bl5[1] = 0x00;
		cb(dsi, handle, peak_bl0, ARRAY_SIZE(peak_bl0));//EXIT APL CODE
		cb(dsi, handle, peak_bl1, ARRAY_SIZE(peak_bl1));
		cb(dsi, handle, peak_bl2, ARRAY_SIZE(peak_bl2));
		cb(dsi, handle, peak_bl3, ARRAY_SIZE(peak_bl3));
		cb(dsi, handle, peak_bl5, ARRAY_SIZE(peak_bl5));
		cb(dsi, handle, peak_bl6, ARRAY_SIZE(peak_bl6));
		cb(dsi, handle, peak_bl7, ARRAY_SIZE(peak_bl7));
	}
	pr_info(" %s end \n", __func__);
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
	struct lcm *ctx = panel_to_lcm(this_panel);
	pr_info(" %s start \n", __func__);
	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}
	if (!cb)
		return -1;
	pr_info("%s: last_bl_level = %d,level = %d\n", __func__, last_bl_level, level);
/* P6 code for HQFEAT-166864 by p-liaoxianguo at 2025/7/28 end */
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
	if (lhbm_flag || fod_in_calibration||ctx->dynamic_fps == 30) {
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
		pr_info("panel skip set backlight %d due to lhbm or fod calibration\n", level);
	} else {
/* P6 code for HQFEAT-137552 by p-liaoxianguo at 2025/8/28 start */
		if (level > APL_LEVEL_START) {
			panel_normal_peak_control(dsi, cb, handle, level, true);
			pr_info("%s peak level = %d \n", __func__, level);
			apl_mode = true;
		} else if (level <= APL_LEVEL_START && apl_mode == true) {
			panel_normal_peak_control(dsi, cb, handle, level, false);
			pr_info("%s exit peak level, bl_level = %d \n", __func__, level);
			apl_mode = false;
		} else {
		mutex_lock(&ctx->panel_lock);
		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		mutex_unlock(&ctx->panel_lock);
		}
/* P6 code for HQFEAT-137552 by p-liaoxianguo at 2025/8/28 end */
	}
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
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
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
static struct mtk_panel_params ext_params_mode_120 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
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
	.aod_delay_cmd_enable = 1,
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 2,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 9,
		.dfps_cmd_table[0] = {0, 2, {0x90, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x48, 0x23} },
		.dfps_cmd_table[2] = {0, 3, {0x51, 0x00, 0x19} },
		.dfps_cmd_table[3] = {0, 3, {0x9C, 0xA5, 0xA5} },
		.dfps_cmd_table[4] = {0, 3, {0xFD, 0x5A, 0x5A} },
		.dfps_cmd_table[5] = {0, 2, {0x9F, 0x01} },
		.dfps_cmd_table[6] = {0, 2, {0xCD, 0x00} },
		.dfps_cmd_table[7] = {0, 3, {0x9C, 0x5A, 0x5A} },
		.dfps_cmd_table[8] = {0, 3, {0xFD, 0xA5, 0xA5} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_mode_90 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
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
	.aod_delay_cmd_enable = 1,
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 2,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 9,
		.dfps_cmd_table[0] = {0, 2, {0x90, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x48, 0x03} },
		.dfps_cmd_table[2] = {0, 3, {0x51, 0x00, 0x19} },
		.dfps_cmd_table[3] = {0, 3, {0x9C, 0xA5, 0xA5} },
		.dfps_cmd_table[4] = {0, 3, {0xFD, 0x5A, 0x5A} },
		.dfps_cmd_table[5] = {0, 2, {0x9F, 0x01} },
		.dfps_cmd_table[6] = {0, 2, {0xCD, 0x00} },
		.dfps_cmd_table[7] = {0, 3, {0x9C, 0x5A, 0x5A} },
		.dfps_cmd_table[8] = {0, 3, {0xFD, 0xA5, 0xA5} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_mode_60 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
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
	.aod_delay_cmd_enable = 1,
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 2,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 9,
		.dfps_cmd_table[0] = {0, 2, {0x90, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x48, 0x13} },
		.dfps_cmd_table[2] = {0, 3, {0x51, 0x00, 0x19} },
		.dfps_cmd_table[3] = {0, 3, {0x9C, 0xA5, 0xA5} },
		.dfps_cmd_table[4] = {0, 3, {0xFD, 0x5A, 0x5A} },
		.dfps_cmd_table[5] = {0, 2, {0x9F, 0x01} },
		.dfps_cmd_table[6] = {0, 2, {0xCD, 0x00} },
		.dfps_cmd_table[7] = {0, 3, {0x9C, 0x5A, 0x5A} },
		.dfps_cmd_table[8] = {0, 3, {0xFD, 0xA5, 0xA5} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_mode_30 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
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
	.aod_delay_cmd_enable = 1,
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 30,
		.short_dfps_cmds_counts = 2,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 2, {0x90, 0x01} },
		.dfps_cmd_table[1] = {0, 3, {0x51, 0x01, 0x55} },
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
	if (drm_mode_vrefresh(m) == MODE_120_FPS) {
		ext->params = &ext_params_mode_120;
	} else if (drm_mode_vrefresh(m) == MODE_90_FPS) {
		ext->params = &ext_params_mode_90;
	} else if (drm_mode_vrefresh(m) == MODE_60_FPS){
		ext->params = &ext_params_mode_60;
	} else if (drm_mode_vrefresh(m) == MODE_30_FPS) {
		if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
			ext_params_mode_30.dyn_fps.dfps_cmd_table[1].para_list[1] = 0x0f;
			ext_params_mode_30.dyn_fps.dfps_cmd_table[1].para_list[2] = 0xff;
		} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
			ext_params_mode_30.dyn_fps.dfps_cmd_table[1].para_list[1] = 0x01;
			ext_params_mode_30.dyn_fps.dfps_cmd_table[1].para_list[2] = 0x55;
		}
		ext->params = &ext_params_mode_30;
	} else{
		ret = 1;
	}
	if (!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	return ret;
}
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 start */
static int panel_get_panel_info(struct drm_panel *panel, char *buf)
	{
	int count = 0;
	struct lcm *ctx;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);
	return count;
}
static int panel_normal_hbm_control(struct drm_panel *panel, uint32_t level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	mutex_lock(&ctx->panel_lock);
	if (level == 1) {
		lcm_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF);
	} else if (level == 0) {
		lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
	}
	mutex_unlock(&ctx->panel_lock);
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
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
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
	switch (doze_brightness) {
	case DOZE_BRIGHTNESS_HBM:
		if (lhbm_flag == false) {
			panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), true);
		}
		break;
	case DOZE_BRIGHTNESS_LBM:
		if (lhbm_flag == false) {
			panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), true);
		}
		break;
	case DOZE_TO_NORMAL:
		if (ctx->dynamic_fps == REFRESH_AOD) {
			if(ctx->doze_brightness == DOZE_BRIGHTNESS_HBM){
				ext_params_mode_60.dyn_fps.dfps_cmd_table[2].para_list[2] = 0xD0;
				ext_params_mode_90.dyn_fps.dfps_cmd_table[2].para_list[2] = 0xD0;
				ext_params_mode_120.dyn_fps.dfps_cmd_table[2].para_list[2] = 0xD0;
                        }
			else if(ctx->doze_brightness == DOZE_BRIGHTNESS_LBM){
				ext_params_mode_60.dyn_fps.dfps_cmd_table[2].para_list[2] = 0x19;
				ext_params_mode_90.dyn_fps.dfps_cmd_table[2].para_list[2] = 0x19;
				ext_params_mode_120.dyn_fps.dfps_cmd_table[2].para_list[2] = 0x19;
                        }else{
				pr_info("ctx->doze_brightness is DOZE_TO_NORMAL or err\n");
                        }
                }
		pr_info("exit aod_mode\n");
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

	pr_info("%s set_doze_brightness state = %d",__func__, doze_brightness);

	if ((DOZE_BRIGHTNESS_LBM  == doze_brightness || DOZE_BRIGHTNESS_HBM  == doze_brightness || DOZE_TO_NORMAL ==doze_brightness) && lhbm_flag == false) {
		if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
			panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), true);
		} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
			panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), true);
		}else if ( DOZE_TO_NORMAL ==doze_brightness && ctx->dynamic_fps == REFRESH_AOD){
                	if(ctx->doze_brightness == DOZE_BRIGHTNESS_HBM){
				ext_params_mode_60.dyn_fps.dfps_cmd_table[2].para_list[2] = 0xD0;
				ext_params_mode_90.dyn_fps.dfps_cmd_table[2].para_list[2] = 0xD0;
				ext_params_mode_120.dyn_fps.dfps_cmd_table[2].para_list[2] = 0xD0;
                        }
			else if(ctx->doze_brightness == DOZE_BRIGHTNESS_LBM){
				ext_params_mode_60.dyn_fps.dfps_cmd_table[2].para_list[2] = 0x19;
				ext_params_mode_90.dyn_fps.dfps_cmd_table[2].para_list[2] = 0x19;
				ext_params_mode_120.dyn_fps.dfps_cmd_table[2].para_list[2] = 0x19;
                        }
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
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
static struct LCM_setting_table fod_calibration[] = {
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
/*int mi_disp_panel_read_fod_reg(u8 reg)
{
	int ret = 0;
	int i =0;
	unsigned char rx_buf[FOD_DATA_LEN] = {0x00};
	struct mtk_ddic_dsi_msg cmds[] = {
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &reg,
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[0],
			.rx_len[0] = 10,
		},
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &reg,
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[10],
			.rx_len[0] = 10,
		}
	};
	panel_ddic_send_cmd(local_hbm_first, ARRAY_SIZE(local_hbm_first), true);
	ret = mtk_ddic_dsi_read_cmd(&cmds[0]);
	panel_ddic_send_cmd(local_hbm_second, ARRAY_SIZE(local_hbm_second), true);
	ret = mtk_ddic_dsi_read_cmd(&cmds[1]);
	panel_ddic_send_cmd(local_hbm_post_cmd, ARRAY_SIZE(local_hbm_post_cmd), true);
	for (i = 0; i < FOD_DATA_LEN; i++) {
		data_reg[i] = rx_buf[i];
		pr_info(" %s reg data[%d] =0x%x\n", __func__,i,data_reg[i]);
        }
	return ret;
}*/
/* P6 code for BUGP6-1193 by p-zhangyundan at 2025/8/5 start */
#define LHBM_INFO_LEN 40
static u8 lhbm_cmdbuf_1200nit[20] = {0};
static u8 lhbm_cmdbuf_250nit[20] = {0};
void lcm_get_lhbm_info(void)
{
    struct device_node *chosen;
    const char *lhbm_info = NULL;
    int ret, i;
    
    pr_info("%s: start LHBM info parsing\n", __func__);
    
    // 获取chosen节点
    chosen = of_find_node_by_path("/chosen");
    if (!chosen) {
        pr_err("Failed to find chosen node\n");
        return;
    }

    // 读取lcm_lhbm_info属性
    ret = of_property_read_string(chosen, "lcm_lhbm_info", &lhbm_info);
    if (ret) {
        pr_err("Failed to read LHBM info: %d\n", ret);
        goto out;
    }

    pr_info("LHBM info string: %s\n", lhbm_info);
    
    // 解析16进制数据
    for (i = 0; i < 20; i++) {
        if (sscanf(lhbm_info + i*2, "%2hhx", &lhbm_cmdbuf_1200nit[i]) != 1) {
            pr_err("Parse error at 1200nit[%d]\n", i);
            break;
        }
    }

    for (i = 20; i < 40; i++) {
        if (sscanf(lhbm_info + i*2, "%2hhx", &lhbm_cmdbuf_250nit[i-20]) != 1) {
            pr_err("Parse error at 250nit[%d]\n", i-20);
            break;
        }
    }

out:
    of_node_put(chosen); // 释放节点引用
}
/* P6 code for BUGP6-1193 by p-zhangyundan at 2025/8/5 end */
bool check_data_valid(enum lhbm_cmd_type type) {

	uint16_t sum = 0;
	uint16_t checksum = 0;
	uint16_t stored_checksum = 0;
	int i =0;
	bool checksum_equal = false;
	bool byte1_valid = false;
	/* P6 code for BUGP6-1193 by p-zhangyundan at 2025/8/5 start */
	switch (type) {
	case TYPE_WHITE_1200:
		for (i = 0; i < LHBM_DATA_LEN; i++) {
			sum += lhbm_cmdbuf_1200nit[i];
		}
		checksum = sum & 0xFFFF;
		stored_checksum = (lhbm_cmdbuf_1200nit[18] << 8) | lhbm_cmdbuf_1200nit[19];
		checksum_equal = (checksum == stored_checksum);
		byte1_valid = (lhbm_cmdbuf_1200nit[0] == 0x31);

		return (checksum_equal && byte1_valid);
	case TYPE_WHITE_250:
		for (i = 0; i < LHBM_DATA_LEN; i++) {
			sum += lhbm_cmdbuf_250nit[i];
		}
		checksum = sum & 0xFFFF;
		stored_checksum = (lhbm_cmdbuf_250nit[18] << 8) | lhbm_cmdbuf_250nit[19];
		checksum_equal = (checksum == stored_checksum);
		byte1_valid = (lhbm_cmdbuf_250nit[0] == 0x31);

		return (checksum_equal && byte1_valid);
	case TYPE_LHBM_OFF:
		break;
	default:
		pr_err("unsuppport cmd \n");
		return -1;
	}
	return false;
	/* P6 code for BUGP6-1193 by p-zhangyundan at 2025/8/5 end */
}
/* P6 code for BUGP6-1190 by p-liaoxianguo at 2025/7/23 start */
/* P6 code for HQFEAT-190582 by p-liaoxianguo at 2025/9/26 start */
void adjust_apl_lhbm(u8 lhbm_cmdbuf_1200nit[20], int bl_level)
{
	int R, G, B, R_prime, G_prime, B_prime = 0;
	long long delta, ratio_scaled = 0;
	unsigned char a14, a15, a16, a17, a14_prime = 0x00;

	if (!lhbm_cmdbuf_1200nit || bl_level < 3071) {
		pr_err("lhbm_cmdbuf_1200nit is null or bl_level < 3071 \n");
		return;
	}

	// 提取原始数组值
	a14 = lhbm_cmdbuf_1200nit[14];
	a15 = lhbm_cmdbuf_1200nit[15];
	a16 = lhbm_cmdbuf_1200nit[16];
	a17 = lhbm_cmdbuf_1200nit[17];
	pr_info("lhbm apl Original values - a14:0x%02X, a15:0x%02X, a16:0x%02X, a17:0x%02X\n",
		lhbm_cmdbuf_1200nit[14], lhbm_cmdbuf_1200nit[15],
		lhbm_cmdbuf_1200nit[16], lhbm_cmdbuf_1200nit[17]);

	// 计算原始RGB值
	R = ((a14 >> 4) & 0x03) * 256 + a15;
	G = ((a14 >> 2) & 0x03) * 256 + a16;
	B = (a14 & 0x03) * 256 + a17;

	// 计算插值因子（精确到小数点后两位）
	delta = bl_level - 3071;

	// 计算ratio * 10000（保留四位小数）
	// 公式: ratio = 1 + (0.89 - 1) * delta / 1024
	ratio_scaled = 10000 - div64_u64(1300LL * delta, 1024);

	// 四舍五入到百分位（小数点后两位）
	ratio_scaled = (ratio_scaled + 50) / 100; // +50实现四舍五入

	// 计算新RGB值（直接舍弃小数取整数）
	R_prime = R * ratio_scaled / 100; // 直接整数除法舍弃小数
	G_prime = G * ratio_scaled / 100;
	B_prime = B * ratio_scaled / 100;

	// 合成新的a14（高位组合）
	a14_prime = ((R_prime >> 8) & 0x03) << 4 |
				((G_prime >> 8) & 0x03) << 2 |
				((B_prime >> 8) & 0x03);

	// 更新数组
	lhbm_cmdbuf_1200nit[14] = a14_prime;
	lhbm_cmdbuf_1200nit[15] = R_prime & 0xFF;
	lhbm_cmdbuf_1200nit[16] = G_prime & 0xFF;
	lhbm_cmdbuf_1200nit[17] = B_prime & 0xFF;
	pr_info("lhbm apl final values - a14:0x%02X, a15:0x%02X, a16:0x%02X, a17:0x%02X\n",
		lhbm_cmdbuf_1200nit[14], lhbm_cmdbuf_1200nit[15],
		lhbm_cmdbuf_1200nit[16], lhbm_cmdbuf_1200nit[17]);
}
#define ALPHA_MAX_COUNT 277
/* P6 code for BUGP6-1190 by p-liaoxianguo at 2025/7/23 end */
static int mi_disp_panel_update_lhbm_reg(struct mtk_dsi *dsi, enum lhbm_cmd_type type, int bl_level)
{
	u8 alpha_buf[2] = {0};
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 start */
	u8 bl_tb[2] = {0};
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 end */
	//int ret = 0;
	int i = 0;
	u8 lhbm_cmdbuf_1200nit_temp[LHBM_DATA_LEN] = {0};
	if (!dsi) {
		pr_err("%s:panel is NULL\n", __func__);
		return -1;
	}
	pr_info("lhbm update, lhbm_cmd_type:%d, bl_level = %d\n", type, bl_level);
	if (bl_level >= 0) {
		if (bl_level <= ALPHA_MAX_COUNT) {
			alpha_buf[0] = (visionox_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = visionox_alpha_set[bl_level] & 0xff;
		}else{
			alpha_buf[0] =0x0f;
			alpha_buf[1] =0xff;
                }
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 start */
		bl_tb[0] = (bl_level >> 8) & 0xFF;
		bl_tb[1] = bl_level & 0xFF;
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 end */
		pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
	} else {
		pr_err("The backlight level wrong!\n");
		return -1;
	}

	switch (type) {
	case TYPE_WHITE_1200:
		local_hbm_normal_white[4].para_list[1] = (local_hbm_normal_white[4].para_list[1] & 0xF0) | (alpha_buf[0] & 0x0F);
		local_hbm_normal_white[4].para_list[2] = alpha_buf[1];
		/*ret = mi_disp_panel_read_fod_reg(REG_DATA_1200nit);
		if (ret < 0) {
			pr_err("%s error\n", __func__);
		}*/
		if(check_data_valid(TYPE_WHITE_1200)){
			for (i = 0; i < LHBM_DATA_LEN; i++) {
				lhbm_cmdbuf_1200nit_temp[i] = lhbm_cmdbuf_1200nit[i];
			}
			if (bl_level >= 3071)
				adjust_apl_lhbm(lhbm_cmdbuf_1200nit,bl_level);
			for (i = 0; i < LHBM_DATA_LEN; i++) {
				local_hbm_normal_white[3].para_list[i] =lhbm_cmdbuf_1200nit[i];
			}
			for (i = 0; i < LHBM_DATA_LEN; i++) {
				lhbm_cmdbuf_1200nit[i] = lhbm_cmdbuf_1200nit_temp[i];
			}
		}else{
			for (i = 0; i < LHBM_DATA_LEN; i++) {
				local_hbm_normal_white[3].para_list[i] =data_reg_1200nit_default[i];
			}
                }
		break;
/* P6 code for HQFEAT-190582 by p-liaoxianguo at 2025/9/26 end */
	case TYPE_WHITE_250:
		local_hbm_normal_white[4].para_list[1] = (local_hbm_normal_white[4].para_list[1] & 0xF0) | (alpha_buf[0] & 0x0F);
		local_hbm_normal_white[4].para_list[2] = alpha_buf[1];
		/*ret = mi_disp_panel_read_fod_reg(REG_DATA_250nit);
		if (ret < 0) {
			pr_err("%s error\n", __func__);
		}*/
		if(check_data_valid(TYPE_WHITE_250)){
			for (i = 0; i < LHBM_DATA_LEN; i++) {
				local_hbm_normal_white[3].para_list[i] =lhbm_cmdbuf_250nit[i];
			}
		}else{
			for (i = 0; i < LHBM_DATA_LEN; i++) {
				local_hbm_normal_white[3].para_list[i] =data_reg_250nit_default[i];
			}
                }
		break;
	case TYPE_LHBM_OFF:
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 start */
		local_hbm_normal_off[1].para_list[0] = bl_tb[0];
		local_hbm_normal_off[1].para_list[1] = bl_tb[1];
/* P6 code for BUGP6-1359 by p-xiewei79 at 2025/7/22 end */
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
	if (fod_in_calibration) {
		bl_level = bl_level_for_fod;
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
		bl_level = doze_lbm_bl;
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
		bl_level = doze_hbm_bl;
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
	} else {
		bl_level = last_bl_level;
	}

	pr_info("%s local hbm_state :%d\n", __func__, lhbm_state);
	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL://0
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT://10
		pr_info("LOCAL_HBM_NORMAL off\n");
		lhbm_flag = false;
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
/* P6 code for BUGP6-3171 by p-chenchen79 at 2025/8/11 start */
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_LHBM_OFF, bl_level);
/* P6 code for BUGP6-3171 by p-chenchen79 at 2025/8/11 end */
		panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = false;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT://6
	case LOCAL_HBM_NORMAL_WHITE_1000NIT://1
		pr_info("LOCAL_HBM_NORMAL_WHITE_1000NIT \n");
		lhbm_flag = true;
/* P6 code for BUGP6-1193 by p-chenchen79 at 2025/8/7 start */
		mi_cfg->lhbm_en = true;
/* P6 code for BUGP6-1193 by p-chenchen79 at 2025/8/7 end */
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_WHITE_1200, bl_level);
		panel_ddic_send_cmd(local_hbm_normal_white, ARRAY_SIZE(local_hbm_normal_white), true);
		mutex_unlock(&ctx->lhbm_lock);
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT://7
	case LOCAL_HBM_NORMAL_WHITE_110NIT://4
		pr_info("LOCAL_HBM_NORMAL_WHITE_110NIT \n");
		lhbm_flag = true;
/* P6 code for BUGP6-1193 by p-chenchen79 at 2025/8/7 start */
		mi_cfg->lhbm_en = true;
/* P6 code for BUGP6-1193 by p-chenchen79 at 2025/8/7 end */
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_WHITE_250, bl_level);
		panel_ddic_send_cmd(local_hbm_normal_white, ARRAY_SIZE(local_hbm_normal_white), true);
		mutex_unlock(&ctx->lhbm_lock);
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}
	return 0;
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
		}
	} else {
/*P6 code for HQFEAT-118719 by p-chenchen79 at 2025/07/02 start*/
#ifdef CONFIG_MI_DISP_DFS_EVENT
		mi_disp_mievent_str(MI_EVENT_PANEL_WP_READ_FAILED);
#endif
/*P6 code for HQFEAT-118719 by p-chenchen79 at 2025/07/02 end*/
		pr_info("[%s]:find chosen failed\n", __func__);
	}
	return tmp_size;
}
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
	lcm_get_lhbm_info();
	return 0;
}
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 start */
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
		if (cmd_msg != NULL) {
			panel_ddic_send_cmd(local_hbm_gir_on, ARRAY_SIZE(local_hbm_gir_on), true);
		}
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
#ifdef KERNEL_FACTORY_BUILD
		pr_info("%s:factory skip set gir: %d,return \n", __func__, ctx->gir_status);
#else
		if (cmd_msg != NULL) {
			panel_ddic_send_cmd(local_hbm_gir_off, ARRAY_SIZE(local_hbm_gir_off), true);
		  }
		ctx->gir_status = 0;
#endif
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
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */

#ifdef CONFIG_MI_ESD_SUPPORT
static void lcm_esd_restore_backlight(struct mtk_dsi *dsi, struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x03, 0xFF);
	pr_info("lcm_esd_restore_backlight \n");
}
#endif
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-149797 by p-chenchen79 at 2025/07/08 start*/
static void lcm_send_delay_cmd(void){
	pr_info("%s start\n",__func__);
	panel_ddic_send_cmd(lcm_exit_aod, ARRAY_SIZE(lcm_exit_aod), true);
	pr_info("%s end\n",__func__);
}
/* P6 code for HQFEAT-149797 by p-chenchen79 at 2025/07/08 end*/
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
	.get_panel_info = panel_get_panel_info,
	.normal_hbm_control = panel_normal_hbm_control,
	.setbacklight_control = lcm_setbacklight_control,
	.get_panel_initialized = get_lcm_initialized,
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/19 start */
	.ext_param_set = mtk_panel_ext_param_set,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.set_only_aod_backlight = panel_set_only_aod_backlight,
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/19 end */
	.set_lhbm_fod = panel_set_lhbm_fod,
	.get_wp_info = panel_get_wp_info,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.backlight_for_calibration = backlight_for_calibration,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.get_panel_factory_max_brightness = panel_get_factory_max_brightness,
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 start */
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
#ifdef CONFIG_MI_ESD_SUPPORT
	.esd_restore_backlight = lcm_esd_restore_backlight,
#endif
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-126105 by p-chenchen79 at 2025/6/27 start */
	.init_power = panel_init_power,
	.power_down = panel_power_down,
/* P6 code for HQFEAT-126105 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-149797 by p-chenchen79 at 2025/07/08 start*/
	.send_delay_cmd = lcm_send_delay_cmd,
/* P6 code for HQFEAT-149797 by p-chenchen79 at 2025/07/08 end*/
};
#endif
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 end */
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
static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vdisplay = fake_heigh;
		mode->vsync_start = fake_heigh + VFP_120;
		mode->vsync_end = fake_heigh + VFP_120 + VSA;
		mode->vtotal = fake_heigh + VFP_120 + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
}
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *disp_mode_120;
	struct drm_display_mode *disp_mode_90;
	struct drm_display_mode *disp_mode_60;
	struct drm_display_mode *disp_mode_30;
	pr_info(" %s start \n", __func__);
	if (need_fake_resolution)
	change_drm_disp_mode_params(&mode_120);
	disp_mode_120 = drm_mode_duplicate(connector->dev, &mode_120);
	if (!disp_mode_120) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_120.hdisplay, mode_120.vdisplay,
			drm_mode_vrefresh(&mode_120));
		return -ENOMEM;
	}
	drm_mode_set_name(disp_mode_120);
	disp_mode_120->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, disp_mode_120);
	disp_mode_90 = drm_mode_duplicate(connector->dev, &mode_90);
	if (!disp_mode_90) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 mode_90.hdisplay,
			 mode_90.vdisplay,
			 drm_mode_vrefresh(&mode_90));
		return -ENOMEM;
	}
	drm_mode_set_name(disp_mode_90);
	disp_mode_90->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, disp_mode_90);
	disp_mode_60 = drm_mode_duplicate(connector->dev, &mode_60);
	if (!disp_mode_60) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 mode_60.hdisplay,
			 mode_60.vdisplay,
			 drm_mode_vrefresh(&mode_60));
		return -ENOMEM;
	}
	drm_mode_set_name(disp_mode_60);
	disp_mode_60->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, disp_mode_60);
	disp_mode_30 = drm_mode_duplicate(connector->dev, &mode_30);
	if (!disp_mode_30) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_30.hdisplay, mode_30.vdisplay,
			drm_mode_vrefresh(&mode_30));
		return -ENOMEM;
	}
	drm_mode_set_name(disp_mode_30);
	disp_mode_30->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, disp_mode_30);
	connector->display_info.width_mm = PHYSICAL_WIDTH / 1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT / 1000;
	pr_info(" %s end \n", __func__);
	return 1;
}
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};
static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;
	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	pr_info(" %s start \n", __func__);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
}
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
	pr_info(" %s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
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
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->prepared = true;
	ctx->enabled = true;
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 start */
	ctx->panel_info = panel_name;
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 end */
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 start */
	ctx->gir_status = 1;
/* P6 code for HQFEAT-126093 by p-chenchen79 at 2025/6/27 end */
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
	ctx->dynamic_fps = 120;
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);
	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 start */
	ret = mtk_panel_ext_create(dev, &ext_params_mode_120, &ext_funcs, &ctx->panel);
/* P6 code for HQFEAT-118666 by p-chenchen79 at 2025/6/27 end */
	if (ret < 0)
		return ret;
#endif
	check_is_need_fake_resolution(dev);
/* P6 code for HQFEAT-126076 by p-chenchen79 at 2025/6/12 start */
	hq_regiser_hw_info(HWID_LCM, "oncell,vendor:41,IC:0F");
/* P6 code for HQFEAT-126076 by p-chenchen79 at 2025/6/12 end */
/* P6 code for HQFEAT-109010 by p-chenchen79 at 2025/06/16 start*/
	this_panel=&ctx->panel;
/* P6 code for HQFEAT-109010 by p-chenchen79 at 2025/06/16 end*/
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 start */
	ctx->hbm_enabled = false;
/* P6 code for HQFEAT-109456 by p-chenchen79 at 2025/6/16 end */
    ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (cmd_msg == NULL) {
		ret= -ENOMEM;
		pr_err("fail to vmalloc for cmd_msg\n");
	} else {
		memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
	}
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
	pr_info("%s-\n", __func__);
	return ret;
}
static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	pr_info(" %s start \n", __func__);
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 start */
	vfree(cmd_msg);
/* P6 code for HQFEAT-137549 by p-chenchen79 at 2025/6/19 end */
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	return 0;
}
static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "p6_41_0f_0a_dsc_vdo,lcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-p6-41-0f-0a-dsc-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("Chen Chen <chenchen5@huaqin.com>");
MODULE_DESCRIPTION("panel-p6-41-0f-0a-dsc-vdo Panel Driver");
MODULE_LICENSE("GPL v2");