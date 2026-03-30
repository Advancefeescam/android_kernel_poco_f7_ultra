# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/common/include
ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_policy/rainbow/include
ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/charger_class/include
ccflags-y += -I$(srctree)/drivers/power/supply/hq_chg/battery_secrete
ccflags-y += -I$(srctree)/drivers/thermal/mediatek
ccflags-y += -I$(srctree)/include/miev
ccflags-y += -I$(srctree)/drivers/misc/hq_typec/tcpc/inc

obj-$(CONFIG_HUAQIN_CHARGER_MANAGER)        += main_chg.o
main_chg-objs := hq_charger_manager.o \
		 hq_charger_manager_misc.o \
		 hq_charge_sysfs.o \

main_chg-$(CONFIG_HQ_JEITA_POLICY)          += hq_jeita_policy.o
main_chg-$(CONFIG_HQ_THERMAL_POLICY)        += hq_thermal_policy.o
main_chg-$(CONFIG_HQ_TERM_RECHARGE_POLICY)  += hq_term_recharge_policy.o
main_chg-$(CONFIG_HQ_SHUTDOWN_POLICY)       += hq_shutdown_policy.o
main_chg-$(CONFIG_XM_BATTERY_HEALTH)        += xm_batt_health.o
main_chg-$(CONFIG_XM_SMART_CHG)             += xm_smart_chg.o
main_chg-$(CONFIG_HQ_REVERSE_CHARGE_POLICY) += hq_reverse_charge_policy.o

obj-$(CONFIG_HUAQIN_CP_POLICY)              += hq_cp_policy.o
obj-$(CONFIG_HUAQIN_CP_POLICY)              += hq_pd_adapter.o
obj-$(CONFIG_PD_BATTERY_SECRET)             += xm_pd_adapter.o
obj-$(CONFIG_HUAQIN_QC_POLICY)              += hq_qc_policy.o
obj-$(CONFIG_XM_CHG_UEVENT)                 += xm_chg_uevent.o
obj-$(CONFIG_XM_CHG_DFS)                    += xm_chg_dfs.o