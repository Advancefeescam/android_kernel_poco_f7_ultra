# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/common/include
ifeq ($(CONFIG_HQ_SYSFS_SUPPORT), m)
subdir-ccflags-y += -I$(srctree)/drivers/misc/hqsysfs
endif

ifeq ($(KERNEL_FACTORY_BUILD),1)
subdir-ccflags-y += -DKERNEL_FACTORY_HQ_CHG
endif

obj-$(CONFIG_HUAQIN_CHARGER_COMMON)     += common/
obj-$(CONFIG_HUAQIN_CHARGER_MANAGER)    += charger_policy/
obj-$(CONFIG_HQ_CHARGER_CLASS)          += charger_class/
obj-$(CONFIG_CHARGE_ARCH_SUBPMIC)       += subpmic/
obj-$(CONFIG_CHARGE_ARCH_CHARGEPUMP)    += chargepump/
obj-$(CONFIG_CHARGE_ARCH_FUELGAUGE)     += fuelgauge/
obj-$(CONFIG_CHARGE_ARCH_BATT_AUTH)     += battery_secrete/
