subdir-ccflags-y += -I$(srctree)/arch/arm64/boot/dts/mediatek/lx_chg
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lxchg_v2/battery
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lxchg_v2/manager
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lxchg_v2/battery/secret
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/lxchg_v2/battery/mt6358_fg

subdir-ccflags-y += -I$(srctree)/include/miev

obj-$(CONFIG_LIXUN_CHARGER_MANAGER)		+= lx_chg.o

# --------------------------------------  lx_manager start -----------------------------------------#
lx_chg-objs += \
	manager/lxchg_manager.o \
	manager/lxchg_jeita.o \
	manager/lxchg_sysfs.o \
	manager/lxchg_notify.o \
	manager/lxchg_voter.o \
	manager/xmchg_dfs.o \
	manager/lx_pd_adapter.o \
	manager/xm_pd_adapter.o \
	manager/lx_cp_policy.o \
	manager/lx_qc_policy.o \
	manager/xm_smart_chg.o \
	manager/lxchg_class_base.o \
	manager/lxchg_class.o \
	manager/lx_adapter_class.o \
	manager/xm_adapter_class.o \
	manager/lxbat_auth_class.o
# --------------------------------------  lx_manager start -----------------------------------------#

# --------------------------------------  lx_charger start -----------------------------------------#
ifeq ($(CONFIG_SC8541_CHARGEPUMP),m)
lx_chg-objs += charger/sc8541_chargepump/sc8541_chargepump.o
endif

ifeq ($(CONFIG_BQ25960_CHARGEPUMP),m)
lx_chg-objs += charger/bq25960_chargepump/bq25960_chargepump.o
endif

ifeq ($(CONFIG_NU6601_CHARGER),m)
lx_chg-objs += \
	charger/nu6601_charger/nu6601_charger.o \
	charger/nu6601_charger/nu6601_base.o \
	charger/nu6601_charger/nu6601_qc35_inf.o
endif

ifeq ($(CONFIG_SC6601_CHARGER),m)
lx_chg-objs += \
	charger/sc6601_charger/sc6601_charger.o \
	charger/sc6601_charger/sc6601_base.o \
	charger/sc6601_charger/sc6601_cid.o \
	charger/sc6601_charger/soft_bc12_inf.o
endif
# --------------------------------------  lx_charger end -----------------------------------------#



# --------------------------------------  lx_battery start -----------------------------------------#
ifeq ($(CONFIG_LIXUN_FUEL_ALGORITHM),m)
lx_chg-objs += battery/lx_fg_algorithm_inf.o
endif

ifeq ($(CONFIG_BQ28Z610_FG_SUPPORT),m)
lx_chg-objs += battery/bq28z610_fg/bq28z610_fg.o
endif

ifeq ($(CONFIG_MT6358_FG_SUPPORT),m)
lx_chg-objs += \
	battery/mt6358_fg/mtk_battery_coulomb_service.o \
	battery/mt6358_fg/mtk_battery.o \
	battery/mt6358_fg/mtk_battery_algo.o \
	battery/mt6358_fg/mt6358-gauge.o \
	battery/mt6358_fg/mtk_battery_daemon.o
endif


ifeq ($(CONFIG_BATT_SECRET_VERIFY),m)

ifeq ($(CONFIG_BATT_VERIFY_BY_DS28E30),m)
lx_chg-objs += \
	battery/secret/ds28e30.o	\
	battery/secret/ds28e30lib/bignum.o	\
	battery/secret/ds28e30lib/deep_cover_coproc.o	\
	battery/secret/ds28e30lib/ecc_generate_key.o	\
	battery/secret/ds28e30lib/ecdsa_generic_api.o	\
	battery/secret/ds28e30lib/ecdsa_high.o		\
	battery/secret/ds28e30lib/sha256_hmac.o	\
	battery/secret/ds28e30lib/sha256_software.o	\
	battery/secret/ds28e30lib/sha256_stone.o	\
	battery/secret/ds28e30lib/ucl_rng.o	\
	battery/secret/ds28e30lib/ucl_sha256.o	\
	battery/secret/ds28e30lib/ucl_sha3.o	\
	battery/secret/ds28e30lib/ucl_sia256.o	\
	battery/secret/ds28e30lib/ucl_sys.o
endif

ifeq ($(CONFIG_BATT_VERIFY_BY_SLG),m)
lx_chg-objs += \
    battery/secret/slg.o   \
    battery/secret/slglib/uECC.o
endif

ifeq ($(CONFIG_BATT_VERIFY_BY_ST),m)
lx_chg-objs += \
    battery/secret/stick.o \
    battery/secret/stickapi/stickapitest.o \
    battery/secret/stickapi/stick_api.o \
    battery/secret/stickapi/libsticknvm.o \
    battery/secret/stickapi/st1wire_kernel.o \
    battery/secret/stickapi/stick_core.o \
    battery/secret/stickapi/stick_kernel_platform.o \
    battery/secret/stickapi/stick_NVM_attributes.o \
    battery/secret/stickapi/libstickcrypto/libstickcrypto.o \
    battery/secret/stickapi/libstickcrypto/libstickcrypto_rng.o \
    battery/secret/stickapi/libstickcrypto/aes.o  \
    battery/secret/stickapi/libstickcrypto/aes_tab.o  \
    battery/secret/stickapi/libstickcrypto/ccm_add_aad.o  \
    battery/secret/stickapi/libstickcrypto/ccm_add_nonce.o  \
    battery/secret/stickapi/libstickcrypto/ccm_done.o  \
    battery/secret/stickapi/libstickcrypto/ccm_init.o  \
    battery/secret/stickapi/libstickcrypto/ccm_memory.o  \
    battery/secret/stickapi/libstickcrypto/ccm_process.o  \
    battery/secret/stickapi/libstickcrypto/ccm_reset.o  \
    battery/secret/stickapi/libstickcrypto/hash_memory.o  \
    battery/secret/stickapi/libstickcrypto/sha512.o  \
    battery/secret/stickapi/libstickcrypto/omac_done.o  \
    battery/secret/stickapi/libstickcrypto/omac_init.o  \
    battery/secret/stickapi/libstickcrypto/omac_memory.o  \
    battery/secret/stickapi/libstickcrypto/omac_memory_multi.o  \
    battery/secret/stickapi/libstickcrypto/omac_process.o  \
    battery/secret/stickapi/libstickcrypto/copy_or_zeromem.o  \
    battery/secret/stickapi/libstickcrypto/crypt_argchk.o  \
    battery/secret/stickapi/libstickcrypto/crypt_cipher_descriptor.o  \
    battery/secret/stickapi/libstickcrypto/crypt_cipher_is_valid.o  \
    battery/secret/stickapi/libstickcrypto/crypt_find_cipher.o  \
    battery/secret/stickapi/libstickcrypto/crypt_find_hash.o  \
    battery/secret/stickapi/libstickcrypto/crypt_hash_descriptor.o  \
    battery/secret/stickapi/libstickcrypto/crypt_hash_is_valid.o  \
    battery/secret/stickapi/libstickcrypto/crypt_prng_descriptor.o  \
    battery/secret/stickapi/libstickcrypto/crypt_prng_is_valid.o  \
    battery/secret/stickapi/libstickcrypto/crypt_register_cipher.o  \
    battery/secret/stickapi/libstickcrypto/crypt_register_hash.o  \
    battery/secret/stickapi/libstickcrypto/error_to_string.o  \
    battery/secret/stickapi/libstickcrypto/mem_neq.o  \
    battery/secret/stickapi/libstickcrypto/zeromem.o  \
    battery/secret/stickapi/libstickcrypto/ec25519_export.o  \
    battery/secret/stickapi/libstickcrypto/tweetnacl.o  \
    battery/secret/stickapi/libstickcrypto/ed25519_export.o  \
    battery/secret/stickapi/libstickcrypto/ed25519_import_raw.o  \
    battery/secret/stickapi/libstickcrypto/ed25519_make_key.o  \
    battery/secret/stickapi/libstickcrypto/ed25519_sign.o  \
    battery/secret/stickapi/libstickcrypto/ed25519_verify.o  \
    battery/secret/stickapi/libstickcrypto/x25519_export.o  \
    battery/secret/stickapi/libstickcrypto/x25519_import_raw.o  \
    battery/secret/stickapi/libstickcrypto/x25519_make_key.o  \
    battery/secret/stickapi/libstickcrypto/x25519_shared_secret.o  \
    battery/secret/stickapi/libstickcrypto/stick_kzalloc.o
endif

ccflags-y := \
    -DLTC_NOTHING \
    -DLTC_NO_FILE \
    -DLTC_RIJNDAEL \
    -DLTC_SHA512 \
    -DLTC_CCM_MODE \
    -DLTC_NO_TEST \
    -DLTC_CURVE25519 \
    -DLTC_DER \
    -DLTC_MPI \
    -DLTC_OMAC \
    -DLTC_HASH_HELPERS \
    -DLTC_STICK \
    -DLTC_SOURCE \
    -DLTC_NO_WCHAR \
    -DRELEASE_BUILDING=1 \
    -fvisibility=hidden \
    -DARGTYPE=1
ifeq ($(MY_CUSTID),)
ccflags-y += -DRELEASE_CUSTID=00
else
ccflags-y += -DRELEASE_CUSTID=$(MY_CUSTID)
endif
ifeq ($(MY_RELNAME),)
ccflags-y += -DRELEASE_NAME=dev
else
ccflags-y += -DRELEASE_NAME=$(MY_RELNAME)
endif
ifeq ($(MY_RELPROD),)
ccflags-y += -DRELEASE_TYPE=DEV -DSTICK_DEBUG_LOG=1
else
ccflags-y += -DRELEASE_TYPE=$(MY_RELPROD)
endif
# List of files to build battery/secret/stickapi
endif

# --------------------------------------  lx_battery end -----------------------------------------#


