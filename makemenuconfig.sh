#!/bin/bash

export PATH=/data/tina_d1_h/prebuilt/gcc/linux-x86/riscv/toolchain-thead-glibc/riscv64-glibc-gcc-thead_20200702/bin/:$PATH

make CROSS_COMPILE=riscv64-unknown-linux-gnu- ARCH=riscv menuconfig

