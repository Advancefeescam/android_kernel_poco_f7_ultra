#!/bin/bash

export LC_ALL=C
export ARCH=arm64

 if ! [ -d "${PWD}/clang" ]; then
echo "Downloading Neutron Clang"
mkdir -p clang
cd clang
bash <(curl -s "https://raw.githubusercontent.com/Neutron-Toolchains/antman/main/antman") -S 09092023
fi

make O=out ARCH=arm64 ares_user_defconfig

PATH="${PWD}/clang/bin:${PATH}"

make -j$(nproc --all) O=out \
                      ARCH=arm64 \
                      CC="clang" \
                      CLANG_TRIPLE=aarch64-linux-gnu- \
                      CROSS_COMPILE="${PWD}/clang/bin/aarch64-linux-gnu-" \
                      CROSS_COMPILE_ARM32="${PWD}/clang/bin/arm-linux-gnueabi-" \
		      LD=ld.lld \
                      STRIP=llvm-strip \
                      AS=llvm-as \
		      AR=llvm-ar \
		      NM=llvm-nm \
		      OBJCOPY=llvm-objcopy \
   		      OBJDUMP=llvm-objdump \
                      CONFIG_NO_ERROR_ON_MISMATCH=y 2>&1 | tee error.log 
