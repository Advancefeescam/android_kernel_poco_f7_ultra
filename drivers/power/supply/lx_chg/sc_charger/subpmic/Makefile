subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/lx_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg/sc_charger
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg/sc_charger/protocol
subdir-ccflags-y += -DCONFIG_ENABLE_SYSFS_DEBUG

obj-$(CONFIG_SUBPMIC_DRIVER)    += subpmic_charger.o subpmic.o subpmic_cid.o subpmic_chargerpump.o
