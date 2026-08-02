cd dev/imagebuilder
make image PROFILE=mediatek_mt7628an-eval-board PACKAGES="luci luci-base luci-theme-openwrt luci-i18n-base-zh-cn gpiod-tools i2c-tools luci-theme-bootstrap usbutils kmod-sound-soc-wm8960 kmod-sound-mt7620"
cd ../..
cp dev/imagebuilder/bin/targets/ramips/mt76x8/openwrt-25.12.5-ramips-mt76x8-mediatek_mt7628an-eval-board-squashfs-sysupgrade.bin ./