cd device/rockchip/
if [ -f .BoardConfig.mk ]; then
	unlink .BoardConfig.mk
	ln -sf rk3308/BoardConfig-JD-H20-HS.mk .BoardConfig.mk
fi
cd -

make -C jd_app/h20_hs clean
make -C jd_app/h20_hs
./build.sh uboot
./build.sh kernel
./build.sh recovery
./build.sh rootfs
./mkfirmware.sh
./build.sh updateimg

