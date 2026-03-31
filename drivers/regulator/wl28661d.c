
// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019-2021, The Linux Foundation. All rights reserved. */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/debug-regulator.h>
#define wl28661d_err(reg, message, ...) \
	pr_err("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)
#define wl28661d_debug(reg, message, ...) \
	pr_debug("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)
#define wl28661d_MAX_LDO              4
#define wl28661d_Chip_REG             0x00
#define wl28661d_DISCHARGE_REG        0x02
#define wl28661d_ENABLE_REG           0x0E
#define wl28661d_ENABLE_DISCHARGE_VAL 0xFF
#define wl28661d_RETRY_WAIT_TIME      500000
#define wl28661d_PINCTRL_ENABLE       "wl28661d_enable"
#define wl28661d_PINCTRL_DISABLE      "wl28661d_disable"
#define chipid_sgm38121               0x80
enum wl28661d_vdd_type {
	VDD_TYPE_DVDD,
	VDD_TYPE_AVDD,
	VDD_TYPE_MAX,
};
enum wl28661d_vdd_index {
	VDD_INDEX_DVDD1,
	VDD_INDEX_DVDD2,
	VDD_INDEX_AVDD1,
	VDD_INDEX_AVDD2,
	VDD_INDEX_MAX,
};
struct wl28661d_regulator {
	u8 addr;
	u8 chip_id;
	int   uv;
	bool *suspended;
	bool  reg_enabled;
	struct device *dev;
	struct regmap *regmap;
	struct device_node    *of_node;
	struct regulator_dev  *rdev;
	struct regulator_desc  rdesc;
	enum wl28661d_vdd_type  type;
	enum wl28661d_vdd_index index;
};
struct wl28661d_pmic {
	bool suspended;
	struct device  *dev;
	struct regmap  *regmap;
	struct pinctrl *pinctrl;
	struct device_node   *of_node;
	struct pinctrl_state *gpio_state_enable;
	struct pinctrl_state *gpio_state_disable;
};
static struct regmap_config wl28661d_regulator_regmap_config = {
	.reg_bits     = 8,
	.val_bits     = 8,
	.max_register = 0xff,
};
struct regulator_data {
	char *name;
	int   min_uv;
	int   max_uv;
	int   step_uv;
};
static const struct regulator_data wl28661d_reg_data[wl28661d_MAX_LDO] = {
	{ "dvdd1",  600000, 1800000,  6000 },
	{ "dvdd2",  600000, 1800000,  6000 },
	{ "avdd1", 1200000, 4300000, 12500 },
	{ "avdd2", 1200000, 4300000, 12500 },
};
static const struct regulator_data sgm38121_reg_data[wl28661d_MAX_LDO] = {
	{ "dvdd1",  504000, 1504000,  8000 },
	{ "dvdd2",  504000, 1504000,  8000 },
	{ "avdd1", 1384000, 3424000,  8000 },
	{ "avdd2", 1384000, 3424000,  8000 },
};
static int wl28661d_vdd_step(u8 chip_id, enum wl28661d_vdd_type type)
{
    if(chip_id == chipid_sgm38121){
        return (type ? 8000 : 8000);
    }else{
        return (type ? 12500 : 6000);
    }
}
static int wl28661d_vdd_real(u8 value, u8 chip_id, enum wl28661d_vdd_type type)
{
    if(chip_id == chipid_sgm38121){
        return (type ? (1384000 + value * 8000) : (504000 + value * 8000));
    }else{
        return (type ? (1200000 + value * 12500) : (600000 + value * 6000));
    }
}
static u8 wl28661d_vdd_reg(int value, u8 chip_id, enum wl28661d_vdd_type type)
{
    if(chip_id == chipid_sgm38121){
        return (type ? ((value - 1384000) / 8000) : ((value - 504000) / 8000));
    }else{
        return (type ? ((value - 1200000) / 12500) : ((value - 600000) / 6000));
    }
}
static int wl28661d_read(struct regmap *regmap,  u8 reg, u8 *val, int count)
{
	int rc;
	pr_debug("wl28661d read: addr-0x%02x, count-%d\n", reg, count);
	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0) {
		pr_err("wl28661d read: failed to read 0x%02x\n", reg);
		usleep_range(wl28661d_RETRY_WAIT_TIME,
			     wl28661d_RETRY_WAIT_TIME + 100);
		rc = regmap_bulk_read(regmap, reg, val, count);
		if (rc < 0)
			pr_err("wl28661d read: failed to read 0x%02x again\n", reg);
	}
	return rc;
}
static int wl28661d_write(struct regmap *regmap, u8 reg, const u8 *val,
			 int count)
{
	int rc;
	pr_debug("wl28661d write: addr-0x%02x, data-0x%02x\n", reg, *val);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0) {
		pr_err("wl28661d write: failed to write 0x%02x\n", reg);
		usleep_range(wl28661d_RETRY_WAIT_TIME,
			     wl28661d_RETRY_WAIT_TIME + 100);
		rc = regmap_bulk_write(regmap, reg, val, count);
		if (rc < 0)
			pr_err("wl28661d write: failed to write 0x%02x again\n", reg);
	}
	return rc;
}
static int wl28661d_masked_write(struct regmap *regmap, u8 reg, u8 mask,
				u8 val)
{
	int rc;
	pr_debug("wl28661d masked write: addr-0x%02x, mask-0x%02x, "
			 "masked_data-0x%02x\n", reg, mask, mask & val);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0) {
		pr_err("wl28661d masked write: failed to write 0x%02x to "
			   "0x%02x with mask 0x%02x\n", val, reg, mask);
		usleep_range(wl28661d_RETRY_WAIT_TIME,
			     wl28661d_RETRY_WAIT_TIME + 100);
		rc = regmap_update_bits(regmap, reg, mask, val);
		if (rc < 0)
			pr_err("wl28661d masked write: failed to write 0x%02x to "
			       "0x%02x with mask 0x%02x\n", val, reg, mask);
	}
	return rc;
}
static int wl28661d_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct wl28661d_regulator *wl28661d_reg = rdev_get_drvdata(rdev);
	int rc, voltage_uv = 0;
	u8  reg_voltage;
	if (*wl28661d_reg->suspended)
		return wl28661d_reg->uv;
	rc = wl28661d_read(wl28661d_reg->regmap, wl28661d_reg->addr,
			  &reg_voltage, 1);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg,
			    "failed to read regulator voltage rc = %d\n", rc);
		return rc;
	}
	voltage_uv = wl28661d_vdd_real(reg_voltage, wl28661d_reg->chip_id, wl28661d_reg->type);
	return voltage_uv;
}
static int wl28661d_regulator_enable(struct regulator_dev *rdev)
{
	struct wl28661d_regulator *wl28661d_reg = rdev_get_drvdata(rdev);
	int rc, current_uv;
	if (*wl28661d_reg->suspended) {
		if (wl28661d_reg->reg_enabled)
			return 0;
		return -EPERM;
	}
	current_uv = wl28661d_regulator_get_voltage(rdev);
	if (current_uv < 0) {
		wl28661d_err(wl28661d_reg, "failed to get current voltage rc = %d\n",
			    current_uv);
		return current_uv;
	}
	rc = wl28661d_masked_write(wl28661d_reg->regmap, wl28661d_ENABLE_REG,
				  1 << wl28661d_reg->index,
				  1 << wl28661d_reg->index);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg, "failed to enable regulator rc = %d\n",
			    rc);
		return rc;
	}
	wl28661d_reg->reg_enabled = true;
	wl28661d_debug(wl28661d_reg, "regulator enabled\n");
	return rc;
}
static int wl28661d_regulator_disable(struct regulator_dev *rdev)
{
	struct wl28661d_regulator *wl28661d_reg = rdev_get_drvdata(rdev);
	int rc;
	if (*wl28661d_reg->suspended) {
		if (!wl28661d_reg->reg_enabled)
			return 0;
		return -EPERM;
	}
	rc = wl28661d_masked_write(wl28661d_reg->regmap, wl28661d_ENABLE_REG,
				  1 << wl28661d_reg->index, 0);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg, "failed to disable regulator rc = %d\n",
			    rc);
		return rc;
	}
	wl28661d_reg->reg_enabled = false;
	wl28661d_debug(wl28661d_reg, "regulator disabled\n");
	return rc;
}
static int wl28661d_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct wl28661d_regulator *wl28661d_reg = rdev_get_drvdata(rdev);
	u8 en_value;
	int rc;
	if (*wl28661d_reg->suspended)
		return wl28661d_reg->reg_enabled;
	rc = wl28661d_read(wl28661d_reg->regmap, wl28661d_ENABLE_REG, &en_value, 1);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg, "failed to read enable reg rc = %d\n", rc);
		return rc;
	}
	return !!(en_value & 1 << wl28661d_reg->index);
}
static int wl28661d_write_voltage(struct wl28661d_regulator *wl28661d_reg,
				 int min_uv, int max_uv)
{
	int rc = 0, voltage;
	u8  reg_voltage;
	voltage = (DIV_ROUND_UP(min_uv, wl28661d_vdd_step(wl28661d_reg->chip_id, wl28661d_reg->type))
		   * wl28661d_vdd_step(wl28661d_reg->chip_id, wl28661d_reg->type));
	if (voltage > max_uv) {
		wl28661d_err(wl28661d_reg, "requested voltage above maximum limit\n");
		return -EINVAL;
	}
	reg_voltage = wl28661d_vdd_reg(voltage, wl28661d_reg->chip_id, wl28661d_reg->type);
	rc = wl28661d_write(wl28661d_reg->regmap, wl28661d_reg->addr, &reg_voltage, 1);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg, "failed to write voltage rc = %d\n", rc);
		return rc;
	}
	wl28661d_reg->uv = voltage;
	return 0;
}
static int wl28661d_regulator_set_voltage(struct regulator_dev *rdev,
					 int min_uv, int max_uv,
					 unsigned int *selector)
{
	struct wl28661d_regulator *wl28661d_reg = rdev_get_drvdata(rdev);
	int rc;
	if (*wl28661d_reg->suspended) {
		if (min_uv <= wl28661d_reg->uv && wl28661d_reg->uv <= max_uv)
			return 0;
		return -EPERM;
	}
	rc = wl28661d_write_voltage(wl28661d_reg, min_uv, max_uv);
	if (rc < 0)
		return rc;
	wl28661d_debug(wl28661d_reg, "voltage set to %d\n", min_uv);
	return rc;
}
static const struct regulator_ops wl28661d_regulator_ops = {
	.enable      = wl28661d_regulator_enable,
	.disable     = wl28661d_regulator_disable,
	.is_enabled  = wl28661d_regulator_is_enabled,
	.set_voltage = wl28661d_regulator_set_voltage,
	.get_voltage = wl28661d_regulator_get_voltage,
};
static int wl28661d_register_ldo(struct wl28661d_regulator *wl28661d_reg,
				const char *name)
{
	const struct regulator_data *reg_data;
	struct device_node *of_node = wl28661d_reg->of_node;
	struct regulator_init_data *init_data;
	struct regulator_config reg_config = {};
	struct device *dev = wl28661d_reg->dev;
	const char *vdd_type;
	int rc, i;
	if(wl28661d_reg->chip_id == chipid_sgm38121){
		reg_data = sgm38121_reg_data;
	}else{
		reg_data = wl28661d_reg_data;
	}
	
	for (i = 0; i < wl28661d_MAX_LDO; i++)
		if (strstr(name, reg_data[i].name))
			break;
	if (i == wl28661d_MAX_LDO) {
		pr_err("wl28661d_pmic: invalid regulator name %s\n", name);
		return -EINVAL;
	}
	rc = of_property_read_u32(of_node, "cell-index", &wl28661d_reg->index);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg, "failed to get regulator index rc = %d\n",
			    rc);
		return rc;
	}
	rc = of_property_read_u8(of_node, "reg", &wl28661d_reg->addr);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg, "failed to get regulator register rc = %d\n",
			    rc);
		return rc;
	}
	rc = of_property_read_string(of_node, "type", &vdd_type);
	if (rc < 0) {
		wl28661d_err(wl28661d_reg, "failed to get regulator type rc = %d\n",
			    rc);
		return rc;
	}
	if (!strcmp(vdd_type, "dvdd"))
		wl28661d_reg->type = VDD_TYPE_DVDD;
	else if (!strcmp(vdd_type, "avdd"))
		wl28661d_reg->type = VDD_TYPE_AVDD;
	else {
		wl28661d_err(wl28661d_reg, "invalid regulator type %s\n",
			    wl28661d_reg->type);
		return -EINVAL;
	}
	init_data = of_get_regulator_init_data(dev, of_node, &wl28661d_reg->rdesc);
	if (init_data == NULL) {
		wl28661d_err(wl28661d_reg, "failed to get regulator data\n");
		return -ENODATA;
	}
	if (!init_data->constraints.name) {
		wl28661d_err(wl28661d_reg, "regulator name missing\n");
		return -EINVAL;
	}
	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
						 | REGULATOR_CHANGE_VOLTAGE;
	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = wl28661d_reg;
	reg_config.of_node = of_node;
	wl28661d_reg->reg_enabled = false;
	wl28661d_reg->uv = reg_data[i].min_uv;
	wl28661d_reg->rdesc.type = REGULATOR_VOLTAGE;
	wl28661d_reg->rdesc.ops = &wl28661d_regulator_ops;
	wl28661d_reg->rdesc.name = init_data->constraints.name;
	wl28661d_reg->rdesc.uV_step = reg_data[i].step_uv;
	wl28661d_reg->rdesc.min_uV = reg_data[i].min_uv;
	wl28661d_reg->rdesc.n_voltages
		= ((reg_data[i].max_uv - reg_data[i].min_uv)
			/ wl28661d_reg->rdesc.uV_step) + 1;
	wl28661d_reg->rdev = devm_regulator_register(dev, &wl28661d_reg->rdesc,
						    &reg_config);
	if (IS_ERR(wl28661d_reg->rdev)) {
		rc = PTR_ERR(wl28661d_reg->rdev);
		wl28661d_err(wl28661d_reg, "failed to register regulator rc = %d\n",
			    rc);
		return rc;
	}
	rc = devm_regulator_debug_register(dev, wl28661d_reg->rdev);
	if (rc)
		wl28661d_err(wl28661d_reg, "failed to register regulator rc = %d\n",
			    rc);
	wl28661d_debug(wl28661d_reg, "regulator registered\n");
	return 0;
}
static int wl28661d_pmic_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct wl28661d_pmic *chip;
	struct wl28661d_regulator *wl28661d_reg;
	struct device_node *child;
	const char *name;
	int rc = 0;
	u8  reg_val;
	u8  chip_id;
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->dev = &client->dev;
	chip->regmap = devm_regmap_init_i2c(client, &wl28661d_regulator_regmap_config);
	if (!chip->regmap)
		return -ENODEV;
	rc = wl28661d_read(chip->regmap, wl28661d_Chip_REG, &chip_id, 1);
	if (rc < 0) {
		pr_err("failed to get chip_id\n");
	}
	chip->pinctrl = devm_pinctrl_get(&client->dev);
	if (!IS_ERR_OR_NULL(chip->pinctrl)) {
		chip->gpio_state_enable =
			pinctrl_lookup_state(chip->pinctrl, wl28661d_PINCTRL_ENABLE);
		if (IS_ERR_OR_NULL(chip->gpio_state_enable)) {
			pr_err("wl28661d_pmic: failed to get wl28661d enabled pinctrl\n");
			return -EINVAL;
		}
		chip->gpio_state_disable =
			pinctrl_lookup_state(chip->pinctrl, wl28661d_PINCTRL_DISABLE);
		if (IS_ERR_OR_NULL(chip->gpio_state_disable)) {
			pr_err("wl28661d_pmic: failed to get wl28661d disabled pinctrl\n");
			return -EINVAL;
		}
		else
		{
			rc = pinctrl_select_state(chip->pinctrl, chip->gpio_state_disable);
			if (rc)
				pr_err("wl28661d_pmic: failed to set wl28661d pin to disable rc = %d\n",
						rc);
		}
	}
	i2c_set_clientdata(client, chip);
	for_each_available_child_of_node(chip->dev->of_node, child) {
		wl28661d_reg = devm_kzalloc(chip->dev, sizeof(*wl28661d_reg), GFP_KERNEL);
		if (!wl28661d_reg)
			return -ENOMEM;
		wl28661d_reg->dev       = chip->dev;
		wl28661d_reg->regmap    = chip->regmap;
		wl28661d_reg->of_node   = child;
		wl28661d_reg->suspended = &(chip->suspended);
		wl28661d_reg->chip_id   = chip_id;
		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc < 0) {
			wl28661d_err(wl28661d_reg, "failed to read register name rc = %d\n",
				    rc);
			return rc;
		}
		rc = wl28661d_register_ldo(wl28661d_reg, name);
		if (rc < 0) {
			wl28661d_err(wl28661d_reg, "failed to register regulator rc = %d\n",
				    rc);
			return rc;
		}
	}
	/* enable regulator output discharge func but none fatal */
	reg_val = wl28661d_ENABLE_DISCHARGE_VAL;
	rc = wl28661d_write(chip->regmap, wl28661d_DISCHARGE_REG, &reg_val, 1);
	if (rc < 0) {
		pr_err("wl28661d: failed to enable discharge rc = %d\n", rc);
		rc = 0;
	}

	dev_set_drvdata(&client->dev, chip);
	pr_debug("wl28661d pimc probe successful\n");
	return rc;
}
static void wl28661d_pmic_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);
}
#ifdef CONFIG_PM_SLEEP
static int wl28661d_pmic_suspend(struct device *dev)
{
	struct wl28661d_pmic *chip = dev_get_drvdata(dev);
	chip->suspended = true;
	return 0;
}
static int wl28661d_pmic_resume(struct device *dev)
{
	struct wl28661d_pmic *chip = dev_get_drvdata(dev);
	chip->suspended = false;
	return 0;
}
#else
static int wl28661d_pmic_suspend(struct device *dev)
{
	return 0;
}
static int wl28661d_pmic_resume(struct device *dev)
{
	return 0;
}
#endif
static const struct dev_pm_ops wl28661d_pmic_pm_ops = {
	.suspend = wl28661d_pmic_suspend,
	.resume  = wl28661d_pmic_resume,
};
static const struct of_device_id wl28661d_pmic_match_table[] = {
	{ .compatible = "xiaomi,wl28661d-pmic" },
	{ },
};
static const struct i2c_device_id wl28661d_pmic_id[] = {
	{ "wl28661d-pmic", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, wl28661d_pmic_id);
static struct i2c_driver wl28661d_pmic_driver = {
	.driver = {
		.name = "wl28661d_pmic",
		.pm   = &wl28661d_pmic_pm_ops,
		.of_match_table = wl28661d_pmic_match_table,
	},
	.probe    = wl28661d_pmic_probe,
	.remove   = wl28661d_pmic_remove,
	.id_table = wl28661d_pmic_id,
};
module_i2c_driver(wl28661d_pmic_driver);
MODULE_DESCRIPTION("Xiaomi wl28661d PMIC Driver");
MODULE_LICENSE("GPL v2");
