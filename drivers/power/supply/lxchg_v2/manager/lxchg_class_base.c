#include "lxchg_class.h"

#include "lxchg_printk.h"
#ifdef TAG
#undef TAG
#define TAG "[LX_CHG_CLASS]"
#endif


static struct class *lxchg_class;

extern int lx_adapter_class_init(void);
extern int xm_adapter_class_init(void);
extern int batt_auth_class_init(void);

#ifdef LXCHG_LED_CLASS

static int flash_led_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct flash_led_dev *flash_led = dev_get_drvdata(dev);

	return strcmp(flash_led->name, name) == 0;
}

struct flash_led_dev *flash_led_find_dev_by_name(const char *name)
{
	struct flash_led_dev *flash_led = NULL;
	struct device *dev = class_find_device(lxchg_class, NULL, name,
					flash_led_match_device_by_name);

	if (dev) {
		flash_led = dev_get_drvdata(dev);
	}

	return flash_led;
}
EXPORT_SYMBOL(flash_led_find_dev_by_name);

struct flash_led_dev *flash_led_register(char *name, struct device *parent,
							struct flash_led_ops *ops, void *private)
{
	struct flash_led_dev *flash_led;
	struct device *dev;
	int ret;

	if (!parent)
		lx_info("Expected proper parent device\n");

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	flash_led = kzalloc(sizeof(*flash_led), GFP_KERNEL);
	if (!flash_led)
		return ERR_PTR(-ENOMEM);

	dev = &(flash_led->dev);

	device_initialize(dev);

	dev->class = lxchg_class;
	dev->parent = parent;
	dev_set_drvdata(dev, flash_led);

	flash_led->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	flash_led->name = name;
	flash_led->ops = ops;

	return flash_led;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(flash_led_register);

#endif

#ifdef LXCHG_CHARGER_CLASS
static int charger_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct charger_dev *charger = dev_get_drvdata(dev);

	return strcmp(charger->name, name) == 0;
}

struct charger_dev *charger_find_dev_by_name(const char *name)
{
	struct charger_dev *charger = NULL;
	struct device *dev = class_find_device(lxchg_class, NULL, name,
					charger_match_device_by_name);

	if (dev) {
		charger = dev_get_drvdata(dev);
	}

	return charger;
}
EXPORT_SYMBOL(charger_find_dev_by_name);

struct charger_dev *charger_register(char *name, struct device *parent,
				struct charger_ops *ops, void *private)
{
	struct charger_dev *charger;
	struct device *dev;
	int ret;

	if (!parent)
		lx_err("Expected proper parent device\n");

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return ERR_PTR(-ENOMEM);

	dev = &(charger->dev);

	device_initialize(dev);

	dev->class = lxchg_class;
	dev->parent = parent;
	dev_set_drvdata(dev, charger);

	charger->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	charger->name = name;
	charger->ops = ops;

	return charger;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(charger_register);

void *charger_get_private(struct charger_dev *charger)
{
	if (!charger)
		return ERR_PTR(-EINVAL);
	return charger->private;
}
EXPORT_SYMBOL(charger_get_private);

int charger_unregister(struct charger_dev *charger)
{
	device_unregister(&charger->dev);
	kfree(charger);
	return 0;
}
EXPORT_SYMBOL(charger_unregister);
#endif

#ifdef LXCHG_CHARGEPUMP_CLASS
static int chargerpump_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct chargerpump_dev *chargerpump = dev_get_drvdata(dev);

	return strcmp(chargerpump->name, name) == 0;
}

struct chargerpump_dev *chargerpump_find_dev_by_name(const char *name)
{
	struct chargerpump_dev *chargerpump = NULL;
	struct device *dev = class_find_device(lxchg_class, NULL, name,
					chargerpump_match_device_by_name);

	if (dev) {
		chargerpump = dev_get_drvdata(dev);
	}

	return chargerpump;
}
EXPORT_SYMBOL(chargerpump_find_dev_by_name);

struct chargerpump_dev *chargerpump_register(char *name, struct device *parent,
							struct chargerpump_ops *ops, void *private)
{
	struct chargerpump_dev *chargerpump;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	chargerpump = kzalloc(sizeof(*chargerpump), GFP_KERNEL);
	if (!chargerpump)
		return ERR_PTR(-ENOMEM);

	dev = &(chargerpump->dev);

	device_initialize(dev);

	dev->class = lxchg_class;
	dev->parent = parent;
	dev_set_drvdata(dev, chargerpump);

	chargerpump->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	chargerpump->name = name;
	chargerpump->ops = ops;

	return chargerpump;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(chargerpump_register);
#endif

#ifdef LXCHG_FG_CLASS
static int fuel_gauge_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct fuel_gauge_dev *fuel_gauge = dev_get_drvdata(dev);

	return strcmp(fuel_gauge->name, name) == 0;
}

struct fuel_gauge_dev *fuel_gauge_find_dev_by_name(const char *name)
{
	struct fuel_gauge_dev *fuel_gauge = NULL;
	struct device *dev = class_find_device(lxchg_class, NULL, name,
					fuel_gauge_match_device_by_name);

	if (dev) {
		fuel_gauge = dev_get_drvdata(dev);
	}

	return fuel_gauge;
}
EXPORT_SYMBOL(fuel_gauge_find_dev_by_name);

struct fuel_gauge_dev *fuel_gauge_register(char *name, struct device *parent,
							struct fuel_gauge_ops *ops, void *private)
{
	struct fuel_gauge_dev *fuel_gauge;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	fuel_gauge = kzalloc(sizeof(*fuel_gauge), GFP_KERNEL);
	if (!fuel_gauge)
		return ERR_PTR(-ENOMEM);

	dev = &(fuel_gauge->dev);

	device_initialize(dev);

	dev->class = lxchg_class;
	dev->parent = parent;
	dev_set_drvdata(dev, fuel_gauge);

	fuel_gauge->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	fuel_gauge->name = name;
	fuel_gauge->ops = ops;

	return fuel_gauge;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(fuel_gauge_register);
#endif

#ifdef LXCHG_BATTINFO_CLASS
static int batt_info_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct batt_info_dev *batt_info = dev_get_drvdata(dev);

	return strcmp(batt_info->name, name) == 0;
}

struct batt_info_dev *batt_info_find_dev_by_name(const char *name)
{
	struct batt_info_dev *batt_info = NULL;
	struct device *dev = class_find_device(lxchg_class, NULL, name,
					batt_info_match_device_by_name);

	if (dev) {
		batt_info = dev_get_drvdata(dev);
	}

	return batt_info;
}
EXPORT_SYMBOL(batt_info_find_dev_by_name);

struct batt_info_dev *batt_info_register(char *name, struct device *parent,
							struct batt_info_ops *ops, void *private)
{
	struct batt_info_dev *batt_info;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	batt_info = kzalloc(sizeof(*batt_info), GFP_KERNEL);
	if (!batt_info)
		return ERR_PTR(-ENOMEM);

	dev = &(batt_info->dev);

	device_initialize(dev);

	dev->class = lxchg_class;
	dev->parent = parent;
	dev_set_drvdata(dev, batt_info);

	batt_info->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	batt_info->name = name;
	batt_info->ops = ops;

	return batt_info;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(batt_info_register);

#endif
int lxchg_class_init(void)
{
	lxchg_class = class_create(THIS_MODULE, "lxchg_class");
	if (IS_ERR(lxchg_class)) {
		lx_err("Unable to create lxchg class, errno = %ld\n",
			PTR_ERR(lxchg_class));
		return PTR_ERR(lxchg_class);
	}

	lxchg_class->dev_uevent = NULL;

	lx_adapter_class_init();
	xm_adapter_class_init();
	batt_auth_class_init();

	lx_info("charger class initialize success\n");

	return 0;
}


