# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/hq_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_class/include
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/subpmic/silergy/include
subdir-ccflags-y += -DCONFIG_ENABLE_SYSFS_DEBUG

# subpmic_sy.ko
obj-$(CONFIG_SUBPMIC_SILERGY)                        += subpmic_sy.o
subpmic_sy-$(CONFIG_SUBPMIC_SILERGY)                 += subpmic_sy_charger.o
subpmic_sy-$(CONFIG_SY_SUBPMIC_BC12_PROTOCOL)        += soft_bc12.o

# sy6976.ko
obj-$(CONFIG_SY_SUBPMIC_SY6976)                      += sy6976.o
