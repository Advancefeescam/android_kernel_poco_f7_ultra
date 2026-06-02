#
# Makefile for the kernel et7480 type-c switch device drivers.
#
obj-$(CONFIG_USB_SWITCH_ET7480_MT6366) += et7480.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek
ccflags-y += -I$(srctree)/drivers
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lxchg_v2/manager
