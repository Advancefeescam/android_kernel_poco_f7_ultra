/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * $Revision: 103375 $
 * $Date: 2022-07-29 10:34:16 +0800 (週五, 29 七月 2022) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef 	_LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/pm_qos.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "nt36xxx_mem_map.h"


#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
#include "../xiaomi/xiaomi_touch.h"
#include <linux/power_supply.h>


#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#define BUS_DRIVER_REMOVE_VOID_RETURN
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#define SPI_CS_DELAY
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#define HAVE_VFS_WRITE
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define reinit_completion(x) INIT_COMPLETION(*(x))
#endif
#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif

#define NVT_DEBUG 1

/* SPI Pinctrl setup */
#define PINCTRL_STATE_ACTIVE		"default"
#define PINCTRL_STATE_SUSPEND		"sleep"
/* SPI Pinctrl setup end */

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_FALLING

//---bus transfer length---
#define BUS_TRANSFER_LENGTH  256

//---SPI driver info.---
#define NVT_SPI_NAME "NVT-36536"

#if NVT_DEBUG
#define NVT_LOG(fmt, args...) \
do { \
	struct rtc_time tm; \
	struct timespec64 tv; \
	unsigned long local_time; \
	ktime_get_real_ts64(&tv); \
	local_time = (u32)(tv.tv_sec - (sys_tz.tz_minuteswest * 60)); \
	rtc_time64_to_tm(local_time, &tm); \
	pr_err("[%02d:%02d:%02d.%03zu] [%s] %s %d: " fmt, tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_nsec/1000000, NVT_SPI_NAME, __func__, __LINE__, ##args); \
} while(0)
#else
#define NVT_LOG(fmt, args...) \
do { \
	struct rtc_time tm; \
	struct timespec64 tv; \
	unsigned long local_time; \
	ktime_get_real_ts64(&tv); \
	local_time = (u32)(tv.tv_sec - (sys_tz.tz_minuteswest * 60)); \
	rtc_time64_to_tm(local_time, &tm); \
	pr_err("[%02d:%02d:%02d.%03zu] [%s] %s %d: " fmt, tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_nsec/1000000, NVT_SPI_NAME, __func__, __LINE__, ##args); \
} while(0)
#endif
#define NVT_ERR(fmt, args...) \
do { \
	struct rtc_time tm; \
	struct timespec64 tv; \
	unsigned long local_time; \
	ktime_get_real_ts64(&tv); \
	local_time = (u32)(tv.tv_sec - (sys_tz.tz_minuteswest * 60)); \
	rtc_time64_to_tm(local_time, &tm); \
	pr_err("[%02d:%02d:%02d.%03zu] [%s] %s %d: " fmt, tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_nsec/1000000, NVT_SPI_NAME, __func__, __LINE__, ##args); \
} while(0)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_PEN_NAME "NVTCapacitivePen"

//---Touch info.---3200*2136
#define TOUCH_MAX_WIDTH 2136
#define TOUCH_MAX_HEIGHT 3200
#define PEN_MAX_WIDTH 2136
#define PEN_MAX_HEIGHT 3200
#define TOUCH_RX_NUM 40
#define TOUCH_TX_NUM 60
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1
//thp start 6.8
#define SUPER_RESOLUTION_FACOTR 100
//thp end 6.8

//---for Pen---
#define PEN_PRESSURE_MAX (8191)
#define PEN_DISTANCE_MAX (1)
#define PEN_TILT_MIN (-60)
#define PEN_TILT_MAX (60)

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define NVT_SAVE_TEST_DATA_IN_FILE 0
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1

#define NVT_TP_THICK 0x41
#define NVT_TP_THIN  0x42
#define NVT_DP_LANS  0x32
#define NVT_DP_PAND  0x57

#define BOOT_UPDATE_FIRMWARE 1
#define BOOT_UPDATE_FIRMWARE_NAME		"novatek_ts_fw.bin"
#define BOOT_UPDATE_FIRMWARE_NAME_CSOT  "novatek_nt36536_violin_fw_csot.bin"
#define BOOT_UPDATE_FIRMWARE_NAME_TM00    "novatek_nt36536_violin_fw_tm00.bin"
#define BOOT_UPDATE_FIRMWARE_NAME_TM01    "novatek_nt36536_violin_fw_tm01.bin"
#define BOOT_UPDATE_FIRMWARE_NAME_TM02    "novatek_nt36536_violin_fw_tm02.bin"
#define MP_UPDATE_FIRMWARE_NAME			"novatek_ts_mp.bin"
#define MP_UPDATE_FIRMWARE_NAME_CSOT    "novatek_nt36536_violin_mp_csot.bin"
#define MP_UPDATE_FIRMWARE_NAME_TM00      "novatek_nt36536_violin_mp_tm00.bin"
#define MP_UPDATE_FIRMWARE_NAME_TM01      "novatek_nt36536_violin_mp_tm01.bin"
#define MP_UPDATE_FIRMWARE_NAME_TM02      "novatek_nt36536_violin_mp_tm02.bin"

#define NVT36536_DRIVER_VERSION	 "nvt_version_2025.04.18-001"

//thp start 6.8
#define BOOT_UPDATE_CONFIG_NAME_CSOT  "violin_nova_thp_config.ini"
#define BOOT_UPDATE_CONFIG_NAME_TM    "violin_nova_thp_config.ini"
//thp end 6.8

#define TOUCH_ID 0

#define REPORT_RATE_GAME (240)
#define REPORT_RATE_NORMAL (120)



#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define CHECK_PEN_DATA_CHECKSUM 0

#if BOOT_UPDATE_FIRMWARE
#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)
#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
#if IS_ENABLED(CONFIG_DRM_PANEL)
#define NVT_DRM_PANEL_NOTIFY 1
#elif IS_ENABLED(_MSM_DRM_NOTIFY_H_)
#define NVT_MSM_DRM_NOTIFY 1
#elif IS_ENABLED(CONFIG_FB)
#define NVT_FB_NOTIFY 1
#elif IS_ENABLED(CONFIG_HAS_EARLYSUSPEND)
#define NVT_EARLYSUSPEND_NOTIFY 1
#endif
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
#define NVT_QCOM_PANEL_EVENT_NOTIFY 1
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK) || IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
#define NVT_MTK_DRM_NOTIFY 1
#endif
#endif

/* MIPP Start */
#define MIPP_PEN_VOLTAGE 0x32
#define MIPP_PEN_FREQUENCY 0x31
#define MIPP_PEN_TIME_OFFSET 9
#define MIPP_PEN_TIME_LENGTH 6
#define MIPP_PEN_DATA_LENGTH 15
#define MIPP_MAX_BUFFER_LENGTH 8
#define MIPP_MAX_UEVENT_LENGTH 30
#define MIPP_PEN_HOPPING_OFFSET  20
#define MIPP_BOTH_HOPPING_OFFSET 10

#ifndef TOUCH_THP_SUPPORT
struct nvt_pen_press {
    long long time_stamp;
	unsigned int pressure;
};

struct nvt_pen_press_buff {
	int head;
	struct nvt_pen_press buff[MIPP_MAX_BUFFER_LENGTH];
};

struct nvt_pen_device {
	dev_t basedev;
	struct cdev cdev;
	struct class class;
	struct device* dev;
};

struct nvt_pen_pdata {
    uint8_t id_table[4];
	spinlock_t spinlock;
    struct nvt_pen_device* dev;
    struct nvt_pen_press_buff* buff;
};
#endif
/* MIPP End */

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
enum nvt_ic_state {
	NVT_IC_SUSPEND_IN,
	NVT_IC_SUSPEND_OUT,
	NVT_IC_RESUME_IN,
	NVT_IC_RESUME_OUT,
	NVT_IC_INIT,
};
#endif /* CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE */

/*
 * struct ts_rawdata_info
 *
 */
#define TS_RAWDATA_BUFF_MAX             7000
#define TS_RAWDATA_RESULT_MAX           100
struct ts_rawdata_info {
	int used_size; /*fill in rawdata size*/
	s16 buff[TS_RAWDATA_BUFF_MAX];
	char result[TS_RAWDATA_RESULT_MAX];
};

//thp start 6.8
#if TOUCH_THP_SUPPORT
struct tp_frame {
	int64_t time_ns;
	uint64_t frame_cnt;
	int fod_pressed;
	int fod_trackingId;
	char tp_raw[5176];
};
#endif	/*TOUCH_THP_SUPPORT*/
//thp end 6.8
#define EVENTBUF_DEBUG_LEN 256

struct nvt_ts_data {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
#if IS_ENABLED(NVT_DRM_PANEL_NOTIFY)
	struct notifier_block drm_panel_notif;
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
	struct notifier_block drm_notif;
#elif IS_ENABLED(NVT_FB_NOTIFY)
	struct notifier_block fb_notif;
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
	struct early_suspend early_suspend;
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
	struct notifier_block disp_notifier;
#endif
#if defined(CONFIG_DRM_PANEL)
	void *notifier_cookie;
#endif
	uint8_t fw_ver;
	uint8_t fw_type;
	uint8_t x_num;
	uint8_t y_num;
	uint8_t max_touch_num;
	uint8_t max_button_num;
	uint32_t int_trigger_type;
	#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	/* lockdown */
	bool lkdown_readed;
	u8 lockdown_info[8];
	/* lockdown end */
	struct pm_qos_request pm_qos_req_irq;
	/* power supply */
	struct workqueue_struct *event_wq;
	struct work_struct power_supply_work;
	struct notifier_block power_supply_notifier;
	int is_usb_exist;
	/* power supply end*/
	/* Mmi tp selftest */
	int result_type;
	/* Mmi tp selftest end */
	/* gamemode setup */
	bool gamemode_enable;
	bool game_in_whitelist;
	bool game_in_whitelist_bak;
	u32 gamemode_config[3][5];
	// struct workqueue_struct *set_touchfeature_wq;
	// struct work_struct set_touchfeature_work;
	/* gamemode setup end */
	/* pen_connect_strategy setup */
	struct work_struct pen_charge_state_change_work;
	struct notifier_block pen_charge_state_notifier;
	bool need_send_hopping_ack;
	bool pen_bluetooth_connect;
	bool pen_charge_connect;
	struct device *dev;
	int pen_count;
	bool pen_shield_flag;
	/* pen_connect_strategy setup end */
	/* gesture mode setup */
	int ic_state;
	int gesture_command;
	int gesture_command_delayed;
	bool dev_pm_suspend;
	struct completion dev_pm_resume_completion;
	/* gesture mode setup end */
	/* resume use work queue setup */
	struct work_struct resume_work;
	/* resume use work queue setup end */
	/* SPI Pinctrl set up */
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	/* SPI Pinctrl setup end */
	#endif
	#ifdef CONFIG_TOUCHSCREEN_NVT_DEBUG_FS
	struct dentry *debugfs;
	uint8_t debug_flag;
	bool fw_debug;
	#endif
	int32_t lcd_id_gpio;
	int32_t lcd_id_value;
	int32_t irq_gpio;
	int32_t reset_gpio;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t hw_crc;
	uint8_t auto_copy;
	uint8_t bld_multi_header;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	bool pen_support;
	bool is_cascade;
	uint8_t x_gang_num;
	uint8_t y_gang_num;
	struct input_dev *pen_input_dev;
	int8_t pen_phys[32];
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t bld_spe_pups_addr;
	#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
	#endif
	#ifdef CONFIG_SPI_MT65XX
    struct mtk_chip_config spi_ctrl;
	#endif
	//thp start 6.8
#if TOUCH_THP_SUPPORT
	/* Xiaomi Host Touch Computing */
	uint16_t frame_len; // max frame length in bytes from polling info
	uint8_t *data_buf;
	struct tp_frame thp_frame;
	bool xm_htc_sw_reset; // software reset is on going
	bool xm_htc_report_coordinate;
	uint8_t *eventbuf_debug;
#else
	struct nvt_pen_pdata* pen_pdata;
#endif /*TOUCH_THP_SUPPORT*/
	bool enable_touch_raw;
	bool xm_htc_polled;
	bool ts_probe_start;
	bool ts_selftest_process;
	bool ts_selftest_process_cmd;
	//struct work_struct update_raw_work;
	//thp end 6.8
	bool doze_test;
	int pen_static_status;
	bool nvt_tool_in_use;
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
};
#endif

//thp start 6.8
enum THP_IC_MODE__COMMADN_TYPE {
	SET_IDLE_THD_TYPE = 0x02,
	SET_IDLE_RATE_TYPE = 0x11,
	SET_ENTER_SLEEP_MODE_TYPE  = 0xFF,
	SET_VSYNC_EN_TYPE = 0xFF,
	SET_HSYNC_EN_TYPE = 0xFF,
	SET_FOD_EN_TYPE = 0x12,
	SET_REPORT_RATE_TYPE = 0x13,
	SET_SCAN_FREQ_TYPE = 0x14,
	SET_SCAN_FREQ_HOPPING_EN_TYPE = 0x15,
	SET_AFE_EN_TYPE = 0x16,
	SET_MC_SCAN_EN_TYPE = 0x17,
	SET_SC_SCAN_EN_TYPE = 0x18,
	SET_MC_CALIBRATION_EN_TYPE = 0x19,
	SET_SC_CALIBRATION_EN_TYPE = 0x1a,
	SET_LINE_SHIRT_EN_TYPE = 0xFF,
	SET_INT_STATE_TYPE = 0x1b,
	SET_BASE_REFRESH_EN_TYPE = 0x1c,
	SET_FRAME_DATA_TYPE_TYPE = 0x1d,
	SET_GAME_MODE_EN_TYPE = 0x1e,
	SET_CHARGING_STATUS_EN_TYPE = 0x1f,
	SET_TOUCH_IC_RESET_EN_TYPE = 0xFF,
	SET_GESTURE_EN_TYPE = 0x20,
	SET_CHLICK_GESTURE_EN_TYPE = 0x21,
	SET_DOUBLE_CHLICK_EN_TYPE = 0x22,
	SET_FLAG_BUF_TYPE = 0x23,
	SET_ACTIVE_STYLUS_PROTOCOL_TYPE = 0xFF,
	ACTIVE_STYLUS_EN_TYPE = 0xFF,
	ACTIVE_STYLUS_ONLY_EN_TYPE = 0xFF,
	ACTIVE_STYLUS_TOUCH_SIMULTANEOUSLY_TYPE = 0xFF,
	ACTIVE_STYLUS_SYNC_SUCESS_TYPE = 0xFF,
	ACTIVE_STYLUS_GESTURE_MODE_TYPE = 0xFF,
	SET_IC_RUN_STEP_TYPE = 0xFF,
	SET_NULL_MODE_TYPE = 0xFF,
	SET_IC_LOG_LEVEL_TYPE = 0x24,
	SET_IC_CALIBRATEION_TYPE = 0x25,
	SET_IC_SELF_TEST_TYPE = 0x26,
	SET_IC_SOFT_RETEST_TYPE = 0x27,
	SET_SCAN_SLOPE_TYPE = 0x28,
	SET_SCAN_VOLTAGE_TYPE = 0x29,
	SET_SCAN_NUM_TYPE = 0x2a,
	SET_SCAN_FREQ_NUM_TYPE = 0x2b,
	SET_IC_WORK_MODE_TYPE = 0x2c,
	SET_FILTER_LEVEL_TYPE = 0x2d,
	SET_RAW_TYPE_TYPE = 0xFF,
	SET_OPEN_TRANSPORT_MODE_TYPE = 0xFF,
	SET_STYLUS_PRESSURE_TYPE       = 0x27,
};
//thp end 6.8

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
    EVENT_MAP_HOST_CMD                      = 0x50,
    EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
    EVENT_MAP_RESET_COMPLETE                = 0x60,
    EVENT_MAP_FWINFO                        = 0x78,
    EVENT_MAP_PROJECTID                     = 0x9A,
} SPI_EVENT_MAP;

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(15*1024)
#define NVT_READ_LEN		(8*1024)		// change 2 to 8 len
#define NVT_XBUF_LEN		(NVT_TRANSFER_LEN+1+DUMMY_BYTES)

//thp start 6.8
#if TOUCH_THP_SUPPORT
#define DATA_BUF_LEN (8 * 1024)
#define XM_HTC_DEFAULT_FRAME_LEN (DATA_BUF_LEN - 256 - 1 - DUMMY_BYTES)
#endif /* #if TOUCH_THP_SUPPORT */
//thp end 6.8

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

//---extern structures---
extern struct nvt_ts_data *ts;

//---extern functions---
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len);
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_eng_reset(void);
void nvt_sw_reset(void);
void nvt_sw_reset_idle(void);
void nvt_boot_ready(void);
void nvt_fw_crc_enable(void);
void nvt_tx_auto_copy_mode(void);
void nvt_read_fw_history_all(void);
int32_t nvt_update_firmware(char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_wait_auto_copy(void);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

int update_pen_status(bool enforce_send_cmd);
int32_t nvt_xm_htc_set_stylus_pressure(int16_t stylus_pressure);
int32_t nvt_set_pen_switch(uint8_t pen_switch);
int nvt_ic_self_test_short(void);
int nvt_ic_self_test_open(void);

//thp start 6.8
#if TOUCH_THP_SUPPORT
int32_t nvt_get_xm_htc_poll_info(void);
int32_t nvt_xm_htc_set_idle_wake_th(int16_t idle_wake_th);
int32_t nvt_xm_htc_set_stylus_enable(int16_t stylus_enable);
int32_t nvt_xm_htc_set_click_gesture_enable(int16_t click_gesture_enable);
int32_t nvt_xm_htc_set_gesture_dbclk(int16_t gesture_dbclk);
int32_t nvt_xm_htc_set_gesture_switch(int16_t gesture_switch);
#endif /* #if TOUCH_THP_SUPPORT */
//thp end 6.8
#endif /* _LINUX_NVT_TOUCH_H */
