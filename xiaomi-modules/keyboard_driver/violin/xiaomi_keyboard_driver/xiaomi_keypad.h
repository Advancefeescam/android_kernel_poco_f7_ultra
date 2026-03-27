#ifndef _XIAOMI_KEYPAD_H_
#define _XIAOMI_KEYPAD_H_
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/rtc.h>
#include <linux/types.h>
#include "xiaomi_keyboard_evdev.h"


struct xiaomi_keypad_pdata
{
    struct class *class;
    dev_t dev_num;
    struct device *dev;
    struct attribute_group attrs;

    struct spinlock last_keypad_events_lock;
    struct proc_dir_entry  *last_keypad_events_proc;
    struct last_keypad_event last_keypad_events;

};

extern int xiaomi_keypad_init(struct class *class, dev_t dev_num, struct xiaomi_keypad_pdata *keypad_pdata);
extern void xiaomi_keypad_deinit(struct xiaomi_keypad_pdata *pdata);

#endif
