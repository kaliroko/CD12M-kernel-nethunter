export ARCH=arm64
KV1=$(make kernelversion)
KD1=$(ls arch/$ARCH/configs)
KD2=$(ls arch/$ARCH/configs/vendor)



echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "        "
echo "-------------------内核版本为:$KV1--------------------------------"



echo "        "
echo "-------------------当前你设置的内核配置KD值为:$KD-------------"
echo "-------------------当前你设置的内核配置KV值为:$KV-------------"
echo "        "


echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

echo "-----内核配置1(arch/arm64/config/下存在的内核配置文件)，即#export KD=xxxxx_defconfig中xxxxx_defconfig可选的值---------------"
echo "        "
echo "$KD1"
echo "        "
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

echo "        "
echo "-----内核配置2(arch/arm64/config/vendor下存在的内核配置文件)，即#export KD=vendor/xxxxx_defconfig中xxxxx_defconfig可选的值------"
echo "        "
echo "$KD2"
echo "        "
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"







