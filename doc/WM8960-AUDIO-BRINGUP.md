# WM8960 Audio Bringup for MT7628AN

## 概要

目标：在 MT7628AN 开发板上通过 I2S 接口驱动 WM8960 codec，实现音频播放和录音。

状态：**驱动层已完整初始化，ALSA 声卡已创建，但 PCM DMA 传输触发内核崩溃（硬件复位），音频尚未实际输出。**

---

## 已完成的工作

### 1. 修复 build-dtb.sh 内核版本不一致

**文件**: `tools/build-dtb.sh`

**问题**: 第 3 行 cpp 预处理命令的 `-I` include 路径引用了 `linux-6.12.66`，但 SDK 实际目录是 `linux-6.12.94`，导致 DTS 编译时找不到 `dt-bindings/` 头文件。

**修复**: `linux-6.12.66` → `linux-6.12.94`，与第 4 行 dtc 编译命令保持一致。

### 2. Kernel 分区对齐 (64KB padding)

**问题**: 内核 uImage 大小非 64KB 对齐，内核启动后打印:
```
mtd: partition "kernel" doesn't end on an erase/write block -- force read-only
```
导致 `/dev/mtd4` 无法写入烧录。

**修复**: 在 `build-dtb.sh` 中 `mkimage` 之后添加 padding 逻辑:
```bash
KERNEL_SIZE=$(stat -c%s syq-mt7628.bin)
ALIGNED=$(( (KERNEL_SIZE + 0xFFFF) / 0x10000 * 0x10000 ))
if [ $ALIGNED -gt $KERNEL_SIZE ]; then
    dd if=/dev/zero bs=1 count=$((ALIGNED - KERNEL_SIZE)) >> syq-mt7628.bin
fi
```
对齐后 kernel 分区大小 0x210000，mtd 变为可写。

### 3. 修复 I2C 引脚冲突

**问题**: 内核日志:
```
mt76x8-pinctrl pinctrl: pin io4 already requested by pinctrl; cannot claim for 10000900.i2c
```
`&state_default` 中的 `groups = "i2c"` 先占用了 I2C 引脚，之后 i2c 驱动 (自带 `pinctrl-0 = <&i2c_pins>`) probe 时再次申请同一引脚被拒绝。

**修复**: 从 board DTS 中删除 `&state_default` 覆盖块。i2c 驱动在 SoC dtsi 中已自带 `pinctrl-0 = <&i2c_pins>`，自动管理引脚。

### 4. 以太网 MAC 地址

**问题**: 网卡 MAC 地址随机生成，未从 factory 分区读取标签 MAC `b0:a3:51:2f:74:18`。

**修复**: 
- factory 分区 nvmem-layout 中添加 `mac-address@4` cell
- 新增 `&ethernet` 节点引用该 cell:

```dts
&ethernet {
    nvmem-cells = <&macaddr_factory_4>;
    nvmem-cell-names = "mac-address";
};
```

### 5. WM8960 硬件探测

**状态**: 正常。I2C 地址 0x1a 可检测到设备。

**关键内核日志**:
```
i2c_dev: i2c /dev entries driver
wm8960 0-001a: supply DCVDD not found, using dummy regulator  (可忽略)
wm8960 0-001a: supply DBVDD not found, using dummy regulator  (可忽略)
wm8960 0-001a: supply AVDD not found, using dummy regulator   (可忽略)
wm8960 0-001a: supply SPKVDD1 not found, using dummy regulator (可忽略)
wm8960 0-001a: supply SPKVDD2 not found, using dummy regulator (可忽略)
```

### 6. I2S 驱动加载

**安装包**: `apk add kmod-sound-mt7620`

**驱动**: `snd_soc_ralink_i2s` (驱动路径: `sound/soc/ralink/ralink-i2s.c`)
- 平台 compatible: `mediatek,mt7628-i2s`
- 时钟源: `pcmi2s` (480MHz) → 分频至 BCLK
- REFCLK 输出: 12MHz (作为 WM8960 MCLK)
- DMA: GDMA channel 4 (TX), channel 6 (RX)

**内核日志**: `ralink-i2s 10000a00.i2s: mclk 480MHz`

### 7. WM8960 MCLK 时钟连接

**问题**: 初始 DTS 中 WM8960 codec 节点没有 `clocks` 属性，导致:
```
wm8960 0-001a: No MCLK configured
wm8960 0-001a: ASoC: error at snd_soc_dai_hw_params on wm8960-hifi: -22
```

**修复**: 在 DTS 根节点添加 12MHz fixed-clock，连接至 WM8960:
```dts
refclk: refclk {
    compatible = "fixed-clock";
    #clock-cells = <0>;
    clock-frequency = <12000000>;
    clock-output-names = "refclk";
};
```

并在 WM8960 节点添加:
```dts
clocks = <&refclk>;
clock-names = "mclk";
```

MT7628 通过 pinmux (`refclk_pins`) 输出 12MHz REFCLK 至 WM8960 MCLK 引脚。

### 8. ALSA 声卡初始化状态

**最终状态（DMA 传输前）**:

```
$ cat /proc/asound/cards
 0 [AudioI2S       ]: simple-card - Audio-I2S
                      Audio-I2S

$ ls /dev/snd/
controlC0  pcmC0D0c  pcmC0D0p  timer

$ cat /proc/asound/pcm
00-00: ralink-i2s-wm8960-hifi wm8960-hifi-0 : playback 1 : capture 1
```

ASoC 组件链:
```
10000a00.i2s  (CPU DAI, I2S controller)
wm8960.0-001a (Codec)
ralink-i2s    (Platform DMA)
wm8960-hifi   (Codec DAI)
```

插拔通路配置:
```
amixer sset 'Left Output Mixer PCM' on
amixer sset 'Right Output Mixer PCM' on
amixer sset 'Playback' 200,200 unmute
amixer sset 'Headphone' 100,100 unmute
```

---

## 阻塞问题：GDMA 驱动写入崩溃

### 现象

任何 PCM 播放尝试（aplay / OSS write）触发设备立即硬件复位：

```
$ aplay /tmp/test_48k.wav
Playing WAVE '/tmp/test_48k.wav' : Signed 16 bit Little Endian, Rate 48000 Hz, Stereo
aplay: pcm_write:2178: write error: I/O error
```

**设备立即 crash，SSH 断开，无内核 panic 日志输出到串口。** 这是 GDMA 访问 I2S FIFO 时触发的总线级别硬件错误。

### 尝试过的方案（全部失败）

| 方案 | 结果 |
|------|------|
| codec 作 master (CBM_CFM) | crash |
| I2S 作 master (CBS_CFS) | crash |
| 48kHz 采样率 | crash |
| 8kHz 采样率 | crash |
| 极小 buffer (period_size=512) | crash |
| OSS /dev/dsp 写入 4 字节 | crash |
| aplay --mmap | crash |
| aplay RW_INTERLEAVED | crash |

结论：只要 GDMA channel 4/6 启动传输就崩溃，与参数无关。

### 相关内核模块

```
ralink_gdma            16384  2    # GDMA engine, 16 channels
snd_soc_ralink_i2s     12288  2    # I2S CPU DAI driver
snd_soc_simple_card    16384  0    # machine driver
snd_soc_wm8960         28672  1    # WM8960 codec driver
virt_dma               12288  1 ralink_gdma
```

DMA 通道分配:
```
dma0chan4  | 10000a00.i2s:tx
dma0chan6  | 10000a00.i2s:rx
```

### 疑点

1. **GDMA 驱动**: `drivers/dma/ralink-gdma.ko` (内核 6.12.94)，没有 OpenWrt 补丁（`target/linux/ramips` 下无 patch 目录）
2. **I2S 驱动**: `sound/soc/ralink/ralink-i2s.c` 配置 `maxburst = 1`, `addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES`
3. **FIFO 大小**: 32 words，阈值 4/4
4. 测试过 master/slave、多种采样率、多种 buffer 大小，均 flash crash，说明是 GDMA 地址/配置层面的问题而非参数不兼容
5. workspace 中另有 `ralink-pcm/` 和 `mt7620a-pcm/` 两个替代 PCM 驱动，使用 PIO 模式，可能可绕过 GDMA

### 可尝试方向

1. **使用 PIO 模式驱动**：编译 workspace 中的 `pcm.c`（PCM 控制器独立驱动，不通过 GDMA）
2. **FIFO 直接写入**: MT7628 datasheet 中有基于 I2S FIFO 直接读写的 PIO 模式
3. **SDK 级修复**: 在 `dev/sdk/` 中修改 GDMA 驱动，重新编译完整内核
4. **串口抓取详细信息**: 如果串口无输出可能是 WDT 触发而非 panic，检查硬件连接和复位源

---

## 当前 DTS 修改汇总

**文件**: `dts/25.12.5/dts-modify/mt7628an_mediatek_syq-mt7628an.dts`

| 修改项 | 说明 |
|--------|------|
| 删除 `&state_default` i2c 覆盖 | 解决 I2C pin 冲突 |
| 添加 `refclk` fixed-clock 节点 | 12MHz, 为 WM8960 提供 MCLK |
| WM8960 添加 `clocks`/`clock-names` | 连接 MCLK 时钟 |
| factory 分区添加 `mac-address@4` | MAC 地址 nvmem cell |
| 添加 `&ethernet` nvmem-cells | 固定网卡 MAC |
| `dailink0_master` 设于 CPU DAI | I2S 作 BCLK/FSYNC master |

---

## 当前构建脚本修改汇总

**文件**: `tools/build-dtb.sh`

| 修改项 | 说明 |
|--------|------|
| `linux-6.12.66` → `linux-6.12.94` | 修复内核头文件路径 |
| kernel padding 到 64KB 对齐 | 使 mtd4 可写 |

---

## 设备信息

| 项目 | 值 |
|------|------|
| SOC | MT7628AN (MIPS 24KEc) |
| 内核 | Linux 6.12.94 |
| OpenWrt | 25.12.5 r33051 |
| IP 地址 | 192.168.1.217 (固定) |
| 标签 MAC | b0:a3:51:2f:74:18 |
| Factory 分区 | mtd2, 0x40000, 64KB |
| 声卡 | card0: Audio-I2S (RALINK-I2S ↔ WM8960) |
| I2C 总线 | I2C-0, WM8960 @ 0x1a |
| I2S 时钟 | pcmi2s 480MHz, REFCLK 12MHz |
| GDMA | 16 channels, rev 3 |
