# qimgv KDE Tweaks

[English](README.md)

面向 KDE 的 [qimgv](https://github.com/easymodo/qimgv) fork，专为无边框 **No Titlebar and Frame** 看图体验调整。

基于上游 `master` 的 `3127a2d2`（qimgv 1.0.3）。个人 fork，与上游项目无关。

## 主要 UX 改进

- **无边框窗口流程**：拖动图片即可移动窗口；中键关闭；不添加自定义标题栏或 hover 按钮。
- **窗口跟随图片**：可预测、DPR 正确的自动缩放；稳定居中；无累积偏移；避免 KWin 水平最大化锁死；每次切图都重置 fit 状态。
- **实用的 1:1**：可选 `Top Center`、`Top Left`、`Center`；小图片始终居中；右键 `1:1` 回到原生 1x，只截住溢出的轴。
- **克制的图片信息**：窗口模式 info 5 秒后自动隐藏，切图后重新出现；位置和颜色可配置；缩放 badge 避开底部 info 并复用其颜色。
- **更快的 fit 切换**：右键菜单前四个尺寸按钮默认点击后关闭菜单。

## 最佳使用方式

给 qimgv 添加 KDE 窗口规则：

```text
No Titlebar and Frame
```

## 文档

- [与原版的完整差异](FORK_DIFFERENCES.zh-CN.md)
- [Full comparison with upstream](FORK_DIFFERENCES.md)
- [架构说明](ARCHITECTURE.md)

## 许可

基于 easymodo 的 qimgv，GPL-3.0。
