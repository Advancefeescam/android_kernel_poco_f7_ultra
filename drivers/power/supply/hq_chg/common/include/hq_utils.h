/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __HQ_UTILS_H__
#define __HQ_UTILS_H__

#include <linux/minmax.h> /* min & max ... functions */

#ifndef is_between
#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))
#endif

#endif /* __HQ_UTILS_H__ */