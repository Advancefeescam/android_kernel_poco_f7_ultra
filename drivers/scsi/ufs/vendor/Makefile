# SPDX-License-Identifier: GPL-2.0
# Mediatek UFS extended features makefile

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
#L19A code for HQ-194052 by xukai at 20220517 start
ccflags-y += -I$(srctree)/drivers/scsi/ufs/vendor/
ccflags-y += -I$(srctree)/drivers/scsi/

obj-$(CONFIG_SCSI_UFS_MEDIATEK) += ufs-mediatek-mod.o
ufs-mediatek-mod-y := xiaomi/ufs-mediatek.o
ufs-mediatek-mod-y += xiaomi/ufshcd-pltfrm.o
ufs-mediatek-mod-y += xiaomi/ufshcd.o
ufs-mediatek-mod-y += xiaomi/ufshcd-crypto.o
ufs-mediatek-mod-y += xiaomi/ufs_bsg.o
ufs-mediatek-mod-y += xiaomi/ufs-debugfs.o
ufs-mediatek-mod-y += xiaomi/ufs-sysfs.o
ufs-mediatek-mod-y += xiaomi/ufs-mediatek-dbg.o
#ufs-mediatek-mod-y += xiaomi/ufshpb.o
ufs-mediatek-mod-$(CONFIG_UFSFEATURE) += feature/ufsfeature.o feature/ufsfeature-mimic.o
ufs-mediatek-mod-$(CONFIG_UFSSHPB) += feature/hpb/ufsshpb.o
ufs-mediatek-mod-$(CONFIG_UFSTW) += feature/tw/ufstw.o
ufs-mediatek-mod-$(CONFIG_UFSHID) += feature/hid/ufshid.o
ufs-mediatek-mod-$(CONFIG_UFSRINGBUF) += feature/ringbuf/ufsringbuf.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_FFU) += feature/ffu/ufsffu.o
#L19A code for HQ-194052 by xukai at 20220517 end
#L19A-T code for HQ-282138 by xukai at 20230213 start
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_FFU) += feature/ffu/ufsffu.o
#L19A-T code for HQ-282138 by xukai at 20230213 end