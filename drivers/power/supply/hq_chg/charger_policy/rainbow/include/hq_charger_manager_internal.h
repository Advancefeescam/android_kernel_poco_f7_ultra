/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_CHARGER_MANAGER_INTERNAL_H__
#define __HQ_CHARGER_MANAGER_INTERNAL_H__

#define CHARGER_MANAGER_VERSION            "1.1.1"
#define CHARGER_MANAGER_VENDOR             "rainbow"
#ifdef KERNEL_FACTORY_HQ_CHG
#define CHARGER_MANAGER_BUILD_TYPE         "factory"
#else
#define CHARGER_MANAGER_BUILD_TYPE         "user or userdebug"
#endif

/******************************************************************************/
/*                      DEFINE DEFAULT PARAMETERS                             */
/******************************************************************************/
#define DEFAULT_NONE_TYPE_CURRENT          (100)
#define DEFAULT_SDP_CURRENT                (500)
#define DEFAULT_FLOAT_CURRENT              (1000)
#define DEFAULT_CDP_CURRENT                (900)
#define DEFAULT_DCP_CURRENT                (1500)
#define DEFAULT_HVDCP_CURRENT              (3600)
#define DEFAULT_HVDCP_INPUT_CURRENT        (2000)
#define DEFAULT_HVDCP3_CURRENT             (4000)
#define DEFAULT_HVDCP3_INPUT_CURRENT       (3000)
#define DEFAULT_PD2_CURRENT                (3600)
#define DEFAULT_PD2_INPUT_CURRENT          (2000)

#define MTBF_MODE_CDP_CURRENT              (1500)

#define CHARGER_VINDPM_USE_DYNAMIC         1
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT1    4100
#define CHARGER_VINDPM_DYNAMIC_VALUE1      4400
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT2    4200
#define CHARGER_VINDPM_DYNAMIC_VALUE2      4500
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT3    4300
#define CHARGER_VINDPM_DYNAMIC_VALUE3      4600
#define CHARGER_VINDPM_DYNAMIC_BY_VBAT4    4400
#define CHARGER_VINDPM_DYNAMIC_VALUE4      4600
#define CHARGER_VINDPM_DYNAMIC_VALUE5      4700
#define FLOAT_DELAY_TIME                   5000
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
#define WAIT_USB_RDY_TIME                  100
#define WAIT_USB_RDY_MAX_CNT               300
#endif

#define FASTCHARGE_MIN_CURR                1200
#define CHARGER_MANAGER_LOOP_TIME          5000    // 5s
#define CHARGER_MANAGER_LOOP_TIME_OUT      20000   // 20s
#define MAX_UEVENT_LENGTH                  50

#define MIAN_CHG_ADC_LENGTH                180
#define PD20_ICHG_MULTIPLE                 1800
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
#define FG_I2C_ERR_SOC                     15
#define FG_I2C_ERR_VBUS                    6500
#endif

enum power_control_mode {
	DEFAULT_MODE    = 0,
	MTBF_MODE       = 1,
	AGING_MODE      = 2,
	MTBF_CAM_MODE     = 3,
};

/******************************************************************************/
/*                       DEFINE STATIC VARIABLES                              */
/******************************************************************************/
static const char *adc_name[] = {
	"VBUS", "VSYS", "VBAT", "VAC", "IBUS", "IBAT", "TSBUS", "TSBAT", "TDIE",
};

static const char *vbus_type_str[] = {
	"None",
	"SDP",
	"CDP",
	"DCP",
	"QC",
	"FLOAT",
	"Non-Stand",
	"QC3",
	"QC3+",
	"PD",
	"PD_PPS",
};

#endif /* __HQ_CHARGER_MANAGER_INTERNAL_H__ */
