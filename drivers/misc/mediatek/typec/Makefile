# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/inc


ifneq ($(strip $(BUILD_TARGET_PRODUCT)), )
obj-$(CONFIG_TCPC_CLASS) += $(BUILD_TARGET_PRODUCT)/
else
obj-$(CONFIG_TCPC_CLASS) += default/
endif
