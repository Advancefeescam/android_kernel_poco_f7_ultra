// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#if 0
#include <linux/pinctrl/consumer.h>
#endif
#include <linux/kobject.h>
#include <linux/sysfs.h>



#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef LM3601_DTNAME
#define LM3601_DTNAME "mediatek,flashlights_lm3601"
#endif
#ifndef LM3601_DTNAME_I2C
#define LM3601_DTNAME_I2C "mediatek,flashlights_lm3601_i2c"
#endif

#define LM3601_NAME "flashlights-lm3601"

/* define registers */
#define LM3601_REG_SILICON_REVISION (0x00)

#define LM3601_REG_FLASH_FEATURE      (0x08)
#define LM3601_INDUCTOR_CURRENT_LIMIT (0x40)
#define LM3601_FLASH_RAMP_TIME        (0x00)
#define LM3601_FLASH_TIMEOUT          (0x07)

#define LM3601_TORCH_CURRENT_CONTROL (0x04)

#define LM3601_REG_ENABLE (0x01)
#define LM3601_ENABLE_STANDBY (0x00)
#define LM3601_ENABLE_TORCH (0x02)
#define LM3601_ENABLE_FLASH (0x03)

#define LM3601_REG_FLAG (0x0B)

/* define level */
#define LM3601_LEVEL_NUM 9
#define LM3601_LEVEL_TORCH 7
#define LM3601_HW_TIMEOUT 800 /* ms */

#if 0
#define LM3601_PINCTRL_PINSTATE_LOW 0
#define LM3601_PINCTRL_PINSTATE_HIGH 1
#define LM3601_PINCTRL_STATE_HW_EN_HIGH "hwen_high"
#define LM3601_PINCTRL_STATE_HW_EN_LOW  "hwen_low"
static struct pinctrl *lm3601_pinctrl;
static struct pinctrl_state *lm3601_hw_en_high;
static struct pinctrl_state *lm3601_hw_en_low;
#endif

/* define mutex and work queue */
static DEFINE_MUTEX(lm3601_mutex);
static struct work_struct lm3601_work;
static char node_one_buf[20] = {"0"};
static char node_one_buf_current_level[20] = {"0"};
static char get_duty=0;
//static int lm3601_pinctrl_set(int state);
/* lm3601 revision */
static int is_lm3601lt;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *lm3601_i2c_client;

/* platform data */
struct lm3601_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* lm3601 chip data */
struct lm3601_chip_data {
	struct i2c_client *client;
	struct lm3601_platform_data *pdata;
	struct mutex lock;
};


/******************************************************************************
 * lm3601 operations
 *****************************************************************************/
static const int lm3601_current[LM3601_LEVEL_NUM] = {
        2, 64, 110, 188, 258, 302, 329, 350, 376
};

static const unsigned char lm3601_torch_lm3601_level[LM3601_LEVEL_NUM] = {
	0x00, 0x15, 0x24, 0x3F, 0X5F, 0X66, 0X6F, 0x76, 0X7F
};
static const unsigned char lm3601_torch_kdt2691_level[LM3601_LEVEL_NUM] = {
	0x00, 0x15, 0x24, 0x3F, 0X5F, 0X66, 0X6F, 0x76, 0X7F
};
static int lm3601_level = -1;
#if 0
static int lm3601_is_torch(int level)
{
	if (level >= LM3601_LEVEL_TORCH)
		return -1;

	return 0;
}
#endif
static int lm3601_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= LM3601_LEVEL_NUM)
		level = LM3601_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int lm3601_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct lm3601_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_info("failed writing at 0x%02x\n", reg);

	return ret;
}

static int lm3601_read_reg(struct i2c_client *client, u8 reg)
{
	int val = 0;
	struct lm3601_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}

/* flashlight enable function */
static int lm3601_enable(void)
{
	unsigned char reg, val;

	reg = LM3601_REG_ENABLE;
#if 0
	if (!lm3601_is_torch(lm3601_level)) {
		/* torch mode */
		val = LM3601_ENABLE_TORCH;
	} else {
		/* flash mode */
		val = LM3601_ENABLE_FLASH;
	}
#endif
	val = LM3601_ENABLE_TORCH;

	return lm3601_write_reg(lm3601_i2c_client, reg, val);
}

/* flashlight disable function */
static int lm3601_disable(void)
{
	unsigned char reg, val;

	reg = LM3601_REG_ENABLE;
	val = LM3601_ENABLE_STANDBY;

	return lm3601_write_reg(lm3601_i2c_client, reg, val);
}

/* set flashlight level */
static int lm3601_set_level(int level)
{
	unsigned char reg, val;

	level = lm3601_verify_level(level);
	lm3601_level = level;

	reg = LM3601_TORCH_CURRENT_CONTROL;
	if(is_lm3601lt)
		val = lm3601_torch_kdt2691_level[level];
	else
		val = lm3601_torch_lm3601_level[level];
	return lm3601_write_reg(lm3601_i2c_client, reg, val);
}
static int lm3601_set_level_current_level(int level)
{
	unsigned char reg, val,ret;

	reg = LM3601_TORCH_CURRENT_CONTROL;
	if(level <= 127 && level > 0)
		val = level;
	else
		val = 127;//max
	ret = lm3601_write_reg(lm3601_i2c_client, reg, val);
	pr_err("current_level [%s][%d]ret=%d\n",__func__,__LINE__,ret);
	val = lm3601_read_reg(lm3601_i2c_client, LM3601_TORCH_CURRENT_CONTROL);
	pr_err("current_level [%s][%d]val=%d\n",__func__,__LINE__,val);
	return ret;
	
}
static int lm3601_get_flag(void)
{
	return lm3601_read_reg(lm3601_i2c_client, LM3601_REG_FLAG);
}

/* flashlight init */
int lm3601_init(void)
{
	int ret;
#if 1
	pr_err("[xxdd_eeeeee] [%s][%d]",__func__,__LINE__);
	//lm3601_pinctrl_set(1);
	/* get silicon revision */
	ret = lm3601_write_reg(lm3601_i2c_client, 0x6,
			0x80);
	is_lm3601lt = lm3601_read_reg(
			lm3601_i2c_client, 0x6);
	pr_info("LM3601(LT) revision(%d).\n", is_lm3601lt >> 3);

	//ret = lm3601_write_reg(lm3601_i2c_client, 0x4,0x5F);
	//ret = lm3601_write_reg(lm3601_i2c_client, 0x2,0x9);
	//ret = lm3601_write_reg(lm3601_i2c_client, 0x1,0x2);
#else
	ret = lm3601_write_reg(lm3601_i2c_client, 0x2,
			0x9);
	pr_err("[xxdd] [%s][%d]",__func__,__LINE__);
	ret = lm3601_write_reg(lm3601_i2c_client, 0x1,
			0x2);
	pr_err("[xxdd] [%s][%d]",__func__,__LINE__);
	ret = lm3601_write_reg(lm3601_i2c_client, 0x4,
			0x5F);
#endif

	pr_err("[xxdd] [%s][%d]",__func__,__LINE__);
	return ret;
}

/* flashlight uninit */
int lm3601_uninit(void)
{
	lm3601_disable();

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer lm3601_timer;
static unsigned int lm3601_timeout_ms;

static void lm3601_work_disable(struct work_struct *data)
{
	pr_err("work queue callback\n");
	lm3601_disable();
}

static enum hrtimer_restart lm3601_timer_func(struct hrtimer *timer)
{
	schedule_work(&lm3601_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int lm3601_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	pr_err("[xxdd_cmd1] [%s][%d] cmd = [%d]",__func__,__LINE__,_IOC_NR(cmd));

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		lm3601_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		lm3601_set_level(fl_arg->arg);
		get_duty=fl_arg->arg;
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (lm3601_timeout_ms) {
				s = lm3601_timeout_ms / 1000;
				ns = lm3601_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&lm3601_timer, ktime,
						HRTIMER_MODE_REL);
			}
			if(get_duty==4)//torch
				lm3601_set_level_current_level(40);//120ma
			else if (get_duty == 6)//flash
				lm3601_set_level_current_level(118);//350ma
			else
				lm3601_set_level_current_level(118);//350ma
			lm3601_enable();

		} else {
			lm3601_disable();
			hrtimer_cancel(&lm3601_timer);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = LM3601_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = LM3601_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = lm3601_verify_level(fl_arg->arg);
		pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = lm3601_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_info("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = LM3601_HW_TIMEOUT;
		break;

	case FLASH_IOC_GET_HW_FAULT:
		pr_info("FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = lm3601_get_flag();
		break;

	default:
		pr_err("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int lm3601_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int lm3601_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int lm3601_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	pr_err("[xxdd] [%s][%d]",__func__,__LINE__);
	mutex_lock(&lm3601_mutex);
	if (set) {
		if (!use_count)
			ret = lm3601_init();
		use_count++;
		pr_err("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = lm3601_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_err("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&lm3601_mutex);
	pr_err("[xxdd] [%s][%d]",__func__,__LINE__);

	return ret;
}

static ssize_t lm3601_strobe_store(struct flashlight_arg arg)
{
	lm3601_set_driver(1);
	lm3601_set_level(arg.level);
	lm3601_timeout_ms = 0;
	lm3601_enable();
	msleep(arg.dur);
	lm3601_disable();
	lm3601_set_driver(0);

	return 0;
}


static ssize_t att_store_lm3601(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long level = simple_strtoul(buf, NULL, 10);
	sprintf(node_one_buf, "%s", buf);
	pr_err("echo lm3601_torch debug buf,   %s ", buf);
	if ((strcmp ("0", buf) == 0) || (strcmp ("0\x0a", buf) == 0)) {
		pr_err(" lm3601_torch  0");
		lm3601_disable();
		lm3601_set_driver(0);
	} else if (level > 0 && level < 9){
		pr_err(" lm3601_torch  %d\n",level);
		lm3601_set_driver(1);
		lm3601_set_level(level);
		lm3601_timeout_ms = 0;
		lm3601_enable();
	} else{
		pr_err(" lm3601_torch  %d\n",LM3601_LEVEL_NUM);
		lm3601_set_driver(1);
		lm3601_set_level(LM3601_LEVEL_NUM-1);
		lm3601_timeout_ms = 0;
		lm3601_enable();
	}
	return count;
}

static ssize_t att_show_lm3601(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s", node_one_buf);
}
#if 1//add xd

static ssize_t att_store_lm3601_current_level(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long level = simple_strtoul(buf, NULL, 10);
	sprintf(node_one_buf_current_level, "%s", buf);
	pr_err("echo lm3601_torch debug buf,   %s ", buf);
	if ((strcmp ("0", buf) == 0) || (strcmp ("0\x0a", buf) == 0)) {
		pr_err(" lm3601_torch  0");
		lm3601_disable();
		lm3601_set_driver(0);
	} else if (level > 0 && level < 255){
		if(level != 1){
			level=level/2;
			pr_err(" lm3601_torch  %d\n",level);
			lm3601_set_driver(1);
			lm3601_set_level_current_level(level);
			lm3601_timeout_ms = 0;
			lm3601_enable();
		} else if (level == 1)
		{
			pr_err(" lm3601_torch  %d\n",level);
			lm3601_set_driver(1);
			lm3601_set_level_current_level(1);
			lm3601_timeout_ms = 0;
			lm3601_enable();
		}
	} else{
		pr_err(" lm3601_torch  %d\n",LM3601_LEVEL_NUM);
		lm3601_set_driver(1);
		lm3601_set_level(LM3601_LEVEL_NUM-1);
		lm3601_timeout_ms = 0;
		lm3601_enable();
	}
	return count;
}

static ssize_t att_show_lm3601_current_level(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s", node_one_buf_current_level);
}
static DEVICE_ATTR(lm3601_torch_current_level, 0664, att_show_lm3601_current_level, att_store_lm3601_current_level);
#endif

static DEVICE_ATTR(lm3601_torch, 0664, att_show_lm3601, att_store_lm3601);

static struct flashlight_operations lm3601_ops = {
	lm3601_open,
	lm3601_release,
	lm3601_ioctl,
	lm3601_strobe_store,
	lm3601_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int lm3601_chip_init(struct lm3601_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * lm3601_init();
	 */

	return 0;
}

static int lm3601_parse_dt(struct device *dev,
		struct lm3601_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				LM3601_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int lm3601_i2c_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lm3601_chip_data *chip;
	int err;

	pr_err("i2c probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct lm3601_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	i2c_set_clientdata(client, chip);
	lm3601_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init chip hw */
	lm3601_chip_init(chip);

	pr_err("i2c probe done.\n");

	return 0;

err_out:
	return err;
}

static int lm3601_i2c_remove(struct i2c_client *client)
{
	struct lm3601_chip_data *chip = i2c_get_clientdata(client);

	pr_err("Remove start.\n");

	client->dev.platform_data = NULL;

	/* free resource */
	kfree(chip);

	pr_err("Remove done.\n");

	return 0;
}

static const struct i2c_device_id lm3601_i2c_id[] = {
	{LM3601_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lm3601_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id lm3601_i2c_of_match[] = {
	{.compatible = LM3601_DTNAME_I2C},
	{},
};
MODULE_DEVICE_TABLE(of, lm3601_i2c_of_match);
#endif

static struct i2c_driver lm3601_i2c_driver = {
	.driver = {
		.name = LM3601_NAME,
#ifdef CONFIG_OF
		.of_match_table = lm3601_i2c_of_match,
#endif
	},
	.probe = lm3601_i2c_probe,
	.remove = lm3601_i2c_remove,
	.id_table = lm3601_i2c_id,
};
#if 0
/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int lm3601_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	//lm3601_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(lm3601_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(lm3601_pinctrl);
		return -1;
	}

	/*  Flashlight pin initialization */
	lm3601_hw_en_high = pinctrl_lookup_state(lm3601_pinctrl,
		LM3601_PINCTRL_STATE_HW_EN_HIGH);
	if (IS_ERR(lm3601_hw_en_high)) {
		pr_err("Failed to init (%s)\n",
			LM3601_PINCTRL_STATE_HW_EN_HIGH);
		ret = PTR_ERR(lm3601_hw_en_high);
	}

	lm3601_hw_en_low = pinctrl_lookup_state(lm3601_pinctrl,
		LM3601_PINCTRL_STATE_HW_EN_HIGH);
	if (IS_ERR(lm3601_hw_en_low)) {
		pr_err("Failed to init (%s)\n",
			LM3601_PINCTRL_STATE_HW_EN_HIGH);
		ret = PTR_ERR(lm3601_hw_en_low);
	}

	return ret;
}

static int lm3601_pinctrl_set(int state)
{
	int ret = 0;

	if (IS_ERR(lm3601_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	if (state == LM3601_PINCTRL_PINSTATE_LOW &&
		!IS_ERR(lm3601_hw_en_low))
		ret = pinctrl_select_state(lm3601_pinctrl,
			lm3601_hw_en_low);
	else if (state == LM3601_PINCTRL_PINSTATE_HIGH &&
		!IS_ERR(lm3601_hw_en_high))
		ret = pinctrl_select_state(lm3601_pinctrl,
			lm3601_hw_en_high);
	else
		pr_err("set err, state(%d)\n", state);

	return ret;
}
#endif
/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int lm3601_probe(struct platform_device *pdev)
{
	struct lm3601_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct lm3601_chip_data *chip = NULL;
	int err;
	int i;
#if 0
	/* init pinctrl */
	if (lm3601_pinctrl_init(pdev)) {
		pr_err("Failed to init pinctrl.\n");
		err = -EFAULT;
		goto err_free;
	}

	pr_err("lm3601 Probe start.\n");
#endif
	if (i2c_add_driver(&lm3601_i2c_driver)) {
		pr_err("Failed to add i2c driver.\n");
		return -1;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		pdev->dev.platform_data = pdata;
		err = lm3601_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err_free;
	}

	/* init work queue */
	INIT_WORK(&lm3601_work, lm3601_work_disable);

	/* init timer */
	hrtimer_init(&lm3601_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	lm3601_timer.function = lm3601_timer_func;
	lm3601_timeout_ms = 800;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&lm3601_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(LM3601_NAME, &lm3601_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}
	sysfs_create_file(&pdev->dev.kobj, &dev_attr_lm3601_torch.attr);
	sysfs_create_file(&pdev->dev.kobj, &dev_attr_lm3601_torch_current_level.attr);
	pr_err("lm3601 Probe done.\n");

	return 0;
err_free:
	chip = i2c_get_clientdata(lm3601_i2c_client);
	i2c_set_clientdata(lm3601_i2c_client, NULL);
	kfree(chip);
	return err;
}

static int lm3601_remove(struct platform_device *pdev)
{
	struct lm3601_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_err("Remove start.\n");

	i2c_del_driver(&lm3601_i2c_driver);

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(LM3601_NAME);

	/* flush work queue */
	flush_work(&lm3601_work);

	pr_err("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lm3601_of_match[] = {
	{.compatible = LM3601_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, lm3601_of_match);
#else
static struct platform_device lm3601_platform_device[] = {
	{
		.name = LM3601_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, lm3601_platform_device);
#endif

static struct platform_driver lm3601_platform_driver = {
	.probe = lm3601_probe,
	.remove = lm3601_remove,
	.driver = {
		.name = LM3601_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lm3601_of_match,
#endif
	},
};

static int __init flashlight_lm3601_init(void)
{
	int ret;

	pr_err("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&lm3601_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif
pr_err("Jamin lm3601  %s",lm3601_platform_driver.driver.of_match_table[0].compatible);

	ret = platform_driver_register(&lm3601_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_err("Init done.\n");

	return 0;
}

static void __exit flashlight_lm3601_exit(void)
{
	pr_err("Exit start.\n");

	platform_driver_unregister(&lm3601_platform_driver);

	pr_err("Exit done.\n");
}

module_init(flashlight_lm3601_init);
module_exit(flashlight_lm3601_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight LM3601 Driver");

