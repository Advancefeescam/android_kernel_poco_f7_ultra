#
# Makefile for all kinds of Fingerprint
#

ifeq ($(strip $(BUILD_TARGET_PRODUCT)),rock)
subdir-ccflags-y += -D SUPPORT_SPEED_SCREEN_UNBLANK
endif

obj-$(CONFIG_GOODIX_FINGERPRINT) += goodix/
obj-$(CONFIG_FPC_FINGERPRINT) += fpc/
