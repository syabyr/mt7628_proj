export ROOT_DIR=`pwd`
export DEV_DIR=$ROOT_DIR/dev
export SDK_DIR=$DEV_DIR/sdk
export STAGING_DIR=$SDK_DIR/staging_dir
export PATH=$STAGING_DIR/toolchain-mipsel_24kc_gcc-14.3.0_musl/bin:$PATH
export CROSS_COMPILE=mipsel-openwrt-linux-
