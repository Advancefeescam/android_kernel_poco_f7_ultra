#include "lxchg_class.h"

#ifdef LXCHG_CHARGER_CLASS
int charger_get_adc(struct charger_dev *charger,
				enum chg_adc_channel channel, uint32_t *value)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_adc == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_adc(charger, channel, value);
}
EXPORT_SYMBOL(charger_get_adc);

int charger_get_vbus_type(struct charger_dev *charger,
				enum chg_type *type)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_vbus_type == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_vbus_type(charger, type);
}
EXPORT_SYMBOL(charger_get_vbus_type);

int charger_get_online(struct charger_dev *charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_online == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_online(charger, en);
}
EXPORT_SYMBOL(charger_get_online);

int charger_is_charge_done(struct charger_dev *charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->is_charge_done == NULL)
		return -EOPNOTSUPP;
	return charger->ops->is_charge_done(charger, en);
}
EXPORT_SYMBOL(charger_is_charge_done);

int charger_get_hiz_status(struct charger_dev *charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_hiz_status == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_hiz_status(charger, en);
}
EXPORT_SYMBOL(charger_get_hiz_status);

int charger_get_input_volt_lmt(struct charger_dev *charger, uint32_t *mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_input_volt_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_input_volt_lmt(charger, mv);
}
EXPORT_SYMBOL(charger_get_input_volt_lmt);

int charger_get_input_curr_lmt(struct charger_dev *charger, uint32_t *ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_input_curr_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_input_curr_lmt(charger, ma);
}
EXPORT_SYMBOL(charger_get_input_curr_lmt);

int charger_get_chg_status(struct charger_dev *charger,
		uint32_t *chg_state, uint32_t *chg_status)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_chg_status == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_chg_status(charger, chg_state, chg_status);
}
EXPORT_SYMBOL(charger_get_chg_status);

int charger_get_chip_chg_status(struct charger_dev *charger, uint32_t *chg_status)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_chip_chg_status == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_chip_chg_status(charger, chg_status);
}
EXPORT_SYMBOL(charger_get_chip_chg_status);

int charger_get_otg_status(struct charger_dev *charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_otg_status == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_otg_status(charger, en);
}
EXPORT_SYMBOL(charger_get_otg_status);

int charger_get_ichg(struct charger_dev *charger, uint32_t *ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_ichg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_ichg(charger, ma);
}
EXPORT_SYMBOL(charger_get_ichg);

int charger_get_term_curr(struct charger_dev *charger, uint32_t *ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_term_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_term_curr(charger, ma);
}
EXPORT_SYMBOL(charger_get_term_curr);

int charger_get_term_volt(struct charger_dev *charger, uint32_t *mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_term_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_term_volt(charger, mv);
}
EXPORT_SYMBOL(charger_get_term_volt);

int charger_set_hiz(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_hiz == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_hiz(charger, en);
}
EXPORT_SYMBOL(charger_set_hiz);

int charger_set_input_curr_lmt(struct charger_dev *charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_input_curr_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_input_curr_lmt(charger, ma);
}
EXPORT_SYMBOL(charger_set_input_curr_lmt);

int charger_disable_power_path(struct charger_dev *charger, bool enable)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->disable_power_path == NULL)
		return -EOPNOTSUPP;
	return charger->ops->disable_power_path(charger, enable);
}
EXPORT_SYMBOL(charger_disable_power_path);

int charger_set_input_volt_lmt(struct charger_dev *charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_input_volt_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_input_volt_lmt(charger, mv);
}
EXPORT_SYMBOL(charger_set_input_volt_lmt);

int charger_set_ichg(struct charger_dev *charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_ichg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_ichg(charger, ma);
}
EXPORT_SYMBOL(charger_set_ichg);

int charger_enable_chg(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->enable_chg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->enable_chg(charger, en);
}
EXPORT_SYMBOL(charger_enable_chg);

int charger_get_chg_enabled(struct charger_dev *charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_chg_enabled == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_chg_enabled(charger, en);
}
EXPORT_SYMBOL(charger_get_chg_enabled);

int charger_set_otg(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_otg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_otg(charger, en);
}
EXPORT_SYMBOL(charger_set_otg);

int charger_set_otg_curr(struct charger_dev *charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_otg_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_otg_curr(charger, ma);
}

int charger_set_otg_volt(struct charger_dev *charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_otg_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_otg_volt(charger, mv);
}

int charger_set_term(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_term == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_term(charger, en);
}
EXPORT_SYMBOL(charger_set_term);

int charger_set_term_curr(struct charger_dev *charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_term_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_term_curr(charger, ma);
}
EXPORT_SYMBOL(charger_set_term_curr);

int charger_set_term_volt(struct charger_dev *charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_term_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_term_volt(charger, mv);
}
EXPORT_SYMBOL(charger_set_term_volt);

int charger_set_qc_term_vbus(struct charger_dev *charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_qc_term_vbus == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_qc_term_vbus(charger);
}
EXPORT_SYMBOL(charger_set_qc_term_vbus);

int charger_set_rechg_volt(struct charger_dev *charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_rechg_vol == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_rechg_vol(charger, mv);
}
EXPORT_SYMBOL(charger_set_rechg_volt);

int charger_adc_enable(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->adc_enable == NULL)
		return -EOPNOTSUPP;
	return charger->ops->adc_enable(charger, en);
}
EXPORT_SYMBOL(charger_adc_enable);

int charger_set_shipmode(struct charger_dev *charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_shipmode == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_shipmode(charger, en);
}
EXPORT_SYMBOL(charger_set_shipmode);

int charger_set_prechg_volt(struct charger_dev *charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_prechg_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_prechg_volt(charger, mv);
}
EXPORT_SYMBOL(charger_set_prechg_volt);

int charger_set_prechg_curr(struct charger_dev *charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_prechg_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_prechg_curr(charger, ma);
}
EXPORT_SYMBOL(charger_set_prechg_curr);

int charger_soft_cid_set_toggle(struct charger_dev *charger, bool toggle)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->soft_cid_set_toggle == NULL)
		return -EOPNOTSUPP;
	return charger->ops->soft_cid_set_toggle(charger, toggle);
}
EXPORT_SYMBOL(charger_soft_cid_set_toggle);

int charger_force_dpdm(struct charger_dev *charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->force_dpdm == NULL)
		return -EOPNOTSUPP;
	return charger->ops->force_dpdm(charger);
}
EXPORT_SYMBOL(charger_force_dpdm);

int charger_reset(struct charger_dev *charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->reset == NULL)
		return -EOPNOTSUPP;
	return charger->ops->reset(charger);
}
EXPORT_SYMBOL(charger_reset);

int charger_set_wd_timeout(struct charger_dev *charger, int ms)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_wd_timeout == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_wd_timeout(charger, ms);
}

int charger_kick_wd(struct charger_dev *charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->kick_wd == NULL)
		return -EOPNOTSUPP;
	return charger->ops->kick_wd(charger);
}

int charger_qc_identify(struct charger_dev *charger, int qc3_enable)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->qc_identify == NULL)
		return -EOPNOTSUPP;
	return charger->ops->qc_identify(charger, qc3_enable);
}
EXPORT_SYMBOL(charger_qc_identify);

int charger_qc3_vbus_puls(struct charger_dev *charger, bool state, int count)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->qc3_vbus_puls == NULL)
		return -EOPNOTSUPP;
	return charger->ops->qc3_vbus_puls(charger, state, count);
}
EXPORT_SYMBOL(charger_qc3_vbus_puls);

int charger_qc2_vbus_mode(struct charger_dev *charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->qc2_vbus_mode == NULL)
		return -EOPNOTSUPP;
	return charger->ops->qc2_vbus_mode(charger, mv);
}
EXPORT_SYMBOL(charger_qc2_vbus_mode);

int charger_mtbf_regs_show(struct charger_dev *charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->mtbf_regs_show == NULL)
		return -EOPNOTSUPP;
	return charger->ops->mtbf_regs_show(charger);
}
EXPORT_SYMBOL(charger_mtbf_regs_show);

#endif

#ifdef LXCHG_CHARGEPUMP_CLASS
int chargerpump_set_chip_init(struct chargerpump_dev *chargerpump)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_chip_init == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_chip_init(chargerpump);
}
EXPORT_SYMBOL(chargerpump_set_chip_init);

int chargerpump_set_enable(struct chargerpump_dev *chargerpump, bool enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_enable == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_enable(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_set_enable);

char* chargerpump_get_name(struct chargerpump_dev *chargerpump)
{
	if (!chargerpump || !chargerpump->ops)
		return "UNKNOWN";
	if (chargerpump->ops->get_name == NULL)
		return "UNKNOWN";
	return chargerpump->ops->get_name(chargerpump);
}
EXPORT_SYMBOL(chargerpump_get_name);

int chargerpump_enable_acdrv_manual(struct chargerpump_dev *chargerpump, bool enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->enable_acdrv_manual == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->enable_acdrv_manual(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_enable_acdrv_manual);

int chargerpump_set_otg_enable(struct chargerpump_dev *chargerpump, bool enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_otg_enable == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_otg_enable(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_set_otg_enable);

int chargerpump_set_vbus_ovp(struct chargerpump_dev *chargerpump, int mv)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_vbus_ovp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_vbus_ovp(chargerpump, mv);
}

int chargerpump_set_ibus_ocp(struct chargerpump_dev *chargerpump, int ma)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_ibus_ocp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_ibus_ocp(chargerpump, ma);
}
EXPORT_SYMBOL(chargerpump_set_ibus_ocp);

int chargerpump_set_vbat_ovp(struct chargerpump_dev *chargerpump, int mv)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_vbat_ovp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_vbat_ovp(chargerpump, mv);
}
EXPORT_SYMBOL(chargerpump_set_vbat_ovp);

int chargerpump_set_ibat_ocp(struct chargerpump_dev *chargerpump, int ma)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_ibat_ocp == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_ibat_ocp(chargerpump, ma);
}
EXPORT_SYMBOL(chargerpump_set_ibat_ocp);

int chargerpump_set_enable_adc(struct chargerpump_dev *chargerpump, bool enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_enable_adc == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_enable_adc(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_set_enable_adc);

int chargerpump_get_is_enable(struct chargerpump_dev *chargerpump, bool *enable)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_is_enable == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_is_enable(chargerpump, enable);
}
EXPORT_SYMBOL(chargerpump_get_is_enable);

int chargerpump_get_status(struct chargerpump_dev *chargerpump, uint32_t *status)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_status == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_status(chargerpump, status);
}
EXPORT_SYMBOL(chargerpump_get_status);

int chargerpump_get_adc_value(struct chargerpump_dev *chargerpump, enum cp_adc_channel ch, int *value)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_adc_value == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_adc_value(chargerpump, ch, value);
}
EXPORT_SYMBOL(chargerpump_get_adc_value);

int chargerpump_get_chip_id(struct chargerpump_dev *chargerpump, int *value)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_chip_id == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_chip_id(chargerpump, value);
}
EXPORT_SYMBOL(chargerpump_get_chip_id);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
int chargerpump_set_cp_workmode(struct chargerpump_dev *chargerpump, int workmode)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->set_cp_workmode == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->set_cp_workmode(chargerpump, workmode);
}
EXPORT_SYMBOL_GPL(chargerpump_set_cp_workmode);
int chargerpump_get_cp_workmode(struct chargerpump_dev *chargerpump, int *workmode)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->get_cp_workmode == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->get_cp_workmode(chargerpump, workmode);
}
EXPORT_SYMBOL_GPL(chargerpump_get_cp_workmode);
#endif

int chargerpump_dump_register(struct chargerpump_dev *chargerpump)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->dump_cp_register == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->dump_cp_register(chargerpump);
}
EXPORT_SYMBOL_GPL(chargerpump_dump_register);

int chargerpump_reset_register(struct chargerpump_dev *chargerpump)
{
	if (!chargerpump || !chargerpump->ops)
		return -EINVAL;
	if (chargerpump->ops->reset_cp_register == NULL)
		return -EOPNOTSUPP;
	return chargerpump->ops->reset_cp_register(chargerpump);
}
EXPORT_SYMBOL_GPL(chargerpump_reset_register);

void *chargerpump_get_private(struct chargerpump_dev *chargerpump)
{
	if (!chargerpump)
		return ERR_PTR(-EINVAL);
	return chargerpump->private;
}
EXPORT_SYMBOL(chargerpump_get_private);

int chargerpump_unregister(struct chargerpump_dev *chargerpump)
{
	device_unregister(&chargerpump->dev);
	kfree(chargerpump);
	return 0;
}
#endif

#ifdef LXCHG_LED_CLASS
int flash_camera_set_led_flash_curr(struct flash_led_dev *flash_led, enum FLASH_LED_ID id, int ma)
{
	if (!flash_led || !flash_led->ops)
		return -EINVAL;
	if (flash_led->ops->set_led_flash_curr == NULL)
		return -EOPNOTSUPP;
	return flash_led->ops->set_led_flash_curr(flash_led, id, ma);
}
EXPORT_SYMBOL(flash_camera_set_led_flash_curr);

int flash_camera_set_led_flash_time(struct flash_led_dev *flash_led, enum FLASH_LED_ID id, int ms)
{
	if (!flash_led || !flash_led->ops)
		return -EINVAL;
	if (flash_led->ops->set_led_flash_time == NULL)
		return -EOPNOTSUPP;
	return flash_led->ops->set_led_flash_time(flash_led, id, ms);
}
EXPORT_SYMBOL(flash_camera_set_led_flash_time);

int flash_camera_set_led_flash_enable(struct flash_led_dev *flash_led, enum FLASH_LED_ID id, bool en)
{
	if (!flash_led || !flash_led->ops)
		return -EINVAL;
	if (flash_led->ops->set_led_flash_enable == NULL)
		return -EOPNOTSUPP;
	return flash_led->ops->set_led_flash_enable(flash_led, id, en);
}
EXPORT_SYMBOL(flash_camera_set_led_flash_enable);

int flash_camera_set_led_torch_curr(struct flash_led_dev *flash_led, enum FLASH_LED_ID id, int ma)
{
	if (!flash_led || !flash_led->ops)
		return -EINVAL;
	if (flash_led->ops->set_led_torch_curr == NULL)
		return -EOPNOTSUPP;
	return flash_led->ops->set_led_torch_curr(flash_led, id, ma);
}
EXPORT_SYMBOL(flash_camera_set_led_torch_curr);

#if IS_ENABLED(CONFIG_IS_P7_PROJECT)
int flash_camera_set_led_torch_enable(struct flash_led_dev *flash_led, enum FLASH_LED_ID id, bool en, int flashmode)
{
	if (!flash_led || !flash_led->ops)
		return -EINVAL;
	if (flash_led->ops->set_led_torch_enable == NULL)
		return -EOPNOTSUPP;
	return flash_led->ops->set_led_torch_enable(flash_led, id, en, flashmode);
}
EXPORT_SYMBOL(flash_camera_set_led_torch_enable);
#else
int flash_camera_set_led_torch_enable(struct flash_led_dev *flash_led, enum FLASH_LED_ID id, bool en)
{
	if (!flash_led || !flash_led->ops)
		return -EINVAL;
	if (flash_led->ops->set_led_torch_enable == NULL)
		return -EOPNOTSUPP;
	return flash_led->ops->set_led_torch_enable(flash_led, id, en);
}
EXPORT_SYMBOL(flash_camera_set_led_torch_enable);
#endif

void * flash_led_get_private(struct flash_led_dev *flash_led)
{
	if (!flash_led)
		return ERR_PTR(-EINVAL);
	return flash_led->private;
}
EXPORT_SYMBOL(flash_led_get_private);

int flash_led_unregister(struct flash_led_dev *flash_led)
{
	device_unregister(&flash_led->dev);
	kfree(flash_led);
	return 0;
}

#endif

#ifdef LXCHG_FG_CLASS
int fuel_gauge_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_soc_decimal == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_soc_decimal(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_soc_decimal);

int fuel_gauge_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_soc_decimal_rate == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_soc_decimal_rate(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_soc_decimal_rate);

int fuel_gauge_get_rsoc(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_rsoc == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_rsoc(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_rsoc);

int fuel_gauge_set_rsoc_update0(struct fuel_gauge_dev *fuel_gauge, bool value)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->set_rsoc_update0 == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->set_rsoc_update0(fuel_gauge, value);
}
EXPORT_SYMBOL(fuel_gauge_set_rsoc_update0);

#if IS_ENABLED(CONFIG_LIXUN_SOH2_SUPPORT)
int fuel_gauge_get_soh(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_soh == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_soh(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_soh);

int fuel_gauge_set_soh(struct fuel_gauge_dev *fuel_gauge, int value)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->set_soh == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->set_soh(fuel_gauge, value);
}
EXPORT_SYMBOL(fuel_gauge_set_soh);
#endif

int fuel_gauge_get_c_car(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_c_car == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_c_car(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_c_car);

int fuel_gauge_get_v_car(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_v_car == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_v_car(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_v_car);

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
int fuel_gauge_check_i2c_function(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->check_i2c_function == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->check_i2c_function(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_check_i2c_function);
#endif

int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool en)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->set_fastcharge_mode == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->set_fastcharge_mode(fuel_gauge, en);
}
EXPORT_SYMBOL(fuel_gauge_set_fastcharge_mode);

int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_fastcharge_mode == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_fastcharge_mode(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_fastcharge_mode);


void *fuel_gauge_get_private(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge)
		return ERR_PTR(-EINVAL);
	return fuel_gauge->private;
}
EXPORT_SYMBOL(fuel_gauge_get_private);

int fuel_gauge_unregister(struct fuel_gauge_dev *fuel_gauge)
{
	device_unregister(&fuel_gauge->dev);
	kfree(fuel_gauge);
	return 0;
}
#endif


#ifdef LXCHG_BATTINFO_CLASS
int batt_info_get_batt_id(struct batt_info_dev *batt_info)
{
	if (!batt_info || !batt_info->ops)
		return BATTERY_ID_UNKNOWN;
	if (batt_info->ops->get_batt_id == NULL)
		return BATTERY_ID_UNKNOWN;
	return batt_info->ops->get_batt_id(batt_info);
}
EXPORT_SYMBOL(batt_info_get_batt_id);

char *batt_info_get_batt_name(struct batt_info_dev *batt_info)
{
	if (!batt_info || !batt_info->ops)
		return BATTERY_NAME_UNKNOWN;
	if (batt_info->ops->get_batt_name == NULL)
		return BATTERY_NAME_UNKNOWN;
	return batt_info->ops->get_batt_name(batt_info);
}
EXPORT_SYMBOL(batt_info_get_batt_name);

int batt_info_get_chip_ok(struct batt_info_dev *batt_info)
{
	if (!batt_info || !batt_info->ops)
		return 0;
	if (batt_info->ops->get_batt_id == NULL)
		return 0;
	return batt_info->ops->get_chip_ok(batt_info);
}
EXPORT_SYMBOL(batt_info_get_chip_ok);

void *batt_info_get_private(struct batt_info_dev *batt_info)
{
	if (!batt_info)
		return ERR_PTR(-EINVAL);
	return batt_info->private;
}
EXPORT_SYMBOL(batt_info_get_private);

int batt_info_unregister(struct batt_info_dev *batt_info)
{
	device_unregister(&batt_info->dev);
	kfree(batt_info);
	return 0;
}
#endif
