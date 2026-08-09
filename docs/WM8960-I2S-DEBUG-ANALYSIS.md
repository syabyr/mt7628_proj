# MT7628 I2S + WM8960 音频调试分析报告

## 概述

MT7628AN 平台使用 I2S 接口连接 WM8960 音频编解码器，通过 simple-audio-card 框架创建 ALSA 声卡。声卡设备已成功注册，但 PCM 播放/录制均返回 EIO，且后续 debugfs 读取会导致内核 Oops 重启。

**调试日期**: 2026-08-09

## 1. 当前状态

### 1.1 已成功工作的部分

| 组件 | 状态 | 说明 |
|------|------|------|
| WM8960 codec | OK | I2C 0x1a 地址被内核驱动，regulator 使用 dummy 供电 |
| I2S 控制器 | OK | 地址 0x10000a00，mclk 480MHz，复位正常 |
| GDMA 引擎 | OK | 16 通道，rev 3，dma0chan4=tx, dma0chan6=rx |
| ALSA 声卡 | OK | card0 "Audio-I2S"，pcmC0D0p/pcmC0D0c 已创建 |
| I2S pinmux | OK | pinctrl-0 = i2s_pins + refclk_pins |
| MCLK | OK | 12MHz fixed-clock → WM8960 clocks 属性 |
| 设备树 DTS | OK | simple-audio-card 配置完成 |
| ALSA mixer | OK | WM8960 全部控件可用 (Headphone, Speaker, Playback, Capture 等) |

### 1.2 内核日志

```
[   47.320951] gdma-rt2880 10002800.gdma: revision: 3, channels: 16
[   47.400229] ralink-i2s 10000a00.i2s: mclk 480MHz
[   47.518812] wm8960 0-001a: supply DCVDD not found, using dummy regulator
[   47.532610] wm8960 0-001a: supply DBVDD not found, using dummy regulator
[   47.546118] wm8960 0-001a: supply AVDD not found, using dummy regulator
[   47.559381] wm8960 0-001a: supply SPKVDD1 not found, using dummy regulator
[   47.573188] wm8960 0-001a: supply SPKVDD2 not found, using dummy regulator
```

### 1.3 模块信息

```
snd_soc_ralink_i2s     12288  2
snd_soc_wm8960         28672  1
snd_soc_simple_card    16384  0
ralink_gdma            16384  2
snd_pcm_dmaengine      12288  1 snd_soc_core
```

### 1.4 I2S 寄存器（空闲状态）

```
CFG0:      0x00014040
  bit30 (DMA_EN)     = 1
  bit14 (RX_THRES)   = 1 (threshold=4)
  bit6  (TX_THRES)   = 1 (threshold=4)
  bit31 (EN)         = 0 ← 空闲状态，I2S 未使能
INT_STATUS: 0x00000000
FF_STATUS:  0x00000010
CFG1:       0x00000000
DIVCMP:     0x00000000
DIVINT:     0x00000000
```

### 1.5 DMA 通道

```
dma0 (10002800.gdma): number of channels: 16
 dma0chan4    | 10000a00.i2s:tx
 dma0chan6    | 10000a00.i2s:rx
```

## 2. 故障现象

### 2.1 播放测试 (speaker-test)

```bash
speaker-test -D hw:0 -c 2 -r 48000 -t sine -f 440 -l 1
```

输出:
```
Playback device is hw:0
Stream parameters are 48000Hz, S16_LE, 2 channels
Rate set to 48000Hz (requested 48000Hz)
 0 - Front Left
Write error: -5,I/O error        ← EIO
xrun_recovery failed: -5,I/O error
Transfer failed: I/O error
```

### 2.2 录制测试 (arecord)

```bash
arecord -v -D hw:0 -c 2 -r 48000 -f S16_LE -d 1 /tmp/test.wav
```

输出:
```
Recording WAVE '/tmp/test.wav' : Signed 16 bit Little Endian, Rate 48000 Hz, Stereo
arecord: pcm_read:2272: read error: I/O error    ← 同样 EIO
```

### 2.3 OSS 路径测试

```bash
dd if=/dev/zero of=/dev/dsp bs=256 count=4
```
结果: 设备静默重启（无日志输出）。

### 2.4 采样率测试

| 采样率 | 播放 | 录制 |
|--------|------|------|
| 8000Hz | EIO | EIO |
| 48000Hz | EIO | EIO |

不同采样率均失败，排除时钟分频计算问题。

### 2.5 时钟方向测试

| 配置 | BCLK/LRCLK 主控 | 结果 |
|------|-----------------|------|
| DTS 原版 | I2S (CPU) | EIO |
| DTS 修改版 | WM8960 (Codec) | EIO |

主从配置均失败，排除时钟方向问题。

## 3. 内核 Oops 分析

### 3.1 触发条件

PCM 操作返回 EIO 后，读取 `/sys/kernel/debug/10000a00.i2s/stats` 或 `/sys/kernel/debug/regmap/10000a00.i2s/registers` 触发内核 Oops。

### 3.2 Oops 详情

```
CPU 0 Unable to handle kernel paging request at virtual address 00004154
epc = 81825b80, ra = 802c84c0
BadVA: 00004154

Process: cat (pid: 2883)
Module: snd_soc_ralink_i2s

$0  : 00000000 00000001 81825b80 00000000
$4  : 818a0528 00000000 00000000 83399d48

Call Trace:
[<802c86f4>] 0x802c86f4
[<802c97d8>] 0x802c97d8
[<801230f4>] 0x801230f4
[<8015c5b4>] 0x8015c5b4
[<82a1e32c>] 0x82a1e32c [snd_soc_ralink_i2s] ← ralink_i2s_stats_show
[<80130b04>] 0x80130b04
...

Code: ... <80674154> ...

Kernel panic - not syncing: Fatal exception
Rebooting in 3 seconds..
```

### 3.3 分析

- **BadVA = 0x00004154**: 极低的虚拟地址，典型空指针 + 偏移量模式
- **epc 在 snd_soc_ralink_i2s 模块内**: ralink_i2s_stats_show 函数
- **触发路径**: debugfs seq_read → ralink_i2s_stats_show → seq_printf → 访问 txstats/rxstats 成员 → 空指针解引用
- **根因**: PCM 操作失败后，驱动的 stats 结构体处于未初始化/损坏状态

## 4. 根因分析

### 4.1 核心问题：DMA 请求线未正确配置

这是 EIO 问题最可能的根因。

**背景**: 6.12 内核中 `snd_dmaengine_dai_dma_data` 结构体移除了 `slave_id` 字段（自 5.16 起废弃），改用 `peripheral_config` / `peripheral_size` 传递外设特定配置。

**I2S 驱动** (`snd-soc-ralink-i2s.ko`):
- 通过 `ralink_i2s_init_dma_data()` 配置 DMA 参数
- 需要告诉 GDMA 引擎 I2S 的 DMA 请求线号 (TX=2, RX=3)
- 旧 API: `dma_data->slave_id = i2s->txdma_req` (已废弃)
- 新 API: `dma_data->peripheral_config` (需显式设置)

**GDMA 驱动** (`ralink-gdma.ko`):
- 使用 `of_dma_xlate_by_chan_id` 分配通道（从 DT `dmas` 属性获取通道号）
- 通道号（4、6）≠ DMA 请求线号（2、3）
- 需要 `device_config` 回调中的 `peripheral_config` 来设置请求线

**问题**: 如果 I2S 驱动未通过 `peripheral_config` 正确传递 DMA 请求线号，GDMA 的通道控制寄存器中将缺少请求线配置，导致：
1. GDMA 通道不知道要监听哪个外设的 DMA 请求
2. I2S FIFO 达到阈值后发出的 DMA 请求被 GDMA 忽略
3. PCM write/read 等不到 DMA 传输完成 → 超时返回 EIO

### 4.2 驱动源码缺失

两个关键驱动的 C 源码均未包含在 SDK 中:

| 文件 | 位置 | 状态 |
|------|------|------|
| ralink-i2s.c | sound/soc/ralink/ | 仅 .ko，无源码 |
| ralink-gdma.c | drivers/dma/ | 仅 .ko，无源码 |

这些是 OpenWrt 专用驱动，不在主线 Linux 6.12 源码树中。OpenWrt 通过补丁系统在完整构建时添加，SDK 只保留了编译产物。

### 4.3 I2S 寄存器验证

通过 debugfs regmap 读取的 I2S 寄存器显示：
- CFG0_EN (bit31) = 0: I2S 模块在空闲时未使能（正常）
- CFG0_DMA_EN (bit30) = 1: DMA 模式已配置
- CFG0_SLAVE (bit16) = 0/1: 取决于主从配置
- DIVCMP/DIVINT = 0: BCLK 分频器在空闲时未配置（正常）

寄存器在空闲状态的值符合预期，说明 I2S 控制器的寄存器访问没有问题。

### 4.4 故障链

```
PCM write/read
    ↓
ALSA core → dmaengine PCM → hw_params → trigger(START)
    ↓
I2S: CFG0_EN=1, CFG0_TX_EN=1 (or CFG0_RX_EN=1)
GDMA: 准备传输，但请求线未配置
    ↓
I2S FIFO 达到阈值，发出 DMA 请求
    ↓
GDMA 无法识别请求（请求线未知或错误）
    ↓
DMA 传输永不开始
    ↓
PCM write/read 超时 → EIO (-5)
    ↓
ALSA 标记 XRUN 状态
    ↓
debugfs 读取 → 驱动内部状态不一致 → Oops
    ↓
Kernel panic → 设备重启
```

## 5. 尝试过的修复

### 5.1 时钟方向切换

- 修改 DTS 使 WM8960 为 BCLK/LRCLK 主控 (codec as master)
- I2S 设为从模式
- **结果**: 无效，EIO 依旧

### 5.2 采样率调整

- 测试 8000Hz / 48000Hz
- **结果**: 无效，排除 BCLK 分频计算错误

### 5.3 不同播放方式

- speaker-test (ALSA 原生)
- aplay (ALSA utils)
- dd → /dev/dsp (OSS 模拟)
- **结果**: 全部返回 EIO 或直接重启

## 6. 可能的修复方向

### 6.1 获取并修复驱动源码 (推荐)

从 OpenWrt 完整源码树获取以下驱动:

```
target/linux/ramips/files/drivers/dma/ralink-gdma.c
target/linux/ramips/files/sound/soc/ralink/ralink-i2s.c
```

适配 6.12 API 变更:

**ralink-i2s.c**:
```c
// 1. DMA 请求线通过 peripheral_config 传递
static void ralink_i2s_init_dma_data(struct ralink_i2s *i2s, struct resource *res)
{
    dma_data->addr = res->start + I2S_REG_WREG;
    dma_data->addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    dma_data->maxburst = 1;
    // 不再使用 slave_id，改用 peripheral_config
    dma_data->peripheral_config = &i2s->txdma_req;
    dma_data->peripheral_size = sizeof(i2s->txdma_req);
}

// 2. snd_soc_dai_driver API 变更
//    - .probe/.remove 移除 → 使用 component 级 probe
//    - .symmetric_rates → .symmetric_rate
//    - dai->active → snd_soc_dai_active(dai)

// 3. platform_driver.remove 返回类型改为 void
// 4. devm_clk_get + clk_prepare_enable → devm_clk_get_enabled
```

**ralink-gdma.c**:
```c
// device_config 需处理 peripheral_config 中的请求线号
static int rt2880_dma_config(struct dma_chan *chan, struct dma_slave_config *cfg)
{
    if (cfg->peripheral_config && cfg->peripheral_size >= sizeof(u32)) {
        u32 req = *(u32 *)cfg->peripheral_config;
        // 将 req 写入 GDMA 通道控制寄存器的请求线字段
    }
}
```

### 6.2 OpenWrt 完整构建

在 OpenWrt 完整构建环境中重新编译内核，确保 I2S/GDMA 驱动与内核版本匹配。

### 6.3 替代驱动方案

使用 MT7628 的 PCM 外设 (0x10002000) 替代 I2S (0x10000a00) —— workspace 中有 `mt7620a-pcm` 驱动备选。

## 7. 技术要点

### 7.1 MT7628 音频子系统架构

```
WM8960 Codec ←→ I2S Controller (0x10000a00) ←→ GDMA (0x10002800) ←→ DDR
    ↕ I2C (0x10000900)                           ↕ ch4=tx, ch6=rx
    ↕ MCLK (GPIO37/REFCLK0, 12MHz)
```

### 7.2 关键寄存器

| 寄存器 | 地址 | 位域 | 说明 |
|--------|------|------|------|
| I2S_CFG0 | 0x10000a00 | bit31=EN, bit30=DMA_EN, bit24=TX_EN, bit20=RX_EN, bit16=SLAVE | 主控制 |
| I2S_DIVINT | 0x10000a24 | [9:0] | BCLK 整数分频 |
| I2S_DIVCMP | 0x10000a20 | bit31=CLK_EN, [8:0] | BCLK 小数分频 |
| I2S_WREG | 0x10000a10 | [31:0] | TX FIFO 写端口 |
| I2S_RREG | 0x10000a14 | [31:0] | RX FIFO 读端口 |

### 7.3 DMA 请求线映射

| 外设 | DMA 请求线 | GDMA 通道 | DT dmas |
|------|-----------|-----------|---------|
| I2S TX | 2 | 4 | <&gdma 4> |
| I2S RX | 3 | 6 | <&gdma 6> |

### 7.4 时钟路径

```
系统 PLL (480MHz)
    ↓
I2S 模块时钟 (SYSC index 10)
    ↓
I2S BCLK 分频器 (DIVINT + DIVCOMP/512)
    ↓
BCLK = 480MHz / (2 × (DIVINT + DIVCOMP/512))
    ↓ (例: 48kHz 16-bit 立体声 → BCLK=1.536MHz, DIVINT=156, DIVCOMP=128)

SYSC REFCLK0 (12MHz)
    ↓
WM8960 MCLK
    ↓ (PLL: freq_in × (R + N/K))
WM8960 SYSCLK → BCLK/LRCLK 生成器
```

## 8. 附录

### 8.1 关键文件

| 文件 | 说明 |
|------|------|
| `dts/25.12.5/dts-modify/mt7628an_mediatek_syq-mt7628an.dts` | 板级 DTS（sound/i2s/wm8960 节点） |
| `dev/sdk/target/linux/ramips/dts/mt7628an.dtsi` | SoC 级 DTSI（i2s 节点定义） |
| `dev/sdk/target/linux/ramips/modules.mk` | 模块包定义 |
| `dev/sdk/build_dir/.../sound/soc/ralink/snd-soc-ralink-i2s.ko` | I2S 驱动模块 |
| `dev/sdk/build_dir/.../drivers/dma/ralink-gdma.ko` | GDMA 驱动模块 |
| `workspace/ralink-i2s/ralink-i2s.c` | I2S 驱动 workspace 版本（需适配 6.12） |
| `workspace/mt7620a-pcm/` | PCM 外设备用驱动 |
| `dev/linux-6.12.94/sound/soc/codecs/wm8960.c` | WM8960 编解码器驱动（主线） |

### 8.2 设备测试命令

```bash
# 查看声卡
cat /proc/asound/cards
cat /proc/asound/devices

# 查看 mixer 控件
amixer scontrols

# 播放测试
speaker-test -D hw:0 -c 2 -r 48000 -t sine -f 440 -l 1
aplay -v /tmp/test.wav

# 录制测试
arecord -v -D hw:0 -c 2 -r 48000 -f S16_LE -d 1 /tmp/test.wav

# 寄存器查看
cat /sys/kernel/debug/regmap/10000a00.i2s/registers
cat /sys/kernel/debug/dmaengine/summary
```
