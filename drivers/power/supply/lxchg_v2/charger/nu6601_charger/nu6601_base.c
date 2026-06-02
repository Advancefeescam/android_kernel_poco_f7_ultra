// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/hardware_info.h>

#include "charger_irq.h"
#include "lxchg_printk.h"

#ifdef TAG
#undef TAG
#define  TAG "[LX_SC6601_BASE]"
#endif

#define NU6601_VERSION                  "1.1.1"

#define NU6601_DEVICE_ID                 0X61

/* intr regs */
#define NU6601_REG_SW_RST                (0x23)
#define NU6601_REG_DEVICE_ID             (0x04)
#define NU6601_REG_INTMNGR_STAT0         (0x0E)
#define NU6601_REG_INTMNGR_STAT1         (0x0F)
#define NU6601_REG_INFRAINT_STAT         (0x10)
#define NU6601_REG_ADCINT_STAT           (0x40)
#define NU6601_REG_QCINT_STAT            (0xB6)
#define NU6601_REG_BC1P2INT_STAT         (0xB0)
#define NU6601_REG_UFCSINT0_STAT         (0x10)
#define NU6601_REG_UFCSINT1_STAT         (0x14)
#define NU6601_REG_UFCSINT2_STAT         (0x18)
#define NU6601_REG_LEDINT1_STAT          (0x80)
#define NU6601_REG_LEDINT2_STAT          (0x84)
#define NU6601_REG_BATTINT_STAT          (0x70)
#define NU6601_REG_BOBULOOPINT_STAT      (0x1C)
#define NU6601_REG_BOBUINT_STAT          (0x18)
#define NU6601_REG_USBINT_STAT           (0x14)
#define NU6601_REG_CHGINT_STAT           (0x10)

#define NU6601_IRQ_EVT_MAX               (128)

struct irq_mapping_tbl {
	const char *name;
	const int id;
};

#define NU6601_IRQ_MAPPING(_name, _id) { .name = #_name, .id = _id}
static const struct irq_mapping_tbl nu6601_irq_mapping_tbl[] = {
	/*0.chg irq 0~7*/
	NU6601_IRQ_MAPPING(usb_det_done, 3),
	NU6601_IRQ_MAPPING(vbus_0v, 4),
	NU6601_IRQ_MAPPING(vbus_gd, 5),
	NU6601_IRQ_MAPPING(chg_fsm, 6),
	NU6601_IRQ_MAPPING(chg_ok, 7),
	/*1.USB irq 8~15*/
	/*2.BOBU irq 16~23*/
	NU6601_IRQ_MAPPING(boost_gd, 20),
	NU6601_IRQ_MAPPING(boost_fail, 21),
	/*3.BOBULOOP irq 24~31*/
	/*4.BATTERY irq 32~39*/
	NU6601_IRQ_MAPPING(vbat_ov, 35),
	/*5.RESERVED irq 40~47*/
	/*6.LED1 irq 48~55*/
	/*7.LED2 IRQ 56~63*/
	NU6601_IRQ_MAPPING(led1_timeout, 56),
	NU6601_IRQ_MAPPING(led2_timeout, 57),
	/*8.INFRA irq 64~71*/
	/*9.ADC irq 72~79*/
	/*10.RESERVED irq 80~87*/
	/*11.QC irq 88~95*/
	NU6601_IRQ_MAPPING(dm_cot_pluse_done, 88),
	NU6601_IRQ_MAPPING(dp_cot_pluse_done, 89),
	NU6601_IRQ_MAPPING(dpdm_2pluse_done, 90),
	NU6601_IRQ_MAPPING(dpdm_3pluse_done, 91),
	NU6601_IRQ_MAPPING(dm_16pluse_done, 92),
	NU6601_IRQ_MAPPING(dp_16pluse_done, 93),
	NU6601_IRQ_MAPPING(hvdcp_det_fail, 94),
	NU6601_IRQ_MAPPING(hvdcp_det_ok, 95),
	/*12.BC1P2 irq 96~103*/
	NU6601_IRQ_MAPPING(dcd_timeout, 99),
	NU6601_IRQ_MAPPING(rid_cid_det, 100),
	NU6601_IRQ_MAPPING(bc1p2_det_done, 103),
	/*13.UFCS2 irq 104~111*/
	/*14.UFCS1 irq 115~119*/
	/*15.UFCS0 irq 120~127*/
};

struct irq_addr_setting_tbl {
	u8 stat_addr;
	u8 mask_flag;
	u8 id;
};

#define NU6601_IRQ_SETTING(_addr, _flag, _id) { .stat_addr = _addr, .mask_flag = _flag, .id = _id}
static struct irq_addr_setting_tbl nu6601_irq_addr_setting_tbl[] = {
	NU6601_IRQ_SETTING(NU6601_REG_CHGINT_STAT, 0x00, 1),
	NU6601_IRQ_SETTING(NU6601_REG_USBINT_STAT, 0xff, 1),
	NU6601_IRQ_SETTING(NU6601_REG_BOBUINT_STAT, 0x03, 1),
	NU6601_IRQ_SETTING(NU6601_REG_BOBULOOPINT_STAT, 0xff, 1),
	NU6601_IRQ_SETTING(NU6601_REG_BATTINT_STAT, 0x10, 1),
	NU6601_IRQ_SETTING(0, 0, 0), //Reserverd
	NU6601_IRQ_SETTING(NU6601_REG_LEDINT1_STAT, 0x00, 1),
	NU6601_IRQ_SETTING(NU6601_REG_LEDINT2_STAT, 0x00, 1),

	NU6601_IRQ_SETTING(NU6601_REG_INFRAINT_STAT, 0x00, 0),
	NU6601_IRQ_SETTING(NU6601_REG_ADCINT_STAT, 0xff, 0),
	NU6601_IRQ_SETTING(0, 0, 0), //Reserverd
	NU6601_IRQ_SETTING(NU6601_REG_QCINT_STAT, 0x00, 2),
	NU6601_IRQ_SETTING(NU6601_REG_BC1P2INT_STAT, 0x00, 2),
	NU6601_IRQ_SETTING(NU6601_REG_UFCSINT2_STAT, 0xff, 2),
	NU6601_IRQ_SETTING(NU6601_REG_UFCSINT1_STAT, 0xff, 2),
	NU6601_IRQ_SETTING(NU6601_REG_UFCSINT0_STAT, 0xff, 2),
};

enum {
	NU6601_SLAVE_ADC,
	NU6601_SLAVE_CHG,
	NU6601_SLAVE_DPDM,
	NU6601_SLAVE_MAX,
};

static const u8 nu6601_slave_addr[] = {
	0x30, 0x31, 0x32,
};

struct nu6601_device {
	struct device *dev;
	struct i2c_client *i2c[NU6601_SLAVE_MAX];
	int irqn;
	struct regmap *rmap;
	struct irq_domain *domain;
	struct irq_chip irq_chip;
	struct mutex irq_lock;
	uint8_t irq_mask;

	atomic_t in_sleep;
	int nu6601_irq;
};

static struct i2c_client *bank_to_i2c(struct nu6601_device *nu6601_dev, u8 bank)
{
	if (bank >= NU6601_SLAVE_MAX)
		return NULL;
	return nu6601_dev->i2c[bank];
}

static int nu6601_regmap_write(void *context, const void *data, size_t count)
{
	struct nu6601_device *nu6601_dev = context;
	struct i2c_client *i2c;
	const u8 *_data = data;

	if (atomic_read(&nu6601_dev->in_sleep)) {
		lx_info("%s in sleep\n", __func__);
		return -EHOSTDOWN;
	}

	i2c = bank_to_i2c(nu6601_dev, _data[0]);
	if (!i2c)
		return -EINVAL;

	return i2c_smbus_write_i2c_block_data(i2c, _data[1], count - 2, _data + 2);
}

static int nu6601_regmap_read(void *context, const void *reg_buf,
				size_t reg_size, void *val_buf, size_t val_size)
{
	int ret;
	struct nu6601_device *nu6601_dev = context;
	struct i2c_client *i2c;
	const u8 *_reg_buf = reg_buf;

	if (atomic_read(&nu6601_dev->in_sleep)) {
		lx_info("%s in sleep\n", __func__);
		return -EHOSTDOWN;
	}

	i2c = bank_to_i2c(nu6601_dev, _reg_buf[0]);
	if (!i2c)
		return -EINVAL;

	ret = i2c_smbus_read_i2c_block_data(i2c, _reg_buf[1], val_size, val_buf);
	if (ret < 0)
		return ret;

	return ret != val_size ? -EIO : 0;
}

static const struct regmap_bus nu6601_regmap_bus = {
	.write = nu6601_regmap_write,
	.read = nu6601_regmap_read,
};

static const struct regmap_config nu6601_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
};

static void nu6601_irq_bus_lock(struct irq_data *data)
{
	struct nu6601_device *nu6601_dev = data->chip_data;
	int i, ret = 0;
	u8 addr, mask, chipid = 0;

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		addr = nu6601_irq_addr_setting_tbl[i].stat_addr + 2;
		if (addr == 2)
			continue;
		chipid = nu6601_irq_addr_setting_tbl[i].id;
		ret = regmap_bulk_read(nu6601_dev->rmap, addr | (chipid << 8), &mask, 1);
		if (ret < 0)
			lx_err("%s: read irq mask fail chipid %d addr %x\n", __func__, chipid, addr);

		nu6601_irq_addr_setting_tbl[i].mask_flag = mask;
	}
}

static void nu6601_irq_bus_unlock(struct irq_data *data)
{
	struct nu6601_device *nu6601_dev = data->chip_data;
	int i, ret = 0;
	u8 addr, mask, chipid = 0;

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		addr = nu6601_irq_addr_setting_tbl[i].stat_addr + 2;
		if (addr == 2)
			continue;
		chipid = nu6601_irq_addr_setting_tbl[i].id;
		mask = nu6601_irq_addr_setting_tbl[i].mask_flag;
		ret = regmap_bulk_write(nu6601_dev->rmap, addr | (chipid << 8), &mask, 1);
		if (ret < 0)
			lx_err("%s: write irq mask fail\n", __func__);
	}
}

static  int nu6601_irq_init(struct nu6601_device *nu6601_dev)
{
	int i, ret = 0;
	u8 addr, mask, val, chipid = 0;

	/* reset chip */
	mask = 0x3f;
	ret = regmap_bulk_write(nu6601_dev->rmap, NU6601_REG_SW_RST | (0 << 8), &mask, 1);
	if (ret < 0) {
		lx_err("reset err !\n");
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		addr = nu6601_irq_addr_setting_tbl[i].stat_addr;
		if (!addr)
			continue;
		chipid = nu6601_irq_addr_setting_tbl[i].id;
		mask = nu6601_irq_addr_setting_tbl[i].mask_flag;

		ret = regmap_bulk_write(nu6601_dev->rmap, (addr + 2) | (chipid << 8), &mask, 1);
		if (ret < 0) {
			lx_err("write err, addr: 0x%x, id %d\n", addr+2, chipid);
			return ret;
		}

		//clear all INT_FLAG
		ret = regmap_bulk_read(nu6601_dev->rmap, (addr + 1) | (chipid << 8), &val, 1);
		if (ret < 0) {
			lx_err("read err, addr: 0x%x, id %d\n", addr+1, chipid);
			return ret;
		}
	}

	return 0;
}

static  const  char *nu6601_get_hwirq_name(struct nu6601_device *nu6601_dev, int hwirq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(nu6601_irq_mapping_tbl); i++) {
		if (nu6601_irq_mapping_tbl[i].id == hwirq)
			return nu6601_irq_mapping_tbl[i].name;
	}
	return "not found";
}

static  void nu6601_irq_disable(struct irq_data *data)
{
	struct nu6601_device *nu6601_dev = data->chip_data;

	lx_info("%s: hwirq = %d, %s\n", __func__, (int)data->hwirq,
		nu6601_get_hwirq_name(nu6601_dev, (int)data->hwirq));
	nu6601_irq_addr_setting_tbl[data->hwirq / 8].mask_flag |= (1 << (data->hwirq % 8));
}

static  void nu6601_irq_enable(struct irq_data *data)
{
	struct nu6601_device *nu6601_dev = data->chip_data;

	lx_info("%s: hwirq = %d, %s\n", __func__, (int)data->hwirq,
		nu6601_get_hwirq_name(nu6601_dev, (int)data->hwirq));
	nu6601_irq_addr_setting_tbl[data->hwirq / 8].mask_flag &= ~(1 << (data->hwirq % 8));
}

static  irqreturn_t nu6601_irq_handler(int irq, void *priv)
{
	struct nu6601_device *nu6601_dev = (struct nu6601_device *)priv;
	u16 irq_stat = 0;
	u8 sub_stat = 0;
	int i = 0, j = 0, ret = 0;
	u8 addr, chipid = 0;

	/*read irq stat*/
	ret = regmap_bulk_read(nu6601_dev->rmap, NU6601_REG_INTMNGR_STAT0  | (0 << 8), (u8 *)&irq_stat, 2);
	if (ret < 0) {
		lx_err("read irq mngr fail\n");
		goto out_irq_handler;
	}
	if (irq_stat == 0)
		goto out_irq_handler;
		
	for (i = 0; i < ARRAY_SIZE(nu6601_irq_addr_setting_tbl); i++) {
		if (!(irq_stat & (1 << i)))
			continue;

		addr = nu6601_irq_addr_setting_tbl[i].stat_addr;
		chipid = nu6601_irq_addr_setting_tbl[i].id;


		/*read sub stat*/
		ret = regmap_bulk_read(nu6601_dev->rmap, (addr + 1) | (chipid << 8), &sub_stat, 1);
		if (ret < 0) {
			lx_err("read irq substa chipd %d 0x%x fail \n", chipid, addr+1);
			goto out_irq_handler;
		}

		for (j = 0; j < 8; j++) {
			if (!(sub_stat & (1 << j)))
				continue;
			ret = irq_find_mapping(nu6601_dev->domain, i * 8 + j);
			if (ret) {
			//	dev_info_ratelimited(nu6601_dev->dev,
			//			"%s: handler irq_domain = (%d, %d)\n",
			//			__func__, i, j);
				handle_nested_irq(ret);
			} 
			//else
			//	lx_err("unmapped %d %d\n", i, j);
		}
	}

out_irq_handler:
	return IRQ_HANDLED;
}


static int nu6601_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hwirq)
{
	struct nu6601_device *nu6601_dev = h->host_data;
	irq_set_chip_data(virq, nu6601_dev);
	irq_set_chip(virq, &nu6601_dev->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, nu6601_dev->irqn);
	irq_set_noprobe(virq);
	return 0;
}
static const struct irq_domain_ops nu6601_domain_ops = {
	.map = nu6601_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static int nu6601_irq_register(struct nu6601_device *nu6601_dev)
{
	int ret = 0;

	ret = nu6601_irq_init(nu6601_dev);
	if (ret < 0)
		return ret;

	nu6601_dev->irq_chip.name = dev_name(nu6601_dev->dev);
	nu6601_dev->irq_chip.irq_disable = nu6601_irq_disable;
	nu6601_dev->irq_chip.irq_enable = nu6601_irq_enable;
	nu6601_dev->irq_chip.irq_bus_lock = nu6601_irq_bus_lock;
	nu6601_dev->irq_chip.irq_bus_sync_unlock = nu6601_irq_bus_unlock;

	nu6601_dev->domain = irq_domain_add_linear(nu6601_dev->dev->of_node,
						 NU6601_IRQ_EVT_MAX,
						 &nu6601_domain_ops,
						 nu6601_dev);
	if (!nu6601_dev->domain) {
		lx_err("failed to create irq domain\n");
		return -ENOMEM;
	}

	ret = devm_request_threaded_irq(nu6601_dev->dev, nu6601_dev->irqn, NULL,
					nu6601_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"nu6601_irq", nu6601_dev);
	if (ret) {
		lx_err("failed to request irq %d for %s\n",
			nu6601_dev->irqn, "nu6601_irq");
		irq_domain_remove(nu6601_dev->domain);
		return ret;
	}

	lx_info("nu6601 irq = %d\n", nu6601_dev->irqn);

	return 0;
}

static void nu6601_del_irq_chip(struct nu6601_device *nu6601_dev)
{
	unsigned int virq;
	int hwirq;

	for (hwirq = 0; hwirq < NU6601_IRQ_EVT_MAX; hwirq++) {
		virq = irq_find_mapping(nu6601_dev->domain, hwirq);
		if (virq)
			irq_dispose_mapping(virq);
	}

	irq_domain_remove(nu6601_dev->domain);
}

static int nu6601_check_chip(struct nu6601_device *nu6601_dev)
{
	int ret;
	u8 did = 0;

	ret = regmap_bulk_read(nu6601_dev->rmap, NU6601_REG_DEVICE_ID | (0 << 8), &did, 1);
	if (ret == 0 && did == NU6601_DEVICE_ID) {
		lx_err("find device nu6601: 0x%x \n", did);
		return 0;
	}

	return -ENODEV;
}

static int nu6601_probe(struct i2c_client *client)
{
    int i = 0, ret = 0;
    struct nu6601_device *nu6601_dev;

    lx_info("%s (%s)\n", __func__, NU6601_VERSION);

    nu6601_dev = devm_kzalloc(&client->dev, sizeof(*nu6601_dev), GFP_KERNEL);
    if (!nu6601_dev)
	return -ENOMEM;

    nu6601_dev->dev = &client->dev;
    i2c_set_clientdata(client, nu6601_dev);
    mutex_init(&nu6601_dev->irq_lock);
    atomic_set(&nu6601_dev->in_sleep, 0);

    for (i = 0; i < NU6601_SLAVE_MAX; i++) {
	if (i == NU6601_SLAVE_ADC) {
	    nu6601_dev->i2c[i] = client;
	    continue;
	}
	// kernel version different, need to check
#if 1
	nu6601_dev->i2c[i] = i2c_new_dummy_device(client->adapter, nu6601_slave_addr[i]);
	if (IS_ERR(nu6601_dev->i2c[i])) {
	    dev_err(&client->dev, "failed to create new i2c[0x%02x] dev\n",
					nu6601_slave_addr[i]);
	    ret = PTR_ERR(nu6601_dev->i2c[i]);
	    goto err;
	}
#endif
    }
    nu6601_dev->rmap = devm_regmap_init(nu6601_dev->dev, &nu6601_regmap_bus, nu6601_dev,
				    &nu6601_regmap_config);
    if (IS_ERR(nu6601_dev->rmap)) {
	lx_err("failed to init regmap\n");
	ret = PTR_ERR(nu6601_dev->rmap);
	goto err;
    }

    // check id
    ret = nu6601_check_chip(nu6601_dev);
    if (ret < 0) {
	lx_err("failed to check device id\n");
	goto err;
    }

    nu6601_dev->nu6601_irq = of_get_named_gpio(client->dev.of_node, "intr-gpio", 0);
    if (ret < 0) {
	lx_err("%s no intr_gpio info\n", __func__);
	return ret;
    } else {
	lx_info("%s intr_gpio infoi %d\n", __func__, nu6601_dev->nu6601_irq);
    }
    ret = gpio_request(nu6601_dev->nu6601_irq, "nu6601 irq pin");
    if (ret) {
	lx_err("%s: %d gpio request failed\n", __func__, ret);
	return ret;
    }
    lx_info("%s gpio_irq=%d\n", __func__, nu6601_dev->nu6601_irq);
    nu6601_dev->irqn = gpio_to_irq(nu6601_dev->nu6601_irq);

    // add irq
    ret = nu6601_irq_register(nu6601_dev);
    if (ret < 0) {
	lx_err("failed to add irq chip\n");
	goto err;
    }

    enable_irq_wake(nu6601_dev->irqn);
    device_init_wakeup(nu6601_dev->dev, true);
    hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "charger");
    lx_err("%s:HARDWARE_CHARGER_IC is set!\n", __func__);

    lx_info("%s probe successfully!\n", __func__);

    return devm_of_platform_populate(nu6601_dev->dev);
err:
    mutex_destroy(&nu6601_dev->irq_lock);
    return ret;
}

static int nu6601_remove(struct i2c_client *client)
{
    struct nu6601_device *nu6601_dev = i2c_get_clientdata(client);

    nu6601_del_irq_chip(nu6601_dev);
    mutex_destroy(&nu6601_dev->irq_lock);
    return 0;
}

static int nu6601_suspend(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct nu6601_device *nu6601_dev = i2c_get_clientdata(i2c);
    if (device_may_wakeup(dev))
	enable_irq_wake(nu6601_dev->irqn);
    disable_irq(nu6601_dev->irqn);
    return 0;
}

static int nu6601_resume(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct nu6601_device *nu6601_dev = i2c_get_clientdata(i2c);
    enable_irq(nu6601_dev->irqn);
    if (device_may_wakeup(dev))
	disable_irq_wake(nu6601_dev->irqn);
    return 0;
}

static int nu6601_suspend_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct nu6601_device *nu6601_dev = i2c_get_clientdata(i2c);

	atomic_set(&nu6601_dev->in_sleep, 1);
	return 0;
}

static int nu6601_resume_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct nu6601_device *nu6601_dev = i2c_get_clientdata(i2c);

	atomic_set(&nu6601_dev->in_sleep, 0);
	return 0;
}

static const struct dev_pm_ops nu6601_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nu6601_suspend, nu6601_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(nu6601_suspend_noirq, nu6601_resume_noirq)
};

static const struct of_device_id nu6601_of_match[] = {
	{ .compatible = "nuvolta,nu6601_base",},
	{ },
};
MODULE_DEVICE_TABLE(of, nu6601_of_match);

static struct i2c_driver nu6601_driver = {
	.probe_new = nu6601_probe,
	.remove = nu6601_remove,
	.driver = {
		.name = "nu6601_base",
		.pm = &nu6601_pm_ops,
		.of_match_table = of_match_ptr(nu6601_of_match),
	},
};

int nu6601_base_init(void)
{
	int rc;
	rc = i2c_add_driver(&nu6601_driver);
	if (rc)
		lx_err("Failed to register I2C driver: %d\n", rc);
	else
		lx_info("i2c_add_driver success!\n");
	return rc;
}

MODULE_AUTHOR("mick.ye@nuvoltatech.com");
MODULE_DESCRIPTION("Nuvolta NU6601 Charger Driver");
MODULE_LICENSE("GPL v2");
