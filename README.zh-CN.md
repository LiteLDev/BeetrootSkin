# BeetrootSkin

<img src="docs/logo.png" alt="" width="128" align="right">

[English](README.md) | 简体中文

[![Release](https://img.shields.io/github/v/release/LiteLDev/BeetrootSkin)](https://github.com/LiteLDev/BeetrootSkin/releases)
[![License](https://img.shields.io/github/license/LiteLDev/BeetrootSkin)](COPYING.LESSER)

吃什么、吃多少，心里有数。BeetrootSkin 把 Java 版的人气模组 [AppleSkin](https://github.com/squeek502/AppleSkin) 带到了基岩版：物品提示里直接写着食物的饥饿值和饱和度，手里拿着食物就能预览吃下去的效果，平时看不见的饱和度和消耗度也会显示在 HUD 上。

<p align="center"><img src="docs/showcase/hud-preview.webp" alt="手持熟牛肉时，HUD 上闪烁显示它能恢复的鸡腿和生命值" width="720"></p>

## 功能

- **提示框里的食物数值。** 鼠标移到任意食物上，就能看到它能补多少鸡腿、多少饱和度。腐肉、生鸡肉这类吃了会出事的食物用绿色的"坏食物"图标标出。
- **先看效果再下嘴。** 手持食物时，饥饿条会闪烁显示吃下后补上的鸡腿和饱和度，心形会闪烁显示能回多少血，金苹果的生命恢复也算在内。
- **看得见的饱和度。** 鸡腿上的金色轮廓就是剩余的饱和度，它耗完之前饥饿值不会掉。
- **看得见的消耗度。** 鸡腿后面有一条格纹进度条，疾跑、跳跃、战斗都会让它变长，涨满就扣一点饱和度或饥饿值。
- **随时查看精确数值。** 按 `F6` 显示一行文字，列出精确的饥饿值、饱和度和消耗度。

<p align="center"><img src="docs/showcase/tooltips.webp" alt="几种食物的提示框，显示鸡腿和饱和度" width="720"></p>

单人游戏、局域网世界和服务器里都能用。只需你自己安装，服务器什么都不用装。

## 安装

需要 Windows 版 Minecraft 基岩版 **1.26.51.01** 和 LeviLamina **26.51.x**（例如用 [LeviLauncher](https://github.com/LiteLDev/LeviLauncher) 安装）。

```sh
lip install github.com/LiteLDev/BeetrootSkin#client
```

也可以从[最新发布](https://github.com/LiteLDev/BeetrootSkin/releases/latest)下载 `beetrootskin-client-windows-x64.zip`，解压后让 `BeetrootSkin/` 位于游戏的 `mods/` 文件夹中。

## 使用方法

装好即用，进入生存或冒险模式的世界就行。

| 想看 | 怎么做 |
| --- | --- |
| 一种食物能恢复多少 | 在物品栏或箱子里把鼠标移到它上面 |
| 现在吃下去会怎样 | 把它拿在手上（主手或副手都行） |
| 饱和度和消耗度 | 看饥饿条就行 |
| 精确的饥饿值、饱和度、消耗度 | 按 `F6`（可在 *设置 → 键盘和鼠标* 中改键） |

## 设置

所有功能默认开启。如需调整，编辑 `mods/BeetrootSkin/config/config.json`，保存后一秒内生效，游戏中也可以改。配置项名称与 AppleSkin 相同。

| 配置项 | 默认值 | 作用 |
| --- | --- | --- |
| `showFoodValuesInTooltipAlways` | `true` | 设为 `false` 后，只有按住 `Shift` 时才显示食物数值。 |
| `showFoodValuesInTooltip` | `true` | 按住 `Shift` 时在提示框中显示食物数值。 |
| `showFoodValuesHudOverlay` | `true` | 闪烁显示手持食物能恢复的饥饿值和饱和度。 |
| `showFoodValuesHudOverlayWhenOffhand` | `true` | 副手拿着的食物也预览。 |
| `showFoodHealthHudOverlay` | `true` | 闪烁显示手持食物能恢复的生命值。 |
| `showSaturationHudOverlay` | `true` | 饥饿条上的金色饱和度轮廓。 |
| `showFoodExhaustionHudUnderlay` | `true` | 鸡腿后面的消耗度格纹条。 |
| `showVanillaAnimationsOverlay` | `true` | 让叠加层跟着饥饿条一起抖动。 |
| `maxHudOverlayFlashAlpha` | `0.65` | 闪烁预览最亮时的不透明度，`0` 到 `1`。 |
| `showFoodDebugInfo` | `false` | 显示精确数值那一行（即 `F6` 切换的内容）。 |

基岩版的回血规则和 Java 版不同（饥饿值不低于 18 时匀速回血），生命值预览按基岩版规则计算，所以同一种食物的预览可能和 Java 版 AppleSkin 不一样。

## 更多

更多截图和视频：https://levimc.org/mods/github.com/LiteLDev/BeetrootSkin/?tab=gallery

想自己构建，或者基于它开发模组？请看 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

LGPL-3.0 © 2026 Zijian Zhang。HUD 和提示框图标来自 squeek502 的 AppleSkin，以 Unlicense 发布（见 [NOTICE](assets/textures/NOTICE)）。本项目与 Mojang Studios、Microsoft 及 AppleSkin 项目均无关联。
