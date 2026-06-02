// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "kd_imgsensor.h"


#include "imgsensor_hw.h"
#include "imgsensor_cfg_table.h"

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#if defined(OV50C40_SUNNY_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV50C40_SUNNY_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV50C40_AAC_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV50C40_AAC_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(S5KJN1_OFILM_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJN1_OFILM_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1050, 1},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 9}
		},
	},
#endif
#if defined(HI1337_AAC_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI1337_AAC_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif
#if defined(HI1337_OFILM_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI1337_OFILM_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif
#if defined(GC5035_SUNNY_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC5035_SUNNY_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(HI556_OFILM_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI556_OFILM_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
//			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif
#if defined(OV8856_AAC_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8856_AAC_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(OV8856_OFILM_FRONT_SENSOR_ID)
	{
		SENSOR_DRVNAME_OV8856_OFILM_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(OV02B1B_SUNNY_DEPTH_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B1B_SUNNY_DEPTH_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 6},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 9}
		},
	},
#endif

#if defined(SC201CS_OFILM_DEPTH_SENSOR_ID)
	{
		SENSOR_DRVNAME_SC201CS_OFILM_DEPTH_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
//			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 4},
		},
	},
#endif
/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 start*/
#if defined(SC202CS_OFILM_DEPTH_SENSOR_ID)
	{
		SENSOR_DRVNAME_SC202CS_OFILM_DEPTH_MIPI_RAW,
		{
			{DOVDD, Vol_1800,0},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 4},
		},
	},
#endif
/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 END*/

	/* add new sensor before this line */
	{NULL,},
};


#if defined(FACTORY_CAMERA_MODE) || defined(__XIAOMI_CAMERA__)
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence_V1_1[] = {
#if defined(HI1337_AAC_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI1337_AAC_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif
#if defined(HI1337_OFILM_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI1337_OFILM_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif
#if defined(GC5035_SUNNY_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC5035_SUNNY_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(HI556_OFILM_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI556_OFILM_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
//			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif
#if defined(OV8856_AAC_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8856_AAC_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(OV8856_OFILM_FRONT_SENSOR_ID)
	{
		SENSOR_DRVNAME_OV8856_OFILM_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(OV02B1B_SUNNY_DEPTH_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B1B_SUNNY_DEPTH_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 6},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 9}
		},
	},
#endif

#if defined(SC201CS_OFILM_DEPTH_SENSOR_ID)
        {
                SENSOR_DRVNAME_SC201CS_OFILM_DEPTH_MIPI_RAW,
                {
                        {RST, Vol_Low, 1},
                        {SensorMCLK, Vol_High, 0},
                        {DOVDD, Vol_1800, 0},
                        {AVDD, Vol_2800, 0},
//                      {DVDD, Vol_1200, 1},
                        {RST, Vol_High, 4},
                },
        },
#endif

/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 start*/
#if defined(SC202CS_OFILM_DEPTH_SENSOR_ID)
	{
		SENSOR_DRVNAME_SC202CS_OFILM_DEPTH_MIPI_RAW,
		{
			{DOVDD, Vol_1800,0},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 4},
		},
	},
#endif
/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 end*/

	/* add new sensor before this line */
	{NULL,},
};





struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence_V1_2[] = {
#if defined(OV50C40_SUNNY_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV50C40_SUNNY_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV50C40_AAC_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV50C40_AAC_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(S5KJN1_OFILM_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KJN1_OFILM_MAIN_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1050, 1},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 9}
		},
	},
#endif
#if defined(GC5035_SUNNY_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_GC5035_SUNNY_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 2}
		},
	},
#endif

#if defined(HI556_OFILM_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_HI556_OFILM_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
//			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 3}
		},
	},
#endif

#if defined(OV8856_AAC_FRONT_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8856_AAC_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(OV8856_OFILM_FRONT_SENSOR_ID)
	{
		SENSOR_DRVNAME_OV8856_OFILM_FRONT_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 1},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(OV02B1B_SUNNY_DEPTH_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV02B1B_SUNNY_DEPTH_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 6},
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 9}
		},
	},
#endif

#if defined(SC201CS_OFILM_DEPTH_SENSOR_ID)
        {
                SENSOR_DRVNAME_SC201CS_OFILM_DEPTH_MIPI_RAW,
                {
                        {RST, Vol_Low, 1},
                        {SensorMCLK, Vol_High, 0},
                        {DOVDD, Vol_1800, 0},
                        {AVDD, Vol_2800, 0},
//                      {DVDD, Vol_1200, 1},
                        {RST, Vol_High, 4},
                },
        },
#endif

/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 start*/
#if defined(SC202CS_OFILM_DEPTH_SENSOR_ID)
	{
		SENSOR_DRVNAME_SC202CS_OFILM_DEPTH_MIPI_RAW,
		{
			{DOVDD, Vol_1800,0},
			{AVDD, Vol_2800, 0},
			{RST, Vol_High, 1},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 4},
		},
	},
#endif
/*L19 code for HQ-161200 by TianGuchen at 2022/01/24 end*/

	/* add new sensor before this line */
	{NULL,},
};

#endif
