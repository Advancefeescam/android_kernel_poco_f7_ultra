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
#include <linux/slab.h>
#if 0
#include <linux/pinctrl/consumer.h>
#endif
#include <linux/kobject.h>
#include <linux/sysfs.h>



#include "flashlight-core.h"
#include "flashlight-dt.h"
#include "lxchg_class.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef SC6601_DTNAME
#define SC6601_DTNAME "mediatek,flashlights_sc6601"
#endif

#define SC6601_NAME "flashlights-sc6601"

/* define registers */
#define sc6601_REG_SILICON_REVISION (0x00)

#define sc6601_REG_FLASH_FEATURE      (0x08)
#define sc6601_INDUCTOR_CURRENT_LIMIT (0x40)
#define sc6601_FLASH_RAMP_TIME        (0x00)
#define sc6601_FLASH_TIMEOUT          (0x07)

#define sc6601_TORCH_CURRENT_CONTROL (0x04)

#define sc6601_REG_ENABLE (0x01)
#define sc6601_ENABLE_STANDBY (0x00)
#define sc6601_ENABLE_TORCH (0x02)
#define sc6601_ENABLE_FLASH (0x03)

#define sc6601_REG_FLAG (0x0B)

/* define level */
#define sc6601_LEVEL_NUM 29
#define sc6601_LEVEL_TORCH 17
#define sc6601_HW_TIMEOUT 800 /* ms */
/*N6 code for HQ-304409 by xiexinli at 20230712 start*/
#define sc6601_FLASH_CURRENT 1000   /* ma */
#define sc6601_TORCH_CURRENT 150   /* ma */
#define sc6601_LONGEXPOSURE_CURRENT 400   /* ma */
#define sc6601_LONGEXPOSURE_DUTY 16
#define sc6601_TORCH_DUTY 6
/*N6 code for HQ-304409 by xiexinli at 20230711 end*/
#if 0
#define sc6601_PINCTRL_PINSTATE_LOW 0
#define sc6601_PINCTRL_PINSTATE_HIGH 1
#define sc6601_PINCTRL_STATE_HW_EN_HIGH "hwen_high"
#define sc6601_PINCTRL_STATE_HW_EN_LOW  "hwen_low"
static struct pinctrl *sc6601_pinctrl;
static struct pinctrl_state *sc6601_hw_en_high;
static struct pinctrl_state *sc6601_hw_en_low;
#endif
/*N6 code for HQ-307144 by xiexinli at 20230711 start*/
static char node_one_buf[20] = {"0"};
/*N6 code for HQ-307144 by xiexinli at 20230711 end*/
/* define mutex and work queue */
static DEFINE_MUTEX(sc6601_mutex);
static struct work_struct sc6601_work;
static int get_duty;
//static int sc6601_pinctrl_set(int state);
/* sc6601 revision */

/* define usage count */
static int use_count;

/* platform data */
struct sc6601_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

struct flash_led_dev *g_led_dev = NULL;

/******************************************************************************
 * sc6601 operations
 *****************************************************************************/
static const int sc6601_current[sc6601_LEVEL_NUM] = {
	 25,   45,   65,  85, 105, 125, 150, 175,  200,  225,
	250,  275,  300, 325, 350, 375, 400, 450,  500,  550,
	600,  650,  700, 750, 800, 850, 900, 950, 1000
};

static const int mt6370_flash[sc6601_LEVEL_NUM] = {
	 25,   45,   65,  85, 105, 125, 150, 175,  200,  225,
	250,  275,  300, 325, 350, 375, 400, 450,  500,  550,
	600,  650,  700, 750, 800, 850, 900, 950, 1000
};

static const int mt6370_torch[sc6601_LEVEL_TORCH] = {
	 25,   45,   65,  85, 105, 125, 150, 175,  200,  225,
	250,  275,  300, 325, 350, 375, 400
};

#if 0
static int sc6601_is_torch(int level)
{
	if (level >= sc6601_LEVEL_TORCH)
		return -1;

	return 0;
}
#endif


int sc6601_camera_set_led_flash_curr(enum FLASH_LED_ID id, int ma)
{
	if (!g_led_dev) {
		g_led_dev = flash_led_find_dev_by_name("flash_led");
		if (!g_led_dev) {
			pr_info("%s failed to flash led device\n", __func__);
			return -ENODEV;
		}
	}

	return flash_camera_set_led_flash_curr(g_led_dev, id, ma);
}


int sc6601_camera_set_led_flash_time(enum FLASH_LED_ID id, int ms)
{
	if (!g_led_dev) {
		g_led_dev = flash_led_find_dev_by_name("flash_led");
		if (!g_led_dev) {
			pr_info("%s failed to flash led device\n", __func__);
			return -ENODEV;
		}
	}

	return flash_camera_set_led_flash_time(g_led_dev, id, ms);
}


int sc6601_camera_set_led_flash_enable(enum FLASH_LED_ID id, bool en)
{
	if (!g_led_dev) {
		g_led_dev = flash_led_find_dev_by_name("flash_led");
		if (!g_led_dev) {
			pr_info("%s failed to flash led device\n", __func__);
			return -ENODEV;
		}
	}

	return flash_camera_set_led_flash_enable(g_led_dev, id, en);
}

int sc6601_camera_set_led_torch_curr(enum FLASH_LED_ID id, int ma)
{
	if (!g_led_dev) {
		g_led_dev = flash_led_find_dev_by_name("flash_led");
		if (!g_led_dev) {
			pr_info("%s failed to flash led device\n", __func__);
			return -ENODEV;
		}
	}

	return flash_camera_set_led_torch_curr(g_led_dev, id, ma);
}

int sc6601_camera_set_led_torch_enable(enum FLASH_LED_ID id, bool en)
{
	if (!g_led_dev) {
		g_led_dev = flash_led_find_dev_by_name("flash_led");
		if (!g_led_dev) {
			pr_info("%s failed to flash led device\n", __func__);
			return -ENODEV;
		}
	}

	return flash_camera_set_led_torch_enable(g_led_dev, id, en);
}

static int sc6601_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= sc6601_LEVEL_NUM)
		level = sc6601_LEVEL_NUM - 1;

	return level;
}

/* flashlight enable function */
static int sc6601_enable(void)
{
	unsigned char reg, val;

	reg = sc6601_REG_ENABLE;
#if 0
	if (!sc6601_is_torch(sc6601_level)) {
		/* torch mode */
		val = sc6601_ENABLE_TORCH;
	} else {
		/* flash mode */
		val = sc6601_ENABLE_FLASH;
	}
#endif
	val = sc6601_ENABLE_TORCH;

	return 0;
}

/* flashlight disable function */
static int sc6601_disable(void)
{
	//unsigned char reg, val;

	//reg = sc6601_REG_ENABLE;
	//val = sc6601_ENABLE_STANDBY;

	//return sc6601_write_reg(sc6601_i2c_client, reg, val);
	return 0;
}

/* set flashlight level */
static int sc6601_set_level(int level)
{
	return 0;
}
static int sc6601_get_flag(void)
{
	return 0;
}

/* flashlight init */
int sc6601_init(void)
{
	return 0;
}

/* flashlight uninit */
int sc6601_uninit(void)
{
	sc6601_disable();

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer sc6601_timer;
static unsigned int sc6601_timeout_ms;

static void sc6601_work_disable(struct work_struct *data)
{
	pr_info("work queue callback\n");
	sc6601_disable();
}

static enum hrtimer_restart sc6601_timer_func(struct hrtimer *timer)
{
	schedule_work(&sc6601_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int sc6601_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
  	int duty = 0;
	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	pr_info("[xxdd_cmd1] [%s][%d] cmd = [%d]",__func__,__LINE__,_IOC_NR(cmd));

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		sc6601_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		sc6601_set_level(fl_arg->arg);
		mutex_lock(&sc6601_mutex);
		get_duty=fl_arg->arg;
		if(get_duty<0)
		{
			get_duty=0;
		}else if(get_duty>(sc6601_LEVEL_NUM -1))
		{
			get_duty= sc6601_LEVEL_NUM - 1;
		}
            	duty = get_duty;
		mutex_unlock(&sc6601_mutex);
		break;
	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
            	mutex_lock(&sc6601_mutex);
		duty = get_duty;
		mutex_unlock(&sc6601_mutex);
		if (fl_arg->arg == 1) {
			if(duty==sc6601_LONGEXPOSURE_DUTY || duty==sc6601_TORCH_DUTY )
			{
				sc6601_camera_set_led_torch_curr(FLASH_LED1, mt6370_torch[duty]);
				sc6601_camera_set_led_torch_enable(FLASH_LED1, true);
			}else
			{
				sc6601_camera_set_led_flash_curr(FLASH_LED1, mt6370_flash[duty]);
				sc6601_camera_set_led_flash_enable(FLASH_LED1, true);
			}
		} else {
			if(duty==sc6601_LONGEXPOSURE_DUTY || duty==sc6601_TORCH_DUTY )
			{
				sc6601_camera_set_led_torch_enable(FLASH_LED1, false);
			}else
			{
				sc6601_camera_set_led_flash_enable(FLASH_LED1, false);
			}
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = sc6601_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = sc6601_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = sc6601_verify_level(fl_arg->arg);
		pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = sc6601_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_info("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = sc6601_HW_TIMEOUT;
		break;

	case FLASH_IOC_GET_HW_FAULT:
		pr_info("FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = sc6601_get_flag();
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int sc6601_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sc6601_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sc6601_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	pr_info("[xxdd] [%s][%d]",__func__,__LINE__);
	mutex_lock(&sc6601_mutex);
	if (set) {
		if (!use_count)
			ret = sc6601_init();
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = sc6601_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&sc6601_mutex);
	pr_info("[xxdd] [%s][%d]",__func__,__LINE__);

	return ret;
}

static ssize_t sc6601_strobe_store(struct flashlight_arg arg)
{
	sc6601_set_driver(1);
	sc6601_set_level(arg.level);
	sc6601_timeout_ms = 0;
	sc6601_enable();
	msleep(arg.dur);
	sc6601_disable();
	sc6601_set_driver(0);

	return 0;
}
/*N6 code for HQ-307144 by xiexinli at 20230711 start*/
static ssize_t att_store_sc6601(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	sprintf(node_one_buf, "%s", buf);
	pr_info("echo sc6601_flash debug buf,   %s ", buf);
	if ((strcmp ("0", buf) == 0) || (strcmp ("0\x0a", buf) == 0)) {
		pr_info(" sc6601_flash  0");
		sc6601_camera_set_led_torch_enable(FLASH_LED1, false);
		sc6601_camera_set_led_flash_enable(FLASH_LED1, false);
		sc6601_set_driver(0);
	} else if ((strcmp ("3", buf) == 0) || (strcmp ("3\x0a", buf) == 0)) {
		pr_info(" sc6601_flash  3");
		sc6601_set_driver(1);
		sc6601_camera_set_led_torch_curr(FLASH_LED1, sc6601_LONGEXPOSURE_CURRENT);
		sc6601_timeout_ms = 0;
		sc6601_camera_set_led_torch_enable(FLASH_LED1, true);
	} else if ((strcmp ("4", buf) == 0) || (strcmp ("4\x0a", buf) == 0)) {
		pr_info(" sc6601_flash  4");
		sc6601_set_driver(1);
		sc6601_camera_set_led_flash_curr(FLASH_LED1, sc6601_FLASH_CURRENT);
		sc6601_timeout_ms = 0;
		sc6601_camera_set_led_flash_enable(FLASH_LED1, true);
	}else {
		pr_info(" sc6601_flash  1");
		sc6601_set_driver(1);
		sc6601_camera_set_led_torch_curr(FLASH_LED1, sc6601_TORCH_CURRENT);
		sc6601_timeout_ms = 0;
		sc6601_camera_set_led_torch_enable(FLASH_LED1, true);
	}
	return count;
}

static ssize_t att_show_sc6601(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s", node_one_buf);
}

static DEVICE_ATTR(sc6601_flash, 0664, att_show_sc6601, att_store_sc6601);
/*N6 code for HQ-307144 by xiexinli at 20230711 end*/
static struct flashlight_operations sc6601_ops = {
	sc6601_open,
	sc6601_release,
	sc6601_ioctl,
	sc6601_strobe_store,
	sc6601_set_driver
};

static int sc6601_parse_dt(struct device *dev,
		struct sc6601_platform_data *pdata)
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
				SC6601_NAME);
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

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int sc6601_probe(struct platform_device *pdev)
{
	struct sc6601_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;
	int i;

	pr_debug("Probe start.\n");
	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		pr_info("to devm_kzalloc.\n");
		pdev->dev.platform_data = pdata;
		err = sc6601_parse_dt(&pdev->dev, pdata);
		if (err) {
			pr_info("error  to sc6601_parse_dt.\n");
			goto err_free;
		}
	}

	g_led_dev = flash_led_find_dev_by_name("flash_led");
	if (!g_led_dev)
		pr_info("%s failed to flash led device\n", __func__);

	/* init work queue */
	INIT_WORK(&sc6601_work, sc6601_work_disable);
	pr_info("OK  to INIT_WORK.\n");

	/* init timer */
	hrtimer_init(&sc6601_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sc6601_timer.function = sc6601_timer_func;
	sc6601_timeout_ms = 800;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&sc6601_ops)) {
				err = -EFAULT;
				pr_info("error  to flashlight_dev_register_by_device_id.\n");
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(SC6601_NAME, &sc6601_ops)) {
			err = -EFAULT;
			pr_info("error  to flashlight_dev_register.\n");
			goto err_free;
		}
	}
/*N6 code for HQ-307144 by xiexinli at 20230711 start*/
	sysfs_create_file(&pdev->dev.kobj, &dev_attr_sc6601_flash.attr);
/*N6 code for HQ-307144 by xiexinli at 20230711 end*/
	pr_info("sc6601 Probe done.\n");

	return 0;
err_free:
	pr_info("sc6601 Probe failed.\n");
	return err;
}

static int sc6601_remove(struct platform_device *pdev)
{
	struct sc6601_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_info("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(SC6601_NAME);

	/* flush work queue */
	flush_work(&sc6601_work);

	pr_info("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sc6601_of_match[] = {
	{.compatible = SC6601_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, sc6601_of_match);
#else
static struct platform_device sc6601_platform_device[] = {
	{
		.name = SC6601_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sc6601_platform_device);
#endif

static struct platform_driver sc6601_platform_driver = {
	.probe = sc6601_probe,
	.remove = sc6601_remove,
	.driver = {
		.name = SC6601_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sc6601_of_match,
#endif
	},
};

static int __init flashlight_sc6601_init(void)
{
	int ret;

	pr_info("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&sc6601_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif
pr_info("Jamin sc6601  %s",sc6601_platform_driver.driver.of_match_table[0].compatible);

	ret = platform_driver_register(&sc6601_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_info("Init done.\n");

	return 0;
}

static void __exit flashlight_sc6601_exit(void)
{
	pr_info("Exit start.\n");

	platform_driver_unregister(&sc6601_platform_driver);

	pr_info("Exit done.\n");
}

module_init(flashlight_sc6601_init);
module_exit(flashlight_sc6601_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("Flashlight sc6601 Driver");
