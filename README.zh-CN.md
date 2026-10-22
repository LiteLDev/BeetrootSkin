# BeetrootSkin

[English](README.md) | 简体中文

[![standard-readme compliant](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg)](https://github.com/RichardLitt/standard-readme)
[![Build](https://github.com/LiteLDev/BeetrootSkin/actions/workflows/build.yml/badge.svg)](https://github.com/LiteLDev/BeetrootSkin/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/LiteLDev/BeetrootSkin)](https://github.com/LiteLDev/BeetrootSkin/releases)
[![License](https://img.shields.io/github/license/LiteLDev/BeetrootSkin)](COPYING.LESSER)

为 Minecraft 基岩版提供 AppleSkin 风格的食物与饥饿信息，是一个 LeviLamina 客户端模组。

BeetrootSkin 把 Java 版 [AppleSkin](https://github.com/squeek502/AppleSkin) 的功能带到了基岩版：在食物的提示框中显示它能恢复的饥饿值和饱和度；手持食物时预览吃下后获得的饥饿值、饱和度和生命值；并在 HUD 上显示原本隐藏的饱和度和消耗度。模组完全运行在客户端，服务器无需安装任何东西。

## 目录

- [背景](#背景)
- [功能](#功能)
- [安装](#安装)
- [使用](#使用)
- [配置](#配置)
- [API](#api)
- [构建](#构建)
- [维护者](#维护者)
- [参与贡献](#参与贡献)
- [许可证](#许可证)

## 背景

Minecraft 记录的饥饿信息远比 HUD 上的十个鸡腿要多：饱和度会先于饥饿值被消耗，决定了满饥饿条能维持多久；每个动作都会积累消耗度，攒满后扣除一点饱和度或饥饿值。而食物的提示框里根本看不出一种食物值多少。在 Java 版上，[squeek502](https://github.com/squeek502) 的 [AppleSkin](https://github.com/squeek502/AppleSkin) 多年来一直让这些信息一目了然。BeetrootSkin 将它移植到基岩版，基于支持 Windows 基岩版客户端的 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 构建。

HUD 和提示框所用的贴图（`assets/textures/icons.png` 和 `tooltip_hunger_outline.png`）来自 AppleSkin 本身，由 squeek502 以 [Unlicense](https://unlicense.org) 发布（见 [assets/textures/NOTICE](assets/textures/NOTICE)）。感谢 squeek502 创作了 AppleSkin 并将其美术资源贡献到公有领域。代码则是为基岩版重新编写的。

## 功能

AppleSkin 的所有功能均已提供，配置项名称也与之相同：

- **提示框中的食物数值**：在任意物品栏中悬停食物，即可看到它恢复的饥饿值（鸡腿）和饱和度，使用 AppleSkin 的图标绘制。腐肉、生鸡肉等带有负面效果的食物会使用"坏食物"图标。默认始终显示，也可设为仅在按住 `Shift` 时显示。
- **饥饿值与饱和度恢复预览**：主手（或副手）持有食物时，饥饿条会闪烁显示吃下后增加的鸡腿和饱和度。
- **饱和度叠加层**：当前饱和度以金色轮廓绘制在饥饿条上。
- **消耗度底层**：饥饿条下方有一条进度条，随着消耗度积累而填充，满时将扣除一点饱和度或饥饿值。
- **生命值恢复预览**：手持食物时，心形图标会闪烁显示吃下后能恢复的生命值，包括由新的饥饿值和饱和度带来的自然回复，以及金苹果等食物的生命恢复效果。
- **调试信息**：HUD 上的一行文字，显示精确的饥饿值、饱和度和消耗度。
- **原版动画**：叠加层会跟随原版饥饿条的抖动，始终对齐对应的图标。

基岩版特有说明：

- **没有 F3 界面。** 基岩版没有 Java 版的 F3 调试界面，因此调试信息改为 HUD 上单独的一行。在游戏中按 **`F6`** 显示或隐藏（保存为 `showFoodDebugInfo`）。
- **基岩版的回血机制。** 基岩版在饥饿值不低于 18 时以固定速度回血，没有 Java 版那种由饱和度加速的快速回血。生命值预览遵循基岩版的规则，因此同一种食物的预览可能与 Java 版 AppleSkin 不同。获得的饱和度按基岩版公式计算：`营养值 × 饱和度系数 × 2`。
- **仅客户端。** 服务器无需安装任何东西。在单人游戏和由你主持的局域网世界中，数值直接取自内置服务器，完全精确；在专用服务器和加入的世界中，BeetrootSkin 使用服务器同步给客户端的饥饿值、饱和度和消耗度属性，可能略微滞后于服务器上的实际值。

## 安装

前置要求：

- Windows（x64）版 Minecraft 基岩版 **1.26.51.01**。
- LeviLamina **26.51.x** 客户端，例如通过 [LeviLauncher](https://github.com/LiteLDev/LeviLauncher) 安装。

然后用以下任一方式把 BeetrootSkin 安装到该游戏实例中。

使用 [lip](https://github.com/futrime/lip)：

```sh
lip install github.com/LiteLDev/BeetrootSkin#client
```

手动安装：从[最新发布](https://github.com/LiteLDev/BeetrootSkin/releases/latest)下载 `beetrootskin-client-windows-x64.zip` 并解压，使 `BeetrootSkin/` 位于游戏的 `mods/` 文件夹中，与 `LeviLamina/` 并列。

## 使用

无需任何设置。进入生存或冒险模式的世界：

- 在物品栏中悬停食物，查看提示框中的数值；
- 手持食物，预览它恢复的饥饿值、饱和度和生命值；
- 游戏过程中留意饥饿条上的饱和度轮廓和消耗度进度条；
- 按 `F6` 显示或隐藏调试信息。

## 配置

设置保存在 `mods/BeetrootSkin/config/config.json`。首次启动时会以默认值创建该文件。可随时编辑，游戏运行中也可以：保存文件后约一秒内生效，无需重启（日志中会出现 `Config reloaded: ...` 及变更的配置项）。如果文件不是有效的 JSON，将保留当前设置并记录一条警告，文件保持原样等待你修正。配置项名称与 AppleSkin 相同，其文档同样适用。

| 键 | 默认值 | 含义 |
| --- | --- | --- |
| `showFoodValuesInTooltip` | `true` | 按住 `Shift` 时在食物提示框中显示饥饿值和饱和度图标。 |
| `showFoodValuesInTooltipAlways` | `true` | 不按 `Shift` 也显示（与 AppleSkin 相同，此项可单独生效）。 |
| `showSaturationHudOverlay` | `true` | 在饥饿条上以轮廓绘制当前饱和度。 |
| `showFoodValuesHudOverlay` | `true` | 在饥饿条上闪烁显示手持食物将恢复的饥饿值和饱和度。 |
| `showFoodValuesHudOverlayWhenOffhand` | `true` | 副手持有的食物也显示预览。 |
| `showFoodExhaustionHudUnderlay` | `true` | 在饥饿条下方绘制消耗度进度条。 |
| `showFoodHealthHudOverlay` | `true` | 在心形图标上闪烁显示手持食物将恢复的生命值。 |
| `showVanillaAnimationsOverlay` | `true` | 让叠加层跟随原版饥饿条的抖动。 |
| `maxHudOverlayFlashAlpha` | `0.65` | 闪烁预览的最大不透明度，范围 `0`（不可见）到 `1`，超出范围的值会被截断。 |
| `showFoodDebugInfo` | `false` | 显示包含精确饥饿值、饱和度和消耗度的调试信息。游戏中按 `F6` 切换。 |
| `version` | `1` | 配置格式版本，由模组管理，请勿修改。 |

缺失的键使用默认值；当前版本不认识的键会在保存时原样保留。如果模组自行保存文件（例如按下 `F6` 后）时，文件中有模组无法读取的内容，该内容会先被复制到 `config.json.bak`。

## API

其他 LeviLamina 模组可以通过 [AppleSkin API](https://github.com/squeek502/AppleSkin/tree/1.21.11-neoforge/java/squeek/appleskin/api) 的 C++ 移植与 BeetrootSkin 集成。这些事件是 `ll::event::EventBus` 上的 LeviLamina 事件，在客户端线程上发布：

| 事件 | 时机 | 取消后 |
| --- | --- | --- |
| `beetroot::api::FoodValuesEvent` | BeetrootSkin 确定某个物品的食物数值时（提示框，以及所有 HUD 预览所用的手持食物）。修改 `modifiedFoodValues()`（或 `defaultFoodValues()`）后，所有显示都使用修改后的数值，提示框会像 AppleSkin 一样用描边区分默认值与修改值。 | （不可取消） |
| `beetroot::api::TooltipOverlayEvent::Pre` | 即将向提示框加入食物数值。 | 该物品不显示食物数值 |
| `beetroot::api::TooltipOverlayEvent::Render` | 即将绘制食物数值（可修改 `x()`/`y()`，可用 `renderContext()` 自行绘制）。 | 保留空间但不绘制 |
| `beetroot::api::HUDOverlayEvent::Exhaustion` | 绘制消耗度底条（`exhaustion()`）。 | 隐藏消耗度底条 |
| `beetroot::api::HUDOverlayEvent::Saturation` | 绘制饱和度叠加层（`saturationLevel()`）。 | 隐藏饱和度叠加层及手持食物的饱和度增量 |
| `beetroot::api::HUDOverlayEvent::HungerRestored` | 绘制手持食物的饥饿值预览（`currentFoodLevel()`、`itemStack()`、`foodValues()`）。 | 隐藏饥饿值预览 |
| `beetroot::api::HUDOverlayEvent::HealthRestored` | 绘制手持食物的生命值预览（`modifiedHealth()`、`itemStack()`、`foodValues()`）。 | 隐藏生命值预览 |

HUD 事件带有叠加层的位置（GUI 单位，`x()`/`y()`，修改即可移动）：饥饿条的右边缘和顶部，或者心形条的左边缘和最下一行的顶部。`beetroot::api::FoodValues` 包含 `hunger`、`saturation`（获得的饱和度）和 `canAlwaysEat`。另外导出了两个函数：`apiVersion()` 和 `queryFoodValues(stack, player)`（物品的默认值与修改值，会触发 `FoodValuesEvent`）。

使用方法：

1. 获取 SDK：从发布页下载 `beetrootskin-sdk-client-windows-x64.zip`，或者自行构建 BeetrootSkin，SDK 会写到 `bin/BeetrootSkin-sdk/`。其中包含 `include/beetrootskin/api/*.h` 和 `lib/BeetrootSkin.lib`。
2. 把 SDK 的 `include` 加入包含路径，并链接 `BeetrootSkin.lib`（这些头文件也需要 LeviLamina 的头文件）。
3. 在你的 `manifest.json` 中声明依赖，使 BeetrootSkin 先于你的模组加载（并注册事件）：`"dependencies": [{ "name": "BeetrootSkin" }]`。
4. 在模组的 `enable()` 中订阅：

```cpp
#include "beetrootskin/api/Events.h"

#include "ll/api/event/EventBus.h"
#include "mc/world/item/ItemStackBase.h"

// In your mod's enable():
auto& bus = ll::event::EventBus::getInstance();
mListeners.push_back(bus.emplaceListener<beetroot::api::FoodValuesEvent>([](beetroot::api::FoodValuesEvent& ev) {
    if (ev.itemStack().getTypeName() == "minecraft:bread") ev.modifiedFoodValues().hunger *= 2;
}));
mListeners.push_back(bus.emplaceListener<beetroot::api::HUDOverlayEvent::Exhaustion>(
    [](beetroot::api::HUDOverlayEvent::Exhaustion& ev) { ev.cancel(); }
));
mListeners.push_back(bus.emplaceListener<beetroot::api::TooltipOverlayEvent::Pre>(
    [](beetroot::api::TooltipOverlayEvent::Pre& ev) {
        if (ev.itemStack().getTypeName() == "minecraft:apple") ev.cancel();
    }
));
// In disable(): bus.removeListener(l) for each of them.
```

[samples/api_sample](samples/api_sample/ApiSample.cpp) 是一个完整的示例模组，正是这样做的；它通过 `xmake f --api_sample=y` 构建，供端到端场景 `api` 使用。

## 构建

需要 Windows x64、[xmake](https://xmake.io)、带 MSVC 14.4x 或更新版本的 Visual Studio Build Tools，以及 LLVM（`clang-cl`）。

```powershell
git config --global core.longpaths true
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y                                                  # 生成 bin/BeetrootSkin/
xmake build -y beetroot-tests; xmake run beetroot-tests   # 单元测试
```

模组版本号取自 `git describe`，因此请在至少有一次提交的 git 克隆中构建。[AGENTS.md](AGENTS.md)（英文）介绍了架构、约定以及与基岩版内部交互的规则；[tools/e2e](tools/e2e/README.md) 是驱动真实游戏的端到端测试工具。

## 维护者

[@futrime (Zijian Zhang)](https://github.com/futrime)

## 参与贡献

欢迎提交 PR 和 issue。请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 并遵守[行为准则](CODE_OF_CONDUCT.md)。安全问题请按 [SECURITY.md](SECURITY.md) 中的说明报告。

## 许可证

LGPL-3.0 © 2026 Zijian Zhang

BeetrootSkin 是自由软件：你可以依据 GNU 宽通用公共许可证第 3 版（与 LeviLamina 相同的许可证）的条款重新分发和/或修改它。参见 [COPYING.LESSER](COPYING.LESSER) 和 [COPYING](COPYING)。

`assets/textures/` 中的贴图来自 squeek502 的 AppleSkin，以 Unlicense 发布。参见 [assets/textures/NOTICE](assets/textures/NOTICE)。

BeetrootSkin 与 Mojang Studios、Microsoft 及 AppleSkin 项目均无关联。
