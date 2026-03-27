// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024, X-Ring technologies Inc., All rights reserved.
 */

#include <linux/version.h>
#include <linux/limits.h>
#include <dt-bindings/xring/common/xctrl_cpu_rpc/include/xctrlcpu_rpc_def.h>
#include <dt-bindings/xring/common/tsens/tsens_comm.h>
#include "xr_soc_tm.h"

/* lpc device attribute */
#define create_sysfs_lpc_var(_name, _var)						\
	static ssize_t									\
	_name##_show(struct device *dev, struct device_attribute *devattr, char *buf)	\
	{										\
		s32 val;								\
		int ret;								\
		uint32_t msg[THERMAL_VOTEMNG_MAX_WORD];					\
		struct xr_thermal_data *data;						\
											\
		data = to_xr_thermal_data(dev);						\
		if (!data || !data->votemng)						\
			return sprintf(buf, "NAN");					\
		msg[THERMAL_VOTEMNG_CMD_WORD] = THERMAL_VOTEMNG_CMD_GET_VAR;		\
		msg[THERMAL_VOTEMNG_P1_WORD] = _var;					\
		ret = vote_mng_msg_send(data->votemng,					\
			msg,								\
			THERMAL_VOTEMNG_MAX_WORD,					\
			VOTE_MNG_MSG_SYNC);						\
		if (ret) {								\
			dev_err(dev, "votemng msg send error");				\
			return 0;							\
		}									\
		val = msg[THERMAL_VOTEMNG_RET_WORD];					\
		return sprintf(buf, "%d\n", val);					\
	}										\
	static ssize_t									\
	_name##_store(struct device *dev, struct device_attribute *devattr,		\
		const char *buf, size_t count)						\
	{										\
		s32 val;								\
		int ret;								\
		uint32_t msg[THERMAL_VOTEMNG_MAX_WORD];					\
		struct xr_thermal_data *data;						\
											\
		if (kstrtos32(buf, 10, &val) != 0)					\
			return -EINVAL;							\
		data = to_xr_thermal_data(dev);						\
		if (data == NULL || data->votemng == NULL)				\
			return -EINVAL;							\
		msg[THERMAL_VOTEMNG_CMD_WORD] = THERMAL_VOTEMNG_CMD_SET_VAR;		\
		msg[THERMAL_VOTEMNG_P1_WORD] = _var;					\
		msg[THERMAL_VOTEMNG_P2_WORD] = val;					\
		ret = vote_mng_msg_send(data->votemng,					\
					msg,						\
					THERMAL_VOTEMNG_MAX_WORD,			\
					VOTE_MNG_MSG_SYNC);				\
		if (ret) {								\
			dev_err(dev, "votemng msg send error");				\
			return 0;							\
		}									\
		return count;								\
	}										\
	static DEVICE_ATTR(_name, 0644, _name##_show, _name##_store)

create_sysfs_lpc_var(throttle, THERMAL_VAR_THROTTOLE);
create_sysfs_lpc_var(polling_delay, THERMAL_VAR_POOLING_DELAY);
create_sysfs_lpc_var(ddr_max_cstate, THERMAL_VAR_DDR_MAX_CSTATE);

static struct attribute *lpc_device_attrs[] = {
	&dev_attr_polling_delay.attr,
	&dev_attr_throttle.attr,
	&dev_attr_ddr_max_cstate.attr,
	NULL,
};

static const struct attribute_group lpc_device_attr_group = {
	.attrs = lpc_device_attrs,
};

static const struct attribute_group *lpc_device_attr_groups[] = {
	&lpc_device_attr_group,
	NULL,
};

/* bcl device attribute */
static s32 bcl_soc_power;
static s32 bcl_total_power;
static s32 bcl_audio_power;
static s32 bcl_base_power;
static s32 bcl_display_power;
static s32 bcl_camera_power;
static s32 bcl_gpu_prof_limit;
static s32 bcl_npu_prof_limit;

static void bcl_power_init(void)
{
	bcl_total_power = 100000;
	bcl_audio_power = 0;
	bcl_base_power = 12500;
	bcl_display_power = 0;
	bcl_camera_power = 0;
	bcl_gpu_prof_limit = 556;
	bcl_npu_prof_limit = 537;
}
static s32 bcl_power_reallocate(void)
{
	/* calc peri base power */
	bcl_soc_power = bcl_total_power - bcl_audio_power - bcl_display_power - bcl_camera_power - bcl_base_power;
	return bcl_soc_power;
}

#define create_sysfs_bcl_var(_name, _var)						\
	static ssize_t									\
	_name##_show(struct device *dev, struct device_attribute *devattr, char *buf)	\
	{										\
		return sprintf(buf, "%d\n", _var);					\
	}										\
	static ssize_t									\
	_name##_store(struct device *dev, struct device_attribute *devattr,		\
		const char *buf, size_t count)						\
	{										\
		s32 val;								\
		int ret;								\
		uint32_t msg[THERMAL_VOTEMNG_MAX_WORD];					\
		struct xr_thermal_data *data;						\
											\
		if (kstrtos32(buf, 10, &val) != 0)					\
			return -EINVAL;							\
		data = bcl_to_xr_thermal_data(dev);					\
		if (data == NULL || data->votemng_bcl == NULL)				\
			return -EINVAL;							\
		mutex_lock(&data->bcl_lock);						\
		_var = val;								\
		bcl_power_reallocate();							\
		msg[0] = 0x5000C;							\
		msg[1] = bcl_soc_power;							\
		msg[2] = bcl_gpu_prof_limit;						\
		msg[3] = bcl_npu_prof_limit;						\
		ret = vote_mng_msg_send(data->votemng_bcl,				\
					msg,						\
					THERMAL_VOTEMNG_MAX_WORD,			\
					VOTE_MNG_MSG_SYNC);				\
		mutex_unlock(&data->bcl_lock);						\
		dev_info(dev, "[bcl_power]total:%d base:%d soc:%d ad:%d dp:%d cm:%d\n",	\
			bcl_total_power, bcl_base_power, bcl_soc_power, bcl_audio_power,\
			bcl_display_power, bcl_camera_power);				\
		if (ret) {								\
			dev_err(dev, "votemng msg send error");				\
			return 0;							\
		}									\
		return count;								\
	}										\
	static DEVICE_ATTR(_name, 0644, _name##_show, _name##_store)

#define create_sysfs_bcl_var_ro(_name, _var)						\
	static ssize_t									\
	_name##_show(struct device *dev, struct device_attribute *devattr, char *buf)	\
	{										\
		return sprintf(buf, "%d\n", _var);					\
	}										\
	static DEVICE_ATTR(_name, 0644, _name##_show, NULL)

create_sysfs_bcl_var(total_power, bcl_total_power);
create_sysfs_bcl_var(audio_power, bcl_audio_power);
create_sysfs_bcl_var(display_power, bcl_display_power);
create_sysfs_bcl_var(camera_power, bcl_camera_power);
create_sysfs_bcl_var(soc_power, bcl_soc_power);
create_sysfs_bcl_var(gpu_prof_limit, bcl_gpu_prof_limit);
create_sysfs_bcl_var(npu_prof_limit, bcl_npu_prof_limit);
create_sysfs_bcl_var(base_power, bcl_base_power);

static struct attribute *bcl_device_attrs[] = {
	&dev_attr_total_power.attr,
	&dev_attr_audio_power.attr,
	&dev_attr_display_power.attr,
	&dev_attr_camera_power.attr,
	&dev_attr_soc_power.attr,
	&dev_attr_gpu_prof_limit.attr,
	&dev_attr_npu_prof_limit.attr,
	&dev_attr_base_power.attr,
	NULL,
};

static const struct attribute_group bcl_device_attr_group = {
	.attrs = bcl_device_attrs,
};

static const struct attribute_group *bcl_device_attr_groups[] = {
	&bcl_device_attr_group,
	NULL,
};

int xr_thermal_sysfs_setup(struct xr_thermal_data *data)
{
	int ret;
	struct class *class;

	/* create thermal class */
	class = class_create("xr_thermal");
	if (IS_ERR(class)) {
		pr_err("sysfs class create error\n");
		return (int)PTR_ERR(class);
	}

	/* create lpc device*/
	ret = dev_set_name(&data->lpc_dev, "lpc");
	if (ret)
		goto class_destroy;

	data->lpc_dev.class = class;
	data->lpc_dev.groups = lpc_device_attr_groups;

	ret = device_register(&data->lpc_dev);
	if (ret)
		goto release_device1;

	/* create bcl device*/
	mutex_init(&data->bcl_lock);
	ret = dev_set_name(&data->bcl_dev, "bcl");
	if (ret)
		goto release_device2;
	data->bcl_dev.class = class;
	data->bcl_dev.groups = bcl_device_attr_groups;

	ret = device_register(&data->bcl_dev);
	if (ret)
		goto release_device2;
	bcl_power_init();
	bcl_power_reallocate();
	data->class = class;
	return 0;

release_device2:
	put_device(&data->bcl_dev);
release_device1:
	put_device(&data->lpc_dev);
class_destroy:
	class_destroy(class);
	return ret;
}

int xr_thermal_sysfs_teardown(struct xr_thermal_data *data)
{
	device_unregister(&data->lpc_dev);
	class_destroy(data->class);
	return 0;
}

u32 xr_thermal_bcl_soc_power_get(void)
{
	return bcl_soc_power;
}
EXPORT_SYMBOL(xr_thermal_bcl_soc_power_get);
