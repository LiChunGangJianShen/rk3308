# 编译步骤：
## env
source envsetup.sh 选rockchip_rk3308_bs_a610ks

## 编译uboot
./build.sh uboot

## 编译kernel
./build.sh kernel

## 编译recovery
./build.sh recovery

## buildroot
./build.sh rootfs 或 ./build.sh buildroot

## build firmware
1. ./mkfirmware.sh
2. ./build.sh updateimg  会在根目录下rockdev中生成update.img，这个就可以用rk的升级工具升级了
3. `adb push ./update.img /userdata/`下载固件
4. `adb shell updateEngine --update --image_url=/userdata/update.img --reboot`完成升级

## 上传dl
curl -H "Authentication:Token e52c2c860e76460482ba743abe315593" -sSX PUT -F "file=@dl.tar.gz" "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/put/kuaishou/stannis/a610ks_dl/v1?expireDays=0"
## list
curl -sS "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/listByModule?dirPath=kuaishou/stannis&moduleName=a610ks_dl"
## 下载dl
curl -sS "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/get/kuaishou/stannis/a610ks_dl/v1/dl.tar.gz" > ./dl.tar.gz

## 上传dl
curl -H "Authentication:Token e52c2c860e76460482ba743abe315593" -sSX PUT -F "file=@prebuilts.tar.gz" "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/put/kuaishou/stannis/a610ks_rk3308bs_prebuilts/v1?expireDays=0"
## list
curl -sS "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/listByModule?dirPath=kuaishou/stannis&moduleName=a610ks_rk3308bs_prebuilts"
## 下载dl
curl -sS "https://artifact.corp.kuaishou.com/api/repo/artifact/kuaishou/get/kuaishou/stannis/a610ks_rk3308bs_prebuilts/v1/prebuilts.tar.gz" > ./prebuilts.tar.gz

# change log
- 解决高温降频问题
## 2023-10-09
1. 在原本的线上模型基础上进行fine tune，aecmos分数的变化不大。
2. 在大混响条件下，远端单讲漏回声的问题。
## 2023-10-31
1. 在大混响条件下, 提升体验