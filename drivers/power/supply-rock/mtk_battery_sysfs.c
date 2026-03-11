#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/ctype.h>

#include "mtk_battery_sysfs.h"

#define MTK_BATTERY_ATTR(_name)					\
{									\
	.attr = { .name = #_name },					\
	.show = mtk_battery_show_property,				\
	.store = mtk_battery_store_property,				\
}

struct class *mtk_battery_class;
EXPORT_SYMBOL_GPL(mtk_battery_class);

static struct device_type mtk_battery_dev_type;

static struct device_attribute mtk_battery_attrs[];

static ssize_t mtk_battery_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
    ssize_t ret = 0;
    struct mtk_battery_core *bsy = dev_get_drvdata(dev);
    const ptrdiff_t off = attr - mtk_battery_attrs;
    union mtk_battery_propval value;

    ret = mtk_battery_class_get_property(bsy, off, &value);
    if (ret < 0) {
        if (ret == -ENODATA)
            dev_dbg(dev, "driver has no data for '%s' property\n",
                attr->attr.name);
        else if (ret != -ENODEV && ret != -EAGAIN)
			dev_err_ratelimited(dev,
					"driver failed to report `%s' property: %zd\n",
					attr->attr.name, ret);
        return ret;
    }

    if (off == MTK_BATTERY_PROP_BATTERY_VENDOR)
        return sprintf(buf, "%s\n",
					battery_vendor_text[value.intval]);
    else if (off == MTK_BATTERY_PROP_REAL_TYPE)
        return sprintf(buf, "%s\n",
					charger_type_text[value.intval]);
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 start*/
    else if (off == MTK_BATTERY_PROP_TYPEC_MODE)
        return sprintf(buf, "%s\n",
					typec_text[value.intval]);
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 end*/
    else
        return sprintf(buf, "%d\n", value.intval);
}

static ssize_t mtk_battery_store_property(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
    int ret = 0;
    struct mtk_battery_core *bsy = dev_get_drvdata(dev);
    const ptrdiff_t off = attr - mtk_battery_attrs;
    union mtk_battery_propval value;

    switch (off) {
    case MTK_BATTERY_PROP_BATTERY_VENDOR:
        ret = sysfs_match_string(battery_vendor_text, buf);
        break;
    default:
        ret = -EINVAL;
    }

/*deal with int val*/
    if (ret < 0) {
        long long_val;

        ret = kstrtol(buf, 10, &long_val); 
        if (ret < 0)
            return ret;

        ret = long_val; 
    }
    value.intval = ret;
    
    ret = mtk_battery_class_set_property(bsy, off, &value);
    if (ret < 0)
        return ret;

    return count;
}

static struct device_attribute mtk_battery_attrs[] = {
    MTK_BATTERY_ATTR(battery_id),
    MTK_BATTERY_ATTR(battery_id_voltage),
    MTK_BATTERY_ATTR(battery_vendor),
    MTK_BATTERY_ATTR(usb_otg),
    MTK_BATTERY_ATTR(typec_cc_orientation),
    MTK_BATTERY_ATTR(real_type),
/*L19A HQ-194739 modify input_suspend by gengyifei at 2022/5/12 start*/
    MTK_BATTERY_ATTR(input_suspend),
/*L19A HQ-194739 modify input_suspend by gengyifei at 2022/5/12 end*/
/*L19A HQ-193822 modify charging enable node by miaozhichao at 2022/4/18 start*/
    MTK_BATTERY_ATTR(charging_enabled),
/*L19A HQ-193822 modify charging enable node by miaozhichao at 2022/4/18 end*/
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 start*/
    MTK_BATTERY_ATTR(typec_mode),
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 end*/
/*L19A HQ-194258add shutdown delay 30s by gengyifeiat 2022/4/29 start*/
    MTK_BATTERY_ATTR(shutdown_delay),
/*L19A HQ-194258 add shutdown delay 30s by gengyifeiat 2022/4/29 end*/
/*L19A HQ-194773 charge logger resistance by tongjiacheng 2022/05/31 start*/
    MTK_BATTERY_ATTR(resistance),
/*L19A HQ-194773 charge logger resistance by tongjiacheng 2022/05/31 end*/
/*L19AT HQ-254910 charge_counter by zhaohan 2022/10/18 start*/
    MTK_BATTERY_ATTR(charge_counter),
/*L19AT HQ-254910 charge_counter by zhaohan 2022/10/18 end*/
};

static struct attribute *
__mtk_battery_attrs[ARRAY_SIZE(mtk_battery_attrs) + 1];

static umode_t mtk_battery_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr,
					   int attrno)
{
    struct device *dev = container_of(kobj, struct device, kobj);
    struct mtk_battery_core *bsy = dev_get_drvdata(dev);
    umode_t mode = S_IRUSR | S_IRGRP | S_IROTH;
    int i;

    for (i = 0; i < bsy->desc->num_properties; i++) {
        int property = bsy->desc->properties[i];

        if (property == attrno) {
            if (bsy->desc->property_is_writeable &&
                bsy->desc->property_is_writeable(bsy, property) > 0)
            mode |= S_IWUSR;

            return mode;
        }
        
    }

    return 0;
}

static struct attribute_group mtk_battery_attr_group = {
    .attrs = __mtk_battery_attrs,
    .is_visible = mtk_battery_attr_is_visible,
};

static const struct  attribute_group *mtk_battery_attr_groups[] = {
    &mtk_battery_attr_group,
    NULL,
};

static void mtk_battery_init_attrs(struct device_type *dev_type)
{
    int i;

    dev_type->groups = mtk_battery_attr_groups;

    for (i = 0; i < ARRAY_SIZE(mtk_battery_attrs); i++)
        __mtk_battery_attrs[i] = &mtk_battery_attrs[i].attr;
}

static void mtk_battery_dev_release(struct device *dev)
{
    struct mtk_battery_core *bsy = to_mtk_battery_core(dev);
    dev_dbg(dev, "%s\n", __func__);

    kfree(bsy);
}

static char *kstruprdup(const char *str, gfp_t gfp)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = toupper(*str++);

	*ustr = 0;

	return ret;
}


static int mtk_battery_uevent(struct  device *dev, struct kobj_uevent_env *env)
{
    struct mtk_battery_core *bsy = dev_get_drvdata(dev);
    char *prop_buf;
    int ret = 0, j;
    char *attrname;

    if (!bsy || !bsy->desc) {
		dev_dbg(dev, "No power supply yet\n");
		return ret;
	}

    ret = add_uevent_var(env, "MTK_BATTERY_NAME=%s", bsy->desc->name);
    if (ret)
		return ret;
    
    prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
    if (!prop_buf)
        return -ENOMEM;

   for (j = 0; j < bsy->desc->num_properties; j++)  {
       struct device_attribute *attr;
       char *line;

       attr = &mtk_battery_attrs[bsy->desc->properties[j]];

       ret = mtk_battery_show_property(dev, attr, prop_buf);
       if (ret == -ENODEV || ret == -ENODATA) {
           ret = 0;
           continue;
       }

       if (ret < 0)
            goto out;

        line = strchr(prop_buf, '\n');
        if (line)
            *line = 0;

        attrname = kstruprdup(attr->attr.name, GFP_KERNEL);
        if (!attrname) {
            ret = -ENOMEM;
            goto out;
        }

        ret = add_uevent_var(env, "MTK_BATTERY_%s=%s", attrname, prop_buf);
        kfree(attrname);
        if (ret)
            goto out;
   }

out:
    free_page((unsigned long)prop_buf);

    return ret;
}

struct mtk_battery_core *__must_check
mtk_battery_class_register(struct device *parent,
                                                        const struct mtk_battery_desc *desc,
                                                        const struct mtk_battery_config *cfg)
{
    struct device *dev;
    struct mtk_battery_core *bsy;
    int rc;

    if (!parent)
        pr_warn("%s: Expected proper parent device for '%s'\n",
                            __func__, desc->name);
    
    if (!desc || !desc->name || !desc->properties || !desc->num_properties)
        return ERR_PTR(-EINVAL);

    bsy = kzalloc(sizeof(*bsy), GFP_KERNEL);
    if (!bsy)
        return ERR_PTR(-ENOMEM);
    
    dev = &bsy->dev;
    device_initialize(dev);

    dev->class = mtk_battery_class;
    dev->type = &mtk_battery_dev_type;
    dev->parent = parent; 
    dev->release = mtk_battery_dev_release;
    dev_set_drvdata(dev, bsy);
    bsy->desc = desc;

    if (cfg) {
        dev->groups = cfg->attr_grp;
        bsy->drv_data = cfg->drv_data;
        bsy->of_node = cfg->of_node;
    }

    rc = dev_set_name(dev, "%s", desc->name);
    if (rc)
        goto dev_set_name_failed;

    rc =device_add(dev);
    if (rc)
        goto device_add_failed;
    
    bsy->intialized = true;

    return bsy;

device_add_failed:
dev_set_name_failed:
    put_device(dev);
    return ERR_PTR(rc);
}
EXPORT_SYMBOL_GPL(mtk_battery_class_register);

void mtk_battery_class_unregister(struct mtk_battery_core *bsy)
{
    bsy->removing = true;
    device_unregister(&bsy->dev);
}
EXPORT_SYMBOL_GPL(mtk_battery_class_unregister);

void *mtk_battery_class_get_drvdata(struct mtk_battery_core *bsy)
{
    return bsy->drv_data;
}
EXPORT_SYMBOL_GPL(mtk_battery_class_get_drvdata);

int mtk_battery_class_get_property(struct mtk_battery_core *bsy,
            enum mtk_battery_property bsp,
            union mtk_battery_propval *val)
{
    if (!bsy->intialized)
        return -EAGAIN;

    return bsy->desc->get_property(bsy, bsp, val);
}
EXPORT_SYMBOL_GPL(mtk_battery_class_get_property);

int mtk_battery_class_set_property(struct mtk_battery_core *bsy,
            enum mtk_battery_property bsp,
            union mtk_battery_propval *val)
{
    if (!bsy->desc->set_property)
        return -ENODEV;
    
    return bsy->desc->set_property(bsy, bsp, val);
}
EXPORT_SYMBOL_GPL(mtk_battery_class_set_property);

static int __init mtk_battery_class_init(void)
{
    mtk_battery_class = class_create(THIS_MODULE, "mtk-battery");
    if (IS_ERR(mtk_battery_class))
        return PTR_ERR(mtk_battery_class);

    mtk_battery_class->dev_uevent = mtk_battery_uevent;

    mtk_battery_init_attrs(&mtk_battery_dev_type);

    return 0;
}

static void __exit mtk_battery_class_exit(void)
{
    class_destroy(mtk_battery_class);
}

subsys_initcall(mtk_battery_class_init);
module_exit(mtk_battery_class_exit);

MODULE_LICENSE("GPL");
