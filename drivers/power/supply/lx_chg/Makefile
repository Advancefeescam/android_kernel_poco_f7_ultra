# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg/charger_policy
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg/charger_class
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg/fuelgauge
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg/include
ifeq ($(CONFIG_LX_SYSFS_SUPPORT), m)
subdir-ccflags-y += -I$(srctree)/drivers/misc/lxsysfs
endif

obj-$(CONFIG_LIXUN_CHARGER_MANAGER)    += charger_policy/
obj-$(CONFIG_LIXUN_CHARGER_CLASS)      += charger_class/
obj-$(CONFIG_SC_CHARGER)                += sc_charger/
obj-$(CONFIG_NU_CHARGER)                += nu_charger/
obj-$(CONFIG_BQ_CHARGERPUMP_BQ25960)    += bq_charger/
obj-y                                   += fuelgauge/
obj-y                                   += battery_secrete/


ifeq ($(KERNEL_FACTORY_BUILD),1)
KBUILD_CFLAGS += -DKERNEL_FACTORY_BUILD=1
endif
