// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

/*P6 code for  HQFEAT-120425  by yangxiongwei at 20250613 start*/
#define MAX_EEPROM_SIZE_64K 0xFFFF
#define MAX_EEPROM_SIZE_32K 0x8000
#define MAX_EEPROM_SIZE_16K 0x4000

extern unsigned int gc08a8_sunny_read_otp_info(unsigned int addr, unsigned char *data, unsigned int size);
unsigned int gc08a8_sunny_read_region( struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size)
{
	return gc08a8_sunny_read_otp_info(addr, data, size);
}

extern unsigned int gc08a8_aac_read_otp_info(unsigned int addr, unsigned char *data, unsigned int size);
unsigned int gc08a8_aac_read_region( struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size)
{
	return gc08a8_aac_read_otp_info(addr, data, size);
}
/*P6 code for  HQFEAT-120425  by yangxiongwei at 20250613 end*/

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
/*P6 code for  HQFEAT-120425  by yangxiongwei at 20250613 start*/
	{GC08A8_SUNNY_ULTRA_SENSOR_ID, 0xA0, gc08a8_sunny_read_region, MAX_EEPROM_SIZE_64K},
	{GC08A8_AAC_ULTRA_SENSOR_ID, 0xA0, gc08a8_aac_read_region, MAX_EEPROM_SIZE_64K},
/*P6 code for  HQFEAT-120425  by yangxiongwei at 20250613 end*/
/*P6 code for HQFEAT-124027 by geyanjie start*/
	{S5KHP3_OFILM_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_32K},
	{S5KHP3_AAC_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_32K},
/*P6 code for HQFEAT-149851 by p-chenxiaoyong1 at 2025-07-10 start*/
	{S5KHP3_AACII_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_32K},
/*P6 code for HQFEAT-149851 by p-chenxiaoyong1 at 2025-07-10 end*/
/*P6 code for HQFEAT-178146 by p-niekangyu at 2025-08-25 start*/
	{S5KHP3_AACIII_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_32K},
	/*P6 code for HQFEAT-178146 by p-niekangyu at 2025-08-25 end*/
/*P6 code for HQFEAT-124027 by geyanjie end*/
/*P6 code for HQFEAT-120415 by zhangxiaohu at 20250610 start*/
	{OV32D40_SUNNY_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	{OV32D40_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region},
/*P6 code for HQFEAT-120415 by zhangxiaohu at 20250610 end*/
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


