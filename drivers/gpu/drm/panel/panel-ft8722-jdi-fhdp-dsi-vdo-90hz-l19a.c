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

/*L19A code for HQ-203969 by zhangkexin at 2022/04/26 start*/
#include <linux/hqsysfs.h>
/*L19A code for HQ-203969 by zhangkexin at 2022/04/26 end*/
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 start*/
#include <linux/gfp.h>
#include <linux/kmemleak.h>
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 end*/

/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
#include <drm/drm_connector.h>
#include "../mediatek/mediatek_v2/mtk_notifier_odm.h"
/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/
/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 start*/
#if (defined CONFIG_MI_ESD_SUPPORT) && (defined PROJECT_ROCK)
#include "../mediatek/mediatek_v2/mtk_disp_notify.h"
extern bool esd_flag;
#endif
/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 end*/

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
extern int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);

#define PHYSICAL_WIDTH 68429
#define PHYSICAL_HEIGHT 152571

#ifdef PROJECT_ROCK
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 start*/
extern int real_refresh;
extern int temp_refresh;
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 end*/
#endif
/*L19A code for HQ-198762 by chenzimo at 22/04/25 start*/
/*L19A code for HQ-194729 by zhangkexin at 2022/05/06 start*/
bool fts_gesture_flag;
EXPORT_SYMBOL(fts_gesture_flag);
/*L19A code for HQ-194729 by zhangkexin at 2022/05/06 end*/
/*L19A code for HQ-198762 by chenzimo at 22/04/25 end*/
#ifdef PROJECT_ROCK
/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
struct drm_notifier_data g_notify_data1;
/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/
#endif
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	int error;
};

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

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#ifdef PROJECT_ROCK
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 start*/
static ssize_t mtkfb_get_refresh(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	ret = scnprintf(buf, PAGE_SIZE, "%3d\n", real_refresh);
	return ret;
}
static ssize_t mtkfb_set_refresh(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
		sscanf(buf, "%3d", &real_refresh);
		return len;
}
static DEVICE_ATTR(mtkfb_fps, 0644, mtkfb_get_refresh, mtkfb_set_refresh);
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 end*/
#endif
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
	pr_info("%s start \n", __func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 start*/
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(3 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(12 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 end*/

	/*L19A code for HQ-194283 by chenzimo at 22/05/06 start*/
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	/*L19A code for HQ-194283 by chenzimo at 22/05/06 end*/

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	usleep_range(2000, 2001);

	pr_info("%s end !\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

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
#ifdef PROJECT_ROCK
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
	int blank;
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/
#endif
	struct lcm *ctx = panel_to_lcm(panel);
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 start*/
#if (defined CONFIG_MI_ESD_SUPPORT) && (defined PROJECT_ROCK)
          int data = MTK_DISP_BLANK_POWERDOWN;
#endif
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 end*/

	if (!ctx->prepared)
		return 0;
#ifdef PROJECT_ROCK
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 start*/
	#ifdef CONFIG_MI_ESD_SUPPORT
	if (esd_flag == true) {
	    pr_info("%s, Now esd_flag = %d\n", __func__, esd_flag);
	    mtk_disp_notifier_call_chain(MTK_DISP_ESD_EVENT_SUSPEND, &data);
	}
	#endif
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 end*/

	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
	blank = DRM_BLANK_POWERDOWN;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/
#endif

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 start*/
	lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x5A, 0xA5, 0x95, 0x27);

/*L19A code for HQ-194729 by zhangkexin at 2022/05/06 start*/
	if (!fts_gesture_flag) {
/*L19A code for HQ-194729 by zhangkexin at 2022/05/06 end*/
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		usleep_range(2000, 2001);
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 end*/

	ctx->error = 0;
	ctx->prepared = false;
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
#ifdef PROJECT_ROCK
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
	int blank;
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/
#endif
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 start*/
#if (defined CONFIG_MI_ESD_SUPPORT) && (defined PROJECT_ROCK)
	int data = MTK_DISP_BLANK_UNBLANK;
#endif
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 end*/

	if (ctx->prepared)
		return 0;

	/*L19A code for HQ-198762 by chenzimo at 22/04/25 start*/
	gpiod_set_value(ctx->reset_gpio, 0);
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 end*/

	usleep_range(2000, 2001);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n", __func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 start*/
	usleep_range(3000, 3001);
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 end*/

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n", __func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	_lcm_i2c_write_bytes(0x0, 0xf);
	_lcm_i2c_write_bytes(0x1, 0xf);

	/*L19A code for HQ-198762 by chenzimo at 22/04/25 start*/
	msleep(2);
	/*L19A code for HQ-198762 by chenzimo at 22/04/25 end*/

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

#ifdef PROJECT_ROCK
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 start*/
	#ifdef CONFIG_MI_ESD_SUPPORT
	if (esd_flag == true) {
	    pr_info("%s, Now esd_flag = %d\n", __func__, esd_flag);
	    mtk_disp_notifier_call_chain(MTK_DISP_ESD_EVENT_RESUME, &data);
	}
	#endif
	/*L19A code for HQ-221066 by zhangkexin at 2022/07/27 end*/

	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
	blank = DRM_BLANK_UNBLANK;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/
#endif
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

/*L19A code for HQ-198809 by zhangkexin at 2022/04/21 start*/
#define HFP (225)
/*L19A code for HQ-198809 by zhangkexin at 2022/04/21 end*/
#define HSA (20)
#define HBP (24)
#define VFP (1290)
/*L19A code for HQ-221211 by chenzimo at 2022/07/04 start*/
#define VFP_90HZ (46)
#define VFP_50HZ (2040)
#define VFP_30HZ (5000)
/*L19A code for HQ-221211 by chenzimo at 2022/07/04 end*/
#define VSA (2)
#define VBP (18)
#define VAC (2408)
#define HAC (1080)
static u32 fake_heigh = 2408;
static u32 fake_width = 1080;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	/*L19A code for HQ-198809 by zhangkexin at 2022/04/21 start*/
	.clock = 300935,
	/*L19A code for HQ-198809 by zhangkexin at 2022/04/21 end*/
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
#ifdef PROJECT_ROCK
/*L19A code for HQ-194275 by chenzimo at 2022/05/12 start*/
static struct drm_display_mode performance_mode = {
	.clock = 301340,
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

static struct drm_display_mode refresh_50_mode = {
	.clock = 303390,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_50HZ,
	.vsync_end = VAC + VFP_50HZ + VSA,
	.vtotal = VAC + VFP_50HZ + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};

static struct drm_display_mode refresh_30_mode = {
	.clock = 303039,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_30HZ,
	.vsync_end = VAC + VFP_30HZ + VSA,
	.vtotal = VAC + VFP_30HZ + VSA + VBP,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
/*L19A code for HQ-194275 by chenzimo at 2022/05/12 end*/
#endif

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

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
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
/*L19A code for HQ-209605 by zhangkexin at 2022/05/20 start*/
	pr_info("%s backlight = %d\n", __func__, level);
/*L19A code for HQ-209605 by zhangkexin at 2022/05/20 end*/
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

/*L19A code for HQ-194275 by chenzimo at 2022/05/12 start*/
static struct mtk_panel_params ext_params = {
	.pll_clk = 545,
	.vfp_low_power = 2070,
	.cust_esd_check = 0,
/*L19A code for HQ-194729 by zhangkexin at 2022/05/06 start*/
	.esd_check_enable = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 start*/
	.lcm_index = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 end*/
	.lcm_esd_check_table[0] = {
		.cmd = 0,
		.count = 1,
		.para_list[0] = 0x9c,
	},
/*L19A code for HQ-194729 by zhangkexin at 2022/05/06 end*/
	.is_cphy = 1,
	.data_rate = 1090,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 start*/
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 end*/
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 start*/
	.phy_timcon = {
		.hs_trail = 26,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 start*/
		.hs_prpr = 12,
		.lpx = 14,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 end*/
	},
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 end*/
};
#ifdef PROJECT_ROCK
static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 545,
	.vfp_low_power = 2070,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 start*/
	.lcm_index = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 end*/
	.lcm_esd_check_table[0] = {
		.cmd = 0,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 1,
	.data_rate = 1090,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 start*/
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 end*/
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 start*/
	.phy_timcon = {
		.hs_trail = 26,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 start*/
		.hs_prpr = 12,
		.lpx = 14,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 end*/
	},
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 end*/
};

static struct mtk_panel_params ext_params_50hz = {
	.pll_clk = 545,
	.vfp_low_power = 0,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 start*/
	.lcm_index = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 end*/
	.lcm_esd_check_table[0] = {
		.cmd = 0,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 1,
	.data_rate = 1090,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 start*/
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 end*/
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 start*/
	.phy_timcon = {
		.hs_trail = 26,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 start*/
		.hs_prpr = 12,
		.lpx = 14,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 end*/
	},
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 end*/
};

static struct mtk_panel_params ext_params_30hz = {
	.pll_clk = 545,
	.vfp_low_power = 0,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 start*/
	.lcm_index = 1,
/*L19A code for HQ-196851 by chenzimo at 2022/05/27 end*/
	.lcm_esd_check_table[0] = {
		.cmd = 0,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 1,
	.data_rate = 1090,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 start*/
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	/*L19A code for HQ-194730 by chenzimo at 2022/05/30 end*/
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 start*/
	.phy_timcon = {
		.hs_trail = 26,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 start*/
		.hs_prpr = 12,
		.lpx = 14,
		/*L19A code for HQ-255958 by zhangkexin at 2022/10/26 end*/
	},
	/*L19A code for HQ-194837 by chenzimo at 2022/06/09 end*/
};

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 50)
		ext->params = &ext_params_50hz;
	else if (drm_mode_vrefresh(m) == 30)
		ext->params = &ext_params_30hz;
	else
		ret = 1;
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 start*/
	temp_refresh = drm_mode_vrefresh(m);
	real_refresh = temp_refresh;
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 end*/
	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
			struct drm_connector *connector,
			struct mtk_panel_params **ext_para,
			unsigned int mode)
{
	int ret = 0;

	if (mode == 0)
		*ext_para = &ext_params;
	else if (mode == 1)
		*ext_para = &ext_params_90hz;
	else if (mode == 2)
		*ext_para = &ext_params_50hz;
	else if (mode == 3)
		*ext_para = &ext_params_30hz;
	else
		ret = 1;

	return ret;
}
#endif
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
#ifdef PROJECT_ROCK
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
#endif
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
};
#endif
/*L19A code for HQ-194275 by chenzimo at 2022/05/12 end*/

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

/*L19A code for HQ-194275 by chenzimo at 2022/05/12 start*/
static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;
#ifdef PROJECT_ROCK
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;
	struct drm_display_mode *mode4;

	if (need_fake_resolution) {
		change_drm_disp_mode_params(&default_mode);
		change_drm_disp_mode_params(&performance_mode);
		change_drm_disp_mode_params(&refresh_50_mode);
		change_drm_disp_mode_params(&refresh_30_mode);
	}
#else
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
#ifdef PROJECT_ROCK
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

	mode3 = drm_mode_duplicate(connector->dev, &refresh_50_mode);
	if (!mode3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			refresh_50_mode.hdisplay, refresh_50_mode.vdisplay,
			drm_mode_vrefresh(&refresh_50_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode3);

	mode4 = drm_mode_duplicate(connector->dev, &refresh_30_mode);
	if (!mode4) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			refresh_30_mode.hdisplay, refresh_30_mode.vdisplay,
			drm_mode_vrefresh(&refresh_30_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode4);
	mode4->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode4);
#endif

	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 152;

	return 1;
}
/*L19A code for HQ-194275 by chenzimo at 2022/05/12 end*/

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
#ifdef PROJECT_ROCK
/*L19A code for HQ-194283 by chenzimo at 22/05/06 start*/
extern ssize_t lcm_mipi_reg_write(char *buf, size_t count);
extern ssize_t lcm_mipi_reg_read(char *buf);

static ssize_t mipi_reg_show(struct device *device,
		    struct device_attribute *attr,
		   char *buf)
{
	pr_info("%s, L19A project \n", __func__);
	return lcm_mipi_reg_read(buf);
}
static ssize_t mipi_reg_store(struct device *device,
		   struct device_attribute *attr,
	   const char *buf, size_t count)
{
	int rc = 0;
	pr_info("%s, L19A project \n", __func__);
	rc = lcm_mipi_reg_write((char *)buf, count);
	return rc;
}

/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
extern int panel_event;
static ssize_t panel_event_show(struct device *device,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t ret = 0;
	struct drm_connector *connector = dev_get_drvdata(device);
	if (!connector) {
		pr_info("%s-%d connector is NULL \r\n",__func__, __LINE__);
		return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", panel_event);
}
/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/

static DEVICE_ATTR_RW(mipi_reg);
/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
static DEVICE_ATTR_RO(panel_event);
/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/

static struct attribute *ft8722_attrs[] = {
        &dev_attr_mipi_reg.attr,
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 start*/
	&dev_attr_panel_event.attr,
	/*L19A code for HQ-194827 by chenzimo at 2022/5/25 end*/
        NULL,
};

static const struct attribute_group ft8722_attr_group = {
        .attrs = ft8722_attrs,
};
/*L19A code for HQ-194283 by chenzimo at 22/05/06 end*/
#endif

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
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
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
#ifdef PROJECT_ROCK
	/*L19A code for HQ-203969 by zhangkexin at 2022/04/26 start*/
	hq_regiser_hw_info(HWID_LCM, "incell,vendor:31,IC:03");
	/*L19A code for HQ-203969 by zhangkexin at 2022/04/26 end*/
#endif

	ctx->dev = dev;
	dsi->lanes = 3;
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

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			 PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "cannot get bias-gpios 1 %ld\n",
			 PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->prepared = true;
	ctx->enabled = true;

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
	check_is_need_fake_resolution(dev);
#ifdef PROJECT_ROCK
	/*L19A code for HQ-194283 by chenzimo at 22/05/06 start*/
	ret = sysfs_create_group(&dev->kobj, &ft8722_attr_group);
	if (ret)
		return ret;
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 start*/
	device_create_file(dev, &dev_attr_mtkfb_fps);
/*L19A code for HQ-212320 by jiangyue at 2022/05/25 end*/
	/*L19A code for HQ-194283 by chenzimo at 22/05/06 end*/
#endif
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
	{ .compatible = "ft8722,jdi,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-ft8722-jdi-fhdp-dsi-vdo-90hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("jdi ft8722 VDO Panel Driver");
MODULE_LICENSE("GPL v2");

