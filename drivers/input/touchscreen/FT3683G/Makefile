# Makefile for the focaltech touchscreen drivers.
# N6 code for HQ-310178 by zhangyundan at 2023/08/010 start
ifeq ($(KERNEL_FACTORY_BUILD),1)
KBUILD_CFLAGS += -DKERNEL_FACTORY_BUILD=1
endif
# N6 code for HQ-310178 by zhangyundan at 2023/08/010 end
# P6 code for HQFEAT-109432 charge_mode&earphone_mode by qingweijie at 2025/6/23 start
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
# P6 code for HQFEAT-109432 charge_mode&earphone_mode by qingweijie at 2025/6/23 end
obj-$(CONFIG_TOUCHSCREEN_FTS) += ft3683g.o
ft3683g-y += focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_spi_v2.o \
		focaltech_flash.o \
		focaltech_upgrade_ft3683g.o \
		focaltech_test.o \
		focaltech_test_ini.o \
		focaltech_test_ft3683g.o