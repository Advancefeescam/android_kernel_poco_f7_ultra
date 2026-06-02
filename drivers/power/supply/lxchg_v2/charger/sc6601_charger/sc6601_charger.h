/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2022 - 2023 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef __SC6601_BUCK_CHARGER_H__
#define __SC6601_BUCK_CHARGER_H__


// Reg DEFINE
enum {
    SC6601_REG_DEVICE_ID = 0X00,
    SC6601_REG_HK_GEN_STATE,
    SC6601_REG_HK_GEN_FLG,
    SC6601_REG_HK_GEN_MASK,
    SC6601_REG_VAC_VBUS_OVP,
    SC6601_REG_TSBUS_FAULT,
    SC6601_REG_TSBAT_FAULT,
    SC6601_REG_HK_CTRL,
    SC6601_REG_HK_INT_STAT = 0X09,
    SC6601_REG_HK_INT_FLG,
    SC6601_REG_HK_INT_MASK,
    SC6601_REG_HK_FLT_STAT,
    SC6601_REG_HK_FLT_FLG,
    SC6601_REG_HK_FLT_MASK,
    SC6601_REG_HK_ADC_CTRL,
    SC6601_REG_HK_IBUS_ADC = 0X11,
    SC6601_REG_HK_VBUS_ADC = 0X13,
    SC6601_REG_HK_VAC_ADC = 0X15,
    SC6601_REG_HK_VBATSNS_ADC = 0X17,
    SC6601_REG_HK_VBAT_ADC = 0X19,
    SC6601_REG_HK_IBAT_ADC = 0X1B,
    SC6601_REG_HK_VSYS_ADC = 0X1D,
    SC6601_REG_HK_TSBUS_ADC = 0X1F,
    SC6601_REG_HK_TSBAT_ADC = 0X21,
    SC6601_REG_HK_TDIE_ADC = 0X23,
    SC6601_REG_HK_BATID_ADC = 0X25,
    /****** Buck charger Reg ******/
    SC6601_REG_VSYS_MIN = 0x30,
    SC6601_REG_VBAT,
    SC6601_REG_ICHG_CC,
    SC6601_REG_VINDPM,
    SC6601_REG_IINDPM,
    SC6601_REG_ICO_CTRL,
    SC6601_REG_PRECHARGE_CTRL,
    SC6601_REG_TERMINATION_CTRL,
    SC6601_REG_RECHARGE_CTRL,
    SC6601_REG_VBOOST_CTRL,
    SC6601_REG_PROTECTION_DIS,
    SC6601_REG_RESET_CTRL,
    SC6601_REG_CHG_CTRL,
    SC6601_REG_CHG_INT_STAT = 0x41,
    SC6601_REG_CHG_INT_FLG = 0x44,
    SC6601_REG_CHG_INT_MASK = 0x47,
    SC6601_REG_CHG_FLT_STAT = 0x50,
    SC6601_REG_CHG_FLT_FLG = 0x52,
    SC6601_REG_CHG_FLT_MASK = 0x54,
    SC6601_REG_JEITA_TEMP = 0x56,
    SC6601_REG_BUCK_STAT,
    SC6601_REG_BUCK_FLG,
    SC6601_REG_BUCK_MASK,
    SC6601_REG_Internal = 0x5D,
    /*******LED Reg *******/
    SC6601_REG_LED_CTRL = 0x80,
    SC6601_REG_FLED1_BR_CTR,
    SC6601_REG_FLED2_BR_CTR,
    SC6601_REG_FLED_TIMER,
    SC6601_REG_TLED1_BR_CTR,
    SC6601_REG_TLED2_BR_CTR,
    SC6601_REG_LED_PRO,
    SC6601_REG_LED_STATE,
    SC6601_REG_LED_FLAG = 0x89,
    SC6601_REG_LED_MASK,
    /****** DPDM Reg ******/
    SC6601_REG_DPDM_EN = 0x90,
    SC6601_REG_DPDM_CTRL,
    SC6601_REG_DPDM_QC_CTRL,
    SC6601_REG_DPDM_TFCP_CTRL,
    SC6601_REG_DPDM_INT_FLAG,
    SC6601_REG_DPDM_INT_MASK,
    SC6601_REG_QC3_INT_FLAG,
    SC6601_REG_QC3_INT_MASK,
    SC6601_REG_DP_STAT,
    SC6601_REG_DM_STAT,
    SC6601_REG_DPDM_INTERNAL,
    SC6601_REG_DPDM_CTRL_2 = 0x9D,
    SC6601_REG_DPDM_NONSTD_STAT,
    SC6601_REG_MAX,
};

/***************** hk **********************/
#define SC6601_TDIE_ADC_DIS                BIT(0)
#define SC6601_TSBAT_ADC_DIS               BIT(1)
#define SC6601_TSBUS_ADC_DIS               BIT(2)
#define SC6601_ADC_EN                      BIT(15)

static const int sc6601_chg_wd_time[] = {
    0, 500, 1000, 2000, 20000, 40000, 80000, 160000,
};

#define SC6601_HK_V2X_UVLO_MASK             BIT(7)
#define SC6601_HK_POR_MASK                  BIT(6)
#define SC6601_HK_RESET_MASK                BIT(5)
#define SC6601_HK_ADC_DONE_MASK             BIT(4)
#define SC6601_HK_REGN_OK_MASK              BIT(3)
#define SC6601_HK_VBAT_UVLO_MASK            BIT(2)
#define SC6601_HK_VBUS_PRESENT_MASK         BIT(1)
#define SC6601_HK_VAC_PRESENT_MASK          BIT(0)

/***************** charger **********************/
#define SC6601_BUCK_TDIE_REG_MASK           BIT(22)
#define SC6601_BUCK_TSBAT_COOL_MASK         BIT(21)
#define SC6601_BUCK_TSBAT_WARM_MASK         BIT(20)
#define SC6601_BUCK_ICO_MASK                BIT(18)
#define SC6601_BUCK_IINDPM_MASK             BIT(17)
#define SC6601_BUCK_VINDPM_MASK             BIT(16)
#define SC6601_BUCK_CHG_MASK                BIT(13)
#define SC6601_BUCK_BOOST_OK_MASK           BIT(12)
#define SC6601_BUCK_VSYSMIN_MASK            BIT(11)
#define SC6601_BUCK_QB_ON_MASK              BIT(10)
#define SC6601_BUCK_BATFET_MASK             BIT(8)
#define SC6601_BUCK_VSYS_SHORT_MASK         BIT(4)
#define SC6601_BUCK_VSLEEP_BUCK_MASK        BIT(3)
#define SC6601_BUCK_VBAT_DPL_MASK           BIT(2)
#define SC6601_BUCK_VBAT_LOW_BOOST_MASK     BIT(1)
#define SC6601_BUCK_VBUS_GOOD_MASK          BIT(0)


#define SC6601_QC_EN                       BIT(0)
#define SC6601_BUCK_FLAG_BOOST_GOOD        BIT(12)
#define SC6601_BUCK_FLAG_VBUS_GOOD         BIT(0)
#define SC6601_BUCK_GET_CHG_STATE(state)   ((state >> 13) & 0x7)

#define SC6601_BUCK_VBAT_OFFSET            3840
#define SC6601_BUCK_VBAT_STEP              8
#define SC6601_BUCK_VBAT_MIN               3840
#define SC6601_BUCK_VBAT_MAX               4856

#define SC6601_BUCK_ICHG_OFFSET            0
#define SC6601_BUCK_ICHG_STEP              50
#define SC6601_BUCK_ICHG_MIN               0
#define SC6601_BUCK_ICHG_MAX               3600

#define SC6601_BUCK_IINDPM_OFFSET          100
#define SC6601_BUCK_IINDPM_STEP            50
#define SC6601_BUCK_IINDPM_MIN             100
#define SC6601_BUCK_IINDPM_MAX             3250

#define SC6601_BUCK_IINDPM_ICO_OFFSET      100
#define SC6601_BUCK_IINDPM_ICO_STEP        50

#define SC6601_BUCK_IPRECHG_OFFSET         50
#define SC6601_BUCK_IPRECHG_STEP           50

#define SC6601_BUCK_ITERM_OFFSET           100
#define SC6601_BUCK_ITERM_STEP             50
#define SC6601_BUCK_ITERM_MIN              100
#define SC6601_BUCK_ITERM_MAX              1650


#define SC6601_BUCK_OTG_VOLT_OFFSET        3900
#define SC6601_BUCK_OTG_VOLT_STEP          100
#define SC6601_BUCK_OTG_VOLT_MIN           3900
#define SC6601_BUCK_OTG_VOLT_MAX           5800

#define SC6601_BUCK_PRE_CURR_OFFSET        50
#define SC6601_BUCK_PRE_CURR_STEP          50
#define SC6601_BUCK_PRE_CURR_MIN           50
#define SC6601_BUCK_PRE_CURR_MAX           800

#define SC6601_VCLAMP_48MV	3

static const int vsys_min[] = {
    2600, 2800, 3000, 3200, 3400, 3500, 3600, 3700,
};

static const int boost_curr[] = {
    500, 900, 1300, 1500, 2100, 2500, 2900, 3250,
};

static const int vindpm[] = {
    4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700,
    7600, 8200, 8400, 8600, 10000, 10500, 10700,
};

static const int prechg_volt[] = {
    2900, 3000, 3100, 3200,
};

static const int rechg_volt[] = {
    100, 200, 300, 400,   /* below VBAT_REG */
};

enum sc6601_bc12_type {
	BC12_TYPE_NONE,
	BC12_TYPE_SDP,
	BC12_TYPE_CDP,
	BC12_TYPE_DCP,
	BC12_TYPE_HVDCP,
	BC12_TYPE_FLOAT,
	BC12_TYPE_NONSTANDARD,
	BC12_TYPE_HVDCP_3,
	BC12_TYPE_HVDCP_3p5,
};

/****************** led ****************************/
#define SC6601_LED_FLAG_FLASH_DONE         (BIT(7) | BIT(6))
#define SC6601_LED_CURR_STEP               12500
#define SC6601_LED_CURR_OFFSET             25000

#define SC6601_LED_FLASH_CURR_MIN      25
#define SC6601_LED_FLASH_CURR_MAX      1500
#define SC6601_LED_FLASH_CURR_OFFSET   25000
#define SC6601_LED_FLASH_CURR_STEP     12500

#define SC6601_LED_TORCH_CURR_MIN      25
#define SC6601_LED_TORCH_CURR_MAX      500
#define SC6601_LED_TORCH_CURR_OFFSET   25000
#define SC6601_LED_TORCH_CURR_STEP     12500

#define SC6601_LED_VBAT_MIN_OFFSET      2800
#define SC6601_LED_VBAT_MIN_STEP        100
#define SC6601_LED_VBAT_MIN_MIN         2800
#define SC6601_LED_VBAT_MIN_MAX         3500

#define SC6601_DISABLE_FLASH_TIMEOUT        -1
#define SC6601_DEFAULT_FLASH_TIMEOUT        200

#define SC6601_LED_FTIMEOUT1_MASK       BIT(7)
#define SC6601_LED_FTIMEOUT2_MASK       BIT(6)
#define SC6601_LED_OVP_MASK             BIT(5)
#define SC6601_LED1_SHORT_MASK          BIT(4)
#define SC6601_LED2_SHORT_MASK          BIT(3)
#define SC6601_LED_TORCH_UVP_MASK       BIT(2)
#define SC6601_LED_TORCH_OVP_MASK       BIT(1)
#define SC6601_LED_VBAT_LOW_MASK        BIT(0)

static const int led_time[] = {
    10, 20, 30, 40, 50, 60, 70, 80, 90,
    100, 150, 200, 250, 300, 350, 400,
};

/****************** dpdm ****************************/
#define SC6601_DPDM_FORCE_INDET            BIT(7)
#define SC6601_DPDM_AUTO_INDET_EN          BIT(6)
#define SC6601_DPDM_HVDCP_EN               BIT(5)

#define SC6601_DPDM_BC12_DETECT_DONE       BIT(2)
#define SC6601_DPDM_BC1_2_DONE_MASK        BIT(2)
#define SC6601_DPDM_DP_OVP_MASK            BIT(1)
#define SC6601_DPDM_DM_OVP_MASK            BIT(0)



struct buck_init_data {
	u32 vsyslim;
	u32 batsns_en;
	u32 vbat;
	u32 ichg;
	u32 vindpm;
	u32 iindpm_dis;
	u32 iindpm;
	u32 ico_enable;
	u32 iindpm_ico;
	u32 vprechg;
	u32 iprechg;
	u32 iterm_en;
	u32 iterm;
	u32 rechg_dis;
	u32 rechg_dg;
	u32 rechg_volt;
	u32 vboost;
	u32 iboost;
	u32 conv_ocp_dis;
	u32 tsbat_jeita_dis;
	u32 ibat_ocp_dis;
	u32 vpmid_ovp_otg_dis;
	u32 vbat_ovp_buck_dis;
	u32 ibat_ocp;
};

enum sc6601_chg_fields {
	F_VAC_OVP, F_VBUS_OVP,
	F_TSBUS_FLT,
	F_TSBAT_FLT,
	F_ACDRV_MANUAL_PRE, F_ACDRV_EN, F_ACDRV_MANUAL_EN, F_WD_TIME_RST, F_WD_TIMER,
	F_REG_RST, F_VBUS_PD, F_VAC_PD, F_CID_EN,
	F_ADC_EN, F_ADC_FREEZE, F_BATID_ADC_EN,
	F_EDL_ACTIVE_LEVEL,
	/******* charger *******/
	F_VSYS_MIN,     /* REG30 */
	F_BATSNS_EN, F_VBAT, /* REG31 */
	F_ICHG_CC, /* REG32 */
	F_VINDPM_VBAT, F_VINDPM_DIS, F_VINDPM, /* REG33 */
	F_IINDPM_DIS, F_IINDPM,  /* REG34 */
	F_FORCE_ICO, F_ICO_EN, F_IINDPM_ICO,  /* REG35 */
	F_VBAT_PRECHG, F_IPRECHG,    /* REG36 */
	F_TERM_EN, F_ITERM,  /* REG37 */
	F_RECHG_DIS, F_RECHG_DG, F_VRECHG,    /* REG38 */
	F_VBOOST, F_IBOOST,  /* REG39 */
	F_CONV_OCP_DIS, F_TSBAT_JEITA_DIS, F_IBAT_OCP_DIS, F_VPMID_OVP_OTG_DIS, F_VBAT_OVP_BUCK_DIS,    /* REG3A */
	F_T_BATFET_RST, F_T_PD_nRST, F_BATFET_RST_EN, F_BATFET_DLY, F_BATFET_DIS, F_nRST_SHIPMODE_DIS,   /* REG3B */
	F_HIZ_EN, F_PERFORMANCE_EN, F_DIS_BUCKCHG_PATH, F_DIS_SLEEP_FOR_OTG, F_QB_EN, F_BOOST_EN, F_CHG_EN,   /* REG3C */
	F_VBAT_TRACK, F_IBATOCP, F_VSYSOVP_DIS, F_VSYSOVP_TH,  /* REG3D */
	F_BAT_COMP, F_VCLAMP, F_JEITA_ISET_COOL, F_JEITA_VSET_WARM,    /* REG3E */
	F_TMR2X_EN, F_CHG_TIMER_EN, F_CHG_TIMER, F_TDIE_REG_DIS, F_TDIE_REG, F_PFM_DIS,  /* REG3F */
	F_BAT_COMP_OFF, F_VBAT_LOW_OTG, F_BOOST_FREQ, F_BUCK_FREQ, F_BAT_LOAD_EN, /* REG40 */
	/*
	F_VSYS_SHORT_STAT, F_VSLEEP_BUCK_STAT, F_VBAT_DPL_STAT, F_VBAT_LOW_BOOST_STAT, F_VBUS_GOOD_STAT,
	F_CHG_STAT, F_BOOST_OK_STAT, F_VSYSMIN_REG_STAT, F_QB_ON_STAT, F_BATFET_STAT,
	F_TDIE_REG_STAT, F_TSBAT_COOL_STAT, F_TSBAT_WARM_STAT, F_ICO_STAT, F_IINDPM_STAT, F_VINDPM_STAT, */
	F_JEITA_COOL_TEMP, F_JEITA_WARM_TEMP, F_BOOST_NTC_HOT_TEMP, F_BOOST_NTC_COLD_TEMP, /* REG56 */
	F_TESTM_EN, /* REG5D */
	F_KEY_EN_OWN,   /* REG5E */
	/****** led ********/
	F_TRPT, F_FL_TX_EN, F_TLED2_EN, F_TLED1_EN, F_FLED2_EN, F_FLED1_EN,  /* reg80 */
	F_FLED1_BR, /* reg81 */
	F_FLED2_BR,/* reg82 */
	F_FTIMEOUT, F_FRPT, F_FTIMEOUT_EN,/* reg83 */
	F_TLED1_BR,/* reg84 */
	F_TLED2_BR,/* reg85 */
	F_PMID_FLED_OVP_DEG, F_VBAT_MIN_FLED, F_VBAT_MIN_FLED_DEG, F_LED_POWER,/* reg86 */
	/****** DPDPM ******/
	F_FORCE_INDET, F_AUTO_INDET_EN, F_HVDCP_EN, F_QC_EN,
	F_DP_DRIV, F_DM_DRIV, F_BC1_2_VDAT_REF_SET, F_BC1_2_DP_DM_SINK_CAP,
	F_QC2_V_MAX, F_QC3_PULS, F_QC3_MINUS, F_QC3_5_16_PLUS, F_QC3_5_16_MINUS, F_QC3_5_3_SEQ, F_QC3_5_2_SEQ,
	F_I2C_DPDM_BYPASS_EN, F_DPDM_PULL_UP_EN, F_WDT_TFCP_MASK, F_WDT_TFCP_FLAG, F_WDT_TFCP_RST, F_WDT_TFCP_CFG, F_WDT_TFCP_DIS,
	F_VBUS_STAT, F_BC1_2_DONE, F_DP_OVP, F_DM_OVP,
	F_DM_500K_PD_EN, F_DP_500K_PD_EN, F_DM_SINK_EN, F_DP_SINK_EN, F_DP_SRC_10UA,

	F_MAX_FIELDS,
};

enum sc6601_adc_ch {
	SC6601_ADC_IBUS = 0,
	SC6601_ADC_VBUS,
	SC6601_ADC_VAC,
	SC6601_ADC_VBATSNS,
	SC6601_ADC_VBAT,
	SC6601_ADC_IBAT,
	SC6601_ADC_VSYS,
	SC6601_ADC_TSBUS,
	SC6601_ADC_TSBAT,
	SC6601_ADC_TDIE,
	SC6601_ADC_BATID,
	SC6601_ADC_INVALID,
};

enum {
	SC6601_CHG_STATE_NO_CHG = 0,
	SC6601_CHG_STATE_TRICK,
	SC6601_CHG_STATE_PRECHG,
	SC6601_CHG_STATE_CC,
	SC6601_CHG_STATE_CV,
	SC6601_CHG_STATE_TERM,
};

enum DPDM_DRIVE {
	DPDM_HIZ = 0,
	DPDM_20K_DOWN,
	DPDM_V0_6,
	DPDM_V1_8,
	DPDM_V2_7,
	DPDM_V3_3,
	DPDM_500K_DOWN,
};

enum DPDM_CAP {
	DPDM_CAP_SNK_50UA = 0,
	DPDM_CAP_SNK_100UA,
	DPDM_CAP_SRC_10UA,
	DPDM_CAP_SRC_250UA,
	DPDM_CAP_DISABLE,
};

struct chip_state {
	bool online;
	bool boost_good;
	int vbus_type;
	int chg_state;
	int vindpm;
};

enum {
	IRQ_HK = 0,
	IRQ_BUCK,
	IRQ_DPDM,
	IRQ_LED,
	IRQ_MAX,
};

enum LED_FLASH_MODULE {
	LED1_FLASH = 1,
	LED2_FLASH,
	LED_ALL_FLASH,
};

struct sc6601_chg_device {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *rmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];

	struct buck_init_data buck_init;
	struct chip_state state;

	struct delayed_work led_work;
	enum LED_FLASH_MODULE led_index;
	struct completion flash_run;
	struct completion flash_end;
	bool led_state;
	atomic_t led_work_running;

	unsigned long request_otg;
	int irq[IRQ_MAX];

	struct charger_dev *sc_charger;
	struct chargerpump_dev *charger_pump;

	struct flash_led_dev *led_dev;
	bool chg_done;
	bool use_soft_bc12;
	struct soft_bc12 *bc;
	struct notifier_block bc12_result_nb;

	struct timer_list bc12_timeout;

	struct mutex bc_detect_lock;
	struct mutex adc_read_lock;

	struct work_struct qc_detect_work;
	int qc_result;
	int qc_vbus;
	bool qc3_support;
	struct tcpc_device *tcpc;
	int input_suspend;
};

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int tcpci_set_continuous_time(struct tcpc_device *tcpc);
#endif
