# qimgv KDE Tweaks

[中文说明](README.zh-CN.md)

A KDE-oriented fork of [qimgv](https://github.com/easymodo/qimgv), tuned for borderless **No Titlebar and Frame** image viewing.

Based on upstream `master` at `3127a2d2` (qimgv 1.0.3). Personal fork, not affiliated with upstream.

## Major UX improvements

- **Borderless window workflow**: drag the image to move the window; middle click closes. No custom titlebar or hover buttons.
- **Window follows the image**: predictable, DPR-correct auto-resize; centered placement; no accumulated offset; no KWin horizontal-maximize lock; fresh fit state on every image switch.
- **Practical 1:1**: choose `Top Center`, `Top Left`, or `Center`; small images always centered; right-click `1:1` uses native 1x and only caps the overflowing axis.
- **Quiet image info**: windowed info auto-hides after 5 seconds and returns on image change; position and colors are configurable; zoom badge moves away from bottom info and shares its colors.
- **Faster fit switching**: the first four right-click size buttons close the menu by default.

## Best experience with

Apply this KDE window rule to qimgv:

```text
No Titlebar and Frame
```

## Docs

- [Full comparison with upstream](FORK_DIFFERENCES.md)
- [与原版的完整差异](FORK_DIFFERENCES.zh-CN.md)
- [Architecture](ARCHITECTURE.md)

## License

Based on qimgv by easymodo. GPL-3.0.
