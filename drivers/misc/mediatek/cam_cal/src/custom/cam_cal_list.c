// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_32K 0x8000
#define MAX_EEPROM_SIZE_16K 0x4000

extern unsigned int sc202v_sunny_read_otp_info(struct i2c_client *client,unsigned int addr, unsigned char *data, unsigned int size);
extern unsigned int sc202v_aac_read_otp_info(struct i2c_client *client,unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	{SC202V_SUNNY_DEPTH_SENSOR_ID, 0xA2, sc202v_sunny_read_otp_info},
	{SC202V_AAC_DEPTH_SENSOR_ID, 0xA2, sc202v_aac_read_otp_info},
	{OV20B40_OFILM_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	{OV20B40_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	/*Below is commom sensor */
	{S5KHM9_SUNNY_MAIN_SENSOR_ID, 0xA2, Common_read_region,MAX_EEPROM_SIZE_16K},
	{S5KHM9_AAC_MAIN_SENSOR_ID, 0xA2, Common_read_region,MAX_EEPROM_SIZE_16K},
	{S5KHM9_OFILM_MAIN_SENSOR_ID, 0xA2, Common_read_region,MAX_EEPROM_SIZE_16K},
	{OV48B_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX766_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{IMX766DUAL_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC8054_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID, 0xA0, Common_read_region},
	{IMX481_SENSOR_ID, 0xA2, Common_read_region},
	{GC02M0_SENSOR_ID, 0xA8, Common_read_region},
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{IMX319_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3M5SX_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX686_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI846_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KGD1SP_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


