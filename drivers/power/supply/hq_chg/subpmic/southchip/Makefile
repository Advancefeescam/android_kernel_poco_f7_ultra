# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/hq_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_class/include
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/subpmic/southchip/include
subdir-ccflags-y += -DCONFIG_ENABLE_SYSFS_DEBUG

# subpmic_sc.ko
obj-$(CONFIG_SUBPMIC_SOUTHCHIP)                      += subpmic_sc.o
subpmic_sc-$(CONFIG_SUBPMIC_SOUTHCHIP)               += subpmic_sc_charger.o
subpmic_sc-$(CONFIG_SC_SUBPMIC_BC12_PROTOCOL)        += soft_bc12.o

# subpmic_cid.ko
# TDDO: cid function should be merge to subpmic_sc.ko
obj-m += subpmic_cid.o

# sc6601.ko
obj-$(CONFIG_SC_SUBPMIC_SC6601)                      += sc6601.o
