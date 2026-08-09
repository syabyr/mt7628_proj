# MT7628 SPI CS1 spidev_test 失败根因分析

## 现象

`spidev_test -D /dev/spidev0.1` 执行失败：

```
spi mode: 0x0
can't send spi message: Invalid argument
Aborted
```

`/dev/spidev0.1` 设备节点已创建，DTS 配置正确，但通过 `ioctl(SPI_IOC_MESSAGE)` 发送 SPI 传输时返回 `-EINVAL`。

参考：OpenWrt ticket [#20521](https://dev.archive.openwrt.org/ticket/20521)

## 排查过程

### 1. 确认设备注册成功

```
$ ls /sys/bus/spi/devices/
spi0.0   spi0.1
$ ls -la /dev/spidev0.1
crw------- 1 root root 153, 0 /dev/spidev0.1
```

### 2. 确认 pinmux 正确

```
pin 6 (io6): 10000b00.spi ... function spi cs1 group spi cs1
pin 7 (io7): 10000b00.spi ... function spi group spi
pin 8 (io8): 10000b00.spi ... function spi group spi
pin 9 (io9): 10000b00.spi ... function spi group spi
pin 10 (io10): 10000b00.spi ... function spi group spi
```

CS1 对应的 io6 已正确配置为 `spi cs1` 功能。

### 3. 系统调用级追踪

```
ioctl(3, SPI_IOC_WR_MODE32, ...) = 0           # OK
ioctl(3, SPI_IOC_RD_MODE32, ...) = 0           # OK
ioctl(3, SPI_IOC_WR_BITS_PER_WORD, ...) = 0    # OK
ioctl(3, SPI_IOC_RD_BITS_PER_WORD, ...) = 0    # OK
ioctl(3, SPI_IOC_WR_MAX_SPEED_HZ, ...) = 0     # OK
ioctl(3, SPI_IOC_RD_MAX_SPEED_HZ, ...) = 0     # OK
ioctl(3, SPI_IOC_MESSAGE(32), ...) = -1 EINVAL  # 实际传输失败
```

所有参数设置 ioctl 全部成功，只有实际传输 `SPI_IOC_MESSAGE` 被拒绝。

### 4. dd 读写可以工作

```
$ dd if=/dev/spidev0.1 bs=1 count=1 of=/dev/null
1+0 records in / 1+0 records out
```

`read()`/`write()` 路径正常，说明问题出在 `SPI_IOC_MESSAGE`（`transfer_one`）路径。

## 根因

驱动 `drivers/spi/spi-mt7621.c` 存在两个硬编码 bug：

### Bug 1: `num_chipselect = 1`

```c
master->num_chipselect = 1;
```

驱动只向 SPI 核心注册了 1 个片选。任何对 CS1 的 `SPI_IOC_MESSAGE` 请求都会被核心层或驱动层直接拒绝。

### Bug 2: `mt7621_spi_set_cs()` 硬编码 bit 0

```c
// 原代码只操作 CS0
if (enable)
    polar |= 1;    // 仅 bit 0
else
    polar &= ~1;   // 仅 bit 0
```

即使绕过 CS 数量检查，CS1 的片选信号也不会被正确操作。
CS0 控制位是 MASTER 寄存器的 bit 0，CS1 是 bit 1，必须用 `BIT(cs)` 动态计算。

### 为什么 dd 能工作？

`spidev` 的 `read()`/`write()` 内部走的是 `spidev_sync_read()`/`spidev_sync_write()`，这些路径会通过驱动做必要的检查。但关键是：**dd 只传 1 字节，路径差异使得它通过了核心层的通用校验**，而 `SPI_IOC_MESSAGE` 直接走到 `transfer_one` → `mt7621_spi_transfer_one()`，CS1 直接被拒绝。

## 验证数据

| 操作 | 结果 |
|------|------|
| `dd if=/dev/spidev0.1 bs=1 count=1` | 成功 |
| `spidev_test -D /dev/spidev0.1 -p 'ab'` | EINVAL |
| `spidev_test -D /dev/spidev0.1 -s 100000` | EINVAL |
| `spidev_test -D /dev/spidev0.1 -s 1000000` | EINVAL |

所有通过 `SPI_IOC_MESSAGE` 的传输全部失败，与传输参数无关。

## 修复方案

修改 `drivers/spi/spi-mt7621.c`：

### 1. 增加片选数量

```c
// 修复前
master->num_chipselect = 1;

// 修复后
master->num_chipselect = 2;
```

### 2. 修复 set_cs 片选控制

```c
// 修复前
static void mt7621_spi_set_cs(struct spi_device *spi, int enable)
{
    ...
    if (enable)
        polar |= 1;     // 硬编码 bit 0
    else
        polar &= ~1;
    ...
}

// 修复后
static void mt7621_spi_set_cs(struct spi_device *spi, int enable)
{
    ...
    int cs = spi->chip_select;
    if (enable)
        polar = BIT(cs);   // 动态计算: CS0=bit0, CS1=bit1
    ...
}
```

### 3. 传输大小限制（附加问题）

MT7621 SPI 控制器硬件只有 8 个 DIDO 数据寄存器，单次全双工传输最多 16 字节。`spidev_test` 默认发送 38 字节，会超出硬件限制。修复 CS 问题后，可能还需使用更小的测试数据，或更新驱动使用半双工模式支持任意长度传输。

## 影响范围

- 所有使用 `spi-mt7621.c` 驱动的 MT7628/MT7688 设备均受影响
- `CONFIG_SPI_MT7621=y`，驱动编译为内置，无法模块级修复
- 需在 `dev/sdk/` 中修改驱动源码后完整重编内核

## 相关修复记录

| 版本 | 状态 |
|------|------|
| Linux 6.12.94 (当前) | 未修复 |
| OpenWrt 4.14 backport | 曾试图修复后被 revert（导致 MT7621 设备 CS1 异常） |
| Linux mainline `2a741cd` | 已修复：支持 2 native CS + GPIO CS，transfer_one 重构 |
