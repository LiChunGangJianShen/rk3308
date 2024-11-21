# build all
source envsetup.sh rockchip_rk3308_bs_a610ks
# ./build.sh uboot
./build.sh kernel && ./build.sh recovery && ./build.sh rootfs && ./mkfirmware.sh && ./build.sh updateimg