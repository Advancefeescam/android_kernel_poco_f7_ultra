# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_POWER_RESET)	+= reset/


ifneq ($(strip $(BUILD_TARGET_PRODUCT)), )
obj-$(CONFIG_POWER_SUPPLY)	+= supply-$(BUILD_TARGET_PRODUCT)/
else
obj-$(CONFIG_POWER_SUPPLY)	+= supply-default/
endif
