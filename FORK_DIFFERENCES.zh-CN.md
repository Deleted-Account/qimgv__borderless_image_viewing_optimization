# qimgv KDE Tweak Fork — 与原版的区别

[English](FORK_DIFFERENCES.md)

本文档描述本 fork 与上游 [qimgv](https://github.com/easymodo/qimgv) 在行为和实现上的差异。

- 上游基线：`master` 的 `3127a2d2`（`fix wrong qt version check`），qimgv 1.0.3。
- Fork 分支：`kde-tweaks`。
- Fork 目标：让 qimgv 在 KDE Plasma / XWayland 下更适合作为无边框、按图片尺寸驱动的看图器，提供可预测的窗口尺寸、`1:1` 定位，以及克制的图片信息显示。

本 fork 没有添加自定义标题栏，也没有添加 hover 才出现的最小/最大/关闭按钮。它预期配合 KDE 窗口规则 **No Titlebar and Frame** 使用。窗口移动通过拖动图片实现，关闭仍沿用原有的鼠标中键绑定。

## 1. 总览

| 方面 | 原版 qimgv | 本 fork |
|---|---|---|
| 窗口自动缩放 | 独立的 `Automatic window resize` 和 `Screen area limit` 设置；旧的 `preShowResize()` 路径存在 DPR/装饰层时序问题 | 重写为 `MW::applyAutoResize()`；按 DPR 使用逻辑尺寸；可用区上限固定 100%；开启自动缩放时不再恢复保存几何/最大化状态 |
| Expand 控件 | 复选框 + 较简单的限制 | 纯滑杆 `Expand image`；新增 `alwaysExpand`；半档值存入 `expandScaleX2` |
| Fit UI | 多个 fit 选项和历史设置 | 主界面收敛为一个 `Crop oversized image (1:1)`，映射到 `FIT_ORIGINAL` / `FIT_WINDOW` |
| `1:1` 焦点 | `Top`、`Center`、`At cursor` | `Top Center`、`Top Left`、`Center`；删除 `At cursor`；右键 `1:1` 复用同一设置 |
| 小图片 `1:1` | 可能按焦点锚点出现在角落 | 图片完整放得下时始终居中 |
| 切换图片 | 手动 fit/缩放状态可能跨图片保留 | 每次切图重置 fit 模式和保存的视口位置 |
| 右键 `1:1` 窗口 | 可能直接变成整个可用工作区 | 回到原生 1x，并逐轴 clamp：只截住溢出轴 |
| 左键拖图片 | 根据状态拖出文件或平移 | 普通左键拖移动窗口；`Shift+左键` 保留拖出文件 |
| 平移手势 | 手势有限 | 左右键同时按住可平移，不区分按下顺序；超屏 `1:1` 滚轮平移；`Shift+滚轮` 横向平移 |
| 窗口模式 info | 底部状态栏 / 标题栏 | 新增可复用 info 浮层，支持超时、位置和自定义颜色 |
| 缩放 badge | 左下角且下方有留白，使用主题 overlay 颜色 | 贴角；底部 info 时移到左上角；复用 windowed info 颜色 |
| 右键 fit 按钮 | 菜单保持打开 | 前 4 个尺寸按钮可立即关闭菜单（默认开启） |
| 构建/部署 | 通用 release 构建 | 可选本机 CPU 原生优化构建（AVX2/FMA + LTO）和本地部署脚本 |

## 2. 窗口尺寸与图片 fit 模型

上游提供多个 fit 模式，以及独立的窗口自动缩放设置。本 fork 将日常使用模型收敛为两个概念：

1. **Expand image** 决定期望的图片/窗口尺寸。
2. **`1:1` / crop** 决定当期望尺寸放不下屏幕时的行为。

### 2.1 从 Preferences UI 移除

- `Automatic window resize`
- `Screen area limit for auto resize`

旧的 `autoResizeWindow` 配置键仍被内部读取，但不再出现在 UI 中。`autoResizeLimit` 仍保留 setter/getter，但不再参与新的尺寸计算；fork 使用 `QScreen::availableGeometry()` 的 100%。

### 2.2 Expand 滑杆

`Expand image` 改为纯滑杆：

- `-` 表示不放大；
- `3` 表示 `1.5x`，`4` 表示 `2x`，`5` 表示 `2.5x`，依此类推；
- 滑杆值以半档单位存入 `expandScaleX2`。

新增 `alwaysExpand` 复选框控制是否允许小数倍率：

- **未勾选**：只选择/应用整数倍率；
- **勾选**：允许半档和最终的小数倍率。

设：

- `A` 为可用工作区；
- `I` 为经过 DPR 修正的图片逻辑尺寸；
- `N` 为滑杆 Expansion 倍率。

整数模式：

```text
k = max(1, min(floor(N), floor(A.width / I.width), floor(A.height / I.height)))
desired = I * k
```

小数模式：

```text
k = max(1, min(N, A.width / I.width, A.height / I.height))
desired = I * k
```

### 2.3 Crop oversized image (1:1)

Fit UI 收敛为一个主复选框：

> `Crop oversized image (1:1)`

内部仍映射到 `FIT_ORIGINAL` 与 `FIT_WINDOW`。

勾选时：

- 图片不缩小；
- 溢出部分由 viewport 裁剪；
- 隐藏区域仍可平移查看；
- 定位由 `Focus in 1:1 mode` 决定。

未勾选时：

- 必要时把图片缩小到可用工作区；
- 窗口和图片居中。

### 2.4 窗口定位修复

旧的 `MW::preShowResize()` 被 `MW::applyAutoResize()` 取代。

主要修复：

- 使用 `QPixmap::deviceIndependentSize()`，避免把设备像素当逻辑像素；
- 从 `frameGeometry()` 与 `geometry()` 计算边框尺寸；
- 将 client 矩形居中到可用工作区；
- 窗口尚未显示时先挂起尺寸请求；
- 开启自动缩放时不恢复保存的几何/最大化状态；
- 避免超宽图触发 KWin `_NET_WM_STATE_MAXIMIZED_HORZ` 后锁死窗口宽度；
- 图片、动画和缩放 resize 使用同一条路径。

### 2.5 手动 `1:1` 的窗口尺寸

右键 `1:1` 仍通过 `setFitOriginal()` 和 `nativeFitRequested` 回到原生 1x，但 `MW::resizeForNativeFit()` 现在逐轴 clamp：

```text
windowSize.width  = min(nativeImage.width,  available.width)
windowSize.height = min(nativeImage.height, available.height)
```

因此：

- 超宽图只截宽度，保留原生高度；
- 纵向超长图只截高度，保留原生宽度；
- 窗口不会被强制撑满整个可用工作区。

### 2.6 切图时重置 fit 状态

上游的 `keepFitMode` 会让 `FIT_ORIGINAL` 跨图片保留。本 fork 在每次切图时重置：

- `imageFitMode` 为 `imageFitModeDefault`；
- 保存的视口位置为居中。

这避免了对某张超屏图手动点 `1:1` 后影响下一张图。随后窗口尺寸由正常 auto-resize 路径重新计算。

## 3. `Focus in 1:1 mode`

上游提供 `Top`、`Center`、`At cursor`。本 fork 改为：

| 选项 | 超屏 1:1 定位 |
|---|---|
| `Top Center` | 水平居中；顶边对齐；图片更宽时显示水平中间部分 |
| `Top Left` | 左上角对齐；从图片左上角开始渲染 |
| `Center` | 图片中心对齐 viewport 中心 |

改动点：

- 删除 `At cursor`；
- `Center` 保持配置值 `1`，避免现有配置含义漂移；
- 同一设置同时控制自动 `FIT_ORIGINAL` 渲染和右键 `1:1`；
- 即使未勾选 `Crop oversized image (1:1)` 也生效；
- 图片完整放得下时始终居中，不受焦点选项影响。

## 4. 鼠标交互模型

### 4.1 窗口模式

| 手势 | fork 行为 |
|---|---|
| 普通左键拖动 | 通过 `windowHandle()->startSystemMove()` 移动窗口；KDE 贴边和无边框窗口均可用 |
| `Shift + 左键拖动` | 拖出文件（DnD），保留上游功能 |
| 右键拖动 | 在 viewport 内缩放；缩放会经节流路径同步窗口尺寸 |
| 左右键同时按住 | 平移图片；不区分按下顺序 |
| 鼠标滚轮 | 超屏 `FIT_ORIGINAL` 时纵向平移；到达图片边缘后把事件交回 MW 切图 |
| `Shift + 滚轮` | 横向平移 |
| 鼠标中键 | 关闭窗口，沿用上游绑定 |

### 4.2 全屏模式

- 右键拖动仍为缩放；
- 左键拖动仅在图片超出屏幕时平移；
- `Shift + 左键拖动` 仍为拖出文件；
- 中键仍为关闭。

### 4.3 viewer 几何不变量

fork 使用以原点为中心的 scene：

- scene 原点 = 图片中心 = viewport 中心；
- pixmap item 使用 `pos(0,0)` 和居中 offset；
- scene rectangle 大幅扩大，避免边界/clamp 问题。

这取代了旧的 `10000` 偏移和较小 scene rectangle 的 workaround。

## 5. 窗口模式 info 浮层

上游已有 fullscreen info overlay。本 fork 复用其实现，用第二个 proxy 实例和独立样式支持窗口模式。

### 5.1 行为

- 默认开启；
- 显示图片序号、文件名、分辨率和大小；
- 5 秒后**瞬间隐藏**，不播放渐隐；
- 每次切图重新显示；
- 切图会重启 5 秒计时；
- `Always show windowed info bar` 可关闭超时。

### 5.2 位置选项

下拉框提供 6 个位置：

1. `Top Left`（默认）
2. `Top`
3. `Top Right`
4. `Bottom Left`
5. `Bottom`
6. `Bottom Right`

右侧位置会右对齐文字。所有位置都使用零布局边距，贴合所选边缘。

info 文本左右各加一个不换行空格 `U+00A0`，提供视觉留白但不触发单词换行。

### 5.3 颜色

背景色和文字色均有 `Default` / `Custom` 模式：

- 配置值为空表示 Default；
- 自定义值以 `#AARRGGBB` 保存；
- 背景色对话框启用 alpha 通道选择；
- 默认值为 `rgba(70, 70, 70, 190)` 和 `#e8e8e8`；
- 颜色注入 QSS 占位符 `%windowed_info_bg%` 与 `%windowed_info_text%`。

### 5.4 相关配置键

```ini
windowedInfoBar=true
windowedInfoBarAlways=false
windowedInfoBarPosition=0
windowedInfoBarBackground=
windowedInfoBarText=
```

旧位置索引 `6` / `7` 会迁移到 `Bottom` / `Bottom Right`；已删除的选项回退到 `Top Left`。

## 6. 缩放 badge

缩放百分比 badge 被调整以避免和窗口 info 冲突。

- 默认位置：左下角贴角，无下方留白；
- 当 windowed info 为 `Bottom Left`、`Bottom` 或 `Bottom Right` 时，badge 移到左上角；
- windowed info 关闭、位于顶部，或窗口处于全屏时，badge 保持左下角；
- badge 复用 windowed info 的背景色和文字色。

## 7. 右键菜单 fit 按钮

右键菜单第一排有 6 个图片尺寸按钮，按视觉顺序为：

1. `Fit Window`
2. `Fit Width`
3. `Fit Window Stretch`
4. `1:1 / Original`
5. `Zoom In`
6. `Zoom Out`

新增设置：

```ini
closeContextMenuOnFitChange=true
```

开启时（默认），点击前 4 个按钮会立即关闭右键菜单。`Zoom In`、`Zoom Out` 和菜单中其他所有按钮行为不变。

UI 文案：

> `Close context menu after fit change`

## 8. 构建与部署

fork 添加本地部署脚本 `deploy-system.sh`，安装：

- `qimgv` 到 `/usr/lib/qimgv/qimgv`；
- `player_mpv.so` 到 `/usr/lib64/qimgv/player_mpv.so`；
- 翻译文件到 `/usr/local/share/qimgv/translations`；
- `/usr/bin/qimgv` wrapper，导出 `QT_QPA_PLATFORM=xcb`。

使用 XWayland 是为了在本工作流中可靠地重定位已映射窗口。

本机优化构建命令：

```bash
cmake -S . -B build-native \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_QT5=OFF -DEXIV2=ON -DVIDEO_SUPPORT=ON \
  -DCMAKE_PREFIX_PATH="$PWD/.build-deps/prefix/usr" \
  -DQt6Svg_DIR="$PWD/.build-deps/prefix/usr/lib64/cmake/Qt6Svg" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$PWD/.build-deps/prefix/usr/lib64" \
  -DCMAKE_SHARED_LINKER_FLAGS="-L$PWD/.build-deps/prefix/usr/lib64" \
  -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native -fno-plt" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native -fno-plt" \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

cmake --build build-native -j$(nproc)
sudo ./deploy-system.sh
```

部署出的二进制针对构建机器 CPU 优化（开发机支持 AVX2/FMA）。它不适合直接复制到其他机器。

## 9. 新增文档

- `ARCHITECTURE.md` — 面向新 session / agent 的架构速览。
- `WINDOW_SCALING_CONSENSUS.md` — 窗口尺寸、fit 模式、1:1 定位和平移的行为契约。
- `WINDOW_POSITIONING_NOTES.md` — 窗口定位重写前的设计讨论记录。
- `FORK_DIFFERENCES.md` 和 `FORK_DIFFERENCES.zh-CN.md` — 本文档。

## 10. 非目标与限制

- 没有添加自定义标题栏，也没有 hover 最小/最大/关闭按钮。
- 无边框外观依赖 KDE 的 **No Titlebar and Frame** 窗口规则。
- 原生优化构建与机器绑定，因为使用了 `-march=native`。
- Qt 本身没有重新编译；编译优化只作用于 qimgv 及其插件，不作用于系统 Qt。
- 这是面向 KDE 的个人定制 fork，不是提交给上游的合并提案。
