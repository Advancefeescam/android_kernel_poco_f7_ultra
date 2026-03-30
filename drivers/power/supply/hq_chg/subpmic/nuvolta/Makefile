# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/hq_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_class/include
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/subpmic/nuvolta/include
subdir-ccflags-y += -DCONFIG_ENABLE_BOOT_DEBUG -DCONFIG_ENABLE_SYSFS_DEBUG

# subpmic_nu.ko
obj-$(CONFIG_SUBPMIC_NUVOLTA)               += subpmic_nu.o
subpmic_nu-$(CONFIG_SUBPMIC_NUVOLTA)        += subpmic_nu6601.o nu6601_qc35.o

# nu6601.ko
obj-$(CONFIG_NU_SUBPMIC_NU6601)             += nu6601.o
