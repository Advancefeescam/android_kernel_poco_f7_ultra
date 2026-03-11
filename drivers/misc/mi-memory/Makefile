# SPDX-License-Identifier: GPL-2.0
#
# Makefile for mi memory info
#
#L19A code for HQ-194052 by xukai at 20220517 start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/vendor/xiaomi/

obj-m 	+= mi-memory.o

mi-memory-m += memory_debugfs.o
mi-memory-m += ufs_info.o
mi-memory-m += dram_info.o
#L19A code for HQ-194052 by xukai at 20220517 end