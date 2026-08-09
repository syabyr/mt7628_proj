# OpenWrt 分区刷写指南 (MT7628/MIPS 平台)

## 1. 查看分区布局

```bash
cat /proc/mtd
```

典型输出（MT7628AN, 16MB Flash）:

```
dev:    size     erasesize  name
mtd0: 00030000 00010000 "u-boot"
mtd1: 00010000 00010000 "u-boot-env"
mtd2: 00010000 00010000 "factory"
mtd3: 00fb0000 00010000 "firmware"
mtd4: 00210000 00010000 "kernel"
mtd5: 00da0000 00010000 "rootfs"
mtd6: 009d0000 00010000 "rootfs_data"
```

| 分区 | 说明 | 可写 | 风险 |
|------|------|------|------|
| u-boot | 引导程序 | 只读 | 写坏变砖 |
| u-boot-env | U-Boot 环境变量 | 只读 | |
| factory | WiFi EEPROM/MAC | 只读 | 丢失则 MAC/WiFi 失效 |
| firmware | uImage (kernel+rootfs) | 按需 | |
| kernel | 内核 (uImage 子分区) | 可写 | 写坏无法启动 |
| rootfs | 根文件系统 (uImage 子分区) | 一般只读 | |
| rootfs_data | overlay 数据分区 | 可写 | 写入失败则覆盖层崩溃 |

## 2. 刷写单分区（推荐用于开发调试）

### 2.1 上传文件到设备

```bash
# 宿主端
scp <filename> root@<device_ip>:/tmp/
```

### 2.2 刷写并重启

```bash
# 设备端
mtd write /tmp/<filename> <partition_name>
reboot
```

一行命令：
```bash
ssh root@<device_ip> "mtd write /tmp/<filename> <partition_name> && reboot"
```

### 2.3 本工程实例：刷写内核

```bash
# 宿主机执行
scp ./dev/imagebuilder/build_dir/target-mipsel_24kc_musl/linux-ramips_mt76x8/mediatek_mt7628an-eval-board-kernel.bin root@192.168.1.217:/tmp/

ssh root@192.168.1.217 "mtd write /tmp/mediatek_mt7628an-eval-board-kernel.bin kernel && reboot"
```

典型输出：
```
Unlocking kernel ...
Writing from /tmp/mediatek_mt7628an-eval-board-kernel.bin to kernel ...
[e][w][e][w]...
```

## 3. 注意事项

### 3.1 分区擦除块对齐

内核分区必须 64KB (0x10000) 对齐，否则内核将分区标记为只读：

```
mtd: partition "kernel" doesn't end on an erase/write block -- force read-only
```

**解决方案**：构建脚本中在 `mkimage` 之后 padding 内核到 64KB 边界：

```bash
KERNEL_SIZE=$(stat -c%s syq-mt7628.bin)
ALIGNED=$(( (KERNEL_SIZE + 0xFFFF) / 0x10000 * 0x10000 ))
if [ $ALIGNED -gt $KERNEL_SIZE ]; then
    dd if=/dev/zero bs=1 count=$((ALIGNED - KERNEL_SIZE)) >> syq-mt7628.bin
fi
```

### 3.2 检查分区可写状态

```bash
dmesg | grep "read-only"
```

如果有 `force read-only` 日志，说明分区未对齐或已被内核锁定。

### 3.3 危险分区不可写入

- **u-boot (mtd0)** — 写坏直接变砖，只能通过 SPI 编程器恢复
- **u-boot-env (mtd1)** — 写坏导致启动参数丢失
- **factory (mtd2)** — 含 WiFi 校准数据和 MAC 地址，丢失后不可恢复

### 3.4 scp 前获取设备 IP

设备 IP 可能因 MAC 地址变化而改变（DHCP 动态分配时）。本工程已固定设备 IP：

```bash
# 查看设备状态
ssh root@<device_ip> "ifconfig eth0.2 | grep 'inet addr'"
```

## 4. 完整固件升级（sysupgrade）

如果修改了 rootfs / 内核模块等，需要整体升级 firmware 分区：

```bash
# 构建 sysupgrade 镜像
make -C dev/sdk package/compile

# 上传并升级
scp dev/sdk/bin/targets/ramips/mt76x8/openwrt-*-sysupgrade.bin root@<ip>:/tmp/
ssh root@<ip> "sysupgrade -n /tmp/openwrt-*-sysupgrade.bin"
```

`-n` 参数表示不保留旧配置（干净升级）。

## 5. 设备重启等待

MT7628 + OpenWrt 6.12 启动时间约 40-50 秒（包括内核加载、模块加载、WiFi 初始化等）。

```bash
# 等待设备上线
for i in $(seq 1 30); do
    ping -c 1 -W 2 <device_ip> >/dev/null 2>&1 && break
    sleep 2
done
ssh root@<device_ip> "echo 'online'"
```

## 6. 问题排查

| 问题 | 可能原因 | 解决 |
|------|----------|------|
| 刷写后设备不启动 | 内核镜像损坏或配置错误 | 串口查看启动日志，uboot 中断后 TFTP 恢复 |
| `Can't open device for writing` | 分区只读或未对齐 | 检查 `dmesg` 中的 `force read-only` 日志 |
| `No route to host` | 设备正在重启 | 等待 50 秒后重试 |
| `Host key verification failed` | 新设备或 IP 变更 | `ssh-keyscan -H <ip> >> ~/.ssh/known_hosts` |
