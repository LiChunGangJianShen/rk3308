cd device/rockchip/
if [ -f .BoardConfig.mk ]; then
	unlink .BoardConfig.mk
	ln -sf rk3308/BoardConfig-JD-H20.mk .BoardConfig.mk
fi
cd -

make -C jd_app/h20 clean
make -C jd_app/h20
./build.sh uboot
./build.sh kernel
./build.sh recovery
./build.sh rootfs
./mkfirmware.sh
./build.sh updateimg

