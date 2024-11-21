## set link
ks_speakerphone_dir="$3"
if [ ! "$ks_speakerphone_dir"x = x ]
then
    ks_speakerphone_dir=$(cd $3; pwd)
    rm -rf ks
    rm -rf buildroot/package/ks
    rm -rf device/rockchip/rk3308/oem
	ln -s ${ks_speakerphone_dir}/ks/ ks
    ln -s ${ks_speakerphone_dir}/ks/ buildroot/package/ks
    ln -s ${ks_speakerphone_dir}/device/rockchip/rk3308/oem/ device/rockchip/rk3308/oem
else
    ks_speakerphone_dir=$(cd ../ks_speakerphone; pwd)
fi
echo "ks_speakerphone: $ks_speakerphone_dir"

## download dl
if [ ! -d ./buildroot/dl/ ]
then
    echo "download dl"
    curl -sS "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/get/kuaishou/stannis/a610ks_dl/v1/dl.tar.gz" > ./buildroot/dl.tar.gz
    cd buildroot && tar xf dl.tar.gz
    cd ..
fi
## download prebuilts
if [ ! -d ./prebuilts/ ]
then
    echo "download prebuilts"
    curl -sS "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/get/kuaishou/stannis/a610ks_rk3308bs_prebuilts/v1/prebuilts.tar.gz" > ./prebuilts.tar.gz
    tar xf prebuilts.tar.gz
fi

if [ "$1"x = releasex ]
then
    patch -p1 < ./ks/patch/disable-adb-debug.patch
fi
if [ "$2"x = otax ]
then
    rm -rf device/rockchip/rk3308/oem/factory_mode
fi

# build all
source envsetup.sh rockchip_rk3308_bs_a610ks
./build.sh uboot && ./build.sh kernel && ./build.sh recovery && ./build.sh rootfs && ./mkfirmware.sh && ./build.sh updateimg
