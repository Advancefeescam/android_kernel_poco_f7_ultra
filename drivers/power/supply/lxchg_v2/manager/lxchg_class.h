
#ifndef __LXCHG_CLASS_H__
#define __LXCHG_CLASS_H__


#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/of.h>

#define LXCHG_CHARGER_CLASS
#define LXCHG_CHARGEPUMP_CLASS
#define LXCHG_FG_CLASS
#define LXCHG_LED_CLASS
#define LXCHG_BATTINFO_CLASS


#ifdef LXCHG_CHARGER_CLASS

enum vendor_supply_id {
	FIRST_SUPPLY = 1,
	SECOND_SUPPLY,
	THIRD_SUPPLY,
};

enum chg_adc_channel {
	CHG_ADC_VBUS      = 0,
	CHG_ADC_VSYS      = 1,
	CHG_ADC_VBAT      = 2,
	CHG_ADC_VAC       = 3,
	CHG_ADC_IBUS      = 4,
	CHG_ADC_IBAT      = 5,
	CHG_ADC_TSBUS     = 6,
	CHG_ADC_TSBAT     = 7,
	CHG_ADC_TDIE      = 8,
	CHG_ADC_MAX       = 9,
};

enum chg_type {
	VBUS_TYPE_NONE = 0,
	/* bc1.2 */
	VBUS_TYPE_SDP,
	VBUS_TYPE_CDP,
	VBUS_TYPE_DCP,
	VBUS_TYPE_FLOAT,
	VBUS_TYPE_NON_STAND,
	/* qc */
	VBUS_TYPE_HVDCP,
	VBUS_TYPE_HVDCP_3,
	VBUS_TYPE_HVDCP_3P5,
	/* pd */
	VBUS_TYPE_PD,
	VBUS_TYPE_PD_PPS,
	VBUS_TYPE_MI_PPS,
};

struct charger_dev {
	struct device dev;
	char *name;
	void *private;
	struct charger_ops *ops;
	enum chg_type bc12_type;
	int m_pd_active;
	int vendor_supply_id;
	int input_suspend;
};

struct charger_ops {
	int (*get_adc)(struct charger_dev *charger, enum chg_adc_channel channel, uint32_t *value);
	int (*get_vbus_type)(struct charger_dev *charger, enum chg_type *type);
	int (*get_online)(struct charger_dev *charger, bool *en);
	int (*is_charge_done)(struct charger_dev *charger, bool *en);
	int (*get_hiz_status)(struct charger_dev *charger, bool *en);
	int (*get_ichg)(struct charger_dev *charger, uint32_t *ma);
	int (*get_input_volt_lmt)(struct charger_dev *charger, uint32_t *mv);
	int (*get_input_curr_lmt)(struct charger_dev *charger, uint32_t *ma);
	int (*get_chg_status)(struct charger_dev *charger, uint32_t *chg_state,
							uint32_t *chg_status);
	int (*get_chip_chg_status)(struct charger_dev *charger, uint32_t *chg_status);
	int (*get_otg_status)(struct charger_dev *charger, bool *en);
	int (*get_term_curr)(struct charger_dev *charger, uint32_t *ma);
	int (*get_term_volt)(struct charger_dev *charger, uint32_t *mv);
	int (*set_hiz)(struct charger_dev *charger, bool en);
	int (*set_input_curr_lmt)(struct charger_dev *charger, int ma);
	int (*disable_power_path)(struct charger_dev *charger, bool ma);
	int (*set_input_volt_lmt)(struct charger_dev *charger, int mv);
	int (*set_ichg)(struct charger_dev *charger, int ma);
	int (*enable_chg)(struct charger_dev *charger, bool en);
	int (*get_chg_enabled)(struct charger_dev *charger, bool *en);
	int (*set_otg)(struct charger_dev *charger, bool en);
	int (*set_otg_curr)(struct charger_dev *charger, int ma);
	int (*set_otg_volt)(struct charger_dev *charger, int mv);
	int (*set_term)(struct charger_dev *charger, bool en);
	int (*set_term_curr)(struct charger_dev *charger, int ma);
	int (*set_term_volt)(struct charger_dev *charger, int mv);
	int (*set_qc_term_vbus)(struct charger_dev *charger);
	int (*adc_enable)(struct charger_dev *charger, bool en);
	int (*set_prechg_volt)(struct charger_dev *charger, int mv);
	int (*set_prechg_curr)(struct charger_dev *charger, int ma);
	int (*force_dpdm)(struct charger_dev *charger);
	int (*reset)(struct charger_dev *charger);
	int (*request_dpdm)(struct charger_dev *charger, bool en);
	int (*set_wd_timeout)(struct charger_dev *charger, int ms);
	int (*kick_wd)(struct charger_dev *charger);
	int (*set_shipmode)(struct charger_dev *charger, bool en);
	int (*set_rechg_vol)(struct charger_dev *charger, int mv);
	int (*qc_identify)(struct charger_dev *charger, int qc3_enable);
	int (*qc3_vbus_puls)(struct charger_dev *charger, bool state, int count);
	int (*qc2_vbus_mode)(struct charger_dev *charger, int mv);
	int (*mtbf_regs_show)(struct charger_dev *charger);
	int (*soft_cid_set_toggle)(struct charger_dev *charger, bool toggle);
};


int charger_unregister(struct charger_dev *charger);
struct charger_dev *charger_find_dev_by_name(const char *name);
struct charger_dev *charger_register(char *name, struct device *parent,
							struct charger_ops *ops, void *private);
void *charger_get_private(struct charger_dev *charger);
int charger_get_adc(struct charger_dev *charger, enum chg_adc_channel channel, uint32_t *value);
int charger_get_vbus_type(struct charger_dev *charger, enum chg_type *type);
int charger_get_online(struct charger_dev *charger, bool *en);
int charger_is_charge_done(struct charger_dev *charger, bool *en);
int charger_get_hiz_status(struct charger_dev *charger, bool *en);
int charger_get_ichg(struct charger_dev *charger, uint32_t *ma);
int charger_get_input_volt_lmt(struct charger_dev *charger, uint32_t *mv);
int charger_get_input_curr_lmt(struct charger_dev *charger, uint32_t *ma);
int charger_get_chg_status(struct charger_dev *charger, uint32_t *chg_state, uint32_t *chg_status);
int charger_get_chip_chg_status(struct charger_dev *charger, uint32_t *chg_status);
int charger_get_otg_status(struct charger_dev *charger, bool *en);
int charger_get_term_curr(struct charger_dev *charger, uint32_t *ma);
int charger_get_term_volt(struct charger_dev *charger, uint32_t *mv);
int charger_set_hiz(struct charger_dev *charger, bool en);
int charger_set_input_curr_lmt(struct charger_dev *charger, int ma);
int charger_disable_power_path(struct charger_dev *charger, bool ma);
int charger_set_input_volt_lmt(struct charger_dev *charger, int mv);
int charger_set_ichg(struct charger_dev *charger, int ma);
int charger_enable_chg(struct charger_dev *charger, bool en);
int charger_get_chg_enabled(struct charger_dev *charger, bool *en);
int charger_set_otg(struct charger_dev *charger, bool en);
int charger_set_otg_curr(struct charger_dev *charger, int ma);
int charger_set_otg_volt(struct charger_dev *charger, int mv);
int charger_set_term(struct charger_dev *charger, bool en);
int charger_set_term_curr(struct charger_dev *charger, int ma);
int charger_set_term_volt(struct charger_dev *charger, int mv);
int charger_set_qc_term_vbus(struct charger_dev *charger);
int charger_set_rechg_volt(struct charger_dev *charger, int mv);
int charger_adc_enable(struct charger_dev *charger, bool en);
int charger_set_prechg_volt(struct charger_dev *charger, int mv);
int charger_set_prechg_curr(struct charger_dev *charger, int ma);
int charger_force_dpdm(struct charger_dev *charger);
int charger_reset(struct charger_dev *charger);
int charger_set_wd_timeout(struct charger_dev *charger, int ms);
int charger_kick_wd(struct charger_dev *charger);
int charger_request_qc20(struct charger_dev *charger, int mv);
int charger_qc_identify(struct charger_dev *charger, int qc3_enable);
int charger_qc3_vbus_puls(struct charger_dev *charger, bool state, int count);
int charger_qc2_vbus_mode(struct charger_dev *charger, int mv);
int charger_set_shipmode(struct charger_dev *charger, bool en);
int charger_mtbf_regs_show(struct charger_dev *charger);
int charger_soft_cid_set_toggle(struct charger_dev *charger, bool toggle);


#endif

#ifdef LXCHG_CHARGEPUMP_CLASS
#define CHARGERPUMP_ERROR_VBUS_HIGH        BIT(0)
#define CHARGERPUMP_ERROR_VBUS_LOW         BIT(1)
#define CHARGERPUMP_ERROR_VBUS_OVP         BIT(2)
#define CHARGERPUMP_ERROR_IBUS_OCP         BIT(3)
#define CHARGERPUMP_ERROR_VBAT_OVP         BIT(4)
#define CHARGERPUMP_ERROR_IBAT_OCP         BIT(5)

#define CP_DEV_ID_NU2115 (0x90)

enum cp_adc_channel {
	CP_ADC_VBUS      = 0,
	CP_ADC_VSYS      = 1,
	CP_ADC_VBAT      = 2,
	CP_ADC_VAC       = 3,
	CP_ADC_IBUS      = 4,
	CP_ADC_IBAT      = 5,
	CP_ADC_TSBUS     = 6,
	CP_ADC_TSBAT     = 7,
	CP_ADC_TDIE      = 8,
	CP_ADC_MAX       = 9,
};

struct chargerpump_dev {
	struct device dev;
	char *name;
	void *private;
	struct chargerpump_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
};

struct chargerpump_ops {
	int (*set_chip_init)(struct chargerpump_dev *chargerpump);
	int (*set_enable)(struct chargerpump_dev *chargerpump, bool enable);
	int (*set_vbus_ovp)(struct chargerpump_dev *chargerpump, int mv);
	int (*set_ibus_ocp)(struct chargerpump_dev *chargerpump, int ma);
	int (*set_vbat_ovp)(struct chargerpump_dev *chargerpump, int mv);
	int (*set_ibat_ocp)(struct chargerpump_dev *chargerpump, int ma);
	int (*set_enable_adc)(struct chargerpump_dev *chargerpump, bool enable);

	int (*get_is_enable)(struct chargerpump_dev *chargerpump, bool *enable);
	int (*get_status)(struct chargerpump_dev *chargerpump, uint32_t *status);
	int (*get_adc_value)(struct chargerpump_dev *chargerpump, enum cp_adc_channel ch, int *value);

	int (*get_chip_id)(struct chargerpump_dev *chargerpump, int *value);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	int (*set_cp_workmode)(struct chargerpump_dev *chargerpump, int workmode);
	int (*get_cp_workmode)(struct chargerpump_dev *chargerpump, int *workmode);
#endif
	int (*dump_cp_register)(struct chargerpump_dev *chargerpump);
	int (*reset_cp_register)(struct chargerpump_dev *chargerpump);
	int (*set_otg_enable)(struct chargerpump_dev *chargerpump, bool enable);
	char* (*get_name)(struct chargerpump_dev *chargerpump);
	int (*enable_acdrv_manual)(struct chargerpump_dev *chargerpump, bool enable);
};

struct chargerpump_dev *chargerpump_find_dev_by_name(const char *name);
struct chargerpump_dev *chargerpump_register(char *name, struct device *parent,
							struct chargerpump_ops *ops, void *private);
void *chargerpump_get_private(struct chargerpump_dev *charger);
int chargerpump_unregister(struct chargerpump_dev *charger);

int chargerpump_set_chip_init(struct chargerpump_dev *charger_pump);
int chargerpump_set_enable(struct chargerpump_dev *charger_pump, bool enable);
int chargerpump_set_otg_enable(struct chargerpump_dev *charger_pump, bool enable);
int chargerpump_set_vbus_ovp(struct chargerpump_dev *charger_pump, int mv);
int chargerpump_set_ibus_ocp(struct chargerpump_dev *charger_pump, int ma);
int chargerpump_set_vbat_ovp(struct chargerpump_dev *charger_pump, int mv);
int chargerpump_set_ibat_ocp(struct chargerpump_dev *charger_pump, int ma);
int chargerpump_set_enable_adc(struct chargerpump_dev *charger_pump, bool enable);
int chargerpump_get_is_enable(struct chargerpump_dev *charger_pump, bool *enable);
int chargerpump_get_status(struct chargerpump_dev *charger_pump, uint32_t *status);
int chargerpump_get_adc_value(struct chargerpump_dev *charger_pump, enum cp_adc_channel ch, int *value);
int chargerpump_get_chip_id(struct chargerpump_dev *chargerpump_dev, int *id);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
int chargerpump_set_cp_workmode(struct chargerpump_dev *chargerpump, int workmode);
int chargerpump_get_cp_workmode(struct chargerpump_dev *chargerpump, int *workmode);
#endif
int chargerpump_dump_register(struct chargerpump_dev *chargerpump);
int chargerpump_reset_register(struct chargerpump_dev *chargerpump);
char* chargerpump_get_name(struct chargerpump_dev *chargerpump);
int chargerpump_enable_acdrv_manual(struct chargerpump_dev *chargerpump, bool enable);
#endif

#ifdef LXCHG_FG_CLASS
struct fuel_gauge_dev {
	struct device dev;
	char *name;
	void *private;
	struct fuel_gauge_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
};

struct fuel_gauge_ops {
	int (*get_soc_decimal)(struct fuel_gauge_dev *);
	int (*get_soc_decimal_rate)(struct fuel_gauge_dev *);
	int (*get_rsoc)(struct fuel_gauge_dev *);
	int (*set_rsoc_update0)(struct fuel_gauge_dev *fuel_gauge, bool value);
#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
	int (*get_soh)(struct fuel_gauge_dev *);
	int (*set_soh)(struct fuel_gauge_dev *fuel_gauge, int value);
#endif
	int (*get_c_car)(struct fuel_gauge_dev *);
	int (*get_v_car)(struct fuel_gauge_dev *);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int (*check_i2c_function)(struct fuel_gauge_dev *);
#endif
	int (*set_fastcharge_mode)(struct fuel_gauge_dev *, bool);
	int (*get_fastcharge_mode)(struct fuel_gauge_dev *);
};



struct fuel_gauge_dev *fuel_gauge_find_dev_by_name(const char *name);
struct fuel_gauge_dev *fuel_gauge_register(char *name, struct device *parent,
			struct fuel_gauge_ops *ops, void *private);

void *fuel_gauge_get_private(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_unregister(struct fuel_gauge_dev *fuel_gauge);

int fuel_gauge_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
int fuel_gauge_check_i2c_function(struct fuel_gauge_dev *fuel_gauge);
#endif
int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool);
int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_rsoc(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_rsoc_update0(struct fuel_gauge_dev *fuel_gauge, bool value);
#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
int fuel_gauge_get_soh(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_soh(struct fuel_gauge_dev *fuel_gauge, int value);
#endif
int fuel_gauge_get_c_car(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_v_car(struct fuel_gauge_dev *fuel_gauge);

#endif

#ifdef LXCHG_LED_CLASS

enum FLASH_LED_ID {
	FLASH_LED1 = 1,
	FLASH_LED2,
	FLASH_LEDMAX,
};

struct flash_led_dev {
	struct device dev;
	char *name;
	void *private;
	struct flash_led_ops *ops;
};

struct flash_led_ops {
	int (*set_led_flash_curr)(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, int ma);
	int (*set_led_flash_time)(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, int ma);
	int (*set_led_flash_enable)(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, bool en);
	int (*set_led_torch_curr)(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, int ma);
#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
	int (*set_led_torch_enable)(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, bool en, int flashmode);
#else
	int (*set_led_torch_enable)(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, bool en);
#endif
};

struct flash_led_dev *flash_led_find_dev_by_name(const char *name);
struct flash_led_dev *flash_led_register(char *name, struct device *parent,
							struct flash_led_ops *ops, void *private);
void *flash_led_get_private(struct flash_led_dev *led);
int flash_led_unregister(struct flash_led_dev *led);


int flash_camera_set_led_flash_curr(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, int ma);
int flash_camera_set_led_flash_time(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, int ms);
int flash_camera_set_led_flash_enable(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, bool en);
int flash_camera_set_led_torch_curr(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, int ma);
#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
int flash_camera_set_led_torch_enable(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, bool en, int flashmode);
#else
int flash_camera_set_led_torch_enable(struct flash_led_dev *flash_led,enum FLASH_LED_ID index, bool en);
#endif

#endif

#ifdef LXCHG_BATTINFO_CLASS
#define BATTERY_ID_UNKNOWN 0xff
#define BATTERY_NAME_UNKNOWN "UNKNOWN"

struct batt_info_dev {
	struct device dev;
	char *name;
	void *private;
	struct batt_info_ops *ops;
};

struct batt_info_ops {
	int (*get_batt_id)(struct batt_info_dev *batt_info);
	char* (*get_batt_name)(struct batt_info_dev *batt_info);
	int (*get_chip_ok)(struct batt_info_dev *batt_info);
};

struct batt_info_dev *batt_info_find_dev_by_name(const char *name);
struct batt_info_dev *batt_info_register(char *name, struct device *parent,
							struct batt_info_ops *ops, void *private);
void *batt_info_get_private(struct batt_info_dev *info);
int batt_info_unregister(struct batt_info_dev *info);
int batt_info_get_batt_id(struct batt_info_dev *batt_info);
char * batt_info_get_batt_name(struct batt_info_dev *batt_info);
int  batt_info_get_chip_ok(struct batt_info_dev *batt_info);
#endif

#endif
