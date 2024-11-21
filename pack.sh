./build.sh uboot
./build.sh kernel
./build.sh recovery
./build.sh rootfs
./mkfirmware.sh
./build.sh updateimg

