#ifndef _EVENT_MIEVENT_H_
#define _EVENT_MIEVENT_H_

#include <linux/module.h>
#include <linux/kernel.h>

struct misight_mievent {
};

static inline struct misight_mievent *cdev_tevent_alloc(unsigned int eventid)
{

		return NULL;
}
static inline int cdev_tevent_add_int(struct misight_mievent *event, const char *key,
		                        long value)
{
		return 0;
}
static inline int cdev_tevent_add_str(struct misight_mievent *event, const char *key,
		                        const char *value)
{
		return 0;
}
static inline int cdev_tevent_write(struct misight_mievent *event)
{
		return 0;
}
static inline void cdev_tevent_destroy(struct misight_mievent *event)
{
	    return;
}
#endif // _EVENT_MIEVENT_H_
