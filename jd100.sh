make -C jd_app/jd100 clean
make -C jd_app/jd100
#./build.sh uboot
#./build.sh kernel
#./build.sh recovery
./build.sh rootfs
./mkfirmware.sh
./build.sh updateimg

