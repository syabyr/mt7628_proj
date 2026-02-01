# compile dtb
echo "build dts from mt7628_proj/dts/25.12.0/dts-modify/mt7628an_mediatek_mt7628an-eval-board.dts"
cp mt7628_proj/dts/25.12.0/dts-modify/mt7628an_mediatek_mt7628an-eval-board.dts dev/sdk/target/linux/ramips/dts/mt7628an_syq-mt7628.dts
mipsel-openwrt-linux-musl-cpp -nostdinc -x assembler-with-cpp -I dev/sdk/build_dir/target-mipsel_24kc_musl/linux-ramips_mt76x8/linux-6.12.66/include/ -undef -D__DTS__  -o  mt7628an_syq-mt7628.dtb.tmp dev/sdk/target/linux/ramips/dts/mt7628an_syq-mt7628.dts
./dev/sdk/build_dir/target-mipsel_24kc_musl/linux-ramips_mt76x8/linux-6.12.66/scripts/dtc/dtc -O dtb -i./dev/sdk/target/linux/ramips/dts -i./dev/sdk/build_dir/target-mipsel_24kc_musl/linux-ramips_mt76x8/linux-6.12.66/scripts/dtc/include-prefixes -Wno-unit_address_vs_reg -Wno-simple_bus_reg -Wno-unit_address_format -Wno-pci_bridge -Wno-pci_device_bus_num -Wno-pci_device_reg -Wno-avoid_unnecessary_addr_size -Wno-alias_paths -Wno-graph_child_address -Wno-graph_port -Wno-unique_unit_address -o mt7628an_syq-mt7628.dtb mt7628an_syq-mt7628.dtb.tmp

# combine
cp ./dev/imagebuilder/build_dir/target-mipsel_24kc_musl/linux-ramips_mt76x8/vmlinux syq-mt7628.bin
cat mt7628an_syq-mt7628.dtb >> syq-mt7628.bin
./dev/sdk/staging_dir/host/bin/lzma e syq-mt7628.bin -lc1 -lp2 -pb2 syq-mt7628.bin.new

mkimage -A mips -O linux -T kernel -C lzma -a 0x80000000 -e 0x80000000 -n 'MIPS OpenWrt Linux-6.12.66' -d syq-mt7628.bin.new syq-mt7628.bin

rm syq-mt7628.bin.new
rm mt7628an_syq-mt7628.dtb.tmp
rm mt7628an_syq-mt7628.dtb

mv syq-mt7628.bin ./dev/imagebuilder/build_dir/target-mipsel_24kc_musl/linux-ramips_mt76x8/mediatek_mt7628an-eval-board-kernel.bin
echo "dtb and bin build complete"