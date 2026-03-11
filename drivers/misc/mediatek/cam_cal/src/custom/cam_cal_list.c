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
/*extern unsigned int gc02m1_ofilm_macro_read_otp_info(struct i2c_client *client,
	unsigned int addr,
	unsigned char *data,
	unsigned int size);
*/
//M6 add start
#ifdef PROJECT_DIAMOND
/* add for m6

add for m6*/
#else
extern unsigned int ov02b1b_read_otp_info(struct i2c_client *client,
        unsigned int addr, unsigned char *data, unsigned int size);
#endif
struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
    /* L19A */
    {S5KJN1_SUNNY_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {S5KJN1_OFILM_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {OV8856_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {OV8856_OFILM_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {GC5035_SUNNY_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {HI556_OFILM_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {GC02M1_OFILM_MACRO_SENSOR_ID, 0xA4,  Common_read_region, MAX_EEPROM_SIZE_16K},
    {GC02M1_AAC_MACRO_SENSOR_ID, 0xA4, Common_read_region, MAX_EEPROM_SIZE_16K},
#ifdef PROJECT_DIAMOND
    {S5KHM2SP_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {S5KHM2SD_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
    {OV16A1QSUNNY_SENSOR_ID, 0xA2, Common_read_region},
    {OV16A1QAAC_SENSOR_ID, 0xA2, Common_read_region},
    {IMX355OFILM_SENSOR_ID, 0xA0, Common_read_region},
    {IMX355AAC_SENSOR_ID, 0xA0, Common_read_region},
    {OV02B10_SENSOR_ID, 0xA4, Common_read_region},
    {SC202CS_SUNNY_SENSOR_ID, 0xA4, Common_read_region},
#else
    {OV02B1B_SUNNY_DEPTH_SENSOR_ID,0x78,ov02b1b_read_otp_info},
#endif
    {SC202CS_OFILM_DEPTH_SENSOR_ID,0x6C,sc202cs_read_otp},
    /* L19A end */
	/*Below is commom sensor */
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


