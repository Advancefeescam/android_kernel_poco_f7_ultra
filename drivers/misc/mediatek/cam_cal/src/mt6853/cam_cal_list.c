/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_16K 0x4000

/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 start*/
extern unsigned int ov02b1b_read_otp_info(struct i2c_client *client,
  	unsigned int addr,
  	unsigned char *data,
  	unsigned int size);
extern unsigned int sc202cs_read_otp_info(struct i2c_client *client,
  	unsigned int addr,
  	unsigned char *data,
  	unsigned int size);
struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{OV50C40_SUNNY_MAIN_SENSOR_ID, 0xa2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV50C40_AAC_MAIN_SENSOR_ID, 0xa2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI1337_AAC_MAIN_SENSOR_ID, 0xa2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI1337_OFILM_MAIN_SENSOR_ID, 0xa2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI556_OFILM_FRONT_SENSOR_ID, 0xa2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC5035_SUNNY_FRONT_SENSOR_ID, 0xa2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV8856_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV8856_OFILM_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV02B1B_SUNNY_DEPTH_SENSOR_ID, 0x78, ov02b1b_read_otp_info},
	{SC202CS_OFILM_DEPTH_SENSOR_ID, 0x6C, sc202cs_read_otp_info},
	{SC201CS_OFILM_DEPTH_SENSOR_ID, 0xA6, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KJN1_OFILM_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};
/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 end*/

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


