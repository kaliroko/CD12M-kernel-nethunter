export ARCH=arm64
export SUBARCH=arm64
export CC="gcc"
make O=out cd12m_defconfig
make -j3 O=out

