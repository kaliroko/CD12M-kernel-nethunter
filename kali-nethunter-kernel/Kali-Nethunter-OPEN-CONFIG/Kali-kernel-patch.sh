#! /bin/bash

#export KV=4.19
#export KD=vendor/xxxxx_defconfig

echo "######kali nethunter的一键补丁脚本，使用方法确保此脚本和kali-nethunter-kernel文件夹在内核源码目录下，修改export KD=vendor/cmi_user_defconfig或当前终端直接 make kernelversion查看内核版本，并设置 export KV值或直接修改。export KD=xxxxx_defconfig或export KD=vendor/xxxxx_defconfig，xxxxx_defconfig为你机型的内核配置并运行此脚本，然后按一般方式构建内核######"
echo "           "

#手机的内核配置文件(KERNEL_DEFCONFIG简称KD，一般在内核源码目录下的arch/arm64/configs或arch/arm64/configs/vendor下，一般为机型代号，高通骁龙处理器代号啥的，比如mi5 的为gemini_defconfig,一加8系列为kona_pref_defconfig,按实际情况修改
#export KD=vendor/cmi_user_defconfig

#KD=vendor/xxxx_defconfig或KD=xxxx_defconfig                                        
echo " -----------------------------------------------------------------------------"
echo "----------------   你设置的内核配置文件为:$KD 设置的内核版本为：$KV ----------------"
echo " -----------------------------------------------------------------------------"

#Kernel version简称KV,即内核的版本号，可在内核源码根目录执行 make kernelversion进行查看
#目前kali官方的nethunter内核补丁支持的内核版本有 3.04  3.04_hammerhead 3.10 3.10_angler 3.10_bullhead 3.18 4.04 4.09 4.14 4.19
export GKI_ROOT=$(pwd)

#加入内核识别，可在刷入好查看内核版本时显示-Kali-Nethunter
sed -i '/CONFIG_LOCALVERSION/d' "$GKI_ROOT/arch/arm64/configs/${KD}"
echo 'CONFIG_LOCALVERSION="-Kali-Nethunter"' >> "$GKI_ROOT/arch/arm64/configs/${KD}"




#export KV=4.19
#
cp -rf /root/Toolchain/kali-nethunter-kernel $GKI_ROOT
cp /root/Toolchain/kali-nethunter-kernel/Kali-Nethunter-OPEN-CONFIG/*.sh $GKI_ROOT
chmod +x *.sh

#4.19
if [ $KV = "4.19" ]
then
    patch -p1 < kali-nethunter-kernel/patches/4.19/add-wifi-injection-4.14.patch
    patch -p1 < kali-nethunter-kernel/patches/4.19/fix-ath9k-naming-conflict.patch
    patch -p1 < kali-nethunter-kernel/patches/4.19/add-rtl88xxau-5.6.4.2-drivers.patch
fi

#4.14
if [ $KV = "4.14" ]
then 
    patch -p1 < kali-nethunter-kernel/patches/4.14/add-wifi-injection-4.14.patch
    patch -p1 < kali-nethunter-kernel/patches/4.14/fix-ath9k-naming-conflict.patch
    patch -p1 < kali-nethunter-kernel/patches/4.14/fix-thread_info.h-compile-time-errors.patch
    patch -p1 < kali-nethunter-kernel/patches/4.14/add-rtl8188eus-to-rtl8xxxu-drivers-4.14.patch
    #patch -p1 < kali-nethunter-kernel/patches/4.14/add-rtl8188eus-to-rtl8xxxu-drivers.patch
    #patch -p1 < kali-nethunter-kernel/patches/4.14/add-rtl88xxau-5.6.4.2-drivers.patch
fi


#4.09
if [ $KV = "4.09" ]
then 
    patch -p1 < kali-nethunter-kernel/patches/4.09/add-wifi-injection-4.14.patch
    patch -p1 < kali-nethunter-kernel/patches/4.09/fix-ath9k-naming-conflict.patch
    #patch -p1 < kali-nethunter-kernel/patches/4.09/add-rtl88xxau-5.6.4.2-drivers.patch
fi


#4.04和3.04  3.04_hammerhead 3.10 3.10_angler 3.10_bullhead 3.18 4.04等补丁过于多，自行研究，而且在手机上很难构建成功。。。。
#4.04内核只选取了部分补丁，并不是是全部。。。
if [ $KV = "4.04" ]
then
    patch -p1 < kali-nethunter-kernel/patches/4.04/add-wifi-injection-4.04.patch
    patch -p1 < kali-nethunter-kernel/patches/4.04/fix-ath9k-naming-conflict.patch
    patch -p1 < kali-nethunter-kernel/patches/4.04/fix-rt2800-injection-4.04.patch
    patch -p1 < kali-nethunter-kernel/patches/4.04/v2-2-6-mac80211-refactor-monitor-representation-in-sdata.patch
fi

#此种方式直接修改内核配置文件$KD,但由于很多依赖问题有很大可能性会构建失败，建议参考（/root/Toolchain/kali-nethunter-kernel目录下）Kali Linux NetHunter内核编译指南 - 纯真's Blog (2024_1_20 00_32_56).html【 关于kali-nethunter-kernel可参考此文，非常的详细https://droidkali.github.io/2021/09/12/build-nethunter-kernel.html/】＜本/root/Toolchain/kali-nethunter-kernel 下有离线网页，es文件管理器选择打开方式为html查看器或kiwi浏览器打开即可查看＞

#在内核源码执行make kernleversion自行判断内核版本后，设置#export KV=x.xx，#export KD=vendor/xxxxx_defconfig后，源码目录执行此脚本后即打入补丁后，直接按一般方式构建内核，或注释掉直接开kai内核配置的脚本，在打入补丁后图形化配置内核并保存内核配置文件并编译kali内核。

#若图形化配置内核则注释掉下↓面的内容。每个开启内核配置的脚本都有相关说明，按需取消相应注释以启用。
#备份一下内核配置文件
cp ./arch/arm64/configs/$KD ./arch/arm64/configs/kernel-`date '+%Y%m%d%H%M%S'`_defconfig

#开启最基础的nethunter内核配置，即开启的nethunter内核配置最少，基本的hid,wifi，bt相关的可用，可最大限度保证可构建内核成功
sh $GKI_ROOT/Kali-Nethunter-base-OPEN-CONFIG.sh $GKI_ROOT/arch/arm64/configs/${KD} -w

#开启kali官方内核适配指南中的nethunter内核配置
#sh $GKI_ROOT/Kali-Nethunter-kali-offical-OPEN-CONFIG.sh $GKI_ROOT/arch/arm64/configs/${KD} -w

#开启所有kali相关的内核配置，可参考Kali Linux NetHunter内核编译指南 - 纯真's Blog中的所有内核配置都开启，在arm64上有很大可能性会导致内核编译失败。
#sh $GKI_ROOT/Kali-Nethunter-full-OPEN-CONFIG.sh $GKI_ROOT/arch/arm64/configs/${KD} -w

#github上某位作者的一键开启nethunter内核配置的脚本
#sh $GKI_ROOT/Kali-Nethunter-OPEN-CONFIG.sh $GKI_ROOT/arch/arm64/configs/${KD} -w


#最保险的方法就是手动选择相应的kali内核补丁,然后图形化配置，最后编译内核。
echo "         "
echo "运用nethunter补丁和开启kali内核所需的相关配置已完成，请按一般方式构建内核"
echo "         "
