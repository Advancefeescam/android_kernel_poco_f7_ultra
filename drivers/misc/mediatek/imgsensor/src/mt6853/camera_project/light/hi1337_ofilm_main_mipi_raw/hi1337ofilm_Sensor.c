/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 HI1337mipi_Sensor.c
 *
 * Project:
 * --------
 *	 SOP
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define PFX "HI1337_ofilm"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <asm/neon.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "hi1337ofilm_Sensor.h"


extern bool read_eeprom_hi1337_ofilm(kal_uint16 addr, BYTE * data, kal_uint32 size);

#undef VENDOR_EDIT

#define USE_BURST_MODE 1
#define ENABLE_PDAF 1
#define OFILM_VENDOR_ID 0x07
//#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */

#if USE_BURST_MODE
static kal_uint16 hi1337_table_write_cmos_sensor(
		kal_uint16 * para, kal_uint32 len);
#endif
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI1337_OFILM_MAIN_SENSOR_ID,

	.checksum_value = 0x60deb14a,
	.pre = {
		.pclk = 576000000,	 //VT CLK : 72MHz * 8 = =	576000000				//record different mode's pclk
		.linelength =  5920, //740*8		//record different mode's linelength
		.framelength = 3237, //1666, 			//record different mode's framelength
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000, //(720M*4/10)
	},
	.cap = {
		.pclk = 576000000,
		.linelength = 5920,
		.framelength = 3237,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000, //(1440M * 4 / 10 )
	},
	.normal_video = {
		.pclk = 576000000,
		.linelength = 5920,
		.framelength = 3237,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 2368,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000, //(1440M * 4 / 10 )
	},
	.hs_video = {
		.pclk = 576000000,
		.linelength = 5864,
		.framelength = 818,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 192000000, //( 480*4/10)
	},
	.slim_video = {
		.pclk = 576000000,
		.linelength = 5920,
		.framelength = 3237, // for 30fps 2020/05/29 Coby //1666,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000, //(720M*4/10)
	},
	.custom1 = {
		.pclk =576000000,
		.linelength = 5920,
		.framelength = 4045,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
		.mipi_pixel_rate = 576000000, //(1440M * 4 / 10 )
	},


	.margin = 4,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */

	.min_gain = 66,	// 66*16=1056
	.max_gain = 16 * BASEGAIN,	//16384
	/*L19 code for HQ-161200 by TianGuchen at 2022/01/04 start*/
	.min_gain_iso = 50,
	/*L19 code for HQ-161200 by TianGuchen at 2021/01/04 end*/
	.exp_step = 1,
	.gain_step = 4, // 4*16=64
	.gain_type = 3,

	.max_frame_length = 0xffffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 1,/* 1, support; 0,not support */
	.sensor_mode_num = 6,	/* support sensor mode num */

	.cap_delay_frame = 1,	/* enter capture delay frame num */
	.pre_delay_frame = 1,	/* enter preview delay frame num */
	.video_delay_frame = 1,	/* enter video delay frame num */
	.hs_video_delay_frame = 1,
	.slim_video_delay_frame = 1,	/* enter slim video delay frame num */
	.custom1_delay_frame = 1,
	.frame_time_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_8MA, //ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_CSI2,//MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x40, 0xff},
	.i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x100,	/* current shutter */
	.gain = 0xe0,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x40, /* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
	{ 4240, 3152,   8,  12, 4224, 3128,	 2112, 1564,  4,  2, 2104, 1560, 0, 0, 2104, 1560},		// preview (2104 x 1560)
	{ 4240, 3152,   8,  14, 4224, 3124,	 4224, 3124,  8,  2, 4208, 3120, 0, 0, 4208, 3120},		// capture (4208 x 3120)
	{ 4240, 3152,   8, 390, 4224, 2372,	 4224, 2372,  8,  2, 4208, 2368, 0, 0, 4208, 2368},		// VIDEO (4208 x 2368)
	{ 4240, 3152,   8, 490, 4224, 2172,	 1408,  724,  64, 2, 1280,  720, 0, 0, 1280,  720},		// hight speed video (1280 x 720)
	{ 4240, 3152,   8, 492, 4224, 2168,	 2112, 1084,  96, 2, 1920, 1080, 0, 0, 1920, 1080},     // slim video (1920 x 1080)
	{ 4240, 3152,   8,  14, 4224, 3124,	 4224, 3124,  8,  2, 4208, 3120, 0, 0, 4208, 3120},		// custom1 (4208 x 3120)
};


#if ENABLE_PDAF

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Capture mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x1070, 0x0C30,	// VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x01, 0x30, 0x0140, 0x0300,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??
	/* Video mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x1070, 0x0940,	// VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x01, 0x30, 0x0140, 0x0250,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 56,
	.i4OffsetY = 24,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 16,
	.i4SubBlkH = 8,
	.i4BlockNumX = 128,
	.i4BlockNumY = 96,
	.iMirrorFlip = 3,
	.i4PosL = {
					{60, 29}, {76, 29}, {68, 33}, {84, 33},
					{60, 45}, {76, 45}, {68, 49}, {84, 49}
				},
	.i4PosR = {
					{60, 25}, {76, 25}, {68, 37}, {84, 37},
					{60, 41}, {76, 41}, {68, 53}, {84, 53}
				},

};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_16_9 = {
	.i4OffsetX = 56,
	.i4OffsetY = 0,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 16,
	.i4SubBlkH = 8,
	.i4BlockNumX = 128,
	.i4BlockNumY = 74,
	.iMirrorFlip = 3,
	.i4PosL = {
					{60, 5}, {76, 5}, {68, 9}, {84, 9},
					{60, 21}, {76, 21}, {68, 25}, {84, 25}
				},
	.i4PosR = {
					{60, 1}, {76, 1}, {68, 13}, {84, 13},
					{60, 17}, {76, 17}, {68, 29}, {84, 29}
				},
	.i4Crop = { {0, 0}, {0, 0}, {0, 376}, {0,0}, {0, 0},{0, 0}, {0, 0}, {0, 0},{0, 0}, {0, 0} },

};

#endif

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
			     (char)(para >> 8), (char)(para & 0xFF)};

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static kal_uint16 read_eeprom_vendor_id(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1,0xA2); //eeprom slave addr 
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
			(char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	pr_err("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x020e, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0206, imgsensor.line_length / 8);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x0716) << 8) | read_cmos_sensor_8(0x0717));

}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

	pr_err("framerate = %d, min framelength should enable %d\n", framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */

			write_cmos_sensor(0x020e, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length*/

		write_cmos_sensor(0x020e, imgsensor.frame_length);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x020D, (shutter & 0xFF0000) >> 16);
	write_cmos_sensor(0x020A, shutter);

	pr_err("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);

}	/*	write_shutter  */

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
} /* set_shutter */


/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint16 shutter,
				     kal_uint16 frame_length,
				     kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x020e, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length */
			write_cmos_sensor(0x020e, imgsensor.frame_length);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x020D, (shutter & 0xFF0000) >> 16);
	write_cmos_sensor(0x020A, shutter);

    pr_err("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0000;
    reg_gain = gain / 4 - 16;

    return (kal_uint16)reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain, max_gain = 16 * BASEGAIN;

	if (gain < BASEGAIN || gain > max_gain) {
		pr_err("Error max gain setting: %d\n", max_gain);

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_err("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ",
		gain, reg_gain, max_gain);

	reg_gain = reg_gain & 0x00FF;
	write_cmos_sensor_8(0x0213, reg_gain);

	return gain;
} /* set_gain */

/*
static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_err("image_mirror = %d", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0000, 0x0000);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0000, 0x0100);

		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0000, 0x0200);

		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0000, 0x0300);

		break;
	default:
		pr_err("Error image_mirror setting");
		break;
	}

}
*/
static kal_uint32 streaming_control(kal_bool enable)
{
	pr_err("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0b00, 0x0100); // stream on
	else
		write_cmos_sensor(0x0b00, 0x0000); // stream off

	mdelay(10);
	return ERROR_NONE;
}

#if USE_BURST_MODE
#define I2C_BUFFER_LEN 1020 /* trans# max is 255, each 3 bytes */
static kal_uint16 hi1337_table_write_cmos_sensor(kal_uint16 *para,
						 kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;
	int ret = 0;
	int retry_cnt = 0;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 4
			|| IDX == len || addr != addr_last) {
			ret = iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						4,
						imgsensor_info.i2c_speed);

			if (ret < 0) {
				while (ret < 0) {
					ret = iBurstWriteReg_multi(puSendCmd,
							tosend,
							imgsensor.i2c_write_id,
							4,
							imgsensor_info.i2c_speed);
					retry_cnt++;

					if (retry_cnt > 3) {
						retry_cnt = 0 ;
						break;
					}
				}
			}

			tosend = 0;
		}
	}
	return 0;
}
#endif


static kal_uint16 hi1337_init_setting[] = {
// Hi1337_0.1.0.40_1440Mbps_20200924.tsf
0x0790, 0x0100,
0x2000, 0x0000,
0x2002, 0x0058,
0x2006, 0x40B2,
0x2008, 0xB062,
0x200A, 0x8436,
0x200C, 0x40B2,
0x200E, 0xB08C,
0x2010, 0x8446,
0x2012, 0x40B2,
0x2014, 0xB0B2,
0x2016, 0x8450,
0x2018, 0x40B2,
0x201A, 0xB0D0,
0x201C, 0x84C6,
0x201E, 0x40B2,
0x2020, 0xB11C,
0x2022, 0x8470,
0x2024, 0x40B2,
0x2026, 0xB142,
0x2028, 0x84B4,
0x202A, 0x40B2,
0x202C, 0xB17E,
0x202E, 0x84B0,
0x2030, 0x40B2,
0x2032, 0xB1AC,
0x2034, 0x84B8,
0x2036, 0x40B2,
0x2038, 0xB1E2,
0x203A, 0x847C,
0x203C, 0x40B2,
0x203E, 0xB450,
0x2040, 0x8478,
0x2042, 0x40B2,
0x2044, 0xB4E4,
0x2046, 0x8476,
0x2048, 0x40B2,
0x204A, 0xB560,
0x204C, 0x847E,
0x204E, 0x40B2,
0x2050, 0xB670,
0x2052, 0x843A,
0x2054, 0x40B2,
0x2056, 0xB852,
0x2058, 0x845C,
0x205A, 0x40B2,
0x205C, 0xB882,
0x205E, 0x845E,
0x2060, 0x4130,
0x2062, 0x4292,
0x2064, 0x0C34,
0x2066, 0x0202,
0x2068, 0x1292,
0x206A, 0xD006,
0x206C, 0x93C2,
0x206E, 0x86DC,
0x2070, 0x2403,
0x2072, 0x407F,
0x2074, 0x0003,
0x2076, 0x3C01,
0x2078, 0x434F,
0x207A, 0x4FC2,
0x207C, 0x023D,
0x207E, 0x425F,
0x2080, 0x86DC,
0x2082, 0x503F,
0x2084, 0x0C80,
0x2086, 0x4F82,
0x2088, 0x752A,
0x208A, 0x4130,
0x208C, 0x1292,
0x208E, 0xD016,
0x2090, 0xB3D2,
0x2092, 0x0B00,
0x2094, 0x2002,
0x2096, 0xD2E2,
0x2098, 0x0381,
0x209A, 0x93C2,
0x209C, 0x0263,
0x209E, 0x2001,
0x20A0, 0x4130,
0x20A2, 0x422D,
0x20A4, 0x403E,
0x20A6, 0x879E,
0x20A8, 0x403F,
0x20AA, 0x192A,
0x20AC, 0x1292,
0x20AE, 0x843E,
0x20B0, 0x3FF7,
0x20B2, 0xB3D2,
0x20B4, 0x0267,
0x20B6, 0x2403,
0x20B8, 0xD0F2,
0x20BA, 0x0040,
0x20BC, 0x0381,
0x20BE, 0x90F2,
0x20C0, 0x0010,
0x20C2, 0x0260,
0x20C4, 0x2002,
0x20C6, 0x1292,
0x20C8, 0x84BC,
0x20CA, 0x1292,
0x20CC, 0xD020,
0x20CE, 0x4130,
0x20D0, 0x1292,
0x20D2, 0x8470,
0x20D4, 0x1292,
0x20D6, 0x8452,
0x20D8, 0x0900,
0x20DA, 0x7118,
0x20DC, 0x1292,
0x20DE, 0x848E,
0x20E0, 0x0900,
0x20E2, 0x7112,
0x20E4, 0x0800,
0x20E6, 0x7A20,
0x20E8, 0x4292,
0x20EA, 0x86EE,
0x20EC, 0x7334,
0x20EE, 0x0F00,
0x20F0, 0x7304,
0x20F2, 0x421F,
0x20F4, 0x8620,
0x20F6, 0x1292,
0x20F8, 0x846E,
0x20FA, 0x1292,
0x20FC, 0x8488,
0x20FE, 0x0B00,
0x2100, 0x7114,
0x2102, 0x0002,
0x2104, 0x1292,
0x2106, 0x848C,
0x2108, 0x1292,
0x210A, 0x8454,
0x210C, 0x43C2,
0x210E, 0x85F6,
0x2110, 0x4292,
0x2112, 0x0C34,
0x2114, 0x0202,
0x2116, 0x1292,
0x2118, 0x8444,
0x211A, 0x4130,
0x211C, 0x4392,
0x211E, 0x7360,
0x2120, 0xB3D2,
0x2122, 0x0B00,
0x2124, 0x2402,
0x2126, 0xC2E2,
0x2128, 0x0381,
0x212A, 0x0900,
0x212C, 0x732C,
0x212E, 0x4382,
0x2130, 0x7360,
0x2132, 0x422D,
0x2134, 0x403E,
0x2136, 0x8700,
0x2138, 0x403F,
0x213A, 0x86F8,
0x213C, 0x1292,
0x213E, 0x843E,
0x2140, 0x4130,
0x2142, 0x4F0C,
0x2144, 0x403F,
0x2146, 0x0267,
0x2148, 0xF0FF,
0x214A, 0xFFDF,
0x214C, 0x0000,
0x214E, 0xF0FF,
0x2150, 0xFFEF,
0x2152, 0x0000,
0x2154, 0x421D,
0x2156, 0x84B0,
0x2158, 0x403E,
0x215A, 0x06F9,
0x215C, 0x4C0F,
0x215E, 0x1292,
0x2160, 0x84AC,
0x2162, 0x4F4E,
0x2164, 0xB31E,
0x2166, 0x2403,
0x2168, 0xD0F2,
0x216A, 0x0020,
0x216C, 0x0267,
0x216E, 0xB32E,
0x2170, 0x2403,
0x2172, 0xD0F2,
0x2174, 0x0010,
0x2176, 0x0267,
0x2178, 0xC3E2,
0x217A, 0x0267,
0x217C, 0x4130,
0x217E, 0x120B,
0x2180, 0x120A,
0x2182, 0x403A,
0x2184, 0x1140,
0x2186, 0x1292,
0x2188, 0xD080,
0x218A, 0x430B,
0x218C, 0x4A0F,
0x218E, 0x532A,
0x2190, 0x1292,
0x2192, 0x84A4,
0x2194, 0x4F0E,
0x2196, 0x430F,
0x2198, 0x5E82,
0x219A, 0x870C,
0x219C, 0x6F82,
0x219E, 0x870E,
0x21A0, 0x531B,
0x21A2, 0x923B,
0x21A4, 0x2BF3,
0x21A6, 0x413A,
0x21A8, 0x413B,
0x21AA, 0x4130,
0x21AC, 0xF0F2,
0x21AE, 0x007F,
0x21B0, 0x0267,
0x21B2, 0x421D,
0x21B4, 0x84B6,
0x21B6, 0x403E,
0x21B8, 0x01F9,
0x21BA, 0x1292,
0x21BC, 0x84AC,
0x21BE, 0x4F4E,
0x21C0, 0xF35F,
0x21C2, 0x2403,
0x21C4, 0xD0F2,
0x21C6, 0xFF80,
0x21C8, 0x0267,
0x21CA, 0xB36E,
0x21CC, 0x2404,
0x21CE, 0xD0F2,
0x21D0, 0x0040,
0x21D2, 0x0267,
0x21D4, 0x3C03,
0x21D6, 0xF0F2,
0x21D8, 0xFFBF,
0x21DA, 0x0267,
0x21DC, 0xC2E2,
0x21DE, 0x0267,
0x21E0, 0x4130,
0x21E2, 0x120B,
0x21E4, 0x120A,
0x21E6, 0x8231,
0x21E8, 0x430B,
0x21EA, 0x93C2,
0x21EC, 0x0C0A,
0x21EE, 0x2404,
0x21F0, 0xB3D2,
0x21F2, 0x0B05,
0x21F4, 0x2401,
0x21F6, 0x431B,
0x21F8, 0x422D,
0x21FA, 0x403E,
0x21FC, 0x192A,
0x21FE, 0x403F,
0x2200, 0x879E,
0x2202, 0x1292,
0x2204, 0x843E,
0x2206, 0x930B,
0x2208, 0x20F4,
0x220A, 0x93E2,
0x220C, 0x0241,
0x220E, 0x24EB,
0x2210, 0x403A,
0x2212, 0x0292,
0x2214, 0x4AA2,
0x2216, 0x0A00,
0x2218, 0xB2E2,
0x221A, 0x0361,
0x221C, 0x2405,
0x221E, 0x4A2F,
0x2220, 0x1292,
0x2222, 0x8474,
0x2224, 0x4F82,
0x2226, 0x0A1C,
0x2228, 0x93C2,
0x222A, 0x0360,
0x222C, 0x34CD,
0x222E, 0x430C,
0x2230, 0x4C0F,
0x2232, 0x5F0F,
0x2234, 0x4F0D,
0x2236, 0x510D,
0x2238, 0x4F0E,
0x223A, 0x5A0E,
0x223C, 0x4E1E,
0x223E, 0x0002,
0x2240, 0x4F1F,
0x2242, 0x192A,
0x2244, 0x1202,
0x2246, 0xC232,
0x2248, 0x4303,
0x224A, 0x4E82,
0x224C, 0x0130,
0x224E, 0x4F82,
0x2250, 0x0138,
0x2252, 0x421E,
0x2254, 0x013A,
0x2256, 0x421F,
0x2258, 0x013C,
0x225A, 0x4132,
0x225C, 0x108E,
0x225E, 0x108F,
0x2260, 0xEF4E,
0x2262, 0xEF0E,
0x2264, 0xF37F,
0x2266, 0xC312,
0x2268, 0x100F,
0x226A, 0x100E,
0x226C, 0x4E8D,
0x226E, 0x0000,
0x2270, 0x531C,
0x2272, 0x922C,
0x2274, 0x2BDD,
0x2276, 0xB3D2,
0x2278, 0x1921,
0x227A, 0x2403,
0x227C, 0x410F,
0x227E, 0x1292,
0x2280, 0x847E,
0x2282, 0x403B,
0x2284, 0x843E,
0x2286, 0x422D,
0x2288, 0x410E,
0x228A, 0x403F,
0x228C, 0x1908,
0x228E, 0x12AB,
0x2290, 0x403D,
0x2292, 0x0005,
0x2294, 0x403E,
0x2296, 0x0292,
0x2298, 0x403F,
0x229A, 0x85EC,
0x229C, 0x12AB,
0x229E, 0x421F,
0x22A0, 0x060E,
0x22A2, 0x9F82,
0x22A4, 0x8628,
0x22A6, 0x288D,
0x22A8, 0x9382,
0x22AA, 0x060E,
0x22AC, 0x248A,
0x22AE, 0x90BA,
0x22B0, 0x0010,
0x22B2, 0x0000,
0x22B4, 0x2C0B,
0x22B6, 0x93C2,
0x22B8, 0x85F6,
0x22BA, 0x2008,
0x22BC, 0x403F,
0x22BE, 0x06A7,
0x22C0, 0xD0FF,
0x22C2, 0x0007,
0x22C4, 0x0000,
0x22C6, 0xF0FF,
0x22C8, 0xFFF8,
0x22CA, 0x0000,
0x22CC, 0x4392,
0x22CE, 0x8628,
0x22D0, 0x403F,
0x22D2, 0x06A7,
0x22D4, 0xD2EF,
0x22D6, 0x0000,
0x22D8, 0xC2EF,
0x22DA, 0x0000,
0x22DC, 0x93C2,
0x22DE, 0x86E3,
0x22E0, 0x2068,
0x22E2, 0xB0F2,
0x22E4, 0x0040,
0x22E6, 0x0B05,
0x22E8, 0x2461,
0x22EA, 0xD3D2,
0x22EC, 0x0410,
0x22EE, 0xB3E2,
0x22F0, 0x0381,
0x22F2, 0x2089,
0x22F4, 0x90B2,
0x22F6, 0x0030,
0x22F8, 0x0A00,
0x22FA, 0x2C52,
0x22FC, 0x93C2,
0x22FE, 0x85F6,
0x2300, 0x204F,
0x2302, 0x430E,
0x2304, 0x430C,
0x2306, 0x4C0F,
0x2308, 0x5F0F,
0x230A, 0x5F0F,
0x230C, 0x5F0F,
0x230E, 0x4F1F,
0x2310, 0x8570,
0x2312, 0xF03F,
0x2314, 0x07FF,
0x2316, 0x903F,
0x2318, 0x0400,
0x231A, 0x343E,
0x231C, 0x5F0E,
0x231E, 0x531C,
0x2320, 0x923C,
0x2322, 0x2BF1,
0x2324, 0x4E0F,
0x2326, 0x930E,
0x2328, 0x3834,
0x232A, 0x110F,
0x232C, 0x110F,
0x232E, 0x110F,
0x2330, 0x9382,
0x2332, 0x85F6,
0x2334, 0x2023,
0x2336, 0x5F82,
0x2338, 0x86E6,
0x233A, 0x403B,
0x233C, 0x86E6,
0x233E, 0x4B2F,
0x2340, 0x12B0,
0x2342, 0xB41C,
0x2344, 0x4F8B,
0x2346, 0x0000,
0x2348, 0x430C,
0x234A, 0x4C0D,
0x234C, 0x5D0D,
0x234E, 0x5D0D,
0x2350, 0x5D0D,
0x2352, 0x403A,
0x2354, 0x86E8,
0x2356, 0x421B,
0x2358, 0x86E6,
0x235A, 0x4B0F,
0x235C, 0x8A2F,
0x235E, 0x4F0E,
0x2360, 0x4E0F,
0x2362, 0x5F0F,
0x2364, 0x7F0F,
0x2366, 0xE33F,
0x2368, 0x8E8D,
0x236A, 0x8570,
0x236C, 0x7F8D,
0x236E, 0x8572,
0x2370, 0x531C,
0x2372, 0x923C,
0x2374, 0x2BEA,
0x2376, 0x4B8A,
0x2378, 0x0000,
0x237A, 0x3C45,
0x237C, 0x9382,
0x237E, 0x85F8,
0x2380, 0x2005,
0x2382, 0x4382,
0x2384, 0x86E6,
0x2386, 0x4382,
0x2388, 0x86E8,
0x238A, 0x3FD7,
0x238C, 0x4F82,
0x238E, 0x86E6,
0x2390, 0x3FD4,
0x2392, 0x503F,
0x2394, 0x0007,
0x2396, 0x3FC9,
0x2398, 0x5F0E,
0x239A, 0x503E,
0x239C, 0xF800,
0x239E, 0x3FBF,
0x23A0, 0x430F,
0x23A2, 0x12B0,
0x23A4, 0xB41C,
0x23A6, 0x4382,
0x23A8, 0x86E6,
0x23AA, 0x3C2D,
0x23AC, 0xC3D2,
0x23AE, 0x0410,
0x23B0, 0x3F9E,
0x23B2, 0x430D,
0x23B4, 0x403E,
0x23B6, 0x0050,
0x23B8, 0x403F,
0x23BA, 0x84D0,
0x23BC, 0x1292,
0x23BE, 0x844E,
0x23C0, 0x3F90,
0x23C2, 0x5392,
0x23C4, 0x8628,
0x23C6, 0x3F84,
0x23C8, 0x403B,
0x23CA, 0x843E,
0x23CC, 0x4A0F,
0x23CE, 0x532F,
0x23D0, 0x422D,
0x23D2, 0x4F0E,
0x23D4, 0x403F,
0x23D6, 0x0E08,
0x23D8, 0x12AB,
0x23DA, 0x422D,
0x23DC, 0x403E,
0x23DE, 0x192A,
0x23E0, 0x410F,
0x23E2, 0x12AB,
0x23E4, 0x3F48,
0x23E6, 0x93C2,
0x23E8, 0x85F6,
0x23EA, 0x2312,
0x23EC, 0x403A,
0x23EE, 0x85EC,
0x23F0, 0x3F11,
0x23F2, 0x403D,
0x23F4, 0x0200,
0x23F6, 0x422E,
0x23F8, 0x403F,
0x23FA, 0x192A,
0x23FC, 0x1292,
0x23FE, 0x844E,
0x2400, 0xC3D2,
0x2402, 0x1921,
0x2404, 0x3F02,
0x2406, 0x422D,
0x2408, 0x403E,
0x240A, 0x879E,
0x240C, 0x403F,
0x240E, 0x192A,
0x2410, 0x1292,
0x2412, 0x843E,
0x2414, 0x5231,
0x2416, 0x413A,
0x2418, 0x413B,
0x241A, 0x4130,
0x241C, 0x4382,
0x241E, 0x052C,
0x2420, 0x4F0D,
0x2422, 0x930D,
0x2424, 0x3402,
0x2426, 0xE33D,
0x2428, 0x531D,
0x242A, 0xF03D,
0x242C, 0x07F0,
0x242E, 0x4D0E,
0x2430, 0xC312,
0x2432, 0x100E,
0x2434, 0x110E,
0x2436, 0x110E,
0x2438, 0x110E,
0x243A, 0x930F,
0x243C, 0x3803,
0x243E, 0x4EC2,
0x2440, 0x052C,
0x2442, 0x3C04,
0x2444, 0x4EC2,
0x2446, 0x052D,
0x2448, 0xE33D,
0x244A, 0x531D,
0x244C, 0x4D0F,
0x244E, 0x4130,
0x2450, 0x120B,
0x2452, 0x120A,
0x2454, 0x93C2,
0x2456, 0x85F6,
0x2458, 0x2003,
0x245A, 0xB3D2,
0x245C, 0x0360,
0x245E, 0x2402,
0x2460, 0x1292,
0x2462, 0x847A,
0x2464, 0x1292,
0x2466, 0x847C,
0x2468, 0x93C2,
0x246A, 0x0600,
0x246C, 0x3803,
0x246E, 0x93C2,
0x2470, 0x0604,
0x2472, 0x3832,
0x2474, 0xD2F2,
0x2476, 0x0F01,
0x2478, 0xB3D2,
0x247A, 0x0363,
0x247C, 0x2418,
0x247E, 0x421F,
0x2480, 0x1246,
0x2482, 0x4F0E,
0x2484, 0x430F,
0x2486, 0x421B,
0x2488, 0x1244,
0x248A, 0x430A,
0x248C, 0xDA0E,
0x248E, 0xDB0F,
0x2490, 0x821E,
0x2492, 0x86F4,
0x2494, 0x721F,
0x2496, 0x86F6,
0x2498, 0x2C1B,
0x249A, 0x421F,
0x249C, 0x1240,
0x249E, 0xF03F,
0x24A0, 0x01FF,
0x24A2, 0x9F82,
0x24A4, 0x0A00,
0x24A6, 0x2814,
0x24A8, 0xD0F2,
0x24AA, 0xFF80,
0x24AC, 0x1240,
0x24AE, 0x93C2,
0x24B0, 0x85F6,
0x24B2, 0x2015,
0x24B4, 0xB0F2,
0x24B6, 0x0020,
0x24B8, 0x0381,
0x24BA, 0x2407,
0x24BC, 0x9292,
0x24BE, 0x862A,
0x24C0, 0x0384,
0x24C2, 0x2C03,
0x24C4, 0xD3D2,
0x24C6, 0x0649,
0x24C8, 0x3C0A,
0x24CA, 0xC3D2,
0x24CC, 0x0649,
0x24CE, 0x3C07,
0x24D0, 0xF0F2,
0x24D2, 0x007F,
0x24D4, 0x1240,
0x24D6, 0x3FEB,
0x24D8, 0xC2F2,
0x24DA, 0x0F01,
0x24DC, 0x3FCD,
0x24DE, 0x413A,
0x24E0, 0x413B,
0x24E2, 0x4130,
0x24E4, 0x425F,
0x24E6, 0x86E2,
0x24E8, 0xD25F,
0x24EA, 0x86E1,
0x24EC, 0x4F4E,
0x24EE, 0x5E0E,
0x24F0, 0x425F,
0x24F2, 0x0204,
0x24F4, 0xF07F,
0x24F6, 0x0003,
0x24F8, 0xF37F,
0x24FA, 0xDF0E,
0x24FC, 0x40B2,
0x24FE, 0x8030,
0x2500, 0x7A00,
0x2502, 0x40B2,
0x2504, 0x0100,
0x2506, 0x7A02,
0x2508, 0x40B2,
0x250A, 0x0D04,
0x250C, 0x7A0C,
0x250E, 0x40B2,
0x2510, 0xFFF0,
0x2512, 0x7A04,
0x2514, 0x93C2,
0x2516, 0x86E0,
0x2518, 0x240A,
0x251A, 0x40B2,
0x251C, 0xFFF1,
0x251E, 0x7A06,
0x2520, 0x40B2,
0x2522, 0xFFF4,
0x2524, 0x7A08,
0x2526, 0x40B2,
0x2528, 0xFFF5,
0x252A, 0x7A0A,
0x252C, 0x3C09,
0x252E, 0x40B2,
0x2530, 0xFFF2,
0x2532, 0x7A06,
0x2534, 0x40B2,
0x2536, 0xFFF4,
0x2538, 0x7A08,
0x253A, 0x40B2,
0x253C, 0xFFF6,
0x253E, 0x7A0A,
0x2540, 0xF03E,
0x2542, 0x0003,
0x2544, 0x5E0E,
0x2546, 0x425F,
0x2548, 0x86E2,
0x254A, 0xD25F,
0x254C, 0x86E1,
0x254E, 0xF31F,
0x2550, 0x5F0F,
0x2552, 0x5F0F,
0x2554, 0x5F0F,
0x2556, 0xD31E,
0x2558, 0xDF0E,
0x255A, 0x4E82,
0x255C, 0x7A12,
0x255E, 0x4130,
0x2560, 0x120B,
0x2562, 0x120A,
0x2564, 0x1209,
0x2566, 0x1208,
0x2568, 0x1207,
0x256A, 0x1206,
0x256C, 0x1205,
0x256E, 0x1204,
0x2570, 0x8231,
0x2572, 0x4F81,
0x2574, 0x0000,
0x2576, 0x4381,
0x2578, 0x0002,
0x257A, 0x4304,
0x257C, 0x411C,
0x257E, 0x0002,
0x2580, 0x5C0C,
0x2582, 0x4C0F,
0x2584, 0x5F0F,
0x2586, 0x5F0F,
0x2588, 0x5F0F,
0x258A, 0x5F0F,
0x258C, 0x5F0F,
0x258E, 0x503F,
0x2590, 0x1980,
0x2592, 0x440D,
0x2594, 0x5D0D,
0x2596, 0x4D0E,
0x2598, 0x5F0E,
0x259A, 0x4E2E,
0x259C, 0x4D05,
0x259E, 0x5505,
0x25A0, 0x5F05,
0x25A2, 0x4516,
0x25A4, 0x0008,
0x25A6, 0x4517,
0x25A8, 0x000A,
0x25AA, 0x460A,
0x25AC, 0x470B,
0x25AE, 0xF30A,
0x25B0, 0xF32B,
0x25B2, 0x4A81,
0x25B4, 0x0004,
0x25B6, 0x4B81,
0x25B8, 0x0006,
0x25BA, 0xB03E,
0x25BC, 0x2000,
0x25BE, 0x2404,
0x25C0, 0xF03E,
0x25C2, 0x1FFF,
0x25C4, 0xE33E,
0x25C6, 0x531E,
0x25C8, 0xF317,
0x25CA, 0x503E,
0x25CC, 0x2000,
0x25CE, 0x4E0F,
0x25D0, 0x5F0F,
0x25D2, 0x7F0F,
0x25D4, 0xE33F,
0x25D6, 0x512C,
0x25D8, 0x4C28,
0x25DA, 0x4309,
0x25DC, 0x4E0A,
0x25DE, 0x4F0B,
0x25E0, 0x480C,
0x25E2, 0x490D,
0x25E4, 0x1202,
0x25E6, 0xC232,
0x25E8, 0x12B0,
0x25EA, 0xFFC0,
0x25EC, 0x4132,
0x25EE, 0x108E,
0x25F0, 0x108F,
0x25F2, 0xEF4E,
0x25F4, 0xEF0E,
0x25F6, 0xF37F,
0x25F8, 0xC312,
0x25FA, 0x100F,
0x25FC, 0x100E,
0x25FE, 0x4E85,
0x2600, 0x0018,
0x2602, 0x4F85,
0x2604, 0x001A,
0x2606, 0x480A,
0x2608, 0x490B,
0x260A, 0x460C,
0x260C, 0x470D,
0x260E, 0x1202,
0x2610, 0xC232,
0x2612, 0x12B0,
0x2614, 0xFFC0,
0x2616, 0x4132,
0x2618, 0x4E0C,
0x261A, 0x4F0D,
0x261C, 0x108C,
0x261E, 0x108D,
0x2620, 0xED4C,
0x2622, 0xED0C,
0x2624, 0xF37D,
0x2626, 0xC312,
0x2628, 0x100D,
0x262A, 0x100C,
0x262C, 0x411E,
0x262E, 0x0004,
0x2630, 0x411F,
0x2632, 0x0006,
0x2634, 0x5E0E,
0x2636, 0x6F0F,
0x2638, 0x5E0E,
0x263A, 0x6F0F,
0x263C, 0x5E0E,
0x263E, 0x6F0F,
0x2640, 0xDE0C,
0x2642, 0xDF0D,
0x2644, 0x4C85,
0x2646, 0x002C,
0x2648, 0x4D85,
0x264A, 0x002E,
0x264C, 0x5314,
0x264E, 0x9224,
0x2650, 0x2B95,
0x2652, 0x5391,
0x2654, 0x0002,
0x2656, 0x92A1,
0x2658, 0x0002,
0x265A, 0x2B8F,
0x265C, 0x5231,
0x265E, 0x4134,
0x2660, 0x4135,
0x2662, 0x4136,
0x2664, 0x4137,
0x2666, 0x4138,
0x2668, 0x4139,
0x266A, 0x413A,
0x266C, 0x413B,
0x266E, 0x4130,
0x2670, 0x120B,
0x2672, 0x120A,
0x2674, 0x1209,
0x2676, 0x8031,
0x2678, 0x000C,
0x267A, 0x425F,
0x267C, 0x0205,
0x267E, 0xC312,
0x2680, 0x104F,
0x2682, 0x114F,
0x2684, 0x114F,
0x2686, 0x114F,
0x2688, 0x114F,
0x268A, 0x114F,
0x268C, 0xF37F,
0x268E, 0x4F0B,
0x2690, 0xF31B,
0x2692, 0x5B0B,
0x2694, 0x5B0B,
0x2696, 0x5B0B,
0x2698, 0x503B,
0x269A, 0xD196,
0x269C, 0x4219,
0x269E, 0x0508,
0x26A0, 0xF039,
0x26A2, 0x2000,
0x26A4, 0x4F0A,
0x26A6, 0xC312,
0x26A8, 0x100A,
0x26AA, 0xE31A,
0x26AC, 0x421F,
0x26AE, 0x86EE,
0x26B0, 0x503F,
0x26B2, 0xFF60,
0x26B4, 0x903F,
0x26B6, 0x00C8,
0x26B8, 0x2C02,
0x26BA, 0x403F,
0x26BC, 0x00C8,
0x26BE, 0x4F82,
0x26C0, 0x7322,
0x26C2, 0xB3D2,
0x26C4, 0x0381,
0x26C6, 0x2009,
0x26C8, 0x421F,
0x26CA, 0x85F8,
0x26CC, 0xD21F,
0x26CE, 0x85F6,
0x26D0, 0x930F,
0x26D2, 0x24B1,
0x26D4, 0x40F2,
0x26D6, 0xFF80,
0x26D8, 0x0619,
0x26DA, 0x1292,
0x26DC, 0xD00A,
0x26DE, 0x430D,
0x26E0, 0x93C2,
0x26E2, 0x86E0,
0x26E4, 0x2003,
0x26E6, 0xB2F2,
0x26E8, 0x0360,
0x26EA, 0x2001,
0x26EC, 0x431D,
0x26EE, 0x425F,
0x26F0, 0x86E3,
0x26F2, 0xD25F,
0x26F4, 0x86E2,
0x26F6, 0xF37F,
0x26F8, 0x5F0F,
0x26FA, 0x425E,
0x26FC, 0x86DD,
0x26FE, 0xDE0F,
0x2700, 0x5F0F,
0x2702, 0x5B0F,
0x2704, 0x4FA2,
0x2706, 0x0402,
0x2708, 0x930D,
0x270A, 0x2007,
0x270C, 0x930A,
0x270E, 0x248E,
0x2710, 0x4F5F,
0x2712, 0x0001,
0x2714, 0xF37F,
0x2716, 0x4FC2,
0x2718, 0x0403,
0x271A, 0x93C2,
0x271C, 0x86DD,
0x271E, 0x2483,
0x2720, 0xC2F2,
0x2722, 0x0400,
0x2724, 0xB2E2,
0x2726, 0x0265,
0x2728, 0x2407,
0x272A, 0x421F,
0x272C, 0x0508,
0x272E, 0xF03F,
0x2730, 0xFFDF,
0x2732, 0xD90F,
0x2734, 0x4F82,
0x2736, 0x0508,
0x2738, 0xB3D2,
0x273A, 0x0383,
0x273C, 0x2484,
0x273E, 0x403F,
0x2740, 0x0508,
0x2742, 0x4FB1,
0x2744, 0x0000,
0x2746, 0x4FB1,
0x2748, 0x0002,
0x274A, 0x4FB1,
0x274C, 0x0004,
0x274E, 0x403F,
0x2750, 0x0500,
0x2752, 0x4FB1,
0x2754, 0x0006,
0x2756, 0x4FB1,
0x2758, 0x0008,
0x275A, 0x4FB1,
0x275C, 0x000A,
0x275E, 0xB3E2,
0x2760, 0x0383,
0x2762, 0x2412,
0x2764, 0xC2E1,
0x2766, 0x0002,
0x2768, 0xB2E2,
0x276A, 0x0383,
0x276C, 0x434F,
0x276E, 0x634F,
0x2770, 0xF37F,
0x2772, 0x4F4E,
0x2774, 0x114E,
0x2776, 0x434E,
0x2778, 0x104E,
0x277A, 0x415F,
0x277C, 0x0007,
0x277E, 0xF07F,
0x2780, 0x007F,
0x2782, 0xDE4F,
0x2784, 0x4FC1,
0x2786, 0x0007,
0x2788, 0xB2F2,
0x278A, 0x0383,
0x278C, 0x2415,
0x278E, 0xF0F1,
0x2790, 0xFFBF,
0x2792, 0x0000,
0x2794, 0xB0F2,
0x2796, 0x0010,
0x2798, 0x0383,
0x279A, 0x434E,
0x279C, 0x634E,
0x279E, 0x5E4E,
0x27A0, 0x5E4E,
0x27A2, 0x5E4E,
0x27A4, 0x5E4E,
0x27A6, 0x5E4E,
0x27A8, 0x5E4E,
0x27AA, 0x415F,
0x27AC, 0x0006,
0x27AE, 0xF07F,
0x27B0, 0xFFBF,
0x27B2, 0xDE4F,
0x27B4, 0x4FC1,
0x27B6, 0x0006,
0x27B8, 0xB0F2,
0x27BA, 0x0020,
0x27BC, 0x0383,
0x27BE, 0x2410,
0x27C0, 0xF0F1,
0x27C2, 0xFFDF,
0x27C4, 0x0002,
0x27C6, 0xB0F2,
0x27C8, 0x0040,
0x27CA, 0x0383,
0x27CC, 0x434E,
0x27CE, 0x634E,
0x27D0, 0x5E4E,
0x27D2, 0x5E4E,
0x27D4, 0x415F,
0x27D6, 0x0008,
0x27D8, 0xC26F,
0x27DA, 0xDE4F,
0x27DC, 0x4FC1,
0x27DE, 0x0008,
0x27E0, 0x93C2,
0x27E2, 0x0383,
0x27E4, 0x3412,
0x27E6, 0xF0F1,
0x27E8, 0xFFDF,
0x27EA, 0x0000,
0x27EC, 0x425E,
0x27EE, 0x0382,
0x27F0, 0xF35E,
0x27F2, 0x5E4E,
0x27F4, 0x5E4E,
0x27F6, 0x5E4E,
0x27F8, 0x5E4E,
0x27FA, 0x5E4E,
0x27FC, 0x415F,
0x27FE, 0x0006,
0x2800, 0xF07F,
0x2802, 0xFFDF,
0x2804, 0xDE4F,
0x2806, 0x4FC1,
0x2808, 0x0006,
0x280A, 0x410F,
0x280C, 0x4FB2,
0x280E, 0x0508,
0x2810, 0x4FB2,
0x2812, 0x050A,
0x2814, 0x4FB2,
0x2816, 0x050C,
0x2818, 0x4FB2,
0x281A, 0x0500,
0x281C, 0x4FB2,
0x281E, 0x0502,
0x2820, 0x4FB2,
0x2822, 0x0504,
0x2824, 0x3C10,
0x2826, 0xD2F2,
0x2828, 0x0400,
0x282A, 0x3F7C,
0x282C, 0x4F6F,
0x282E, 0xF37F,
0x2830, 0x4FC2,
0x2832, 0x0402,
0x2834, 0x3F72,
0x2836, 0x90F2,
0x2838, 0x0011,
0x283A, 0x0619,
0x283C, 0x2B4E,
0x283E, 0x50F2,
0x2840, 0xFFF0,
0x2842, 0x0619,
0x2844, 0x3F4A,
0x2846, 0x5031,
0x2848, 0x000C,
0x284A, 0x4139,
0x284C, 0x413A,
0x284E, 0x413B,
0x2850, 0x4130,
0x2852, 0x0900,
0x2854, 0x7312,
0x2856, 0x421F,
0x2858, 0x0A08,
0x285A, 0xF03F,
0x285C, 0xF7FF,
0x285E, 0x4F82,
0x2860, 0x0A88,
0x2862, 0x0900,
0x2864, 0x7312,
0x2866, 0x421F,
0x2868, 0x0A0E,
0x286A, 0xF03F,
0x286C, 0x7FFF,
0x286E, 0x4F82,
0x2870, 0x0A8E,
0x2872, 0x0900,
0x2874, 0x7312,
0x2876, 0x421F,
0x2878, 0x0A1E,
0x287A, 0xC31F,
0x287C, 0x4F82,
0x287E, 0x0A9E,
0x2880, 0x4130,
0x2882, 0x4292,
0x2884, 0x0A08,
0x2886, 0x0A88,
0x2888, 0x0900,
0x288A, 0x7312,
0x288C, 0x4292,
0x288E, 0x0A0E,
0x2890, 0x0A8E,
0x2892, 0x0900,
0x2894, 0x7312,
0x2896, 0x4292,
0x2898, 0x0A1E,
0x289A, 0x0A9E,
0x289C, 0x4130,
0x289E, 0x7400,
0x28A0, 0x8058,
0x28A2, 0x1807,
0x28A4, 0x00E0,
0x28A6, 0x7002,
0x28A8, 0x17C7,
0x28AA, 0x0045,
0x28AC, 0x0006,
0x28AE, 0x17CC,
0x28B0, 0x0015,
0x28B2, 0x1512,
0x28B4, 0x216F,
0x28B6, 0x005B,
0x28B8, 0x005D,
0x28BA, 0x00DE,
0x28BC, 0x00DD,
0x28BE, 0x5023,
0x28C0, 0x00DE,
0x28C2, 0x005B,
0x28C4, 0x0410,
0x28C6, 0x0091,
0x28C8, 0x0015,
0x28CA, 0x0040,
0x28CC, 0x7023,
0x28CE, 0x1653,
0x28D0, 0x0156,
0x28D2, 0x0001,
0x28D4, 0x2081,
0x28D6, 0x700E,
0x28D8, 0x2F99,
0x28DA, 0x005C,
0x28DC, 0x0000,
0x28DE, 0x5040,
0x28E0, 0x0045,
0x28E2, 0x213A,
0x28E4, 0x0303,
0x28E6, 0x0148,
0x28E8, 0x0049,
0x28EA, 0x0045,
0x28EC, 0x0046,
0x28EE, 0x081D,
0x28F0, 0x00DE,
0x28F2, 0x00DD,
0x28F4, 0x00DC,
0x28F6, 0x00DE,
0x28F8, 0x04D6,
0x28FA, 0x2014,
0x28FC, 0x2081,
0x28FE, 0x704E,
0x2900, 0x2F99,
0x2902, 0x005C,
0x2904, 0x0002,
0x2906, 0x5060,
0x2908, 0x31C0,
0x290A, 0x2122,
0x290C, 0x7800,
0x290E, 0xC08C,
0x2910, 0x0001,
0x2912, 0x9038,
0x2914, 0x59F7,
0x2916, 0x907A,
0x2918, 0x03D8,
0x291A, 0x8D90,
0x291C, 0x01C0,
0x291E, 0x7400,
0x2920, 0x2002,
0x2922, 0x70DF,
0x2924, 0x3F40,
0x2926, 0x0240,
0x2928, 0x7800,
0x292A, 0x0021,
0x292C, 0x7400,
0x292E, 0x0001,
0x2930, 0x70DF,
0x2932, 0x3F5F,
0x2934, 0x7012,
0x2936, 0x2F01,
0x2938, 0x7800,
0x293A, 0x7400,
0x293C, 0x2004,
0x293E, 0x70DF,
0x2940, 0x3F20,
0x2942, 0x0240,
0x2944, 0x7800,
0x2946, 0x0041,
0x2948, 0x7400,
0x294A, 0x2008,
0x294C, 0x70DF,
0x294E, 0x3F20,
0x2950, 0x0240,
0x2952, 0x7800,
0x2954, 0x0041,
0x2956, 0x7400,
0x2958, 0x0004,
0x295A, 0x70DF,
0x295C, 0x3F5F,
0x295E, 0x7012,
0x2960, 0x2F01,
0x2962, 0x7800,
0x2964, 0x7400,
0x2966, 0x2010,
0x2968, 0x70DF,
0x296A, 0x3F40,
0x296C, 0x0240,
0x296E, 0x7800,
0x2970, 0x0000,
0x2972, 0xB89E,
0x2974, 0x0000,
0x2976, 0xB89E,
0x2978, 0xB90E,
0x297A, 0x0002,
0x297C, 0x0063,
0x297E, 0xB93A,
0x2980, 0x0063,
0x2982, 0xB91E,
0x2984, 0x0063,
0x2986, 0xB948,
0x2988, 0x0063,
0x298A, 0xB956,
0x298C, 0xB92A,
0x298E, 0x0004,
0x2990, 0x0063,
0x2992, 0xB948,
0x2994, 0x0063,
0x2996, 0xB964,
0x2998, 0x0063,
0x299A, 0xB93A,
0x299C, 0x0063,
0x299E, 0xB92C,
0x29A0, 0xB92A,
0x29A2, 0x0004,
0x29A4, 0x0066,
0x29A6, 0x0067,
0x29A8, 0x00AF,
0x29AA, 0x01CF,
0x29AC, 0x0087,
0x29AE, 0x0083,
0x29B0, 0x011B,
0x29B2, 0x035A,
0x29B4, 0x00FA,
0x29B6, 0x00F2,
0x29B8, 0x00A6,
0x29BA, 0x00A4,
0x29BC, 0xFFFF,
0x29BE, 0x002D,
0x29C0, 0x005A,
0x29C2, 0x0000,
0x29C4, 0x0000,
0x29C6, 0xB9A4,
0x29C8, 0xB970,
0x29CA, 0xB9BE,
0x29CC, 0xB97C,
0x29CE, 0xB990,
0x29D0, 0xB97C,
0x29D2, 0xB990,
0x29D4, 0xB97C,
0x29D6, 0xB990,
0x29D8, 0xB97C,
0x29DA, 0xB990,
0x29DC, 0xB97C,
0x29DE, 0xB990,
0x29E0, 0xB97C,
0x29E2, 0xB990,
0x29E4, 0xB97C,
0x29E6, 0xB990,
0x29E8, 0xB97C,
0x29EA, 0xB990,
0x29EC, 0xB97C,
0x29EE, 0xB990,
0x29F0, 0xB97C,
0x29F2, 0xB990,
0x29F4, 0xB97C,
0x29F6, 0xB990,
0x29F8, 0xB97C,
0x29FA, 0xB990,
0x29FC, 0xB97C,
0x29FE, 0xB990,
0x2A00, 0xB97C,
0x2A02, 0xB990,
0x2A04, 0xB97C,
0x2A06, 0xB990,
0x2A08, 0xB97C,
0x2A0A, 0xB990,
0x3710, 0x871E,
0x3712, 0xB9EC,
0x3714, 0xB9CA,
0x3716, 0xD140,
0x3718, 0xB9CC,
0x371A, 0xB9C8,
0x371C, 0x0000,
0x371E, 0x0040,
0x3720, 0x0040,
0x3722, 0x0040,
0x3724, 0x0040,
0x3726, 0x0044,
0x3728, 0x0049,
0x372A, 0x004D,
0x372C, 0x0052,
0x372E, 0x0057,
0x3730, 0x005C,
0x3732, 0x0062,
0x3734, 0x0068,
0x3736, 0x006E,
0x3738, 0x0074,
0x373A, 0x007A,
0x373C, 0x0080,
0x373E, 0x0087,
0x3740, 0x008E,
0x3742, 0x0095,
0x3744, 0x009C,
0x3746, 0x00A4,
0x3748, 0x00AB,
0x374A, 0x00B2,
0x374C, 0x00BA,
0x374E, 0x00C1,
0x3750, 0x00C7,
0x3752, 0x00CD,
0x3754, 0x00D4,
0x3756, 0x00DA,
0x3758, 0x00E0,
0x375A, 0x00E6,
0x375C, 0x00E6,
0x375E, 0x0000,
0x3760, 0x0000,
0x3762, 0x0000,
0x3764, 0x0000,
0x3766, 0x0000,
0x3768, 0x0000,
0x376A, 0x0000,
0x376C, 0x0000,
0x376E, 0x0000,
0x3770, 0x0000,
0x3772, 0x0000,
0x3774, 0x0000,
0x3776, 0x0000,
0x3778, 0x0000,
0x377A, 0x0000,
0x377C, 0x0000,
0x377E, 0x0000,
0x3780, 0x0000,
0x3782, 0x0000,
0x3784, 0x0000,
0x3786, 0x0000,
0x3788, 0x0000,
0x378A, 0x0000,
0x378C, 0x0000,
0x378E, 0x0000,
0x3790, 0x0000,
0x3792, 0x0000,
0x3794, 0x0000,
0x3796, 0x0000,
0x3798, 0x0000,
0x379A, 0x0000,
0x379C, 0x0000,
0x026A, 0xFFFF,
0x026C, 0x00FF,
0x026E, 0x0000,
0x0360, 0x1E8E,
0x040E, 0x01EB,
0x0600, 0x1130,
0x0602, 0x3112,
0x0604, 0x8048,
0x0606, 0x00E9,
0x0676, 0x07FF,
0x0678, 0x0002,
0x067A, 0x0505,
0x067C, 0x0505,
0x06A8, 0x0240,
0x06AA, 0x00CA,
0x06AC, 0x0041,
0x06B4, 0x3FFF,
0x06DE, 0x0505,
0x06E0, 0x0505,
0x06E2, 0xFF00,
0x06E4, 0x8369,
0x06E6, 0x8369,
0x06E8, 0x8369,
0x06EA, 0x8369,
0x052A, 0x0000,
0x052C, 0x0000,
0x0F06, 0x0002,
0x1102, 0x0008,
0x0A04, 0xB4C5,
0x0A06, 0xC400,
0x0A08, 0x988A,
0x0A0A, 0xF386,
0x0A0E, 0xEEC0,
0x0A12, 0x0000,
0x0A18, 0x0010,
0x0A1E, 0x000F,
0x0A20, 0x0015,
0x0C00, 0x0021,
0x0C16, 0x0002,
0x0708, 0x6FC0,
0x070C, 0x0000,
0x0780, 0x010F,
0x120C, 0x1428,
0x121A, 0x0000,
0x121C, 0x1896,
0x121E, 0x0032,
0x1220, 0x0000,
0x1222, 0x96FF,
0x1244, 0x0000,
0x105C, 0x0F0B,
0x1958, 0x003F,
0x195A, 0x004C,
0x195C, 0x0097,
0x195E, 0x0221,
0x1960, 0x03FF,
0x1980, 0x007D,
0x1982, 0x0028,
0x1984, 0x2018,
0x1986, 0x0010,
0x1988, 0x0000,
0x198A, 0x0000,
0x198C, 0x0428,
0x198E, 0x0000,
0x1990, 0x1B33,
0x1992, 0x0000,
0x1994, 0x3000,
0x1996, 0x0002,
0x1962, 0x003F,
0x1964, 0x004C,
0x1966, 0x0097,
0x1968, 0x0221,
0x196A, 0x03FF,
0x19C0, 0x007D,
0x19C2, 0x0028,
0x19C4, 0x2018,
0x19C6, 0x0010,
0x19C8, 0x0000,
0x19CA, 0x0000,
0x19CC, 0x0428,
0x19CE, 0x0000,
0x19D0, 0x1B33,
0x19D2, 0x0000,
0x19D4, 0x3000,
0x19D6, 0x0002,
0x196C, 0x003F,
0x196E, 0x004C,
0x1970, 0x0097,
0x1972, 0x0221,
0x1974, 0x03FF,
0x1A00, 0x007D,
0x1A02, 0x0028,
0x1A04, 0x2018,
0x1A06, 0x0010,
0x1A08, 0x0000,
0x1A0A, 0x0000,
0x1A0C, 0x0428,
0x1A0E, 0x0000,
0x1A10, 0x1B33,
0x1A12, 0x0000,
0x1A14, 0x3000,
0x1A16, 0x0002,
0x1976, 0x003F,
0x1978, 0x004C,
0x197A, 0x0097,
0x197C, 0x0221,
0x197E, 0x03FF,
0x1A40, 0x007D,
0x1A42, 0x0028,
0x1A44, 0x2018,
0x1A46, 0x0010,
0x1A48, 0x0000,
0x1A4A, 0x0000,
0x1A4C, 0x0428,
0x1A4E, 0x0000,
0x1A50, 0x1B33,
0x1A52, 0x0000,
0x1A54, 0x3000,
0x1A56, 0x0002,
0x0C34, 0x0300,
0x027E, 0x0100,
};

static kal_uint16 hi1337_capture_setting[] = {
////////////////////////////////////////////////////////////
//	Sensor                   : 	Hi-1337
//	Mode                     : 	 "4208x3120_PD2_VC"
//	Image width              : 	4208
//	Image height             : 	3120
//	Frame rate               : 	30.06
//	MIPI Speed(Mbps)         : 	1440
//	MIPI Lane                : 	4
//	MCLK(Mhz)                : 	24
//	Pixel Order              : 	 GB
////////////////////////////////////////////////////////////
0x0204, 0x0000,
0x0206, 0x02E4,
0x020A, 0x0CA1,
0x020E, 0x0CA5,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x002E,
0x022A, 0x0017,
0x022C, 0x0E1F,
0x022E, 0x0C61,
0x0234, 0x1111,
0x0236, 0x1111,
0x0238, 0x1111,
0x023A, 0x1111,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0000,
0x0F04, 0x0008,
0x0B02, 0x0100,
0x0B04, 0x00DC,
0x0B12, 0x1070,
0x0B14, 0x0C30,
0x0B20, 0x0100,
0x1100, 0x1100,
0x1108, 0x0202,
0x1118, 0x0000,
0x0A10, 0xB040,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0C30,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0000,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0A00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0100,
0x1012, 0x01B3,
0x1014, 0x007F,
0x1016, 0x007F,
0x101A, 0x007F,
0x1020, 0xC10C,
0x1022, 0x0B36,
0x1024, 0x050D,
0x1026, 0x1311,
0x1028, 0x1B0E,
0x102A, 0x140A,
0x102C, 0x2200,
0x1038, 0x1100,
0x103E, 0x0001,
0x1042, 0x0108,
0x1044, 0x0100,
0x1046, 0x0004,
0x1048, 0x0100,
0x1066, 0x0100,
0x1600, 0xE000,
0x1608, 0x0040,
0x160A, 0x1000,
0x160C, 0x001A,
0x160E, 0x0C00,
0x0268, 0x00E5,
0x1246, 0x0124,
};

static kal_uint16 hi1337_preview_setting[] = {
////////////////////////////////////////////////////////////
//	Sensor                   : 	Hi-1337
//	Mode                     : 	 "2104x1560"
//	Image width              : 	2104
//	Image height             : 	1560
//	Frame rate               : 	30.06
//	MIPI Speed(Mbps)         : 	720
//	MIPI Lane                : 	4
//	MCLK(Mhz)                : 	24
//	Pixel Order              : 	 GB
////////////////////////////////////////////////////////////

0x0204, 0x0200,
0x0206, 0x02E4,
0x020A, 0x0CA1,
0x020E, 0x0CA5,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x002C,
0x022A, 0x0015,
0x022C, 0x0E2D,
0x022E, 0x0C61,
0x0234, 0x3311,
0x0236, 0x3311,
0x0238, 0x3311,
0x023A, 0x2222,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0400,
0x0F04, 0x0004,
0x0B02, 0x0100,
0x0B04, 0x00FC,
0x0B12, 0x0838,
0x0B14, 0x0618,
0x0B20, 0x0200,
0x1100, 0x1100,
0x1108, 0x0402,
0x1118, 0x0000,
0x0A10, 0xB070,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0618,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0100,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0A00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x06C2,
0x1012, 0x00CD,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC106,
0x1022, 0x061B,
0x1024, 0x0407,
0x1026, 0x0A09,
0x1028, 0x1308,
0x102A, 0x0A05,
0x102C, 0x1200,
0x1038, 0x0000,
0x103E, 0x0101,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x06E0,
0x1600, 0x0400,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,
0x0268, 0x00E5,
0x1246, 0x0124,
};

static kal_uint16 hi1337_normal_video_setting[] = {
////////////////////////////////////////////////////////////
//	Sensor                   : 	Hi-1337
//	Mode                     : 	 "4208x2368_PD2_VC"
//	Image width              : 	4208
//	Image height             : 	2368
//	Frame rate               : 	30.06
//	MIPI Speed(Mbps)         : 	1440
//	MIPI Lane                : 	4
//	MCLK(Mhz)                : 	24
//	Pixel Order              : 	 GB
////////////////////////////////////////////////////////////
0x0204, 0x0000,
0x0206, 0x02E4,
0x020A, 0x0CA1,
0x020E, 0x0CA5,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x01A6,
0x022A, 0x0017,
0x022C, 0x0E1F,
0x022E, 0x0AE9,
0x0234, 0x1111,
0x0236, 0x1111,
0x0238, 0x1111,
0x023A, 0x1111,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0000,
0x0F04, 0x0008,
0x0B02, 0x0100,
0x0B04, 0x00DC,
0x0B12, 0x1070,
0x0B14, 0x0940,
0x0B20, 0x0100,
0x1100, 0x1100,
0x1108, 0x0002,
0x1118, 0x023E,
0x0A10, 0xB040,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0940,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0000,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0A00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0100,
0x1012, 0x01B3,
0x1014, 0x007F,
0x1016, 0x007F,
0x101A, 0x007F,
0x1020, 0xC10C,
0x1022, 0x0B36,
0x1024, 0x050D,
0x1026, 0x1311,
0x1028, 0x1B0E,
0x102A, 0x140A,
0x102C, 0x2200,
0x1038, 0x1100,
0x103E, 0x0001,
0x1042, 0x0108,
0x1044, 0x0100,
0x1046, 0x0004,
0x1048, 0x0100,
0x1066, 0x0100,
0x1600, 0xE000,
0x1608, 0x0040,
0x160A, 0x1000,
0x160C, 0x0002,
0x160E, 0x0940,
0x0268, 0x00E5,
0x1246, 0x0124,
};

static kal_uint16 hi1337_hs_video_setting[] = {
////////////////////////////////////////////////////////////
//	Sensor                   : 	Hi-1337
//	Mode                     : 	 "1280x720_120fps"
//	Image width              : 	1280
//	Image height             : 	720
//	Frame rate               : 	120.08
//	MIPI Speed(Mbps)         : 	480
//	MIPI Lane                : 	4
//	MCLK(Mhz)                : 	24
//	Pixel Order              : 	 GB
////////////////////////////////////////////////////////////
0x0204, 0x0000,
0x0206, 0x02DD,
0x020A, 0x032E,
0x020E, 0x0332,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x020A,
0x022A, 0x0017,
0x022C, 0x0E3D,
0x022E, 0x0A83,
0x0234, 0x1111,
0x0236, 0x3333,
0x0238, 0x3333,
0x023A, 0x1133,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0800,
0x0F04, 0x0040,
0x0B02, 0x0100,
0x0B04, 0x00FC,
0x0B12, 0x0500,
0x0B14, 0x02D0,
0x0B20, 0x0300,
0x1100, 0x1100,
0x1108, 0x0002,
0x1118, 0x02A2,
0x0A10, 0xB040,
0x0C14, 0x00C0,
0x0C18, 0x0F00,
0x0C1A, 0x02D0,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0200,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0A00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x046A,
0x1012, 0x00A1,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC105,
0x1022, 0x0512,
0x1024, 0x0305,
0x1026, 0x0708,
0x1028, 0x1206,
0x102A, 0x0705,
0x102C, 0x0D00,
0x1038, 0x0000,
0x103E, 0x0201,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x047E,
0x1600, 0x0000,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,
0x0268, 0x00E7,
0x1246, 0x0127,
};

static kal_uint16 hi1337_slim_video_setting[] = {
////////////////////////////////////////////////////////////
//	Sensor                   : 	Hi-1337
//	Mode                     : 	 "1920x1080_30fps"
//	Image width              : 	1920
//	Image height             : 	1080
//	Frame rate               : 	30.06
//	MIPI Speed(Mbps)         : 	720
//	MIPI Lane                : 	4
//	MCLK(Mhz)                : 	24
//	Pixel Order              : 	 GB
////////////////////////////////////////////////////////////

0x0204, 0x0000,
0x0206, 0x02E4,
0x020A, 0x0CA1,
0x020E, 0x0CA5,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x020C,
0x022A, 0x0017,
0x022C, 0x0E2D,
0x022E, 0x0A81,
0x0234, 0x1111,
0x0236, 0x3311,
0x0238, 0x3311,
0x023A, 0x1122,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0400,
0x0F04, 0x0060,
0x0B02, 0x0100,
0x0B04, 0x00FC,
0x0B12, 0x0780,
0x0B14, 0x0438,
0x0B20, 0x0200,
0x1100, 0x1100,
0x1108, 0x0002,
0x1118, 0x02A4,
0x0A10, 0xB040,
0x0C14, 0x00C0,
0x0C18, 0x0F00,
0x0C1A, 0x0438,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0100,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0A00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x06C2,
0x1012, 0x0107,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC106,
0x1022, 0x061B,
0x1024, 0x0407,
0x1026, 0x0A09,
0x1028, 0x1308,
0x102A, 0x0A05,
0x102C, 0x1200,
0x1038, 0x0000,
0x103E, 0x0101,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x06E0,
0x1600, 0x0400,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,
0x0268, 0x00E5,
0x1246, 0x0124,
};

static kal_uint16 hi1337_custom1_setting[] = {
////////////////////////////////////////////////////////////
//	Sensor                   : 	Hi-1337
//	Mode                     : 	 "4208x3120_PD2_VC_24FPS"
//	Image width              : 	4208
//	Image height             : 	3120
//	Frame rate               : 	24.05
//	MIPI Speed(Mbps)         : 	1440
//	MIPI Lane                : 	4
//	MCLK(Mhz)                : 	24
//	Pixel Order              : 	 GB
////////////////////////////////////////////////////////////

0x0204, 0x0000,
0x0206, 0x02E4,
0x020A, 0x0FC9,
0x020E, 0x0FCD,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x002E,
0x022A, 0x0017,
0x022C, 0x0E1F,
0x022E, 0x0C61,
0x0234, 0x1111,
0x0236, 0x1111,
0x0238, 0x1111,
0x023A, 0x1111,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0000,
0x0F04, 0x0008,
0x0B02, 0x0100,
0x0B04, 0x00DC,
0x0B12, 0x1070,
0x0B14, 0x0C30,
0x0B20, 0x0100,
0x1100, 0x1100,
0x1108, 0x0202,
0x1118, 0x0000,
0x0A10, 0xB040,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0C30,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0000,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0A00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0100,
0x1012, 0x01B3,
0x1014, 0x007F,
0x1016, 0x007F,
0x101A, 0x007F,
0x1020, 0xC10C,
0x1022, 0x0B36,
0x1024, 0x050D,
0x1026, 0x1311,
0x1028, 0x1B0E,
0x102A, 0x140A,
0x102C, 0x2200,
0x1038, 0x1100,
0x103E, 0x0001,
0x1042, 0x0108,
0x1044, 0x0100,
0x1046, 0x0004,
0x1048, 0x0100,
0x1066, 0x0100,
0x1600, 0xE000,
0x1608, 0x0040,
0x160A, 0x1000,
0x160C, 0x001A,
0x160E, 0x0C00,
0x0268, 0x00E5,
0x1246, 0x0124,
};


static void sensor_init(void)
{
	pr_err("[%s] Hi-1337 initial setting\n", __func__);
	hi1337_table_write_cmos_sensor(hi1337_init_setting,
		sizeof(hi1337_init_setting)/sizeof(kal_uint16));
	pr_err("[%s] Hi-1337 initial setting end\n", __func__);

}	/*	  sensor_init  */

static void preview_setting(void)
{
	pr_err("[%s] Hi-1337 preview setting\n", __func__);

	hi1337_table_write_cmos_sensor(hi1337_preview_setting,
		sizeof(hi1337_preview_setting)/sizeof(kal_uint16));

	pr_err("[%s] Hi-1337 preview setting end\n", __func__);

} /* preview_setting */


/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
	pr_err("[%s] Hi-1337 capture setting\n", __func__);

	hi1337_table_write_cmos_sensor(hi1337_capture_setting,
		sizeof(hi1337_capture_setting)/sizeof(kal_uint16));

	pr_err("[%s] Hi-1337 capture setting end\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_err("[%s] Hi-1337 normal_video setting\n", __func__);

	hi1337_table_write_cmos_sensor(hi1337_normal_video_setting,
		sizeof(hi1337_normal_video_setting)/sizeof(kal_uint16));

	pr_err("[%s] Hi-1337 normal_video setting end\n", __func__);
}

static void hs_video_setting(void)
{
	pr_err("[%s] Hi-1337 hs_video setting\n", __func__);

	hi1337_table_write_cmos_sensor(hi1337_hs_video_setting,
		sizeof(hi1337_hs_video_setting)/sizeof(kal_uint16));

	pr_err("[%s] Hi-1337 hs_video setting end\n", __func__);
}

static void slim_video_setting(void)
{
	pr_err("[%s] Hi-1337 slim_video setting\n", __func__);

	hi1337_table_write_cmos_sensor(hi1337_slim_video_setting,
		sizeof(hi1337_slim_video_setting)/sizeof(kal_uint16));

	pr_err("[%s] Hi-1337 slim_video setting end\n", __func__);
}

static void custom1_setting(void)
{
	pr_debug("[%s] Hi-1337 custom1 setting\n", __func__);

	hi1337_table_write_cmos_sensor(hi1337_custom1_setting,
		sizeof(hi1337_custom1_setting)/sizeof(kal_uint16));

	pr_debug("[%s] Hi-1337 custom1 setting end\n", __func__);
}


/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 vendor_id =  0;
	/*sensor have two i2c address 0x34 & 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id()+1;
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_err("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
				vendor_id = read_eeprom_vendor_id(0x0001);
				if(vendor_id == OFILM_VENDOR_ID){
					pr_err("find hi1337 ofilm ,vendor_id:  0x%x\n",vendor_id);
					return ERROR_NONE;
				}
			}

			pr_err("Read sensor id fail,read:0x%x id: 0x%x\n", return_sensor_id(), imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if ((*sensor_id != imgsensor_info.sensor_id)  || (vendor_id != OFILM_VENDOR_ID)) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	/* initail sequence write in  */
	pr_err("%s HI-1337 initial setting start \n", __func__);
	sensor_init();
	pr_err("%s HI-1337 initial setting end \n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
} /* open */

/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	pr_err("%s E\n",__func__);
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
	pr_err("%s X\n",__func__);
	return ERROR_NONE;
} /* close */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_err("%s E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();


	return ERROR_NONE;
} /* preview */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_err("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_err(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_err("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_err("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	hs_video_setting();

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_err("%s. 720P@240FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	slim_video_setting();

	return ERROR_NONE;
}	/* slim_video */
				
static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom1_setting();

	return ERROR_NONE;
}	/* capture() */


static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	pr_err("E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;

	return ERROR_NONE;
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_err("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
#if ENABLE_PDAF
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV; //PDAF_TYPE2
#else
	sensor_info->PDAF_Support = 0;
#endif
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;


	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_err("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	default:
		pr_err("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_err("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker*/ {
		imgsensor.autoflicker_en = KAL_TRUE;
		pr_err("enable! fps = %d", framerate);
	} else {
		 /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_err("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_err(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
			, framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength
				+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
			/ imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		pr_err("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_err("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_err("set_test_pattern_mode enable: %d", enable);

	if (enable) {
		write_cmos_sensor(0x1038, 0x0000); //mipi_virtual_channel_ctrl
		write_cmos_sensor(0x1042, 0x0008); //mipi_pd_sep_ctrl_h, mipi_pd_sep_ctrl_l
		write_cmos_sensor(0x0b04, 0x00C1);
		write_cmos_sensor(0x0C0A, 0x0200);
	} else {
		write_cmos_sensor(0x0C0A, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


/*L19 code for HQ-173148 by huabinchen at 2021/12/17 start*/
static kal_uint32 get_sensor_temperature(void)
{
	UINT32 temperature = 120;
	INT32 temperature_convert = 0;
	INT32 A = 79959;
	INT32 B = 24921;
	INT32 C = 900;
	temperature = read_cmos_sensor_8(0x078F) & 0xFF;
	temperature_convert = ((C * A) / (C + temperature*100/8) - B)/100;
	pr_err("walf %s:temperature_convert=%d, temperature is %d, ", __func__, temperature_convert, temperature );
	if (temperature_convert > 85)
		temperature_convert = 85;
	else if (temperature_convert < -40)
		temperature_convert = -40;
	
	return temperature_convert;
	
}
/*L19 code for HQ-173148 by huabinchen at 2021/12/17 end*/

/*L19 code for HQ-172980 by huabinchen at 2022/01/13 start*/
static kal_uint32 ana_gain_table_16x[] = {
1024,  1088,  1152,  1216,  1280,  1344,  1408,  1472,
1536,  1600,  1664,  1728,  1792,  1856,  1920,  1984,
2048,  2176,  2304,  2432,  2560,  2688,  2816,  2944,
3072,  3200,  3328,  3456,  3584,  3712,  3840,  3968,
4096,  4352,  4608,  4864,  5120,  5376,  5632,  5888,
 6144,  6400,  6656,  6912,  7168,  7424,  7680,  7936,
8192,  8704,  9216,  9728, 10240, 10752, 11264, 11776,
12288, 12800, 13312, 13824, 14336, 14848, 15360, 15872, 16384
};
/*L19 code for HQ-172980 by huabinchen at 2022/01/13 end*/

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_err("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
/*L19 code for HQ-172980 by huabinchen at 2022/01/13 start*/
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
	if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL
		|| *(feature_data + 0) == 0) {
			*(feature_data + 0) =sizeof(ana_gain_table_16x);
	} else {
		memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)ana_gain_table_16x,
			sizeof(ana_gain_table_16x));
	}
	break;
/*L19 code for HQ-172980 by huabinchen at 2022/01/13 end*/
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		 set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		 /* night_mode((BOOL) *feature_data); */
		break;
	#ifdef VENDOR_EDIT
	case SENSOR_FEATURE_CHECK_MODULE_ID:
		*feature_return_para_32 = imgsensor_info.module_id;
		break;
	#endif
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		 get_default_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_err("SENSOR_FEATURE_GET_PDAF_DATA\n");
		read_eeprom_hi1337_ofilm((kal_uint16)(*feature_data),
				(char *)(uintptr_t)(*(feature_data+1)),
				(kal_uint32)(*(feature_data+2)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_err("current fps :%d\n", (UINT32)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_err("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	#if 0
		pr_err("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
	#endif
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[3],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[4],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[5],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_err("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
/*L19 code for HQ-161200 by zongshuoshuo at 2021/12/10 start*/
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_16_9,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
/*L19 code for HQ-161200 by zongshuoshuo at 2021/12/10 end*/
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_err(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; // type2 - VC enable
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		pr_err("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		pr_err("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_err("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		break;

	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_err("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)),
					(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_err("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		#if 0
		ihdr_write_shutter((UINT16)*feature_data,
				   (UINT16)*(feature_data+1));
		#endif
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_err("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_err("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	/*L19 code for 161200 by zhangmeng at 2021.11.22 start*/
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*feature_return_para_32 = 1; /* NON */
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			//*feature_return_para_32 = 1; /*BINNING_SUM*/
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_err("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

		break;
	/*L19 code for 161200 by zhangmeng at 2021.11.22 end*/
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom1.pclk /
			(imgsensor_info.custom1.linelength - 80))*
			imgsensor_info.custom1.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
	break;

	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0], sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1], sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1], sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
			//pr_info("error: get wrong vc_INFO id = %d",
			//*feature_data_32);
			//memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
	break;

	default:
		break;
	}
	return ERROR_NONE;
} /* feature_control() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HI1337_OFILM_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
} /* HI1337_OFILM_MIPI_RAW_SensorInit */
