# qimgv KDE Tweaks

[中文说明](README.zh-CN.md)

A modified version of [qimgv](https://github.com/easymodo/qimgv) tailored for KDE.
I describe it as "tailored for KDE" because I developed and use it within the KDE desktop environment;
KDE makes it easy to enable the "No Titlebar and Frame" setting, allowing qimgv to run completely free of title bars and window borders.

While users on other Linux desktop environments can certainly benefit from the changes in this fork, the best possible experience is achieved by enabling "borderless window" mode—specific to your chosen desktop environment for qimgv.

**This fork is highly optimized for a "No Titlebar and Frame" setup within KDE,
making qimgv an exceptionally good (arguably the best) minimalist, borderless image viewer for Linux.**

✔**本 fork 极致优化了 KDE 桌面对 qimgv 默认开启 "No Titlebar and Frame" 的使用体验，
让 qimgv 成为 Linux 下非常好用（可能是最好用）的无边框窗口化极简主义图片浏览器。**

Upstream base: `master` at `3127a2d2` (qimgv 1.0.3).
Fork branch: `kde-tweaks`.

## What this fork does compared with upstream

1. **Floating info for borderless KDE viewing**
   - Upstream qimgv shows image information only in the title bar. With "No Titlebar and Frame" enabled, that information disappears. This fork adds a configurable, minimalist floating info overlay that hides automatically.
   - The info overlay shows the image index, filename, resolution, and size. It reappears when switching images and hides automatically after 5 seconds.
   - Its position and colors can be adjusted in Preferences → View, with alpha-channel support.
     It can also be set to stay visible. The zoom badge was also adjusted and automatically moves to the other side depending on the info position.

2. **Left-drag the image to move the borderless window**
   - With **No Titlebar and Frame** enabled, there is no title bar to grab. KDE's default `Meta + left drag` works, but it is a bit cumbersome.
     This fork lets you move the window by left-dragging the image directly in windowed mode.

3. **Reworked image switching and oversized-image layout**
   - Upstream: after switching images, where the new image opens depends on the existing state. This fork centers the window on every image switch.
   - For images larger than the available area, enable `Crop oversized image (1:1)` and choose the render starting point from `Top Center`, `Top Left`, or `Center`.
   - The right-click `1:1` action uses the same positioning logic as `Crop oversized image (1:1)`;
     even when Crop is unchecked, right-click `1:1` returns to native 1x and follows that preset.

4. **More control over Expand image and better screen use**
   - With `always expand` off, images are always enlarged by integer multiples. If a small image cannot be enlarged to the configured multiple,
     it falls back to the largest integer multiple that fits.
   - With `always expand` on, for example at 5x, images larger than one fifth of the available screen area use non-integer scaling.
   - Selecting `-` on the slider disables Expand, as in upstream.

5. **Adjustments to the user experience for the right-click menu and its top-row buttons**
   - Added functionality so that clicking any of the four buttons at the top of the right-click menu—excluding the `+` and `-` buttons—will immediately close the right-click menu.

6. **Native Wayland support and related bug fixes**
   - Runs directly as a native Wayland client, no longer depending on XWayland.
   - Fixed the issues exposed by the move to Wayland: thin dark seams at the top/left edges under fractional scaling (e.g. 150%), wrong offset and scale on the first image of a cold start, and cumulative zoom drift when switching images in fullscreen.
   - Wayland does not let apps position their own windows, so a companion KWin script `qimgv-centering` centers the window; outside KDE / KWin it has no effect and everything else works as usual.
   - On non-KDE desktops, deploy and run in XWayland mode to keep optimizations such as centering the window after switching images.

## Recommended setup for the best experience

1. Enable **No Titlebar and Frame** for qimgv via a KDE window rule.
2. In qimgv, under Preferences → View:
   - enable `Crop oversized image (1:1)`
   - choose `Top Center`
   - turn `always expand` on or off according to preference

## Native Wayland mode

Native Wayland is the default. Build the repo first (binaries are taken from `build-native/`), then from the repo root:

```bash
sudo ./deploy-system.sh
```

- Installs the binary system-wide and creates the `/usr/bin/qimgv` launcher (no `QT_QPA_PLATFORM`, so it runs as a native Wayland client).
- Also installs and enables the KWin script `qimgv-centering` (writes `kwinrc` and asks KWin to reload), which centers the qimgv window.
- `SRC` at the top of the script is an absolute repo path; change it to yours on another machine.
- Centering requires KDE Plasma 6 / KWin 6. Other desktops can still run qimgv natively on Wayland, just without the centering script.

### Switching / reverting to XWayland

```bash
sudo ./deploy-system.sh --xwayland
```

- The launcher gets `QT_QPA_PLATFORM=xcb` and runs through XWayland.
- Disables `qimgv-centering` (not needed for window positioning under XWayland).

Restart qimgv after switching modes. You can also do it manually without the script: enable/disable `qimgv-centering` under System Settings → Window Management → KWin Scripts, and add `QT_QPA_PLATFORM=xcb` to the launcher.

## Docs

- [Full comparison with upstream](FORK_DIFFERENCES.md)
- [与原版的完整差异](FORK_DIFFERENCES.zh-CN.md)
- [Architecture](ARCHITECTURE.md)
- [Push policy](PUSH_POLICY.md)

## License

Based on qimgv by easymodo. GPL-3.0.
