/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __HQ_CHARGER_MANAGER_MISC_H__
#define __HQ_CHARGER_MANAGER_MISC_H__

/******************************************************************************/
/*                  STOP CHARGE PROTECT FUNCTIONS DEFINE                          */
/******************************************************************************/
/*
 * Func Name:  STOP_CHARGE_PROTECT
 * Func Owner: qiansuhao.wb@huaqin.com
 * Func Date:  2024.12.18
 * Func Desc:  This function include warm stop charge and cold stop charge feature
 * warm stop charge: if tbat > WARM_STOP_CHARGE_TBAT and vbat > WARM_STOP_CHARGE_VBAT,
 * trigger warm stop chargeif battery temperature above 45 degrees and battery voltage
 * cold stop charge: if tbat < COLD_STOP_CHARGE_TBAT, trigger cold stop charge
 * NOTE: /sys/power/supply/battery/status should be charging in warm stop charge state
 */
#define STOP_CHARGE_PROTECT

#define WARM_STOP_CHARGE_TBAT       (451)      /* set the same as jeita hot temperature lower limit */
#define COLD_STOP_CHARGE_TBAT       (-100)        /* set the same as jeita cold temperature lower limit*/
#define WARM_STOP_CHARGE_VBAT       (4100)     /* set the same as jeita hot temperature fv */
#define HOT_STOP_CHARGE_TBAT        (551)
#define HOT_RECOVER_CHARGE_TBAT     (530)
#define RECHG_VBAT_OFFSET           (150)
#define RECHG_TBAT_WARM_OFFSET      (20)
#define RECHG_TBAT_COLD_OFFSET      (10)

/******************************************************************************/
/*                 REGISTER DUMP DEBUG FUNCTIONS DEFINE                       */
/******************************************************************************/
/*
 * Func Name:  REGISTER_DUMP_DEBUG
 * Func Owner: pengyuzhe@huaqin.com
 * Func Date:  2025.03.03
 * Func Desc:  This function used to monitor charging status and trigger dump registers
 * if error status met.
 */
//#define REGISTER_DUMP_DEBUG

#endif /* __HQ_CHARGER_MANAGER_MISC_H__ */
