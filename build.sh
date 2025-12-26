#!/bin/bash


if [ -z "${BASH_SOURCE}" ];then
	echo Not in bash, switch to it...
	bash -c $0
fi

TOP_DIR=$(pwd)
BOARD_CONFIG_DIR=$TOP_DIR/device/rockchip
COMMON_DIR=$BOARD_CONFIG_DIR/common
h20_app=$TOP_DIR/jd_app/h20
h20s_app=$TOP_DIR/jd_app/h20s
m800_app=$TOP_DIR/jd_app/m800

DEFCONFIG_ARRAY=(
"h20			-build h20" \
"h20_hs			-build h20-hs" \
"h20s			-build h20s" \
"m800			-build m800" \
)


DEFCONFIG_ARRAY_LEN=${#DEFCONFIG_ARRAY[@]}
CHOICE=0

function choose_info()
{
	echo
	echo "You're building on custom config"
	echo "Lunch menu...pick a combo:"
	echo ""
	i=0
	while [[ $i -lt $DEFCONFIG_ARRAY_LEN ]]
	do
		echo "$((${i}+1)). ${DEFCONFIG_ARRAY[$i]}"
		let ++i
	done
	echo
}

function choose_type()
{
	choose_info
	local DEFAULT_NUM
	DEFAULT_NUM=1

	TARGET_BUILD_CONFIG=
	local ANSWER
	while [ -z "${TARGET_BUILD_CONFIG}" ]
	do
		echo -n "Which would you like? ["$DEFAULT_NUM"] "
		if [ -z "$1" ]; then
			read ANSWER
		else
			echo $1
			ANSWER=$1
		fi

		if [ -z "$ANSWER" ]; then
			ANSWER="$DEFAULT_NUM"
		fi

		if [ -n "`echo $ANSWER | sed -n '/^[0-9][0-9]*$/p'`" ]; then
			if [ $ANSWER -le $DEFCONFIG_ARRAY_LEN ] && [ $ANSWER -gt 0 ]; then
				index=$((${ANSWER}-1))
				TARGET_BUILD_CONFIG="${DEFCONFIG_ARRAY[$index]}"
				CHOICE=$index
			else
				echo
				echo "number not in range. Please try again."
				echo
			fi
		fi
		if [ -n "$1" ]; then
			break
		fi
	done
	
	BUILD_TARGET="$(echo ${DEFCONFIG_ARRAY[$CHOICE]}] | cut -d ' ' -f 1)"
	echo
	echo "You choice:  $BUILD_TARGET"
	echo 
}


CPU_ARRAY=(
"rk3308bs	--select rk3308bs"	\
"rk3308hs	--select rk3308hs"	\
)
CPU_ARRAY_LEN=${#CPU_ARRAY[@]}
CHOICE=0

function choose_cpu()
{
	echo
	echo "You're building on custom config"
	echo "Lunch menu...pick a combo:"
	echo ""
	i=0
	while [[ $i -lt $CPU_ARRAY_LEN ]]
	do
		echo "$((${i}+1)). ${CPU_ARRAY[$i]}"
		let ++i
	done
	echo
}

function choose_cpu_type()
{
	choose_cpu
	local DEFAULT_NUM
	DEFAULT_NUM=1

	CPU_CONFIG=
	local ANSWER
	while [ -z "${CPU_CONFIG}" ]
	do
		echo -n "Which would you like? ["$DEFAULT_NUM"] "
		if [ -z "$1" ]; then
			read ANSWER
		else
			echo $1
			ANSWER=$1
		fi

		if [ -z "$ANSWER" ]; then
			ANSWER="$DEFAULT_NUM"
		fi

		if [ -n "`echo $ANSWER | sed -n '/^[0-9][0-9]*$/p'`" ]; then
			if [ $ANSWER -le $CPU_ARRAY_LEN ] && [ $ANSWER -gt 0 ]; then
				index=$((${ANSWER}-1))
				CPU_CONFIG="${CPU_ARRAY[$index]}"
				CHOICE=$index
			else
				echo
				echo "number not in range. Please try again."
				echo
			fi
		fi
		if [ -n "$1" ]; then
			break
		fi
	done
	
	CPU_CONFIG="$(echo ${CPU_ARRAY[$CHOICE]}] | cut -d ' ' -f 1)"
	echo
	echo "You choice:  $CPU_CONFIG"
	echo 
}

function link_board_config()
{	
	original_dir=$(pwd)
	cd $BOARD_CONFIG_DIR || { echo "进入$BOARD_CONFIG_DIR 目录失败";exit 1; }
	if [ -f .BoardConfig.mk ]; then
		unlink .BoardConfig.mk
	fi
	if [ $BUILD_TARGET == h20 ];then
		ln -sf rk3308/BoardConfig-JD-H20.mk .BoardConfig.mk
	elif [ $BUILD_TARGET == h20_hs ];then
		ln -sf rk3308/BoardConfig-JD-H20-HS.mk .BoardConfig.mk
	elif [ $BUILD_TARGET == h20s ];then
		ln -sf rk3308/BoardConfig-JD-H20S.mk .BoardConfig.mk
	elif [ $BUILD_TARGET == m800 ];then
		ln -sf rk3308/BoardConfig-JD-M800.mk .BoardConfig.mk
	fi
	cd "$original_dir" || { echo "错误： 无法回到原目录";exit 1; }
}

function export_variable_value()
{
	link_board_config
	source $BOARD_CONFIG_DIR/.BoardConfig.mk
	export ROOTFS_DIR=$TOP_DIR/buildroot/output/$RK_CFG_BUILDROOT/target
	echo "----------------- $CPU_CONFIG"
	if [ $CPU_CONFIG == rk3308bs ]; then
		source envsetup.sh rockchip_rk3308_bs
	elif [ $CPU_CONFIG == rk3308hs ]; then
		source envsetup.sh rockchip_rk3308_hs
	else
		echo "Error CPU Type Select";exit 1;
	fi
}
#####################################
# app
build_h20()
{
	original_dir=$(pwd)
	app=$h20_app
	cd $app || { echo "进入$app 目录失败";exit 1; }
	echo $(pwd)
	if make clean && make; then
		app_compile_success=1
	else
		app_compile_success=0
	fi
	cd "$original_dir" || { echo "错误： 无法回到原目录";exit 1; }
}

build_h20s()
{
	original_dir=$(pwd)
	app=$h20s_app
	cd $app || { echo "进入$app 目录失败";exit 1; }
	echo $(pwd)
	if make clean && make; then
		app_compile_success=1
	else
		app_compile_success=0
	fi
	cd "$original_dir" || { echo "错误： 无法回到原目录";exit 1; }
}

build_m800()
{
	original_dir=$(pwd)
	app=$m800_app
	cd $app || { echo "进入$app 目录失败";exit 1; }
	echo $(pwd)
	if make clean && make; then
		app_compile_success=1
	else
		app_compile_success=0
	fi
	cd "$original_dir" || { echo "错误： 无法回到原目录";exit 1; }
}

#####################################
choose_cpu_type
choose_type

i=0
while [[ $i -lt 3 ]]
do
	echo "$((${i}+1))"
	let ++i
	sleep 1
done

#=========================
# build target
#=========================
export_variable_value
if [[ $BUILD_TARGET == h20 || $BUILD_TARGET == h20_hs ]];then
	build_h20
elif [ $BUILD_TARGET == h20s ];then
	build_h20s
elif [ $BUILD_TARGET == m800 ];then
	build_m800
fi

if [ $app_compile_success -ne 0 ]; then
	echo
	echo "===================================="
    echo "app编译成功，执行build.sh all"
	i=3
	while [[ $i -gt 0 ]]
	do
		echo "$i"
		let --i
		sleep 1
	done
	echo "===================================="
	echo
    $COMMON_DIR/build.sh all
	$COMMON_DIR/build.sh firmware
	$COMMON_DIR/build.sh updateimg
else
    echo "app编译失败，不执行build.sh all"
    exit 1
fi

exit 0