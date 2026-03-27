/*
 * Copyright (C) 2025-2026, X-Ring technologies Inc., All rights reserved.
 *
 * Description: npu usage agreement for mutex between different cores, such as acpu and sensorhub
 */

#ifndef __NPU_USAGE_MUTEX_H__
#define __NPU_USAGE_MUTEX_H__

#define NPU_USAGE_MUTEX_REG_ADDR_SH (0x415087b8)

#define NPU_MCU_BOOT_STATUS_START_BIT (0)
#define NPU_MCU_BOOT_STATUS_END_BIT (1)
#define NPU_MCU_BOOT_STATUS_MASK ((1 << (NPU_MCU_BOOT_STATUS_END_BIT - NPU_MCU_BOOT_STATUS_START_BIT + 1)) - 1)

#define NPU_USAGE_STATUS_START_BIT (2)
#define NPU_USAGE_STATUS_END_BIT (3)
#define NPU_USAGE_STATUS_MASK ((1 << (NPU_USAGE_STATUS_END_BIT - NPU_USAGE_STATUS_START_BIT + 1)) - 1)

/****************************************************************************
 * Description:
 *  1) enum npu_mcu_boot_status:
 *      NPU_MCU_BOOT_STATUS_PWDN: power down.
 *      NPU_MCU_BOOT_STATUS_PWDNING: is doing power down.
 *      NPU_MCU_BOOT_STATUS_PWUPING: is doing power up.
 *      NPU_MCU_BOOT_STATUS_PWUP: power uped and mcu booted.
 *
 ****************************************************************************/
enum npu_mcu_boot_status_s {
	NPU_MCU_BOOT_STATUS_PWDN = 0,
	NPU_MCU_BOOT_STATUS_PWDNING,
	NPU_MCU_BOOT_STATUS_PWUPING,
	NPU_MCU_BOOT_STATUS_PWUP,
};

/****************************************************************************
 * Description:
 *  1) enum npu_usage_status:
 *      NPU_USAGE_IDLE: npu is idle
 *      NPU_USAGE_ACPU: acpu is using npu
 *      NPU_USAGE_SENSORHUB: sensorhub is using npu
 ****************************************************************************/
enum npu_usage_status {
	NPU_USAGE_IDLE = 0,
	NPU_USAGE_ACPU,
	NPU_USAGE_SENSORHUB,
};

#endif /* __NPU_USAGE_MUTEX_H__*/
