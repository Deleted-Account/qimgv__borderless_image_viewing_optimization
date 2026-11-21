# qimgv KDE Tweaks

[English](README.md)

面向 KDE 的 [qimgv](https://github.com/easymodo/qimgv) 修改版。
说面向 KDE，是因为我在 KDE 桌面环境下修改和使用，
KDE 能轻松开启 "No Titlebar and Frame" 让 qimgv 彻底无标题栏无边框。

其他 Linux 桌面应该也能体验到这个 fork 的修改便利，极致体验还是推荐对 qimgv 开启对应桌面环境的无边框窗口化。

✔**本 fork 极致优化了 KDE 桌面对 qimgv 默认开启 "No Titlebar and Frame" 的使用体验，
让 qimgv 成为 Linux 下非常好用（可能是最好用）的无边框窗口化极简主义图片浏览器。**

上游基线：`master` 的 `3127a2d2`（qimgv 1.0.3）。
Fork 分支：`kde-tweaks`。

## 和原版相比，这个 fork 做了什么

1. **为 KDE 无边框看图加入浮动 info**
   - 原版 qimgv 图片信息只可显示在标题栏。开启 "No Titlebar and Frame" 后图片信息不可见，
     本 fork 加入了可设置的自动隐藏极简浮动 info。
   - info 显示图片序号、文件名、分辨率和大小，切图后重新出现，5 秒后自动隐藏。
   - 可在 Preferences → View 中调整 info 的位置和颜色，支持透明度通道。
     info 可设置为一直显示。缩放 badge 也做了调整，会自动根据 info 位置移动到另一侧

2. **无边框窗口下，左键拖动图片即可移动窗口**
   - 开启 **No Titlebar and Frame** 后没有标题栏可移动窗口，虽然可通过 KDE 默认的 `Meta + 左键拖动` 移动，
     操作略显繁琐。本 fork 在窗口模式下可直接左键拖动图片移动窗口。

3. **重新整理切图后的图片位置与超屏图片布局**
   - 原版切换图片后，新图片的打开位置由既有状态决定；本 fork 改为每次切图都把窗口居中。
   - 对于超出可用区的图片，可以开启 `Crop oversized image (1:1)`，
     并从 `Top Center`、`Top Left`、`Center` 三种预设中选择渲染起点。
   - 右键菜单的 `1:1` 与 `Crop oversized image (1:1)` 使用同一套定位逻辑；
     即使没勾选 Crop，右键 `1:1` 仍会回到原生 1x 并按该预设定位。

4. **让 Expand image 更可控、更充分利用屏幕**
   - 关闭 `always expand`：始终按整数倍放大图片，小图若不能放大到目标倍数，
     则回退到可容纳的整数倍率。
   - 开启 `always expand`：比如开启 5x，对于大于屏幕可用区域 1/5 的图片，会开启非整数倍缩放。
   - 滑块选 `-` 则是关闭 Expand 功能，与原版一致。

## 个人推荐最佳体验使用设置

1. KDE 窗口规则给 qimgv 开启 **No Titlebar and Frame**。
2. qimgv 在 Preferences → View 中：
   - 开启 `Crop oversized image (1:1)`
   - 选择 `Top Center`
   - `always expand` 按个人喜好开启或关闭

## 文档

- [与原版的完整差异](FORK_DIFFERENCES.zh-CN.md)
- [Full comparison with upstream](FORK_DIFFERENCES.md)
- [架构说明](ARCHITECTURE.md)
- [推送约束](PUSH_POLICY.md)

## 许可

基于 easymodo 的 qimgv，GPL-3.0。
