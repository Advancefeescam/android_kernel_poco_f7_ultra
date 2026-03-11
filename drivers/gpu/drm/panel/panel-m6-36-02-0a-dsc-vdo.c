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
/*M6 code for HQ-248372 by yuyang at 2022/10/11 start*/
#include <linux/hqsysfs.h>
/*M6 code for HQ-248372 by yuyang at 2022/10/11 end*/
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#define PHYSICAL_WIDTH 69552
#define PHYSICAL_HEIGHT 154560

#define DATA_RATE 831

#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2400)

#define DSC_ENABLE 1
#define DSC_VER 17
#define DSC_SLICE_MODE 1
#define DSC_RGB_SWAP 0
#define DSC_DSC_CFG 34
#define DSC_RCT_ON 1
#define DSC_BIT_PER_CHANNEL 8
#define DSC_DSC_LINE_BUF_DEPTH 9
#define DSC_BP_ENABLE 1
#define DSC_BIT_PER_PIXEL 128
#define DSC_SLICE_HEIGHT 20
#define DSC_SLICE_WIDTH 540
#define DSC_CHUNK_SIZE 540
#define DSC_XMIT_DELAY 512
#define DSC_DEC_DELAY 526
#define DSC_SCALE_VALUE 32
#define DSC_INCREMENT_INTERVAL 488
#define DSC_DECREMENT_INTERVAL 7
#define DSC_LINE_BPG_OFFSET 12
#define DSC_NFL_BPG_OFFSET 1294
#define DSC_SLICE_BPG_OFFSET 1302
#define DSC_INITIAL_OFFSET 6144
#define DSC_FINAL_OFFSET 4336
#define DSC_FLATNESS_MINQP 3
#define DSC_FLATNESS_MAXQP 12
#define DSC_RC_MODEL_SIZE 8192
#define DSC_RC_EDGE_FACTOR 6
#define DSC_RC_QUANT_INCR_LIMIT0 11
#define DSC_RC_QUANT_INCR_LIMIT1 11
#define DSC_RC_TGT_OFFSET_HI 3
#define DSC_RC_TGT_OFFSET_LO 3
#ifdef PROJECT_DIAMOND
static unsigned int rc_buf_thresh[14] = { 896,  1792, 2688, 3584, 4480,
					  5376, 6272, 6720, 7168, 7616,
					  7744, 7872, 8000, 8064 };
static unsigned int range_min_qp[15] = { 0, 0, 1, 1, 3, 3, 3, 3,
					 3, 3, 5, 5, 5, 7, 13 };
static unsigned int range_max_qp[15] = { 4, 4,  5,  6,  7,  7,  7, 8,
					 9, 10, 11, 12, 13, 13, 15 };
static int range_bpg_ofs[15] = { 2,  0,   0,   -2,  -4,  -6,  -8, -8,
				 -8, -10, -10, -12, -12, -12, -12 };
#endif

/*M6 code for HQ-243178 by zhengjie at 2022/10/12 start*/
#ifdef PROJECT_DIAMOND
extern bool esd_flag;
#endif
/*M6 code for HQ-243178 by zhengjie at 2022/10/12 end*/
static last_bl_level;
static last_non_zero_bl_level = 511;

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
	struct mutex panel_lock;
};
#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})
#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})
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

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info(" %s start \n", __func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n", __func__,
			PTR_ERR(ctx->reset_gpio));
		return;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(11 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	/* Source optimize */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x17);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
	lcm_dcs_write_seq_static(ctx, 0xF9, 0x01);

	/* aod setting */
#ifdef CONFIG_MI_DISP_VDO_TO_CMD_AOD
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x00, 0x00, 0x04, 0x37, 0x00, 0x00,
				 0x05, 0x9F);
#else
	lcm_dcs_write_seq_static(ctx, 0x17, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x00, 0x00, 0x04, 0x37, 0x00, 0x00,
				 0x09, 0x5F);
#endif
	/* set video drop time */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x3C);
	lcm_dcs_write_seq_static(ctx, 0xF5, 0x81);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
	/* ELVDD_LVDET_OFF */
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x04, 0x38);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x09, 0x60);
	/* Column number:1080 */
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
	/* Row number:2400 */
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);
	/* COMPRESSION_METHOD */
	lcm_dcs_write_seq_static(ctx, 0x90, 0x03, 0x43);
	/* PPS */
	lcm_dcs_write_seq_static(ctx, 0x91, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00,
				 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E,
				 0x05, 0x16, 0x10, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00,
				 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E,
				 0x05, 0x16, 0x10, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00,
				 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E,
				 0x05, 0x16, 0x10, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x97, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00,
				 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E,
				 0x05, 0x16, 0x10, 0xF0);
	/* BCTRL */
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	/* VBP/VFP Video Mode */
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x0C, 0x00, 0x14);
	/* TE enable */
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	/* DBV */
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	/* dimming setting */
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x04, 0x04);
	/* fps 60hz */
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	/* GIR off */
	lcm_dcs_write_seq_static(ctx, 0x5F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x20);
	/* esd config */
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x47, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x18);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0xFB, 0xFB);
	/* ELVDD_ELVSS_Optimize */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x31);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0x90);
	/* IC may reload failed, force reload gamma again */
	lcm_dcs_write_seq_static(ctx, 0xE8, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x84);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x9C);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0xFF, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(130);
	lcm_dcs_write_seq_static(ctx, 0x29);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x47);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x47, 0xC5);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x00);
	pr_info(" %s end !\n", __func__);
}
static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s lcm_disable start \n", __func__);
	if (!ctx->enabled)
		return 0;
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = false;
	pr_info(" %s lcm_disable end \n", __func__);
	return 0;
}
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s lcm_unprepare start \n", __func__);
	if (!ctx->prepared)
		return 0;
	/*M6 code for HQ-243178 by zhengjie at 2022/10/12 start*/
	#ifdef PROJECT_DIAMOND
		if (esd_flag == true) {
			pr_info("%s, Now esd_flag = %d\n", __func__, esd_flag);
			}
	#endif
	/*M6 code for HQ-243178 by zhengjie at 2022/10/12 end*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x47, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);
	ctx->error = 0;
	ctx->prepared = false;
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n", __func__,
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	udelay(1000);

	//VCI 3.0V -> 0
	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n", __func__,
			PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	udelay(1000);

	//VDD 1.2V -> 0
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n", __func__,
			PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	udelay(1000);

	//VDDI 1.8V -> 0
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n", __func__,
			PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);
	pr_info(" %s lcm_unprepare end \n", __func__);
	return 0;
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	pr_info(" %s lcm_prepare start \n", __func__);
	if (ctx->prepared)
		return 0;
	//VDDI 1.8V
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n", __func__,
			PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);

	//VDD 1.2V
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n", __func__,
			PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	udelay(1000);

	//VCI 3.0V
	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n", __func__,
			PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	udelay(10000);
	gpiod_set_value(ctx->reset_gpio, 1);
	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	ctx->prepared = true;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
/*M6 code for HQ-243178 by zhengjie at 2022/10/12 start*/
#ifdef PROJECT_DIAMOND
	if (esd_flag == true) {
	    pr_info("%s, Now esd_flag = %d\n", __func__, esd_flag);
	}
#endif
/*M6 code for HQ-243178 by zhengjie at 2022/10/12 end*/
	pr_info(" %s lcm_prepare end \n", __func__);
	return ret;
}
static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s lcm_enable start \n", __func__);
	if (ctx->enabled)
		return 0;
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = true;
	pr_info(" %s lcm_enable end \n", __func__);
	return 0;
}

#define HFP (24)
#define HSA (8)
#define HBP (16)
#define VFP (2452)
/*M6 code for HQ-242975 by zhengjie at 2022/10/20 start*/
#define VFP_90HZ (780)
#define VFP_120HZ (20)
/*M6 code for HQ-242975 by zhengjie at 2022/10/20 end*/
#define VSA (4)
#define VBP (8)
#define VAC (2400)
#define HAC (1080)
static u32 fake_heigh = 2400;
static u32 fake_width = 1080;
static bool need_fake_resolution;
static struct drm_display_mode default_mode = {
	.clock = 328029,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
#ifdef PROJECT_DIAMOND
/*M6 code for HQ-242975 by zhengjie at 2022/10/20 start*/
static struct drm_display_mode performance_mode = {
	.clock = 325201,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90HZ,
	.vsync_end = VAC + VFP_90HZ + VSA,
	.vtotal = VAC + VFP_90HZ + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};

static struct drm_display_mode refresh_120_mode = {
	.clock = 328029,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_120HZ,
	.vsync_end = VAC + VFP_120HZ + VSA,
	.vtotal = VAC + VFP_120HZ + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
/*M6 code for HQ-242975 by zhengjie at 2022/10/20 end*/
#endif
#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n", __func__,
			PTR_ERR(ctx->reset_gpio));
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
	unsigned char data[3] = { 0x00, 0x00, 0x00 };
	unsigned char id[3] = { 0x00, 0x00, 0x00 };
	ssize_t ret;
	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}
	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);
	if (data[0] == id[0] && data[1] == id[1] && data[2] == id[2])
		return 1;
	pr_info("ATA expect read data is %x %x %x\n", id[0], id[1], id[2]);
	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = { 0x51, 0x00, 0x00 };
	//level = 2047;
	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}

	if (!cb)
		return -1;

	pr_info("%s: last_bl_level = %d,level = %d\n", __func__, last_bl_level,
		level);

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	if (last_bl_level == 0 && level > 0) {
#ifdef CONFIG_MI_DISP
#ifndef CONFIG_FACTORY_BUILD
		if (!get_panel_dead_flag() && ctx->doze_state == 0)
			mi_dsi_panel_tigger_dimming_work(dsi);
#endif
#endif
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

static struct mtk_panel_params ext_params = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	//.vfp_low_power = 0,
	.change_fps_by_vfp_send_cmd = 1,
	.cust_esd_check = 0,
	/*M6 code for HQ-243178 by zhengjie at 2022/10/12 start*/
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	/*M6 code for HQ-243178 by zhengjie at 2022/10/12 end*/
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
			.enable = DSC_ENABLE,
			.ver = DSC_VER,
			.slice_mode = DSC_SLICE_MODE,
			.rgb_swap = DSC_RGB_SWAP,
			.dsc_cfg = DSC_DSC_CFG,
			.rct_on = DSC_RCT_ON,
			.bit_per_channel = DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth = DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable = DSC_BP_ENABLE,
			.bit_per_pixel = DSC_BIT_PER_PIXEL,
			.pic_height = FRAME_HEIGHT,
			.pic_width = FRAME_WIDTH,
			.slice_height = DSC_SLICE_HEIGHT,
			.slice_width = DSC_SLICE_WIDTH,
			.chunk_size = DSC_CHUNK_SIZE,
			.xmit_delay = DSC_XMIT_DELAY,
			.dec_delay = DSC_DEC_DELAY,
			.scale_value = DSC_SCALE_VALUE,
			.increment_interval = DSC_INCREMENT_INTERVAL,
			.decrement_interval = DSC_DECREMENT_INTERVAL,
			.line_bpg_offset = DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset = DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset = DSC_SLICE_BPG_OFFSET,
			.initial_offset = DSC_INITIAL_OFFSET,
			.final_offset = DSC_FINAL_OFFSET,
			.flatness_minqp = DSC_FLATNESS_MINQP,
			.flatness_maxqp = DSC_FLATNESS_MAXQP,
			.rc_model_size = DSC_RC_MODEL_SIZE,
			.rc_edge_factor = DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0 = DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1 = DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi = DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo = DSC_RC_TGT_OFFSET_LO,
#ifdef PROJECT_DIAMOND
			.ext_pps_cfg = {
					.enable = 1,
					.rc_buf_thresh = rc_buf_thresh,
					.range_min_qp = range_min_qp,
					.range_max_qp = range_max_qp,
					.range_bpg_ofs = range_bpg_ofs,
				},
#endif
		},
	.data_rate = DATA_RATE,
	.dyn_fps = {
			.switch_en = 1,
			.vact_timing_fps = 60,
			.dfps_cmd_table[0] = { 0, 2, { 0x2F, 0x02 } },
		},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
/*M6 code for HQ-242975 by zhengjie at 2022/10/20 start*/
static struct mtk_panel_params ext_params_90hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	//.vfp_low_power = 0,
	.change_fps_by_vfp_send_cmd = 1,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
			.cmd = 0x91,
			.count = 2,
			.para_list[0] = 0x89,
			.para_list[1] = 0x28,
		},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
			.enable = DSC_ENABLE,
			.ver = DSC_VER,
			.slice_mode = DSC_SLICE_MODE,
			.rgb_swap = DSC_RGB_SWAP,
			.dsc_cfg = DSC_DSC_CFG,
			.rct_on = DSC_RCT_ON,
			.bit_per_channel = DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth = DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable = DSC_BP_ENABLE,
			.bit_per_pixel = DSC_BIT_PER_PIXEL,
			.pic_height = FRAME_HEIGHT,
			.pic_width = FRAME_WIDTH,
			.slice_height = DSC_SLICE_HEIGHT,
			.slice_width = DSC_SLICE_WIDTH,
			.chunk_size = DSC_CHUNK_SIZE,
			.xmit_delay = DSC_XMIT_DELAY,
			.dec_delay = DSC_DEC_DELAY,
			.scale_value = DSC_SCALE_VALUE,
			.increment_interval = DSC_INCREMENT_INTERVAL,
			.decrement_interval = DSC_DECREMENT_INTERVAL,
			.line_bpg_offset = DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset = DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset = DSC_SLICE_BPG_OFFSET,
			.initial_offset = DSC_INITIAL_OFFSET,
			.final_offset = DSC_FINAL_OFFSET,
			.flatness_minqp = DSC_FLATNESS_MINQP,
			.flatness_maxqp = DSC_FLATNESS_MAXQP,
			.rc_model_size = DSC_RC_MODEL_SIZE,
			.rc_edge_factor = DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0 = DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1 = DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi = DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo = DSC_RC_TGT_OFFSET_LO,
#ifdef PROJECT_DIAMOND
			.ext_pps_cfg = {
					.enable = 1,
					.rc_buf_thresh = rc_buf_thresh,
					.range_min_qp = range_min_qp,
					.range_max_qp = range_max_qp,
					.range_bpg_ofs = range_bpg_ofs,
				},
#endif
		},
	.data_rate = DATA_RATE,
	.dyn_fps = {
			.switch_en = 1,
			.vact_timing_fps = 90,
			.dfps_cmd_table[0] = { 0, 2, { 0x2F, 0x01 } },
		},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_120hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	//.vfp_low_power = 0,
	.change_fps_by_vfp_send_cmd = 1,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
			.cmd = 0x91,
			.count = 2,
			.para_list[0] = 0x89,
			.para_list[1] = 0x28,
		},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
			.enable = DSC_ENABLE,
			.ver = DSC_VER,
			.slice_mode = DSC_SLICE_MODE,
			.rgb_swap = DSC_RGB_SWAP,
			.dsc_cfg = DSC_DSC_CFG,
			.rct_on = DSC_RCT_ON,
			.bit_per_channel = DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth = DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable = DSC_BP_ENABLE,
			.bit_per_pixel = DSC_BIT_PER_PIXEL,
			.pic_height = FRAME_HEIGHT,
			.pic_width = FRAME_WIDTH,
			.slice_height = DSC_SLICE_HEIGHT,
			.slice_width = DSC_SLICE_WIDTH,
			.chunk_size = DSC_CHUNK_SIZE,
			.xmit_delay = DSC_XMIT_DELAY,
			.dec_delay = DSC_DEC_DELAY,
			.scale_value = DSC_SCALE_VALUE,
			.increment_interval = DSC_INCREMENT_INTERVAL,
			.decrement_interval = DSC_DECREMENT_INTERVAL,
			.line_bpg_offset = DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset = DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset = DSC_SLICE_BPG_OFFSET,
			.initial_offset = DSC_INITIAL_OFFSET,
			.final_offset = DSC_FINAL_OFFSET,
			.flatness_minqp = DSC_FLATNESS_MINQP,
			.flatness_maxqp = DSC_FLATNESS_MAXQP,
			.rc_model_size = DSC_RC_MODEL_SIZE,
			.rc_edge_factor = DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0 = DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1 = DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi = DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo = DSC_RC_TGT_OFFSET_LO,
#ifdef PROJECT_DIAMOND
			.ext_pps_cfg = {
					.enable = 1,
					.rc_buf_thresh = rc_buf_thresh,
					.range_min_qp = range_min_qp,
					.range_max_qp = range_max_qp,
					.range_bpg_ofs = range_bpg_ofs,
				},
#endif
		},
	.data_rate = DATA_RATE,
	.dyn_fps = {
			.switch_en = 1,
			.vact_timing_fps = 120,
			.dfps_cmd_table[0] = { 0, 2, { 0x2F, 0x00 } },
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
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	pr_info(" %s lcm:mtk_panel_ext_param_set \n", __func__);
	if (drm_mode_vrefresh(m) == 60) {
		pr_info(" %s lcm:60 \n", __func__);
		ext->params = &ext_params;
	} else if (drm_mode_vrefresh(m) == 90) {
		pr_info(" %s lcm:90 \n", __func__);
		ext->params = &ext_params_90hz;
	} else if (drm_mode_vrefresh(m) == 120) {
		pr_info(" %s lcm:120 \n", __func__);
		ext->params = &ext_params_120hz;
	} else {
		pr_info(" %s lcm:else\n", __func__);
		ret = 1;
	}
	return ret;
}

static void mode_switch_to_60(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	mutex_unlock(&ctx->panel_lock);
}

static void mode_switch_to_90(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
	mutex_unlock(&ctx->panel_lock);
}

static void mode_switch_to_120(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	mutex_unlock(&ctx->panel_lock);
}

static int mode_switch(struct drm_panel *panel, struct drm_connector *connector,
		       unsigned int cur_mode, unsigned int dst_mode,
		       enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (drm_mode_vrefresh(m) == 60 || drm_mode_vrefresh(m) == 30) {
		mode_switch_to_60(panel);
	} else if (drm_mode_vrefresh(m) == 90) {
		mode_switch_to_90(panel);
	} else if (drm_mode_vrefresh(m) == 120) {
		mode_switch_to_120(panel);
	} else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
				   struct drm_connector *connector,
				   struct mtk_panel_params **ext_para,
				   unsigned int mode)
{
	int ret = 0;

	pr_info(" %s lcm:mtk_panel_ext_param_get \n", __func__);
	if (mode == 0) {
		pr_info(" %s lcm:mode=0\n", __func__);
		*ext_para = &ext_params;
	} else if (mode == 1) {
		pr_info(" %s lcm:mode=1\n", __func__);
		*ext_para = &ext_params_90hz;
	} else if (mode == 2) {
		pr_info(" %s lcm:mode=2\n", __func__);
		*ext_para = &ext_params_120hz;
	} else {
		pr_info(" %s lcm:else\n", __func__);
		ret = 1;
	}
	return ret;
}
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.ext_param_get = mtk_panel_ext_param_get,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
};
#endif
/*M6 code for HQ-242975 by zhengjie at 2022/10/20 end*/

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
		mode->vsync_start = fake_heigh + VFP;
		mode->vsync_end = fake_heigh + VFP + VSA;
		mode->vtotal = fake_heigh + VFP + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
}

/*M6 code for HQ-242975 by zhengjie at 2022/10/20 start*/
static int lcm_get_modes(struct drm_panel *panel,
			 struct drm_connector *connector)
{
	struct drm_display_mode *mode;
#ifdef PROJECT_DIAMOND
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;
	if (need_fake_resolution) {
		change_drm_disp_mode_params(&default_mode);
		change_drm_disp_mode_params(&performance_mode);
		change_drm_disp_mode_params(&refresh_120_mode);
	}
#else
	pr_info(" %s lcm_get_modes start \n", __func__);
	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);
#endif
	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode);
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay, performance_mode.vdisplay,
			drm_mode_vrefresh(&performance_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &refresh_120_mode);
	if (!mode3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			refresh_120_mode.hdisplay, refresh_120_mode.vdisplay,
			drm_mode_vrefresh(&refresh_120_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode3);

	connector->display_info.width_mm = PHYSICAL_WIDTH / 1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT / 1000;
	return 1;
}
/*M6 code for HQ-242975 by zhengjie at 2022/10/20 end*/

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

/*M6 code for HQ-242983 by yuyang at 22/09/28 start*/
extern ssize_t lcm_mipi_reg_write(char *buf, size_t count);
extern ssize_t lcm_mipi_reg_read(char *buf);
static ssize_t mipi_reg_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	pr_info("%s, M6 project \n", __func__);
	return lcm_mipi_reg_read(buf);
}
static ssize_t mipi_reg_store(struct device *device,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	int rc = 0;
	pr_info("%s, M6 project \n", __func__);
	rc = lcm_mipi_reg_write((char *)buf, count);
	return rc;
}
static DEVICE_ATTR_RW(mipi_reg);
static struct attribute *gt9916r_attrs[] = {
	&dev_attr_mipi_reg.attr,
	NULL,
};
static const struct attribute_group gt9916r_attr_group = {
	.attrs = gt9916r_attrs,
};
/*M6 code for HQ-242983 by yuyang at 22/09/28 end*/

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
			pr_info("lcm:device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	pr_info(" %s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);

	/*M6 code for HQ-248372 by yuyang at 2022/10/11 start*/
	hq_regiser_hw_info(HWID_LCM, "oncell,vendor:36,IC:02");
	/*M6 code for HQ-248372 by yuyang at 2022/10/11 start*/

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n", __func__,
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs,
		       DRM_MODE_CONNECTOR_DSI);
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
	check_is_need_fake_resolution(dev);
	/*M6 code for HQ-242983 by yuyang at 22/09/28 start*/
	ret = sysfs_create_group(&dev->kobj, &gt9916r_attr_group);
	if (ret)
		return ret;
	/*M6 code for HQ-242983 by yuyang at 22/09/28 end*/
	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "m6_36_02_0a_dsc_vdo,lcm",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
			.name = "panel-m6-36-02-0a-dsc-vdo",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};

module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("panel-m6-36-02-0a-dsc-vdo Panel Driver");
MODULE_LICENSE("GPL v2");