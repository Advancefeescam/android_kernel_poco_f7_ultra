#ifndef _MTK_BATTERY_CLASS_CORE_H_
#define _MTK_BATTERY_CLASS_CORE_H_

#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>

#define to_mtk_battery_core(device) container_of(device, struct mtk_battery_core, dev)

enum mtk_battery_charger_type {
    MTK_BATTERY_CHARGER_TYPE_UNKNOWN,
    MTK_BATTERY_CHARGER_TYPE_FLOAT,
    MTK_BATTERY_CHARGER_TYPE_SDP,
    MTK_BATTERY_CHARGER_TYPE_CDP,
    MTK_BATTERY_CHARGER_TYPE_DCP,
    MTK_BATTERY_CHARGER_TYPE_HV_DCP,
};

enum mtk_battery_type{
    MTK_BATTERY_TYPE_UNKNOWN,
    MTK_BATTERY_TYPE_BATTERY,
};
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 start*/
enum mtk_battery_typec_mode {
    MTK_BATTERY_TYPEC_NONE,

	/*Acting as source*/
	MTK_BATTERY_TYPEC_SINK,		/*Rd only*/
	MTK_BATTERY_TYPEC_SINK_POWERED_CABLE,	/*Rd/Ra*/
	MTK_BATTERY_TYPEC_SINK_DEBUG_ACCESSORY, /*Rd/Rd*/
	MTK_BATTERY_TYPEC_SINK_AUDIO_ADAPTER,	/*Ra/Ra*/
	MTK_BATTERY_TYPEC_POWERED_CABLE_ONLY,	/*Ra only*/

	/*Acting as sink*/
	MTK_BATTERY_TYPEC_SOURCE_DEFAULT,		/*Rp default*/
	MTK_BATTERY_TYPEC_SOURCE_MEDIUM,		/*Rp 1.5A*/
	MTK_BATTERY_TYPEC_SOURCE_HIGH,			/*Rp 3A*/
	MTK_BATTERY_TYPEC_NON_COMPLIANT,
};
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 end*/
enum mtk_battery_property{
    MTK_BATTERY_PROP_BATTERY_ID,
    MTK_BATTERY_PROP_BATTERY_ID_VOLTAGE,
    MTK_BATTERY_PROP_BATTERY_VENDOR,
    MTK_BATTERY_PROP_USB_OTG,
    MTK_BATTERY_PROP_TYPEC_CC_ORIENTATION,
    MTK_BATTERY_PROP_REAL_TYPE,
/*L19A HQ-194739 modify input_suspend by gengyifei at 2022/5/12 start*/
    MTK_BATTERY_PROP_INPUT_SUSPEND,
/*L19A HQ-194739 modify input_suspend by gengyifei at 2022/5/12 end*/
/*L19A HQ-193822 modify charging enable node by miaozhichao at 2022/4/18 start*/
    MTK_BATTERY_PROP_CHARGING_ENABLED,
/*L19A HQ-193822 modify charging enable node by miaozhichao at 2022/4/18 end*/
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 start*/
    MTK_BATTERY_PROP_TYPEC_MODE,
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 end*/
/*L19A HQ-194258add shutdown delay 30s by gengyifeiat 2022/4/29 start*/
    MTK_BATTERY_PROP_SHUTDOWN_DELAY,
/*L19A HQ-194258 add shutdown delay 30s by gengyifeiat 2022/4/29 end*/
/*L19A HQ-194773 charge logger resistance by tongjiacheng 2022/05/31 start*/
    MTK_BATTERY_PROP_RESISTANCE,
/*L19A HQ-194773 charge logger resistance by tongjiacheng 2022/05/31 end*/
/*L19AT HQ-254910 charge_counter by zhaohan 2022/10/18 start*/
    MTK_BATTERY_PROP_CHARGE_COUNTER,
/*L19AT HQ-254910 charge_counter by zhaohan 2022/10/18 end*/
};

union mtk_battery_propval{
    /* data */
    int intval;
    const char *strval;
};

struct mtk_battery_core;

struct mtk_battery_config {
    struct device_node *of_node;

    /*driver private data*/
    void *drv_data;

    /*device specific sysfs attributes*/
    const struct attribute_group **attr_grp;
};

struct mtk_battery_desc {
    const char *name;
    enum mtk_battery_type type;

    const enum mtk_battery_property *properties;

    size_t num_properties;

    int (*get_property)(struct mtk_battery_core *bsy,
                                            enum mtk_battery_property bsp,
                                            union mtk_battery_propval *val);
    int (*set_property)(struct mtk_battery_core *bsy,
                                            enum mtk_battery_property bsp,
                                            union mtk_battery_propval *val);
    
    int (*property_is_writeable)(struct mtk_battery_core *bsy,
                        enum mtk_battery_property bsp);
};

struct mtk_battery_core {
    const struct mtk_battery_desc *desc;

    struct device_node *of_node;

    /*driver private data*/
    void *drv_data;

    struct device dev;

    //atomic_t use_cnt;

    bool intialized;
    bool removing;
};

static const char * const battery_vendor_text[] = {
	"COSMX_100K", "SWD_330K", "Unknown"
};

static const char *const charger_type_text[] = {
    "Unknown", "USB_FLOAT", "USB", "USB_CDP",
    "USB_DCP", "USB_HVDCP",
};
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 start*/
static const char * const typec_text[] = {
		"Nothing attached", "Sink attached", "Powered cable w/ sink",
		"Debug Accessory", "Audio Adapter", "Powered cable w/o sink",
		"Source attached (default current)",
		"Source attached (medium current)",
		"Source attached (high current)",
		"Non compliant",
};
/*L19A HQ-209466 fix typec mode node by tongjiacheng at 2022/05/09 end*/
extern struct mtk_battery_core *__must_check
mtk_battery_class_register(struct device *parent,
                                                        const struct mtk_battery_desc *desc,
                                                        const struct mtk_battery_config *cfg);
extern void mtk_battery_class_unregister(struct mtk_battery_core *bsy);
extern void *mtk_battery_class_get_drvdata(struct mtk_battery_core *bsy);
extern int mtk_battery_class_get_property(struct mtk_battery_core *bsy,
            enum mtk_battery_property bsp,
            union mtk_battery_propval *val);
extern int mtk_battery_class_set_property(struct mtk_battery_core *bsy,
            enum mtk_battery_property bsp,
            union mtk_battery_propval *val);




#endif /*_MTK_BATTERY_CLASS_CORE_H_*/
