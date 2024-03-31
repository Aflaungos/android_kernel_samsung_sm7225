#!/bin/bash

export ARCH=arm64
export PROJECT_NAME=m23xq
CLANG="${HOME}/linux-x86-main/clang-r487747c/bin"
export CLANG_TRIPLE=aarch64-linux-gnu-
export PATH="$CLANG:$PATH"

make -j8 O=out ARCH=arm64 SUBARCH=arm64 CC=clang LLVM_IAS=1 LLVM=1 vendor/m23xq_eur_open_defconfig
make -j8 O=out ARCH=arm64 SUBARCH=arm64 CC=clang LLVM_IAS=1 LLVM=1

$(pwd)/tools/mkdtimg create $(pwd)/out/arch/arm64/boot/dtbo.img --page_size=4096 $(find out/arch/arm64/boot/dts/samsung/m23/m23xq/ -name *.dtbo)