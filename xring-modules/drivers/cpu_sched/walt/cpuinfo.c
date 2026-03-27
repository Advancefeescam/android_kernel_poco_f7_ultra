// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023-2023 XRing Technologies Co., Ltd.
 */

#include <linux/cred.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <trace/hooks/cpuinfo.h>
#include "trace.h"

static const char * const features[] = {" evtstrm", " dgh bti ecv afp wfxt"};
static size_t feature_lengths[ARRAY_SIZE(features)];
static size_t num_features = ARRAY_SIZE(features);

#define MAX_UIDS 20
#define UID_LOW(n)	((n) % 100000)

static unsigned int stored_uids[MAX_UIDS];
static unsigned int store_idx;
static unsigned int store_num;
static bool uid_enable;
static struct mutex cpuinfo_add_lock;

static void cpuinfo_add_uid(unsigned int uid)
{
	unsigned int uid_low = UID_LOW(uid);

	if (uid == 0) {
		uid_enable = true;
		return;
	}

	for (size_t i = 0; i < store_num; ++i) {
		if (stored_uids[i] == uid_low)
			return;
	}

	stored_uids[store_idx] = uid_low;
	store_idx = (store_idx + 1) % MAX_UIDS;
	if (store_num < MAX_UIDS)
		store_num++;
}

static bool cpuinfo_match_uid(unsigned int uid)
{
	unsigned int uid_low = UID_LOW(uid);

	if (!uid_enable)
		return true;

	for (size_t i = 0; i < store_num; ++i) {
		if (stored_uids[i] == uid_low)
			return true;
	}

	return false;
}

static size_t cpuinfo_remove_features(char *str)
{
	char *end = str + strlen(str);
	char *current_pos = str;

	while (current_pos < end) {
		for (size_t i = 0; i < num_features; ++i) {
			if (strncmp(current_pos, features[i], feature_lengths[i]) == 0) {
				char *feature_end = current_pos + feature_lengths[i];

				memmove(current_pos, feature_end, end - feature_end);
				end -= feature_lengths[i];
				current_pos--;
				break;
			}
		}

		current_pos++;
	}

	*end = '\0';
	return end - str;
}

static void cpuinfo_show_hook(void *unused, struct seq_file *m)
{
	unsigned int uid;

	if (m->count <= 4096)
		return;

	uid = from_kuid_munged(current_user_ns(), current_uid());
	if (!cpuinfo_match_uid(uid))
		return;

	m->count = cpuinfo_remove_features(m->buf);
	trace_xr_trim_cpuinfo(uid);
}

static ssize_t cpuinfo_read(struct file *file, char __user *buf, size_t sz, loff_t *ppos)
{
	unsigned char tmp_buf[PAGE_SIZE];
	size_t count = 0;

	if (*ppos > 0)
		return 0;

	for (size_t i = 0; i < store_num; ++i)
		count += scnprintf(tmp_buf + count, sz - count, "%u\n", stored_uids[i]);

	if (copy_to_user(buf, tmp_buf, count))
		return -EFAULT;

	*ppos += count;
	return count;
}

static ssize_t cpuinfo_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned int uid;

	if (kstrtouint_from_user(buf, count, 10, &uid))
		return -EINVAL;

	mutex_lock(&cpuinfo_add_lock);
	cpuinfo_add_uid(uid);
	mutex_unlock(&cpuinfo_add_lock);

	return count;
}

static const struct file_operations cpuinfo_fops = {
	.owner = THIS_MODULE,
	.read = cpuinfo_read,
	.write = cpuinfo_write,
};

static struct miscdevice cpuinfo_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "xr_compitable_enhance",
	.fops = &cpuinfo_fops,
};

void cpuinfo_trim_init(void)
{
	for (size_t i = 0; i < num_features; ++i)
		feature_lengths[i] = strlen(features[i]);

	register_trace_android_rvh_cpuinfo_c_show(cpuinfo_show_hook, NULL);
	(void)misc_register(&cpuinfo_miscdev);
}
