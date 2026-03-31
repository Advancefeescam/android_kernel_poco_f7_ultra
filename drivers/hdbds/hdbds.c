// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023-2028 Xiaomi Technologies Co., Ltd.
 */

#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include "hdbds.h"

struct hd_bds_sysfs *bds_sysfs;
static int boot_state;

void gpio_free_all(struct hd_bds_sysfs *dev)
{
	struct platform_gpio *gpio = &dev->gpio;
    pr_err("HDBDS: %s: free gpios\n", __func__);
	if (gpio_is_valid(gpio->hdbds_rn_vdd))
		gpio_free(gpio->hdbds_rn_vdd);

	if (gpio_is_valid(gpio->hdbds_prrstx))
		gpio_free(gpio->hdbds_prrstx);

	if (gpio_is_valid(gpio->hdbds_boot))
		gpio_free(gpio->hdbds_boot);

	if (gpio_is_valid(gpio->hdbds_l1))
		gpio_free(gpio->hdbds_l1);

	if (gpio_is_valid(gpio->hdbds_l5))
		gpio_free(gpio->hdbds_l5);

	if (gpio_is_valid(gpio->hdsms_tcxo))
		gpio_free(gpio->hdsms_tcxo);

	if (gpio_is_valid(gpio->hdsms_vdd_ldo))
		gpio_free(gpio->hdsms_vdd_ldo);

	if (gpio_is_valid(gpio->hdsms_rd_prrstx))
		gpio_free(gpio->hdsms_rd_prrstx);

	if (gpio_is_valid(gpio->hdsms_rd_boot))
		gpio_free(gpio->hdsms_rd_boot);

	if (gpio_is_valid(gpio->hdsms_rd_int))
		gpio_free(gpio->hdsms_rd_int);
}

int get_valid_gpio(int gpio)
{
	int value = -EINVAL;

	if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
		pr_info("HDBDS: %s: gpio %d value %d\n", __func__, gpio, value);
	}
	return value;
}

void set_valid_gpio(int gpio, int value)
{
	if (gpio_is_valid(gpio)) {
		pr_info("HDBDS: %s: gpio %d value %d\n", __func__, gpio, value);
		gpio_set_value(gpio, value);
		/* hardware dependent delay */
		usleep_range(10000, 10000 + 100);
	}
}

int configure_gpio(unsigned int gpio, int flag, int value)
{
	int ret;

	if (gpio_is_valid(gpio)) {
		/* set direction and value for output pin */
		if (flag & GPIO_OUTPUT) {
			    pr_info("HDBDS: %s: bds o/p gpio[%d] level value %d\n", __func__,
				    gpio, value);
			    ret = gpio_direction_output(gpio, value);
			    pr_info("HDBDS: %s: bds o/p gpio[%d] level %d\n", __func__,
				    gpio, gpio_get_value(gpio));
		} else {
			ret = gpio_direction_input(gpio);
			pr_info("HDBDS: %s: bds i/p gpio[%d]\n", __func__, gpio);

			//ret = gpio_set_pull(gpio, 0);//no up no down
			pr_info("HDBDS: %s: bds i/p gpio[%d]\n", __func__, gpio);
		}

		if (ret) {
			pr_err("HDBDS: %s: unable to set direction for dbs gpio[%d]\n", __func__, gpio);
			gpio_free(gpio);
			return ret;
		}
		/* Consider value as control for input IRQ pin */
		if (flag & GPIO_IRQ) {
			ret = gpio_to_irq(gpio);
			if (ret < 0) {
				pr_err("HDBDS: %s: unable to set irq for gpio[%d]\n", __func__, gpio);
				gpio_free(gpio);
				return ret;
			}
			pr_info("HDBDS: %s: gpio_to_irq successful for gpio[%d]\n", __func__, gpio);
			return ret;
		}
	} else {
		pr_err("HDBDS: %s: invalid gpio\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

void set_bds_boot_gpio(int gpio, int value)
{
	if (gpio_is_valid(gpio)) {
		pr_info("HDBDS: %s: gpio %d value %d\n", __func__, gpio, value);
		gpio_set_value(gpio, value);
		/* hardware dependent delay */
		usleep_range(10000, 10000 + 100);
	}
}

static ssize_t hdbds_rn_vdd_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;
	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("%s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdbds_rn_vdd_enable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_rn_vdd_enable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_rn_vdd_enable)) {
			pr_err("%s: hdbds_rn_vdd_enable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_rn_vdd_enable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_rn_vdd_enable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_rn_vdd_enable default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdbds_rn_vdd_disable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_rn_vdd_disable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_rn_vdd_disable)) {
			pr_err("%s: hdbds_rn_vdd_disable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_rn_vdd_disable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_rn_vdd_disable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_rn_vdd_disable default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdbds_rn_vdd_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdbds_rn_vdd);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdbds_rn_vdd);


static ssize_t hdbds_prrstx_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdbds_prrstx_reset_suspend =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_prrstx_reset_suspend");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_prrstx_reset_suspend)) {
			pr_err("%s: hdbds_prrstx_reset_suspend  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_prrstx_reset_suspend);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_prrstx_reset_suspend);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_prrstx_reset_suspend default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdbds_prrstx_default =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_prrstx_default");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_prrstx_default)) {
			pr_err("%s: hdbds_prrstx_default  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_prrstx_default);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_prrstx_default);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_prrstx_default default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	pr_info("HDBDS: %s exit.\n", __func__);
	return count;
}

static ssize_t hdbds_prrstx_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdbds_prrstx);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdbds_prrstx);

static ssize_t hdbds_boot_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {
        boot_state = 1;
		bds_sysfs->hdbds_boot_default =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_boot_default");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_boot_default)) {
			pr_err("%s: hdbds_boot_default  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_boot_default);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_boot_default);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_boot_default default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {
        boot_state = 0;
		bds_sysfs->hdbds_boot_enable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_boot_enable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_boot_enable)) {
			pr_err("%s: hdbds_boot_enable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_boot_enable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_boot_enable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_boot_enable default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x02) {
        boot_state = 2;
		bds_sysfs->hdbds_boot_suspend =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_boot_suspend");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_boot_suspend)) {
			pr_err("%s: hdbds_boot_suspend  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_boot_suspend);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_boot_suspend);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_boot_suspend default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdbds_boot_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	//int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	//val = get_valid_gpio(bds_sysfs->gpio.hdbds_boot);
	return snprintf(buf, PAGE_SIZE, "%d\n", boot_state);
}
static CLASS_ATTR_RW(hdbds_boot);


static ssize_t hdbds_l1_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;
	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdbds_l1_enable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_l1_enable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_l1_enable)) {
			pr_err("%s: hdbds_l1_enable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_l1_enable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_l1_enable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_l1_enable default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdbds_l1_disable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_l1_disable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_l1_disable)) {
			pr_err("%s: hdbds_l1_disable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_l1_disable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_l1_disable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_l1_disable default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdbds_l1_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdbds_l1);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdbds_l1);

static ssize_t hdbds_l5_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;
	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdbds_l5_enable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_l5_enable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_l5_enable)) {
			pr_err("%s: hdbds_l5_enable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_l5_enable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_l5_enable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_l5_enable default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdbds_l5_disable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdbds_l5_disable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdbds_l5_disable)) {
			pr_err("%s: hdbds_l5_disable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdbds_l5_disable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdbds_l5_disable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdbds_l5_disable default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdbds_l5_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdbds_l5);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdbds_l5);

static ssize_t hdsms_tcxo_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;
	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdsms_tcxo_enable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_tcxo_enable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_tcxo_enable)) {
			pr_err("%s: hdsms_tcxo_enable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_tcxo_enable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_tcxo_enable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_tcxo_enable default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdsms_tcxo_disable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_tcxo_disable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_tcxo_disable)) {
			pr_err("%s: hdsms_tcxo_disable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_tcxo_disable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_tcxo_disable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_tcxo_disable default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdsms_tcxo_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdsms_tcxo);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdsms_tcxo);

static ssize_t hdsms_vdd_ldo_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;
	//struct platform_gpio *bds_gpio = NULL;
	//int rc, vph_pwr_uv = -EINVAL; 
	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	//bds_gpio = &bds_sysfs->gpio;
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
  
	/*if (bds_sysfs->dev == NULL)
          pr_info ("HDBDS: %s: bds_sysfs->dev is null\n", __func__);
	bds_gpio->hdsms_vdet_chan = iio_channel_get(bds_sysfs->dev, "hdsms_vdet");
  
	if (IS_ERR(bds_gpio->hdsms_vdet_chan)) {
		ret = PTR_ERR(bds_gpio->hdsms_vdet_chan);
		pr_info ("HDBDS: %s: iio_channel_get error ret =  %d\n", __func__, ret);
	}
	else
		rc = iio_read_channel_processed(bds_gpio->hdsms_vdet_chan, &vph_pwr_uv);
        pr_info("HDBDS: %s: hdsms_vdet_chan val = %d\n", __func__, vph_pwr_uv);*/

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdsms_vdd_ldo_en =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_vdd_ldo_en");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_vdd_ldo_en)) {
			pr_err("%s: hdsms_vdd_ldo_en  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_vdd_ldo_en);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_vdd_ldo_en);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_vdd_ldo_en default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdsms_vdd_ldo_disable =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_vdd_ldo_disable");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_vdd_ldo_disable)) {
			pr_err("%s: hdsms_vdd_ldo_disable  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_vdd_ldo_disable);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_vdd_ldo_disable);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_rd_prrstx_default default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdsms_vdd_ldo_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdsms_vdd_ldo);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdsms_vdd_ldo);


static ssize_t hdsms_rd_prrstx_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;
	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdsms_rd_reset_suspend =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_rd_reset_suspend");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_rd_reset_suspend)) {
			pr_err("%s: hdsms_rd_reset_suspend  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_rd_reset_suspend);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_rd_reset_suspend);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_rd_reset_suspend default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdsms_rd_prrstx_default =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_rd_prrstx_default");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_rd_prrstx_default)) {
			pr_err("%s: hdsms_rd_prrstx_default  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_rd_prrstx_default);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_rd_prrstx_default);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_rd_prrstx_default default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdsms_rd_prrstx_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdsms_rd_prrstx);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdsms_rd_prrstx);

static ssize_t hdsms_rd_boot_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);
	if (val == 0x01) {

		bds_sysfs->hdsms_rd_boot_default =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_rd_boot_default");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_rd_boot_default)) {
			pr_err("%s: hdsms_rd_boot_default  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_rd_boot_default);
			return ret;
		}

		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_rd_boot_default);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_rd_boot_en default rc=%d\n", __func__, ret);
			return ret;
		}
	} else if (val == 0x00) {

		bds_sysfs->hdsms_rd_boot_en =
			pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "hdsms_rd_boot_en");
		if(IS_ERR_OR_NULL(bds_sysfs->hdsms_rd_boot_en)) {
			pr_err("%s: hdsms_rd_boot_en  \n", __func__);
			ret = PTR_ERR(bds_sysfs->hdsms_rd_boot_en);
			return ret;
		}

			// set the ED and HPD to the default state
		ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hdsms_rd_boot_en);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl hdsms_rd_boot_en default rc=%d\n", __func__, ret);
			return ret;
		}
	}
	return count;
}

static ssize_t hdsms_rd_boot_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdsms_rd_boot);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdsms_rd_boot);


static ssize_t hdsms_rd_int_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("HDBDS: %s: val = %d\n", __func__, val);

	configure_gpio(bds_sysfs->gpio.hdsms_rd_int, GPIO_INPUT, 0);
	if (val == 0x01) {
		//set_valid_gpio(bds_sysfs->gpio.hdsms_rd_int, 1);
	} else if (val == 0x00) {
	    //set_valid_gpio(bds_sysfs->gpio.hdsms_rd_int, 0);
	}
	return count;
}

static ssize_t hdsms_rd_int_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	int val = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	val = get_valid_gpio(bds_sysfs->gpio.hdsms_rd_int);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RW(hdsms_rd_int);

static ssize_t hdsms_vdet_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	pr_info("HDBDS: %s: enter\n", __func__);
	return count;
}

static ssize_t hdsms_vdet_show(struct class *c,
		 struct class_attribute *attr, char *buf)
{
	//int val = 0;
	int ret = 0;
	int rc, vph_pwr_uv = -EINVAL; 
	struct platform_gpio *bds_gpio = NULL;

	pr_info("HDBDS: %s: enter\n", __func__);
	if (!bds_sysfs) {
		pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	bds_gpio = &bds_sysfs->gpio;
  
	if (bds_sysfs->dev == NULL)
          pr_info ("HDBDS: %s: bds_sysfs->dev is null\n", __func__);
	bds_gpio->hdsms_vdet_chan = iio_channel_get(bds_sysfs->dev, "hdsms_vdet");
  
	if (IS_ERR(bds_gpio->hdsms_vdet_chan)) {
		ret = PTR_ERR(bds_gpio->hdsms_vdet_chan);
		iio_channel_release(bds_gpio->hdsms_vdet_chan);
		pr_info ("HDBDS: %s: iio_channel_get error ret =  %d\n", __func__, ret);
	}
	else{
		rc = iio_read_channel_processed(bds_gpio->hdsms_vdet_chan, &vph_pwr_uv);
		
        }
        pr_info("HDBDS: %s: hdsms_vdet_chan val = %d\n", __func__, vph_pwr_uv);

	//val = get_valid_gpio(bds_sysfs->gpio.hdsms_rd_int);
	return snprintf(buf, PAGE_SIZE, "%d\n", vph_pwr_uv);
}
static CLASS_ATTR_RW(hdsms_vdet);

static struct attribute *bds_sysfs_attrs[] = {
	&class_attr_hdbds_rn_vdd.attr,
	&class_attr_hdbds_prrstx.attr,
	&class_attr_hdbds_boot.attr,
	&class_attr_hdbds_l1.attr,
	&class_attr_hdbds_l5.attr,
	&class_attr_hdsms_tcxo.attr,
	&class_attr_hdsms_vdd_ldo.attr,
	&class_attr_hdsms_rd_prrstx.attr,
	&class_attr_hdsms_rd_boot.attr,
	&class_attr_hdsms_rd_int.attr,
	&class_attr_hdsms_vdet.attr,
	NULL,
};
ATTRIBUTE_GROUPS(bds_sysfs);

static int bds_sysfs_parse_dt(struct hd_bds_sysfs *bds_dev)
{
	struct device_node *np = bds_dev->dev->of_node;
	struct platform_gpio *bds_gpio = &bds_dev->gpio;
	int ret = 0;

	if (!np) {
		pr_err("HDBDS: %s: bds sysfs of_node NULL\n", __func__);
		return -EINVAL;
	}

	bds_gpio->hdbds_rn_vdd = -EINVAL;
	bds_gpio->hdbds_prrstx = -EINVAL;
	bds_gpio->hdbds_boot = -EINVAL;
	bds_gpio->hdbds_l1 = -EINVAL;
	bds_gpio->hdbds_l5 = -EINVAL;
	bds_gpio->hdsms_tcxo = -EINVAL;
	bds_gpio->hdsms_vdd_ldo = -EINVAL;
	bds_gpio->hdsms_rd_prrstx = -EINVAL;
	bds_gpio->hdsms_rd_boot = -EINVAL;
	bds_gpio->hdsms_rd_int = -EINVAL;
  
	
  
/*	bds_gpio->hdsms_vdet_chan = iio_channel_get(bds_dev->dev, "hdsms_vdet");
  
	if (IS_ERR(bds_gpio->hdsms_vdet_chan)) {
		ret = PTR_ERR(bds_gpio->hdsms_vdet_chan);
		pr_info ("HDBDS: %s: iio_channel_get error ret =  %d\n", __func__, ret);
	}
        */

	bds_gpio->hdbds_rn_vdd = of_get_named_gpio(np, DTS_HDBDS_RN_VDD_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdbds_rn_vdd)) {
		pr_err("HDBDS: %s: hdbds_rn_vdd gpio invalid %d\n", __func__, bds_gpio->hdbds_rn_vdd);
		return bds_gpio->hdbds_rn_vdd;
	} else {
		ret = gpio_request(bds_gpio->hdbds_rn_vdd, DTS_HDBDS_RN_VDD_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdbds_rn_vdd);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdbds_rn_vdd gpio %d\n", __func__, bds_gpio->hdbds_rn_vdd);

	bds_gpio->hdbds_prrstx = of_get_named_gpio(np, DTS_HDBDS_PRRSTX_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdbds_prrstx)) {
		pr_err("HDBDS: %s: hdbds_prrstx gpio invalid %d\n", __func__, bds_gpio->hdbds_prrstx);
		return bds_gpio->hdbds_prrstx;
	} else {
		ret = gpio_request(bds_gpio->hdbds_prrstx, DTS_HDBDS_PRRSTX_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdbds_prrstx);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdbds_prrstx gpio %d\n", __func__, bds_gpio->hdbds_prrstx);

	bds_gpio->hdbds_boot = of_get_named_gpio(np, DTS_HDBDS_BOOT_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdbds_boot)) {
		pr_err("HDBDS: %s: hdbds_boot gpio invalid %d\n", __func__, bds_gpio->hdbds_boot);
		return bds_gpio->hdbds_boot;
	} else {
		ret = gpio_request(bds_gpio->hdbds_boot, DTS_HDBDS_BOOT_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdbds_boot);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdbds_boot %d\n", __func__, bds_gpio->hdbds_boot);

	bds_gpio->hdbds_l1 = of_get_named_gpio(np, DTS_HDBDS_L1_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdbds_l1)) {
		pr_err("HDBDS: %s: hdbds_l1 gpio invalid %d\n", __func__, bds_gpio->hdbds_l1);
		return bds_gpio->hdbds_l1;
	} else {
		ret = gpio_request(bds_gpio->hdbds_l1, DTS_HDBDS_L1_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdbds_l1);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdbds_l1 %d\n", __func__, bds_gpio->hdbds_l1);

	bds_gpio->hdbds_l5 = of_get_named_gpio(np, DTS_HDBDS_L5_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdbds_l5)) {
		pr_err("HDBDS: %s: reset gpio invalid %d\n", __func__, bds_gpio->hdbds_l5);
		return bds_gpio->hdbds_l5;
	} else {
		ret = gpio_request(bds_gpio->hdbds_l5, DTS_HDBDS_L5_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdbds_l5);
			return ret;
		}
	}
	pr_info("HDBDS: %s: reset gpio %d\n", __func__, bds_gpio->hdbds_l5);

	bds_gpio->hdsms_tcxo = of_get_named_gpio(np, DTS_HDSMS_TCXO_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdsms_tcxo)) {
		pr_err("HDBDS: %s: reset gpio invalid %d\n", __func__, bds_gpio->hdsms_tcxo);
		return bds_gpio->hdsms_tcxo;
	} else {
		ret = gpio_request(bds_gpio->hdsms_tcxo, DTS_HDSMS_TCXO_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdsms_tcxo);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdsms_tcxo gpio %d\n", __func__, bds_gpio->hdsms_tcxo);

	bds_gpio->hdsms_vdd_ldo = of_get_named_gpio(np, DTS_HDSMS_VDD_LDO_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdsms_vdd_ldo)) {
		pr_err("HDBDS: %s: hdsms_vdd_ldo gpio invalid %d\n", __func__, bds_gpio->hdsms_vdd_ldo);
		return bds_gpio->hdsms_vdd_ldo;
	} else {
		ret = gpio_request(bds_gpio->hdsms_vdd_ldo, DTS_HDSMS_VDD_LDO_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdsms_vdd_ldo);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdsms_vdd_ldo gpio %d\n", __func__, bds_gpio->hdsms_vdd_ldo);

	bds_gpio->hdsms_rd_prrstx = of_get_named_gpio(np, DTS_HDSMS_RD_PRRSTX_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdsms_rd_prrstx)) {
		pr_err("HDBDS: %s: hdsms_rd_prrstx gpio invalid %d\n", __func__, bds_gpio->hdsms_rd_prrstx);
		return bds_gpio->hdsms_rd_prrstx;
	} else {
		ret = gpio_request(bds_gpio->hdsms_rd_prrstx, DTS_HDSMS_RD_PRRSTX_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdsms_rd_prrstx);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdsms_rd_prrstx %d\n", __func__, bds_gpio->hdsms_rd_prrstx);

	bds_gpio->hdsms_rd_boot = of_get_named_gpio(np, DTS_HDSMS_RD_BOOT_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdsms_rd_boot)) {
		pr_err("HDBDS: %s: hdsms_rd_boot gpio invalid %d\n", __func__, bds_gpio->hdsms_rd_boot);
		return bds_gpio->hdsms_rd_boot;
	} else {
		ret = gpio_request(bds_gpio->hdsms_rd_boot, DTS_HDSMS_RD_BOOT_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdsms_rd_boot);
			return ret;
		}
	}
	pr_info("HDBDS: %s: hdsms_rd_boot gpio %d\n", __func__, bds_gpio->hdsms_rd_boot);

	bds_gpio->hdsms_rd_int = of_get_named_gpio(np, DTS_HDSMS_RD_INIT_STR, 0);
	if (!gpio_is_valid(bds_gpio->hdsms_rd_int)) {
		pr_err("HDBDS: %s: hdsms_rd_int gpio invalid %d\n", __func__, bds_gpio->hdsms_rd_int);
		return bds_gpio->hdsms_rd_int;
	} else {
		ret = gpio_request(bds_gpio->hdsms_rd_int, DTS_HDSMS_RD_INIT_STR);
		if (ret) {
			pr_err("HDBDS: %s: unable to request bds gpio [%d]\n", __func__, bds_gpio->hdsms_rd_int);
			return ret;
		}
	}
	pr_info("HDBDS: %s: reset gpio %d\n", __func__, bds_gpio->hdsms_rd_int);

	return 0;
}

static int hd_bds_sysfs_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("HDBDS: %s: enter\n", __func__);
	bds_sysfs = devm_kzalloc(&pdev->dev, sizeof(*bds_sysfs), GFP_KERNEL);
	if (!bds_sysfs)
		return -ENOMEM;

	bds_sysfs->dev = &pdev->dev;
	ret = bds_sysfs_parse_dt(bds_sysfs);
	if (ret) {
		pr_err("HDBDS: %s: parse dt fail\n", __func__);
		return ret;
	}

    //initial the pinctrl system
	bds_sysfs->hdbds_pinctrl = devm_pinctrl_get(bds_sysfs->dev);
	if(IS_ERR_OR_NULL(bds_sysfs->hdbds_pinctrl)) {
		pr_err("%s: No pinctrl config specified\n", __func__);
		ret = PTR_ERR(bds_sysfs->dev);
		return ret;
	}

	// the default ED pinctrl state check
	bds_sysfs->hd_boot =
		pinctrl_lookup_state(bds_sysfs->hdbds_pinctrl, "boot");
	if(IS_ERR_OR_NULL(bds_sysfs->hd_boot)) {
		pr_err("%s: HDBDS hd_boot  \n", __func__);
		ret = PTR_ERR(bds_sysfs->hd_boot);
		return ret;
	}

	// set BDS & SMS default state
	ret = pinctrl_select_state(bds_sysfs->hdbds_pinctrl, bds_sysfs->hd_boot);
	if (ret < 0) {
		pr_err("%s: fail to select pinctrl hd_boot default rc=%d\n", __func__, ret);
		return ret;
	}
	pr_info("GPIO initialize of the HDBDS %d:\n", ret);

	platform_set_drvdata(pdev, bds_sysfs);
	bds_sysfs->hd_bds_class.name = CLASS_NAME;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 9))
	bds_sysfs->hd_bds_class.owner = THIS_MODULE;
#endif
	bds_sysfs->hd_bds_class.class_groups = bds_sysfs_groups;
	ret = class_register(&bds_sysfs->hd_bds_class);
	if (ret < 0) {
		pr_err("HDBDS: %s: class register failed %d\n", __func__, ret);
		return ret;
	}

	pr_info("HDBDS: %s: success\n", __func__);
	return 0;
}

static int hd_bds_sysfs_remove(struct platform_device *pdev)
{

    if  (!bds_sysfs) {
		bds_sysfs = platform_get_drvdata(pdev);
		 if  (!bds_sysfs) {
			pr_err("HDBDS: %s: device doesn't exist anymore\n", __func__);
			return -ENODEV;
		 } 
	}
	pr_info("HDBDS: %s\n", __func__);
 	class_unregister(&bds_sysfs->hd_bds_class);
	gpio_free_all(bds_sysfs);

	return 0;
}

static const struct of_device_id match_table[] = {
	{.compatible = "xiaomi,huada_gnss"},
	{},
};

static struct platform_driver hd_bds_sysfs_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "xiaomi,huada_gnss",
		.of_match_table = of_match_ptr(match_table),
	},
	.probe = hd_bds_sysfs_probe,
	.remove = hd_bds_sysfs_remove,
};

static int __init hd_bds_sysfs_init(void)
{
	pr_info("HDBDS: %s: loading driver\n", __func__);
	return platform_driver_register(&hd_bds_sysfs_driver);
}
module_init(hd_bds_sysfs_init);

static void __exit hd_bds_sysfs_exit(void)
{
	pr_info("HDBDS: %s: unloading driver\n", __func__);
	platform_driver_unregister(&hd_bds_sysfs_driver);
}
module_exit(hd_bds_sysfs_exit);

MODULE_DESCRIPTION("hd bds sysfs");
MODULE_LICENSE("GPL v2");
