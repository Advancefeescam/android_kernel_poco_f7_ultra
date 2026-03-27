// SPDX-License-Identifier: GPL-2.0
/*
 *nanosic_platform.c
 *
 * pogo driver
 *
 * Copyright (c) 2024-2024 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <asm/processor.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include "inc/nano_macro.h"
#include <mca/battmngr/qti_use_pogo.h>
#include <mca/protocol/protocol_class.h>
#include <mca/common/mca_event.h>
#include <mca/platform/platform_wireless_class.h>
#include <mca/platform/platform_cp_class.h>
#include <mca/platform/platform_buckchg_class.h>
#include <mca/common/mca_event.h>
#include <linux/ktime.h>

static ADSP_read_cb g_ADSP_read_cb = NULL;

static struct mutex rwlock;

static char buf_tmp[I2C_DATA_LENGTH_READ]={0};
static int length;

#define DEV_TYPE_KEYBOARD       (0x1)
#define DEV_TYPE_ZHIJIA         (0x2)

/*0x38:???  */
#define DEV_KEYBOARD_OBJECT  (0x38)
#define DEV_ZHIJIA_OBJECT    (0x3B)
#define DEV_UNKOWN_OBJECT     (0x0)
#define PROBE_CNT_MAX	100

static u8 gObject = DEV_UNKOWN_OBJECT;

enum bq2589x_vbus_type {
	BQ2589X_VBUS_NONE,
	BQ2589X_VBUS_USB_FLOAT = 4,
	BQ2589X_VBUS_USB_SDP = 6,
	BQ2589X_VBUS_USB_CDP = 9, /*CDP for bq25890, Adapter for bq25892*/
	BQ2589X_VBUS_USB_DCP = 10,
	BQ2589X_VBUS_USB_QC_5v = 16,
	BQ2589X_VBUS_USB_QC2,
	BQ2589X_VBUS_USB_QC2_12v,
    BQ2589X_VBUS_USB_QC3 = 20,
	BQ2589X_VBUS_UNKNOWN,
	BQ2589X_VBUS_NONSTAND,
	BQ2589X_VBUS_OTG,
	BQ2589X_VBUS_TYPE_NUM,
};

#define USER_VOTER			"USER_VOTER"
struct nanochg* g_nodev = NULL;
EXPORT_SYMBOL(g_nodev);

static void nano_report_pogo_plugin_state(int enable)
{
    char event[MCA_EVENT_NOTIFY_SIZE] = {0};
    struct mca_event_notify_data event_data;
    int len;

    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
                   "POWER_SUPPLY_CAR_APP_STATE=%d", enable);
    event_data.event = event;
    event_data.event_len = len;
    mca_event_report_uevent(&event_data);
}
/** ************************************************************************
 *  @func rawdata_show
 */
static inline void rawdata_show(const char* descp, const char* buf , size_t size)
{
    int i=0;
    char display[300]={0};

    if(!descp) {
        dbgprint(DEBUG_LEVEL,"descp is null.\n");
        return;
    }

    for (i = 0; i < (size > 35 ? 35 : size); i++) {
        char str[4]={0};
        char str1[5] = {0};

        snprintf(str1,sizeof(str1),"%s%d%s", "[", i, "]");
        strcat(display, str1);

        snprintf(str,sizeof(str),"%02X%s",buf[i], " ");
        strcat(display,str);
    }

    dbgprint(DEBUG_LEVEL,"%s -> %s\n",descp,display);
}

int Nanosic_vendor_raw_data_inject(void *buf, size_t len)
{
    int ret = 0, i = 0;
    rawdata_show("_vendor_raw_data_inject_buf", buf, len);

    for (i = 0; i < len; i++) {
        buf_tmp[i] = *((char*)(buf + i));
    }

    length = len;
    rawdata_show("_vendor_raw_data_inject_rawdata_buf", buf_tmp, length);

    if (!g_nodev)
        dbgprint(DEBUG_LEVEL,"g_nodev is null.\n");
    else {
        schedule_delayed_work(&g_nodev->analyse_rawdata_work, 0);
    }

    return ret;
}
EXPORT_SYMBOL_GPL(Nanosic_vendor_raw_data_inject);

int Nanosic_charge_i2c_write_callback_register(i2c_write_cb cb)
{
    struct nanochg *nodev = g_nodev;
    int ret = 0;

    if (IS_ERR_OR_NULL(nodev))
    {
        dbgprint(ERROR_LEVEL, "nodev ERROR");
        ret = -ENODEV;
        return ret;
    }

    if (IS_ERR_OR_NULL(cb))
    {
        dbgprint(ERROR_LEVEL, "cb ERROR");
        ret = -EINVAL;
        return ret;
    }

    mutex_lock(&nodev->nano_i2c_write_mutex);

    if (!nodev->pogo_charge_i2c_write)
    {
        nodev->pogo_charge_i2c_write = cb;
    }
    else
    {
        dbgprint(ERROR_LEVEL, "charge_i2c_write already registered");
        ret = -EINVAL;
        goto _out;
    }

    dbgprint(INFO_LEVEL, "charge_i2c_write registered");

_out:
    mutex_unlock(&nodev->nano_i2c_write_mutex);

    return ret;
}
EXPORT_SYMBOL_GPL(Nanosic_charge_i2c_write_callback_register);

int Nanosic_charge_i2c_write_callback_unregister(void)
{
    struct nanochg *nodev = g_nodev;
    int ret = 0;

    if (IS_ERR_OR_NULL(nodev))
    {
        dbgprint(ERROR_LEVEL, "nodev ERROR");
        ret = -ENODEV;
        return ret;
    }

    mutex_lock(&nodev->nano_i2c_write_mutex);
    if (nodev->pogo_charge_i2c_write)
    {
        nodev->pogo_charge_i2c_write = NULL;
    }
    else
    {
        dbgprint(ERROR_LEVEL, "pogo_charge_i2c_write already unregistered");
        ret = -ENODEV;
        goto _out;
    }

    dbgprint(INFO_LEVEL, "charge_i2c_write unregistered");

_out:
    mutex_unlock(&nodev->nano_i2c_write_mutex);

    return ret;
}
EXPORT_SYMBOL_GPL(Nanosic_charge_i2c_write_callback_unregister);


/**
 * @brief This function will call Nanosic_charge_i2c_write_callback
 *        in nanosic_driver.c
 *
 * @param buf Data buf
 * @param len Data length
 *
 * @return Result num of bytes success or failure mode
 */
__attribute__((unused)) static int Nanosic_charge_i2c_write(void *buf, size_t len)
{
    struct nanochg *nodev = g_nodev;
    int ret = 0;

    if (IS_ERR_OR_NULL(nodev))
    {
        dbgprint(ERROR_LEVEL, "nodev ERROR");
        ret = -ENODEV;
        return ret;
    }

    mutex_lock(&nodev->nano_i2c_write_mutex);
    if (!IS_ERR_OR_NULL(nodev->pogo_charge_i2c_write))
    {
        ret = nodev->pogo_charge_i2c_write(buf, len);
    }
    else
    {
        dbgprint(ERROR_LEVEL, "pogo_charge_i2c_write ERROR");
        ret = -ENODEV;
        goto _out;
    }

_out:
    mutex_unlock(&nodev->nano_i2c_write_mutex);

    return ret;
}

static void assign_mcu_data(struct read_mcu_data* mcuData, const char* buf)
{
    if (mcuData) {
        mcuData->input_volt = (int)(buf[11] * 100);
        mcuData->input_watt = buf[12];
        mcuData->vbat_set = buf[13];
        mcuData->ibat_set = buf[14];
        mcuData->work_state = buf[15];
        mcuData->typec_state = buf[16];
        mcuData->dpdm_in_state = buf[17];
        mcuData->protocol_volt = (int)(buf[18] * 100);
        mcuData->protocol_curr = (int)(buf[19] * 50);
        mcuData->vbus_pwr = (int)(buf[20] * 100);
        mcuData->vbus_mon = (int)(buf[21] * 100);
        mcuData->typec_curr = (int)(buf[22] * 50);
        mcuData->ntc_l = buf[23];
        mcuData->ntc_h = buf[24];
        mcuData->dpdm0_state = buf[25];
        mcuData->sink_comm = buf[26];
        dbgprint(DEBUG_LEVEL,"Address:%02x, RegLength:%02x, input_volt:%d, input_watt:%02x, vbat_set:%02x, ibat_set:%02x\n",
                                buf[9], buf[10], mcuData->input_volt, mcuData->input_watt, mcuData->vbat_set, mcuData->ibat_set);

        dbgprint(DEBUG_LEVEL, "work_state:%02x, typec_state:%02x, dpdm_in_state:%02x, protocol_volt:%d\n",
                                mcuData->work_state, mcuData->typec_state, mcuData->dpdm_in_state, mcuData->protocol_volt);
        dbgprint(DEBUG_LEVEL, "protocol_curr:%d, vbus_pwr:%d, vbus_mon:%d, typec_curr:%d\n",
                                mcuData->protocol_curr, mcuData->vbus_pwr, mcuData->vbus_mon, mcuData->typec_curr);
        dbgprint(DEBUG_LEVEL, "ntc_l:%02x, ntc_h:%02x, dpdm0_state:%02x, sink_comm:%02x\n",
                                mcuData->ntc_l, mcuData->ntc_h, mcuData->dpdm0_state, mcuData->sink_comm);
    }
}

static u8 vin_low;
static u8 vin_high;
static u8 i_low;
static u8 i_high;
static void assign_holder_mcu_data(struct read_mcu_data* mcuData, const char* buf)
{
    int vin = 0, curr = 0;
    if (mcuData) {
        mcuData->addr = buf[9];
        switch (mcuData->addr)
        {
        case 0x01:
        dbgprint(DEBUG_LEVEL,"ctrl-1 Address:%02x, RegLength:%02x, data:%02x\n",
                                 buf[9], buf[10], buf[11]);
        break;
        case 0x10:
            mcuData->typec_state = buf[11];
            break;
        case 0x11:
            mcuData->code = buf[11];
            break;
        case 0x13:
            mcuData->vout_low = buf[11];
            vin_low = buf[11];
            break;
        case 0x14:
            mcuData->vout_high = buf[11];
            vin_high = buf[11];
            vin = (int)((vin_high << 8) | vin_low);
            dbgprint(DEBUG_LEVEL,"Volt-Curr Address:%02x, vin:%d\n", buf[9], vin);
            break;
        case 0x1e:
            mcuData->vout_low = buf[11];
            i_low = buf[11];
            break;
        case 0x1f:
            mcuData->vout_high = buf[11];
            i_high = buf[11];
            curr = (int)((i_high << 8) | i_low);
            dbgprint(DEBUG_LEVEL,"Volt-Curr Address:%02x, curr_in:%d\n", buf[9], curr);
            break;
        default:
            break;
        }

        dbgprint(DEBUG_LEVEL,"Address:%02x, RegLength:%02x, data:%02x\n",
                                 buf[9], buf[10], buf[11]);
    }
}

static int nano_get_chg_real_type(int* real_type)
{
	int type;

    if (!g_nodev || !g_battmngr_noti) {
        dbgprint(ERROR_LEVEL, "%s g_nodev or g_battmngr_noti is null\n", __func__);
        return -1;
    }

    if (gObject != DEV_KEYBOARD_OBJECT) {

        if (g_battmngr_noti->irq_msg.value) {
            g_battmngr_noti->pd_msg.pd_active = 1;
            dbgprint(ERROR_LEVEL, "%s holder detect pd type. \n", __func__);
            *real_type = POWER_SUPPLY_USB_TYPE_PD;
        } else
            *real_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

        goto result;
    }

	type = (int)g_nodev->dpdm;

	//dbgprint(ERROR_LEVEL, "%s type=%d, old_type=%d \n", __func__, type);

	if (g_nodev->pd_flag) {
        g_battmngr_noti->pd_msg.pd_active = 1;
		dbgprint(ERROR_LEVEL, "%s detect pd type. \n", __func__);
		*real_type = POWER_SUPPLY_USB_TYPE_PD;
		goto result;
	}

	switch (type)
	{
	case BQ2589X_VBUS_NONSTAND:
	case BQ2589X_VBUS_UNKNOWN:
	case BQ2589X_VBUS_USB_FLOAT:
		*real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
	case BQ2589X_VBUS_USB_SDP:
		*real_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case BQ2589X_VBUS_USB_CDP:
		*real_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case BQ2589X_VBUS_USB_DCP:
		*real_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case BQ2589X_VBUS_USB_QC2:
		*real_type = POWER_SUPPLY_TYPE_USB_HVDCP;
		break;
	 case BQ2589X_VBUS_USB_QC3:
	 	*real_type = POWER_SUPPLY_TYPE_USB_HVDCP_3;
	 	break;
	default:
		*real_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	}

result:
	g_nodev->charge_type = *real_type;
	dbgprint(ERROR_LEVEL, "%s bc1.2 type=%d\n", __func__, *real_type);

	return 0;
}

static int nano_get_chg_usb_type(int* usb_type)
{
	int type;

    if (!g_nodev || !g_battmngr_noti) {
        dbgprint(ERROR_LEVEL, "%s g_nodev or g_battmngr_noti is null\n", __func__);
        return -1;
    }

    if (gObject != DEV_KEYBOARD_OBJECT) {

        if (g_battmngr_noti->irq_msg.value) {
            g_battmngr_noti->pd_msg.pd_active = 1;
            dbgprint(ERROR_LEVEL, "%s holder detect pd type. \n", __func__);
            *usb_type = POWER_SUPPLY_USB_TYPE_PD;
        } else
            *usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

        goto result;
    }

	type = (int)g_nodev->dpdm;

	dbgprint(ERROR_LEVEL, "%s type=%d\n", __func__, type);

	if (g_nodev->pd_flag) {
        g_battmngr_noti->pd_msg.pd_active = 1;
		dbgprint(ERROR_LEVEL, "%s detect pd type. \n", __func__);
		*usb_type = POWER_SUPPLY_USB_TYPE_PD;
		goto result;
	}

	switch (type)
	{
	case BQ2589X_VBUS_NONSTAND:
	case BQ2589X_VBUS_UNKNOWN:
	case BQ2589X_VBUS_USB_FLOAT:
		*usb_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
	case BQ2589X_VBUS_USB_SDP:
		*usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case BQ2589X_VBUS_USB_CDP:
		*usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case BQ2589X_VBUS_USB_DCP:
    case BQ2589X_VBUS_USB_QC2:
    case BQ2589X_VBUS_USB_QC3:
		*usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	default:
		*usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	}

result:
	dbgprint(ERROR_LEVEL, "%s bc1.2 type=%d\n", __func__, *usb_type);

	return 0;
}

static int nano_is_charge_online(u8* online)
{
    if (g_nodev) {
        if (!g_nodev->usb_icl_votable)
            g_nodev->usb_icl_votable = find_votable("ICL");

        if (get_client_vote_locked(g_nodev->usb_icl_votable, USER_VOTER) == 0) {
            *online = false;
            return 1;
        }
		 if (g_battmngr_noti)
            *online = g_battmngr_noti->irq_msg.value;
        return 1;
    } else {
        dbgprint(ERROR_LEVEL,"%s g_nodev is null\n", __func__);
    }

	return -1;
}

static int nano_input_curr_limit(int* curr_limit)
{
    if (g_nodev) {
        if (!g_nodev->usb_icl_votable)
            g_nodev->usb_icl_votable = find_votable("ICL");
        *curr_limit = get_effective_result(g_nodev->usb_icl_votable);
        if (*curr_limit >= 0)
            return 1;

    } else {
        dbgprint(ERROR_LEVEL,"%s g_nodev is null\n", __func__);
    }

	return -1;
}

static int nano_get_power_max(int* powr_max)
{
    if (g_nodev) {
        if (gObject == DEV_KEYBOARD_OBJECT)
            *powr_max = ((g_nodev->protocol_volt / 1000) * (g_nodev->protocol_curr / 1000));
        else if (gObject == DEV_ZHIJIA_OBJECT)
            *powr_max = 22;
        else
            *powr_max = 0;

        if (*powr_max >= 0)
            return 1;

    } else {
        dbgprint(ERROR_LEVEL,"%s g_nodev is null\n", __func__);
    }

	return -1;
}

static void deal_report_event(const char* buf)
{
    static int last_online;

    if (buf && g_nodev) {
        if (gObject == DEV_KEYBOARD_OBJECT) {
            g_nodev->online = buf[16] & 0x01;

            g_nodev->pd_flag = (buf[16] & 0x04) >> 2;

            g_nodev->dpdm_in_state = (buf[17] & 0x80);
            g_nodev->dpdm = buf[25];
            g_nodev->protocol_volt = (int)(buf[18] * 100);
            g_nodev->protocol_curr = (int)(buf[19] * 50);
        } else if (g_battmngr_noti){
            g_nodev->online = g_battmngr_noti->irq_msg.value;
        }

        dbgprint(ERROR_LEVEL, "ENTER -1 last_online:%d, %d.\n", last_online, g_nodev->online);
        // send notify
        if (last_online != g_nodev->online) {
            dbgprint(ERROR_LEVEL, "Send report info.\n");
            //g_battmngr_noti->mcu_msg.msg_type = BATTMNGR_MSG_MCU_TYPE;
            battmngr_notifier_call_chain(BATTMNGR_EVENT_REPORT, g_battmngr_noti);
        }
        last_online = g_nodev->online;

    }
}

static const struct battmngr_ops nano_op = {
    .online = nano_is_charge_online,
    .real_type = nano_get_chg_real_type,
    .usb_type = nano_get_chg_usb_type,
    .input_curr_limit = nano_input_curr_limit,
    .power_max = nano_get_power_max,
};

/** ************************************************************************
 *  @func Nanosic_Calculate_crc
 *
 *  @brief calculate sum
 *
 ** */
static
uint8_t Nanosic_Calculate_crc(uint8_t* data,uint32_t datalen)
{
    uint8_t i=0;
    uint8_t crc = 0;

    for(i=2;i<datalen;i++) /*fixed bug in 2023.6.15*/
        crc += data[i];

    return crc;
}

/** ************************************************************************
 *  @func Nanosic_ADSP_Set_Charge_Status
 *
 *  B0     B1    B2    B3    B4    B5    B6    B7......B32     B33  B34  .......B65
 *  0x32   0x00  0x4E  0x31  0x81  0x38  0x42  DATA0...DATA25  crc   0  .......  0
 *
 *  @brief send command to i2c device driver , Called by adsp device driver
 *  write i2c fail when return < 0 or succee return 66
 ** */
int  Nanosic_ADSP_Set_Charge_Status(uint8_t* data,uint32_t datalen)
{
    int ret = -1;
    uint8_t set_charge_status[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4E,0x31,0x81,0x38,0x42};

    if(!data || datalen > 32)
    {
        dbgprint(ERROR_LEVEL,"Invalid argments\n");
        return ret;
    }

    if(gObject == DEV_UNKOWN_OBJECT){
        dbgprint(ERROR_LEVEL,"invalid object\n");
        return ret;
    }

    mutex_lock(&rwlock);

    set_charge_status[5] = gObject;

    set_charge_status[SET_CHARGE_POWER_OFF+2]    = 1;

    memcpy(set_charge_status+7, data, datalen);

    set_charge_status[33] = Nanosic_Calculate_crc(set_charge_status,sizeof(set_charge_status));

    rawdata_show("request set charge",set_charge_status,sizeof(set_charge_status));

    ret = Nanosic_charge_i2c_write(set_charge_status, sizeof(set_charge_status));
	if (ret < 0) {
        dbgprint(ERROR_LEVEL,"Nanosic_charge_i2c_write fail, ret = %d\n", ret);
	}

    mutex_unlock(&rwlock);

    return ret;
}

/** ************************************************************************
 *  @func Nanosic_ADSP_Read_Charge_Status
 *
 *  B0     B1    B2    B3    B4    B5    B6    B7    B8    B9........B65
 *  0x32   0x00  0x4E  0x31  0x81  0x38  0x40  0x00  crc   0.......  0
 *
 *  @brief send command to i2c device driver , Called by adsp device driver
 *  write i2c fail when return < 0 or succee return 66
 ** */
int  Nanosic_ADSP_Read_Charge_Status(void)
{
    int ret = -1;
    uint8_t read_charge_status[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4E,0x31,0x81,0x38,0x40,0x00};

    if(gObject == DEV_UNKOWN_OBJECT){
        dbgprint(ERROR_LEVEL,"invalid object\n");
        return ret;
    }

    mutex_lock(&rwlock);

    read_charge_status[5] = gObject;

    read_charge_status[8] = Nanosic_Calculate_crc(read_charge_status,sizeof(read_charge_status));

    rawdata_show("request read charge",read_charge_status,sizeof(read_charge_status));

    ret = Nanosic_charge_i2c_write(read_charge_status, sizeof(read_charge_status));
	if (ret < 0) {
        dbgprint(ERROR_LEVEL,"Nanosic_charge_i2c_write fail, ret = %d\n", ret);
	}

    mutex_unlock(&rwlock);

    return ret;
}
EXPORT_SYMBOL(Nanosic_ADSP_Read_Charge_Status);

/** ************************************************************************
 *  @func Nanosic_ADSP_Read_Charge_Status_Single
 *
 *  B0     B1    B2    B3    B4    B5    B6    B7    B8    B9........B65
 *  0x32   0x00  0x4E  0x31  0x81  0x38  0x40  0x00  crc   0.......  0
 *
 *  @brief send command to i2c device driver , Called by adsp device driver
 *  write i2c fail when return < 0 or succee return 66
 ** */
int Nanosic_ADSP_Read_Charge_Status_Single(uint8_t addr, uint8_t addrlen, uint8_t target)
{
    int ret = -1;
    uint8_t read_charge_status[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4E,0x31,0x81,0x38,0x40,0x00};

    if(gObject == DEV_UNKOWN_OBJECT){
        dbgprint(ERROR_LEVEL,"invalid object\n");
        return ret;
    }

    mutex_lock(&rwlock);

    read_charge_status[5] = gObject;
    read_charge_status[6] = 0x40;
    read_charge_status[7] = 0x3;
    read_charge_status[8] = addr;
    read_charge_status[9] = addrlen;
    read_charge_status[10] = target;
    read_charge_status[11] = Nanosic_Calculate_crc(read_charge_status,sizeof(read_charge_status));

    rawdata_show("request read charge single",read_charge_status,sizeof(read_charge_status));

   ret = Nanosic_charge_i2c_write(read_charge_status, sizeof(read_charge_status));
	if (ret < 0) {
        dbgprint(ERROR_LEVEL,"Nanosic_charge_i2c_write fail, ret = %d\n", ret);
	}

    mutex_unlock(&rwlock);

    return ret;
}
EXPORT_SYMBOL(Nanosic_ADSP_Read_Charge_Status_Single);

/** ************************************************************************
 *  @func Nanosic_ADSP_Set_Charge_Status_Single
 *
 *  B0     B1    B2    B3    B4    B5    B6    B7......B32     B33  B34  .......B65
 *  0x32   0x00  0x4E  0x31  0x81  0x38  0x42  DATA0...DATA25  crc   0  .......  0
 *
 *  @brief send command to i2c device driver , Called by adsp device driver
 *  write i2c fail when return < 0 or succee return 66
 ** */
int  Nanosic_ADSP_Set_Charge_Status_Single(uint8_t addr, uint8_t addrlen, uint8_t* data, uint32_t datalen, uint8_t target)
{
    int ret = -1;
    uint8_t set_charge_status[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4E,0x31,0x81,0x38,0x42,25};

    if(!addr || !addrlen || !data || datalen > 32)
    {
        dbgprint(ERROR_LEVEL,"Invalid argments\n");
        return ret;
    }

    if(datalen > 6){
        dbgprint(ERROR_LEVEL,"Invalid datalen\n");
        return ret;
    }

    if(gObject == DEV_UNKOWN_OBJECT){
        dbgprint(ERROR_LEVEL,"invalid object\n");
        return ret;
    }

    mutex_lock(&rwlock);

    set_charge_status[5] = gObject;
    /*add 2bytes header( 0x32,0x00 )*/
    set_charge_status[SET_CHARGE_POWER_OFF+2]    = 1;
    set_charge_status[9] = target;
    set_charge_status[SET_CHARGE_REG_ADDR_OFF+2] = addr;
    set_charge_status[SET_CHARGE_REG_LENG_OFF+2] = addrlen;

    memcpy(set_charge_status+SET_CHARGE_REG_DATA_OFF+2, data, datalen);

    set_charge_status[33] = Nanosic_Calculate_crc(set_charge_status,sizeof(set_charge_status));

    rawdata_show("request set charge single addr",set_charge_status,sizeof(set_charge_status));

    ret = Nanosic_charge_i2c_write(set_charge_status, sizeof(set_charge_status));
	if (ret < 0) {
        dbgprint(ERROR_LEVEL,"Nanosic_charge_i2c_write fail, ret = %d\n", ret);
	}

    mutex_unlock(&rwlock);

    return ret;
}

/** ************************************************************************
 *  @func Nanosic_ADSP_Control_Charge_Power
 *  
 *  B0     B1    B2    B3    B4    B5    B6    B7......B32     B33  B34  .......B65
 *  0x32   0x00  0x4E  0x31  0x81  0x38  0x42  DATA0...DATA25  crc   0  .......  0
 *
 *  @brief send command to i2c device driver , Called by adsp device driver
 *  write i2c fail when return < 0 or succee return 66
 ** */
#if 0
int  Nanosic_ADSP_Control_Charge_Power(bool on)
{
    int ret = -1;
    uint8_t set_charge_status[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4E,0x31,0x81,0x38,0x42,25};

    if(gObject == DEV_UNKOWN_OBJECT){
        dbgprint(ERROR_LEVEL,"invalid object\n");
        return ret;
    }

    mutex_lock(&rwlock);
    set_charge_status[5] = gObject;
    set_charge_status[SET_CHARGE_POWER_OFF+2] = on;  /* on :  1 , off : 0 )*/
    set_charge_status[9] = I2C_ADDR_POGO_PIN;
    set_charge_status[SET_CHARGE_REG_ADDR_OFF+2] = 0x01;
    set_charge_status[SET_CHARGE_REG_LENG_OFF+2] = 1;
    set_charge_status[SET_CHARGE_REG_DATA_OFF+2] = on;

    set_charge_status[33] = Nanosic_Calculate_crc(set_charge_status,sizeof(set_charge_status));

    rawdata_show("request control charge power",set_charge_status,sizeof(set_charge_status));
    ret = Nanosic_charge_i2c_write(set_charge_status, sizeof(set_charge_status));
	if (ret < 0) {
        dbgprint(ERROR_LEVEL,"Nanosic_charge_i2c_write fail, ret = %d\n", ret);
	}

    mutex_unlock(&rwlock);

    return ret;
}
EXPORT_SYMBOL(Nanosic_ADSP_Control_Charge_Power);
#endif
/** ************************************************************************
 *  @func Nanosic_ADSP_Read_803_Version
 *
 *  B0     B1    B2    B3    B4    B5    B6    B7    B8    B9.............B65
 *  0x32   0x00  0x4E  0x30  0x80  0x38  0x1   0x0   crc   0............  0
 *
 *  @brief send command to i2c device driver , Called by adsp device driver
 *  write i2c fail when return < 0 or succee return 66
 ** */
int  Nanosic_ADSP_Read_803_Version(void)
{
    int ret = -1;
    uint8_t read_803_vers_cmd[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4F,0x30,FIELD_HOST,FIELD_803X,0x01,0x00,0x00};

    read_803_vers_cmd[8] = Nanosic_Calculate_crc(read_803_vers_cmd,sizeof(read_803_vers_cmd));

    rawdata_show("request 803 version",read_803_vers_cmd,sizeof(read_803_vers_cmd));

    ret = Nanosic_charge_i2c_write(read_803_vers_cmd, sizeof(read_803_vers_cmd));
    if (ret < 0)
    {
        //dbgprint(ERROR_LEVEL, "i2c write fail, ret = %d, size = %d", ret, sizeof(read_803_vers_cmd));
    }

    return ret;
}
EXPORT_SYMBOL(Nanosic_ADSP_Read_803_Version);

/** ************************************************************************
 *  @func Nanosic_ADSP_register_cb
 *
 *  @brief register read callback function for adsp , Called by adsp device driver
 *
 ** */
int  Nanosic_ADSP_register_cb(ADSP_read_cb cb)
{
    if(IS_ERR_OR_NULL(cb)){
        dbgprint(ERROR_LEVEL,"invalid cb\n");
        return -1;
    }

    if(g_ADSP_read_cb){
        dbgprint(ERROR_LEVEL,"already registered\n");
        return -1;
    }

    g_ADSP_read_cb = cb;

    dbgprint(ERROR_LEVEL,"ADSP callback registered\n");

    return 0;
}

static void Nanosic_Analyse_RawData_work(struct work_struct *work)
{
    struct nanochg *nodev = container_of(work, struct nanochg, analyse_rawdata_work.work);

    char    report_id;
    char    protocol;
    char    source;
    char    object;
    char    command;
    int  retval = 0;
    char *rawdata_buf;
    char len, subcmd, cmd_state, rev,keyboard_state, dev_type, subcmd_status;
    bool connected = false;
    static bool flag = false, flag_pogo = false, flag_keyboard = false;
    char *dev_name = "unknown";
    char pswitch,reserve,ic_type,reg_addr,reg_len,reg_data[19],state_flag;
    bool wpc_present = 0, usb_present;
    int real_type = 0, wls_online = 0, otg_boost_enable = 0, otg_gate_enable = 0;
    u16 ntc, power;
    u8 cc_connect_stat, default_typec_power;

    u16     mcu_version;
    u16     dev_version;
    u8      colour;
    u16     pid;

    if (IS_ERR_OR_NULL(nodev))
    {
        dbgprint(ERROR_LEVEL, "nodev ERROR");
        retval = -ENODEV;
        return;
    }
	rawdata_buf = (char *)buf_tmp;

    STREAM_TO_UINT8(report_id, rawdata_buf);
    STREAM_TO_UINT8(protocol, rawdata_buf);
    STREAM_TO_UINT8(source, rawdata_buf);
    STREAM_TO_UINT8(object, rawdata_buf);
    STREAM_TO_UINT8(command, rawdata_buf);

    rawdata_show("recv data1", buf_tmp, length);

    switch(command) {
        case CMD_KEYBOARD_CONN_STATUS_RSP:
        {
            mutex_lock(&rwlock);

            STREAM_TO_UINT8(len, rawdata_buf);
            STREAM_TO_UINT8(subcmd, rawdata_buf);
            STREAM_TO_UINT8(cmd_state, rawdata_buf);
            STREAM_TO_UINT8(rev, rawdata_buf);
            STREAM_TO_UINT8(keyboard_state, rawdata_buf);
            STREAM_TO_UINT8(dev_type, rawdata_buf);  /*1:keyboard  2:zhijia*/
            rawdata_show("recv data1-conn", buf_tmp, length);
            if (buf_tmp[4] != CMD_KEYBOARD_CONN_STATUS_RSP) {
                mutex_unlock(&rwlock);
                dbgprint(ERROR_LEVEL, "not connect cmd, ignore,cmd:%x\n", buf_tmp[4]);
                break;
            }
            gObject = DEV_UNKOWN_OBJECT;

            if (cmd_state == 0) {
                if (((keyboard_state & 0x3) == 0x3) || ((keyboard_state & 0x3) == 0x1)) {
                    dev_name = "keyboard";
                    if ((keyboard_state & 0x60) == 0x20) {
                        connected = true;
                        gObject = DEV_KEYBOARD_OBJECT;
                    } else {
                        dbgprint(ERROR_LEVEL, "fail to connect keyboard device because no data\n");
                    }
                } else if ((keyboard_state & 0x3) == 0x0) {
                    if ((keyboard_state & 0x68) == 0x28) {
                        connected = true;
                        gObject = DEV_ZHIJIA_OBJECT;
                        dev_name = "zhijia";
                    } else {
                        dbgprint(ERROR_LEVEL, "fail to connect zhijia device\n");
                    }
                }
                g_nodev->gObject = gObject;
                dbgprint(DEBUG_LEVEL, "%s attach_detect connected %s\n", dev_name, connected ? "true" : "fasle");
            }

            mutex_unlock(&rwlock);

            if (g_battmngr_noti && (gObject == DEV_ZHIJIA_OBJECT)) {
                mutex_lock(&g_battmngr_noti->notify_lock);
                dbgprint(ERROR_LEVEL, "Send zhijia connect info.\n");

                g_battmngr_noti->mcu_msg.kb_attach = connected;
                g_battmngr_noti->mcu_msg.object_type = gObject;
                g_battmngr_noti->mcu_msg.msg_type = BATTMNGR_MSG_MCU_TYPE;
                battmngr_notifier_call_chain(BATTMNGR_EVENT_MCU, g_battmngr_noti);
                mutex_unlock(&g_battmngr_noti->notify_lock);
            } else {
                dbgprint(ERROR_LEVEL, "ADSP read cb is null\n");
            }

            if (gObject == DEV_ZHIJIA_OBJECT && !flag_pogo) {
                platform_class_buckchg_ops_get_otg_boost_enable_status(MAIN_BUCK_CHARGER, &otg_boost_enable);
                platform_class_buckchg_ops_get_otg_gate_enable_status(MAIN_BUCK_CHARGER, &otg_gate_enable);
                (void)platform_class_cp_get_int_stat(CP_ROLE_MASTER, VUSB_PRESENT_STAT, &usb_present);
                if (usb_present && (!otg_gate_enable || !otg_boost_enable)) {
                  dbgprint(ERROR_LEVEL, "wire charge, ignore\n");
                  break;
                }
                flag_pogo = true;
                platform_class_wireless_is_present(WIRELESS_ROLE_MASTER, &wls_online);
                (void)platform_class_cp_get_int_stat(CP_ROLE_MASTER, VWPC_PRESENT_STAT, &wpc_present);
                if (wpc_present&& !wls_online) {
                    platform_class_cp_set_pogo_plugin(CP_ROLE_MASTER, true);
                    mca_event_block_notify(MCA_EVENT_TYPE_CHARGER_CONNECT, MCA_EVENT_USB_CONNECT, NULL);
                    real_type = XM_CHARGER_TYPE_POGO;
                    mca_event_block_notify(MCA_EVENT_TYPE_CHARGE_TYPE, MCA_EVENT_CHARGE_TYPE_CHANGE, &real_type);
                    nano_report_pogo_plugin_state(true);
                    g_nodev->last_temp_index = 4;
                    g_nodev->last_map_index = 0;
                    g_nodev->index_init = 1;
                    schedule_delayed_work(&g_nodev->power_distribution_work, 0);
                }
            } else if (gObject == DEV_UNKOWN_OBJECT && flag_pogo) {
                flag_pogo = false;
                platform_class_cp_set_pogo_plugin(CP_ROLE_MASTER, false);
                mca_event_block_notify(MCA_EVENT_TYPE_CHARGER_CONNECT, MCA_EVENT_USB_DISCONNECT, NULL);
                nano_report_pogo_plugin_state(false);
                cancel_delayed_work_sync(&g_nodev->power_distribution_work);
                default_typec_power = 0x18;
                Nanosic_ADSP_Set_Charge_Status_Single(0x3C, 1, &default_typec_power, sizeof(default_typec_power), I2C_ADDR_C3);
            }
            if (gObject == DEV_KEYBOARD_OBJECT && !flag_keyboard) {
                flag_keyboard = true;
                platform_class_cp_set_keyboard_plugin(CP_ROLE_MASTER, true);
            } else if (gObject == DEV_UNKOWN_OBJECT && flag_keyboard) {
                flag_keyboard = false;
                platform_class_cp_set_keyboard_plugin(CP_ROLE_MASTER, false);
            }
            dbgprint(ERROR_LEVEL, "wpc_present:%d,flag_pogo:%d, flag_keyboard:%d, wls_online:%d, gObject:%d\n", wpc_present, flag_pogo, flag_keyboard, wls_online, gObject);
        }
        break;
        case CMD_KEYBOARD_READ_VERSION_RSP:
        {
            STREAM_TO_UINT8(len, rawdata_buf);
            STREAM_TO_UINT16(mcu_version, rawdata_buf);
            STREAM_TO_UINT16(dev_version, rawdata_buf);
            STREAM_TO_UINT8(colour, rawdata_buf);
            STREAM_TO_UINT16(pid, rawdata_buf);
            STREAM_TO_UINT8(dev_type, rawdata_buf);
            dbgprint(DEBUG_LEVEL,"dev_type %d dev_version %d\n",dev_type,dev_version);
            break;
        }
        case CMD_MCU_RSP:
        {
            STREAM_TO_UINT8(len, rawdata_buf);
            STREAM_TO_UINT8(subcmd, rawdata_buf);
            STREAM_TO_UINT8(subcmd_status, rawdata_buf);

            //rawdata_show("got mcu rsp event", rawdata_buf, sizeof(rawdata_buf));
        }
        break;
        case CMD_READ_CHARGE_STATUS_RSP:
        {
            STREAM_TO_UINT8(len, rawdata_buf);
            STREAM_TO_UINT8(pswitch, rawdata_buf);
            STREAM_TO_UINT8(reserve, rawdata_buf);
            STREAM_TO_UINT8(ic_type, rawdata_buf);
            STREAM_TO_UINT8(reg_addr, rawdata_buf);
            STREAM_TO_UINT8(reg_len, rawdata_buf);
            memcpy(reg_data, rawdata_buf, reg_len);
            rawdata_buf += 8;
            STREAM_TO_UINT8(state_flag, rawdata_buf);
            STREAM_TO_UINT16(ntc, rawdata_buf);
            STREAM_TO_UINT16(power, rawdata_buf);
            g_nodev->ntc = ntc;
            g_nodev->power_in = power;
            if (reserve == I2C_ADDR_C3 && reg_addr == 0x2) {
                cc_connect_stat = (buf_tmp[11] & 0x60) >> 5;
                g_nodev->typec_plugin = ((cc_connect_stat == 1) || (cc_connect_stat == 2));
                dbgprint(ERROR_LEVEL, "typec_plugin:%d, ntc:%d\n", g_nodev->typec_plugin, g_nodev->ntc);
            }

            //rawdata_show("got read charge event", rawdata_buf, sizeof(rawdata_buf));
            if (g_battmngr_noti && (gObject == DEV_ZHIJIA_OBJECT)) {
                mutex_lock(&g_battmngr_noti->notify_lock);
                dbgprint(ERROR_LEVEL, "Send charge info.\n");
                if (gObject == DEV_KEYBOARD_OBJECT)
                    assign_mcu_data(&g_battmngr_noti->mcu_msg.mcu_data, buf_tmp);
                else
                    assign_holder_mcu_data(&g_battmngr_noti->mcu_msg.mcu_data, buf_tmp);
                deal_report_event(buf_tmp);
                g_battmngr_noti->mcu_msg.object_type = gObject;
                g_battmngr_noti->mcu_msg.msg_type = BATTMNGR_MSG_MCU_TYPE;
                battmngr_notifier_call_chain(BATTMNGR_EVENT_MCU, g_battmngr_noti);
                mutex_unlock(&g_battmngr_noti->notify_lock);
            } else {
                dbgprint(ERROR_LEVEL, "ADSP read cb is null\n");
            }
        }
        break;
        case CMD_SET_CHARGE_REG_RSP:
        {
            STREAM_TO_UINT8(len,rawdata_buf);
            STREAM_TO_UINT8(subcmd_status,rawdata_buf);
            //rawdata_show("got set charge cmd rsp event", rawdata_buf, sizeof(rawdata_buf));
        }
        break;
        default:
            dbgprint(ERROR_LEVEL, "unkown cmd rsp code\n");
        break;
    }

	//if (g_bcdev) {//zj_1118
	if (1) {
		if (gObject == DEV_KEYBOARD_OBJECT && !flag) {
			flag = true;
			//qti_set_keyboard_plugin(1);//zj_1118
		}
		else if (gObject == DEV_UNKOWN_OBJECT && flag) {
			flag = false;
			//qti_set_keyboard_plugin(0);//zj_1118
		}
	}

    return;
}

static void Nanosic_Power_Distribution_work(struct work_struct *work)
{
    struct nanochg *nodev = container_of(work, struct nanochg, power_distribution_work.work);
    uint8_t i = 0, temp_index = 0, typec_target_power = 0;
    int pogo_target_ibus = 0;
    static uint8_t power_cnt_l = 0, map_flag = 0, power_flag = 0, last_typec_plugin = 0;
    ktime_t time_now_ms;
    static ktime_t time_last_ms;
    static ktime_t time_gap;

    struct temp_power_map *temp_power = NULL;
    struct temp_power_map temp_power_45W[] = {
        {0, 422, 384, TYPEC_POWER_7P5W, POGO_IBUS_0P375, POGO_IBUS_0P75},
        {1, 492, 449, TYPEC_POWER_10W,  POGO_IBUS_0P75, POGO_IBUS_1P25},
        {2, 571, 523, TYPEC_POWER_12W,  POGO_IBUS_1P25, POGO_IBUS_1P75},
        {3, 1800, 523, TYPEC_POWER_15W,  POGO_IBUS_1P5, POGO_IBUS_2P},
    };
    struct temp_power_map temp_power_30W[] = {
        {0, 422, 384, TYPEC_POWER_7P5W, POGO_IBUS_0P375, POGO_IBUS_0P75},
        {1, 492, 449, TYPEC_POWER_10W,  POGO_IBUS_0P75, POGO_IBUS_1P25},
        {2, 571, 523, TYPEC_POWER_10W,  POGO_IBUS_1P, POGO_IBUS_1P5},
        {3, 1800, 523, TYPEC_POWER_10W, POGO_IBUS_1P, POGO_IBUS_1P5},
    };
    dbgprint(ERROR_LEVEL, "Power_Distribution work run\n");
    if (g_nodev->index_init) {
        power_cnt_l = 0;
        map_flag = 0;
        power_flag = 0;
        time_last_ms = 0;
        time_gap = 0;
        g_nodev->index_init = 0;
    }
    time_now_ms = ktime_get();

    Nanosic_ADSP_Read_Charge_Status_Single(0xD1,1, I2C_ADDR_POGO_PIN);
    msleep(5);
    Nanosic_ADSP_Read_Charge_Status_Single(0x3C,1, I2C_ADDR_C3);
    msleep(5);
    Nanosic_ADSP_Read_Charge_Status_Single(0x02,1, I2C_ADDR_C3);
    msleep(5);
    dbgprint(ERROR_LEVEL, "ntc:%d, power:%d\n", nodev->ntc, nodev->power_in);

    if (nodev->power_in < POGO_VOLTAGE_THRESHOLD) {
        power_cnt_l++;
        if (power_cnt_l > 5 && !power_flag) {
            power_flag = 1;
            power_cnt_l = 0;
            time_last_ms = ktime_get();
        }
        if (power_cnt_l > 5) {
            power_cnt_l = 0;
        }
    } else {
        power_cnt_l = 0;
        power_flag = 0;
    }
    if (power_flag) {
        time_gap = ktime_to_ms(ktime_sub(time_now_ms, time_last_ms));
        time_gap /= 1000;
        if (time_gap > MAX_45W_TIME_10M) {
            map_flag = 1;
        }
    } else {
        map_flag = 0;
    }
    dbgprint(ERROR_LEVEL, "time_now_ms:%lld, time_last_ms:%lld, time_gap:%lld\n", time_now_ms, time_last_ms, time_gap);
    temp_power = map_flag ? &temp_power_30W[0] : &temp_power_45W[0];

    for (i = 0; i < 4; i++) {
        if (nodev->ntc < temp_power[i].temp_h) {
            typec_target_power = temp_power[i].typec_power;
            pogo_target_ibus = g_nodev->typec_plugin ? temp_power[i].pogo_ibus : temp_power[i].single_pogo_ibus;
            temp_index = i;
            break;
        }
        dbgprint(ERROR_LEVEL, "temp_index:%d, i:%d\n", temp_index, i);
    }
    if (i == 4) {
        typec_target_power = temp_power[3].typec_power;
        temp_index = 3;
        pogo_target_ibus = g_nodev->typec_plugin ? temp_power[3].pogo_ibus : temp_power[3].single_pogo_ibus;
    }
    if ((temp_index - g_nodev->last_temp_index == 1) && nodev->ntc < temp_power[g_nodev->last_temp_index].temp_l) {
        typec_target_power = temp_power[g_nodev->last_temp_index].typec_power;
        temp_index = g_nodev->last_temp_index;
        pogo_target_ibus = g_nodev->typec_plugin ? temp_power[g_nodev->last_temp_index].pogo_ibus : temp_power[g_nodev->last_temp_index].single_pogo_ibus;
        dbgprint(ERROR_LEVEL, "hold on, temp_index:%d\n", temp_index);
    }
    if (g_nodev->last_temp_index != temp_index || g_nodev->last_map_index != map_flag || last_typec_plugin != g_nodev->typec_plugin) {
        g_nodev->last_temp_index = temp_index;
        g_nodev->last_map_index = map_flag;
        last_typec_plugin = g_nodev->typec_plugin;

        if (g_nodev->typec_plugin) {
            Nanosic_ADSP_Set_Charge_Status_Single(0x3C, 1, &typec_target_power, sizeof(typec_target_power), I2C_ADDR_C3);
        }
        mca_event_block_notify(MCA_EVENT_TYPE_HW_INFO, MCA_EVENT_POGO_IBUS_CHANGE, &pogo_target_ibus);
        dbgprint(ERROR_LEVEL, "typec_target_power:%d\n", typec_target_power);
    }
    dbgprint(ERROR_LEVEL, "ntc:%d, typec_target_power:%d, pogo_target_ibus:%d, temp_index:%d, last_temp_index:%d, power_flag:%d, map_flag:%d\n", nodev->ntc, typec_target_power, pogo_target_ibus, temp_index, g_nodev->last_temp_index, power_flag, map_flag);
    
    schedule_delayed_work(&nodev->power_distribution_work, msecs_to_jiffies(5000));
}
/** ************************************************************************
 *  @func Nanosic_platform_probe
 *
 *  @brief
 *
 ** */
static int Nanosic_platform_probe(struct platform_device *pdev)
{
	struct nanochg *nodev;

	pr_err("%s: Start\n", __func__);

    nodev = devm_kzalloc(&pdev->dev, sizeof(*nodev), GFP_KERNEL);
    if (!nodev) {
        dbgprint(DEBUG_LEVEL,"assign nodev is fail! \n");
        return -1;
    }
    platform_set_drvdata(pdev, nodev);

    mutex_init(&rwlock);
    mutex_init(&nodev->nano_i2c_write_mutex);

    nodev->dev = &pdev->dev;
    battmngr_device_register("nano_ops", nodev->dev, nodev, &nano_op, NULL);

    INIT_DELAYED_WORK(&nodev->analyse_rawdata_work, Nanosic_Analyse_RawData_work);
    INIT_DELAYED_WORK(&nodev->power_distribution_work, Nanosic_Power_Distribution_work);

    g_nodev = nodev;
	dbgprint(DEBUG_LEVEL,"nanosic_platform_probe complete\n");

	return 0;
}

/** ************************************************************************
 *  @func nanosic_platform_remove
 *
 *  @brief
 *
 ** */
static int Nanosic_platform_remove(struct platform_device *pdev)
{
	struct nanochg *nodev = platform_get_drvdata(pdev);

    //kfree(mcuData);

	cancel_delayed_work_sync(&nodev->analyse_rawdata_work);
	cancel_delayed_work_sync(&nodev->power_distribution_work);
	devm_kfree(&pdev->dev,nodev);
	platform_set_drvdata(pdev, NULL);

	dbgprint(DEBUG_LEVEL,"Nanosic_platform_remove complete\n");

	return 0;
}

#ifdef CONFIG_PM
static int Nanosic_platform_resume(struct platform_device *pdev)
{
	struct nanochg *nodev = platform_get_drvdata(pdev);
	cancel_delayed_work_sync(&nodev->analyse_rawdata_work);
	cancel_delayed_work_sync(&nodev->power_distribution_work);

	return 0;
}

static int Nanosic_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct nanochg *nodev = platform_get_drvdata(pdev);
	cancel_delayed_work_sync(&nodev->analyse_rawdata_work);
	cancel_delayed_work_sync(&nodev->power_distribution_work);

	return 0;
}
#endif

static const struct of_device_id nanosic_platform_ids[] = {
	{ .compatible = "nanosic,platform" },
	{ /* Sentinel */ }
};

static struct platform_driver nanosic_platform_driver = {
	.probe = Nanosic_platform_probe,
	.remove	= Nanosic_platform_remove,
#ifdef CONFIG_PM
	.suspend = Nanosic_platform_suspend,
	.resume = Nanosic_platform_resume,
#endif
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "nanosic,platform",
		.of_match_table	= nanosic_platform_ids,
	},
};

/** ************************************************************************
 *  @func nanosic_platform_init
 *
 *  @brief
 *
 ** */
static int __init Nanosic_platform_init(void)
{
    int retval = 0;

    retval = platform_driver_register(&nanosic_platform_driver);
    if(retval < 0)
        dbgprint(ERROR_LEVEL,"platform init %d\n",retval);

    return retval;

}

/** ************************************************************************
 *  @func nanosic_platform_exit
 *
 *  @brief
 *
 ** */
static void __exit Nanosic_platform_exit(void)
{
    platform_driver_unregister(&nanosic_platform_driver);
}

module_init(Nanosic_platform_init);
module_exit(Nanosic_platform_exit);

EXPORT_SYMBOL_GPL(Nanosic_ADSP_register_cb);
EXPORT_SYMBOL_GPL(Nanosic_ADSP_Set_Charge_Status);
EXPORT_SYMBOL_GPL(Nanosic_ADSP_Set_Charge_Status_Single);

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_DESCRIPTION("Nanosic platform Driver");
MODULE_LICENSE("GPL");

