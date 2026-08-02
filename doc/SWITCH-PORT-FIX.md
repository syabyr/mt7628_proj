# MT7628AN Switch Port Link Down 问题排查

## 现象

设备上交换机（rt3050-esw）的 Port 1(LAN1)、Port 2(LAN2)、Port 3(LAN3) 插网线后无反应，始终 `link: down`。只有 Port 0(WAN) 正常工作。

## 排查过程

### 1. 检查 swconfig 状态

```
swconfig dev switch0 show
```

发现 Port 2/3 的 `disable: 1`（被 `portdisable = <0x3c>` 屏蔽）。

### 2. 修复 portdisable（无效）

将 `portdisable = <0x3c>` (0b00111100, 禁用 Port 2/3/4/5) 改为 `0x30` (0b00110000, 仅禁用 Port 4/5)。

Port 2/3 `disable: 0` 但插网线仍 `link: down`，说明问题不在 disable 位。

### 3. 删除 &esw 覆盖块（更糟）

将整个 `&esw` 覆盖删除，与官方 `mt7628an_mediatek_mt7628an-eval-board.dts` 保持一致。但**所有 Port 1/2/3 全部 link:down**。

原因：没有 `portmap` 属性时，`rt3050-esw` 驱动不会初始化内部 EPHY 的硬件层，所以 PHY 芯片根本没被驱动。

### 4. 只保留 portmap（无效）

添加 `portmap = <0x3e>`（不设 portdisable），Port 1/2/3 仍 link:down。但网口数量和 VLAN 配置正确。

### 5. 检查 pinmux 状态

查看 `/sys/kernel/debug/pinctrl/pinctrl/pinmux-pins`，没有发现网络相关引脚的异常占用。

### 6. 审计 DTS 中的所有 pinctrl 配置（关键发现）

#### DTS 文件中存在:

```dts
&pinctrl {
    ephy-digital;    // ← 可疑！
    ...
};
```

#### 验证分析:

`ephy-digital` 是一个 pinctrl 宏/属性，用于将**内部 EPHY 的 LED 引脚切换到 GPIO 模式**。对于 MT7628AN，这个属性**同样影响内部 PHY 的模拟电路初始化**，导致多达 4 个内部 FE PHY 无法正常工作。

统计 MT7628AN 全系 70+ 个 DTS 文件：
- 仅 3 个使用 `ephy-digital`：`creality_wb-01`(3D打印机)、`mediatek_linkit-smart-7688`(IoT 开发板)、`ravpower_rp-wd009`(无线路由器变身文件hub)
- 官方 `mt7628an_mediatek_mt7628an-eval-board.dts` **不使用 `ephy-digital`**

这三个设备都不是标准路由器 —— 它们不需要完整的多口 EPHY 功能。

#### 最终修复：

删除 `&pinctrl` 下的 `ephy-digital;`：

```diff
 &pinctrl {
-    ephy-digital;
-
     sdxc_iot_mode: sdxc_iot_mode {
```

## 根因

`ephy-digital` 属性会将内部 EPHY LED 引脚切换为 GPIO/数字输出模式。但 MT7628AN 硬件实现中，这个模式切换**同时影响 PHY 内部 AFE（模拟前端）的初始化序列**。当这些引脚被配置为数字模式后，内部 PHY 无法正确探测 MDI 接口状态（link detection），导致所有 FE 端口永远显示 `link: down`。

Port 0(WAN) 不受影响是因为它走的是 RGMII/MII 接口到外部 PHY，不经过内部 EPHY 模拟前端。

## 教训

- 不要随意向 DTS 中添加 pinmux 属性，`ephy-digital` 等看似无害的属性在 MT7628 上有全局副作用
- 对比其他参考设计时，注意评估属性的适用范围和目标应用场景
- 网口不识别时，除了 portmap/portdisable，还应检查 pinctrl 中是否有影响 EPHY 引脚的设置

## 当前交换机配置

```
&esw {
    mediatek,portmap = <0x3e>;
};
```

- portmap 0x3e = 0b00111110: Port 1(LAN1), 2(LAN2), 3(LAN3), 4(保留) 可用
- 不设 portdisable (默认 0x00)
- 不设 ephy-digital (保证内部 EPHY 正常工作)
