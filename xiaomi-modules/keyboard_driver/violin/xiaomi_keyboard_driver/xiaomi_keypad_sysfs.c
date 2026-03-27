#include "xiaomi_keyboard_macro.h"
#include "xiaomi_keypad_sysfs.h"

static struct attribute *keypad_attr_group[] = {
    NULL,
};

int xiaomi_keypad_sysfs_init(struct xiaomi_keypad_pdata *pdata)
{
    int ret;

    pdata->attrs.attrs = keypad_attr_group;
    ret = sysfs_create_group(&pdata->dev->kobj, &pdata->attrs);
    if (ret)
    {
        MI_KB_ERR("Cannot create sysfs structure! ret = %d\n", ret);
        ret = -ENODEV;
    }

    MI_KB_LOG("xiaomi_keypad_sysfs_init success!");

    return ret;
}

void xiaomi_keypad_sysfs_deinit(struct xiaomi_keypad_pdata *pdata)
{
    sysfs_remove_group(&pdata->dev->kobj, &pdata->attrs);

    MI_KB_LOG("xiaomi_keypad_sysfs_deinit success!");
}


