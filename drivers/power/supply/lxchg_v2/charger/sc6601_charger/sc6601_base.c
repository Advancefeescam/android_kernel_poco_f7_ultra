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

#define SC6601_VERSION             "1.1.2"

#define SC6601_DEVICE_ID            0X66
#define SC6601_1P1_DEVICE_ID        0x61
#define SC6601A_DEVICE_ID           0x62

#define SC6601_REG_HK_DID          0X00
#define SC6601_REG_HK_IRQ          0x02
#define SC6601_REG_HK_IRQ_MASK     0X03

enum {
    SC6601_SLAVE_HK = 0,
    SC6601_SLAVE_CHG = 0,
#ifdef CONFIG_SC6601_BUCK_CHARGER
    SC6601_SLAVE_LED = 0,
#endif /* CONFIG_SC6601_BUCK_CHARGER */
    SC6601_SLAVE_DPDM = 0,
    SC6601_SLAVE_VOOC = 0,
    SC6601_SLAVE_UFCS,
#ifdef CONFIG_SC6601_6607
    SC6601_SLAVE_LED,
#endif /* CONFIG_SC6601_6607 */
    SC6601_SLAVE_MAX,
};

static const u8 sc6601_slave_addr[] = {
    0x61, 0x63, 0x64,
};

struct sc6601_device {
    struct device *dev;
    struct i2c_client *i2c[SC6601_SLAVE_MAX];
    int irqn;
    struct regmap *rmap;
    struct irq_domain *domain;
    struct irq_chip irq_chip;
    struct mutex irq_lock;
    uint8_t irq_mask;

    atomic_t in_sleep;
    int sc6601_irq;
};

static inline struct i2c_client *bank_to_i2c(struct sc6601_device *sc, u8 bank)
{
    if (bank >= SC6601_SLAVE_MAX)
        return NULL;
    return sc->i2c[bank];
}

static int sc6601_regmap_write(void *context, const void *data, size_t count)
{
    struct sc6601_device *sc = context;
    struct i2c_client *i2c;
    const u8 *_data = data;

    if (atomic_read(&sc->in_sleep)) {
        dev_info(sc->dev, "%s in sleep\n", __func__);
        return -EHOSTDOWN;
    }

    i2c = bank_to_i2c(sc, _data[0]);
    if (!i2c)
        return -EINVAL;

    return i2c_smbus_write_i2c_block_data(i2c, _data[1], count - 2, _data + 2);
}

static int sc6601_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
    int ret;
    struct sc6601_device *sc = context;
    struct i2c_client *i2c;
    const u8 *_reg_buf = reg_buf;

    if (atomic_read(&sc->in_sleep)) {
        dev_info(sc->dev, "%s in sleep\n", __func__);
        return -EHOSTDOWN;
    }

    i2c = bank_to_i2c(sc, _reg_buf[0]);
    if (!i2c)
        return -EINVAL;

    ret = i2c_smbus_read_i2c_block_data(i2c, _reg_buf[1], val_size, val_buf);
    if (ret < 0)
        return ret;

    return ret != val_size ? -EIO : 0;
}

static const struct regmap_bus sc6601_regmap_bus = {
    .write = sc6601_regmap_write,
    .read = sc6601_regmap_read,
};

static const struct regmap_config sc6601_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .reg_format_endian = REGMAP_ENDIAN_BIG,
};

static void sc6601_irq_lock(struct irq_data *data)
{
    struct sc6601_device *sc = irq_data_get_irq_chip_data(data);
    dev_info(sc->dev, "%s \n", __func__);
    mutex_lock(&sc->irq_lock);
}

static irqreturn_t sc6601_irq_thread(int irq, void *data);
static void sc6601_irq_sync_unlock(struct irq_data *data)
{
    struct sc6601_device *sc = irq_data_get_irq_chip_data(data);
    dev_info(sc->dev, "%s \n", __func__);
    regmap_bulk_write(sc->rmap, SC6601_REG_HK_IRQ_MASK, &sc->irq_mask, 1);
    mutex_unlock(&sc->irq_lock);
}

static void sc6601_irq_enable(struct irq_data *data)
{
    struct sc6601_device *sc = irq_data_get_irq_chip_data(data);

    sc->irq_mask &= ~BIT(data->hwirq);
}

static void sc6601_irq_disable(struct irq_data *data)
{
    struct sc6601_device *sc = irq_data_get_irq_chip_data(data);

    sc->irq_mask |= BIT(data->hwirq);
}

static int sc6601_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hwirq)
{
    struct sc6601_device *sc = h->host_data;
    irq_set_chip_data(virq, sc);
    irq_set_chip(virq, &sc->irq_chip);
    irq_set_nested_thread(virq, 1);
    irq_set_parent(virq, sc->irqn);
    irq_set_noprobe(virq);
    return 0;
}

static const struct irq_domain_ops sc6601_domain_ops = {
    .map = sc6601_irq_map,
    .xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t sc6601_irq_thread(int irq, void *data)
{
    struct sc6601_device *sc = data;
    u8 evt = 0;
    bool handle = false;
    int i, ret;

    pm_stay_awake(sc->dev);

    ret = regmap_bulk_read(sc->rmap, SC6601_REG_HK_IRQ, &evt, 1);
    if (ret) {
        dev_err(sc->dev, "failed to read irq event\n");
        return IRQ_HANDLED;
    }

    evt |= BIT(CHARGER_IRQ_HK);

    evt &= ~(sc->irq_mask);

    dev_info(sc->dev, "irq map -> %x, mask -> %x\n", evt, sc->irq_mask);

    for (i = 0; i < CHARGER_IRQ_MAX; i++) {
        if(evt & BIT(i)) {
            handle_nested_irq(irq_find_mapping(sc->domain, i));
            handle = true;
        }
    }

    pm_relax(sc->dev);
    return handle ? IRQ_HANDLED : IRQ_NONE;
}

static int sc6601_add_irq_chip(struct sc6601_device *sc)
{
    int ret = 0;
    int val;

    ret = regmap_bulk_read(sc->rmap, SC6601_REG_HK_IRQ, &val, 1);
    if (ret < 0)
        return ret;
    sc->irq_mask = 0xff;
    ret = regmap_bulk_write(sc->rmap, SC6601_REG_HK_IRQ_MASK, &sc->irq_mask, 1);

    sc->irq_chip.name = dev_name(sc->dev);
    sc->irq_chip.irq_disable = sc6601_irq_disable;
    sc->irq_chip.irq_enable = sc6601_irq_enable;
    sc->irq_chip.irq_bus_lock = sc6601_irq_lock;
    sc->irq_chip.irq_bus_sync_unlock = sc6601_irq_sync_unlock;

    sc->domain = irq_domain_add_linear(sc->dev->of_node,
                        CHARGER_IRQ_MAX, &sc6601_domain_ops, sc);
    if (!sc->domain) {
        dev_err(sc->dev, "failed to create irq domain\n");
        return -ENOMEM;
    }

    ret = devm_request_threaded_irq(sc->dev, sc->irqn,
                            NULL, sc6601_irq_thread,
                            IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(sc->dev), sc);
    if (ret) {
        dev_err(sc->dev, "failed to request irq %d for %s\n", sc->irqn, dev_name(sc->dev));
        irq_domain_remove(sc->domain);
        return ret;
    }

    dev_info(sc->dev, "sc6601 irq = %d\n", sc->irqn);

    return 0;
}

static void sc6601_del_irq_chip(struct sc6601_device *sc)
{
    unsigned int virq;
    int hwirq;

    for (hwirq = 0; hwirq < CHARGER_IRQ_MAX; hwirq++) {
        virq = irq_find_mapping(sc->domain, hwirq);
    if (virq)
    irq_dispose_mapping(virq);
}

irq_domain_remove(sc->domain);
}

static int sc6601_check_chip(struct sc6601_device *sc)
{
    int ret;
    u8 did = 0;

    ret = regmap_bulk_read(sc->rmap, SC6601_REG_HK_DID, &did, 1);
    if (ret < 0 || (did != SC6601_DEVICE_ID && did != SC6601_1P1_DEVICE_ID && did != SC6601A_DEVICE_ID)) {
        return -ENODEV;
    }
    return 0;
}

static int sc6601_probe(struct i2c_client *client)
{
    int i = 0, ret = 0;
    struct sc6601_device *sc;
    u8 evt = 0;

    dev_info(&client->dev, "%s (%s)\n", __func__, SC6601_VERSION);

    sc = devm_kzalloc(&client->dev, sizeof(*sc), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;
    sc->dev = &client->dev;
    i2c_set_clientdata(client, sc);
    mutex_init(&sc->irq_lock);
    atomic_set(&sc->in_sleep, 0);

    for (i = 0; i < SC6601_SLAVE_MAX; i++) {
        if (i == SC6601_SLAVE_CHG) {
            sc->i2c[i] = client;
            continue;
        }
        // kernel version different, need to check
#if 0
        sc->i2c[i] = i2c_new_dummy_device(client->adapter, sc6601_slave_addr[i]);
        if (IS_ERR(sc->i2c[i])) {
            dev_err(&client->dev, "failed to create new i2c[0x%02x] dev\n",
                                        sc6601_slave_addr[i]);
            ret = PTR_ERR(sc->i2c[i]);
            goto err;
        }
#endif
    }

    sc->rmap = devm_regmap_init(sc->dev, &sc6601_regmap_bus, sc,
                                    &sc6601_regmap_config);
    if (IS_ERR(sc->rmap)) {
        dev_err(sc->dev, "failed to init regmap\n");
        ret = PTR_ERR(sc->rmap);
        goto err;
    }
    // check id
    ret = sc6601_check_chip(sc);
    if (ret < 0) {
        dev_err(sc->dev, "failed to check device id\n");
        goto err;
    }

    sc->sc6601_irq = of_get_named_gpio(client->dev.of_node, "intr-gpio", 0);
    if (ret < 0) {
        dev_err(sc->dev, "%s no intr_gpio info\n", __func__);
        return ret;
    } else {
        dev_info(sc->dev, "%s intr_gpio infoi %d\n", __func__, sc->sc6601_irq);
    }
    ret = gpio_request(sc->sc6601_irq, "sc6601 irq pin");
    if (ret) {
        dev_err(sc->dev, "%s: %d gpio request failed\n", __func__, ret);
        return ret;
    }
    dev_info(sc->dev, "%s gpio_irq=%d\n", __func__, sc->sc6601_irq);
    sc->irqn = gpio_to_irq(sc->sc6601_irq);

    // clear flag
    ret = regmap_bulk_read(sc->rmap, SC6601_REG_HK_IRQ, &evt, 1);
    // add irq
    ret = sc6601_add_irq_chip(sc);
    if (ret < 0) {
        dev_err(sc->dev, "failed to add irq chip\n");
        goto err;
    }

    enable_irq_wake(sc->irqn);
    device_init_wakeup(sc->dev, true);

    hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "charger");
    dev_err(sc->dev, "%s:HARDWARE_CHARGER_IC is set!\n", __func__);

    dev_info(&client->dev, "%s probe successfully!\n", __func__);

    return devm_of_platform_populate(sc->dev);
err:
    mutex_destroy(&sc->irq_lock);
    return ret;
}

static int sc6601_remove(struct i2c_client *client)
{
    struct sc6601_device *sc = i2c_get_clientdata(client);

    sc6601_del_irq_chip(sc);
    mutex_destroy(&sc->irq_lock);
    return 0;
}

static int sc6601_suspend(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct sc6601_device *sc = i2c_get_clientdata(i2c);
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irqn);
    disable_irq(sc->irqn);
    return 0;
}

static int sc6601_resume(struct device *dev)
{
    struct i2c_client *i2c = to_i2c_client(dev);
    struct sc6601_device *sc = i2c_get_clientdata(i2c);
    enable_irq(sc->irqn);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irqn);
    return 0;
}

static int sc6601_suspend_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct sc6601_device *sc = i2c_get_clientdata(i2c);

	atomic_set(&sc->in_sleep, 1);
	return 0;
}

static int sc6601_resume_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct sc6601_device *sc = i2c_get_clientdata(i2c);

	atomic_set(&sc->in_sleep, 0);
	return 0;
}

static const struct dev_pm_ops sc6601_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc6601_suspend, sc6601_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(sc6601_suspend_noirq, sc6601_resume_noirq)
};

static const struct of_device_id sc6601_of_match[] = {
	{ .compatible = "southchip,sc6601_base",},
	{ },
};
MODULE_DEVICE_TABLE(of, sc6601_of_match);

static struct i2c_driver sc6601_driver = {
	.probe_new = sc6601_probe,
	.remove = sc6601_remove,
	.driver = {
		.name = "sc6601_base",
		.pm = &sc6601_pm_ops,
		.of_match_table = of_match_ptr(sc6601_of_match),
	},
};

int sc6601_base_init(void)
{
	int rc;
	rc = i2c_add_driver(&sc6601_driver);
	if (rc)
		lx_err("Failed to register I2C driver: %d\n", rc);
	else
		lx_info("i2c_add_driver success!\n");
	return rc;
}


MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("Subpmic Core I2C Drvier");
MODULE_LICENSE("GPL v2");
