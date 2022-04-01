export PATH=/media/tina_d1_open/prebuilt/gcc/linux-x86/riscv/toolchain-thead-glibc/riscv64-glibc-gcc-thead_20200702/bin/:$PATH

make CROSS_COMPILE=riscv64-unknown-linux-gnu- ARCH=riscv
make CROSS_COMPILE=riscv64-unknown-linux-gnu- ARCH=riscv INSTALL_MOD_PATH=test_guu/rootfs/ modules_install
make CROSS_COMPILE=riscv64-unknown-linux-gnu- ARCH=riscv INSTALL_PATH=test_guu/boot/ zinstall
cp arch/riscv/boot/dts/sunxi/board.dtb test_guu/boot/
