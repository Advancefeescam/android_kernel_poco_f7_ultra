ccflags-y += -I$(srctree)/drivers/
ccflags-y += -I$(srctree)/drivers/power/supply/lx_chg/include
ccflags-y += -I$(srctree)/include/miev

obj-$(CONFIG_LIXUN_CHARGER_MANAGER)        += main_chg.o
main_chg-objs:= lx_charger_manager.o lx_jeita.o lx_charge_sysfs.o lx_notify.o lx_voter.o xm_chg_dfs.o lx_pd_adapter.o xm_pd_adapter.o lx_cp_policy.o lx_qc_policy.o xm_smart_chg.o
