# qimgv KDE Tweak Fork — Differences from Upstream

[中文版](FORK_DIFFERENCES.zh-CN.md)

This document describes the behavioral and implementation differences between this fork and upstream [qimgv](https://github.com/easymodo/qimgv).

- Upstream base: `master` at `3127a2d2` (`fix wrong qt version check`), qimgv 1.0.3.
- Fork branch: `kde-tweaks`.
- Fork goal: make qimgv comfortable as a borderless, image-driven viewer under KDE Plasma / XWayland, with predictable window sizing, predictable `1:1` placement, and unobtrusive image information.

The fork adds no custom titlebar or window buttons. It is designed to be used with a KDE window rule such as **No Titlebar and Frame** for qimgv. Window movement is handled by dragging the image itself, and closing remains available via the existing middle-click binding. A windowed floating info overlay replaces the file information normally shown by the title bar / status bar.

## 1. Executive summary

| Area | Upstream qimgv | This fork |
|---|---|---|
| Window auto-resize | Separate `Automatic window resize` and `Screen area limit` settings; older `preShowResize()` path with DPR/decoration timing issues | Reworked `MW::applyAutoResize()`; DPR-correct logical sizes; available work area capped at 100%; saved geometry/maximized state skipped while auto-resize is enabled |
| Expand control | Checkbox + simpler limit | Slider-only `Expand image`, new `alwaysExpand` mode, half-step stops stored as `expandScaleX2` |
| Fit UI | Several fit options / legacy settings | One main checkbox: `Crop oversized image (1:1)`, mapping to `FIT_ORIGINAL` vs `FIT_WINDOW` |
| `1:1` focus | `Top`, `Center`, `At cursor` | `Top Center`, `Top Left`, `Center`; `At cursor` removed; same setting applies to right-click `1:1` |
| Small images in `1:1` | Could follow focus anchor and appear in a corner | Always centered when the whole image fits |
| Image switching | Manual fit/zoom state can persist across images | Fit mode and saved viewport position reset to defaults on every image switch |
| Right-click `1:1` window | Could resize to the full work area | Native 1x, per-axis clamp: only the overflowing axis is capped |
| Left drag on image | Drag out file / pan depending on state | Plain left drag moves the window; `Shift+left` keeps drag-out |
| Panning | Limited gestures | Both-button pan in either press order; wheel pan in oversized `1:1`; `Shift+wheel` horizontal pan |
| Windowed info | Bottom status bar / title bar | New reusable info overlay with timeout, positions and custom colors |
| Zoom badge | Bottom-left with bottom margin; theme overlay colors | Flush to corner; moves to top-left when windowed info is at the bottom; uses windowed info colors |
| Context-menu fit buttons | Menu stays open | First four fit buttons can close the menu immediately (default on) |
| Build/deploy | Generic release build | Optional native CPU build with AVX2/FMA-targeted code and LTO; local deployment helper |
| Archives | Not supported | Open zip/cbz/7z/cb7/rar/cbr/tar directly and browse images inside, read-only |

## 2. Window sizing and image fit model

Upstream exposes multiple fit modes and separate window auto-resize controls. The fork consolidates the daily-use model around two ideas:

1. **Expand image** controls the desired image/window size.
2. **`1:1` / crop** controls what happens when that desired size does not fit the screen.

### 2.1 Removed from the Preferences UI

- `Automatic window resize`
- `Screen area limit for auto resize`

The legacy `autoResizeWindow` config key is still honored internally, but it is no longer shown in the UI. `autoResizeLimit` remains as a legacy setter/getter but no longer controls the new sizing path; the fork uses 100% of `QScreen::availableGeometry()`.

### 2.2 Expand slider

The fork makes `Expand image` slider-only:

- `-` means no expansion.
- `3` means `1.5x`, `4` means `2x`, `5` means `2.5x`, and so on.
- The slider value is stored in `expandScaleX2` in half-step units.

A new `alwaysExpand` checkbox controls whether fractional expansion is allowed:

- **unchecked**: only integer multiples are selected/applied.
- **checked**: half-stops and fractional final scales are allowed.

Let:

- `A` be the available work area.
- `I` be the image logical size, corrected by DPR.
- `N` be the slider expansion factor.

Integer-only mode:

```text
k = max(1, min(floor(N), floor(A.width / I.width), floor(A.height / I.height)))
desired = I * k
```

Fractional mode:

```text
k = max(1, min(N, A.width / I.width, A.height / I.height))
desired = I * k
```

### 2.3 Crop oversized image (1:1)

The fit UI is reduced to one main checkbox:

> `Crop oversized image (1:1)`

Internally this still maps to `FIT_ORIGINAL` versus `FIT_WINDOW`.

When enabled:

- the image is not shrunk to fit;
- overflow is clipped by the viewport;
- hidden areas remain reachable by panning;
- placement is determined by `Focus in 1:1 mode`.

When disabled:

- the image is shrunk to fit the available work area if needed;
- the window and image are centered.

### 2.4 Window placement fixes

The old `MW::preShowResize()` path was replaced by `MW::applyAutoResize()`.

Notable fixes:

- uses `QPixmap::deviceIndependentSize()` instead of raw device pixels;
- computes frame margins from `frameGeometry()` and `geometry()`;
- centers the computed client rectangle in the available work area;
- handles pending resize before the window is visible;
- does not restore saved geometry or maximized state when auto-resize is enabled;
- avoids the KWin `_NET_WM_STATE_MAXIMIZED_HORZ` lock that previously froze the window width after an ultra-wide image;
- applies the same resize path to image switch, animation switch, and throttled zoom resizes.

### 2.5 Manual `1:1` window sizing

Right-click `1:1` still resets rendering to native 1x via `setFitOriginal()` and `nativeFitRequested`, but `MW::resizeForNativeFit()` now clamps each axis independently:

```text
windowSize.width  = min(nativeImage.width,  available.width)
windowSize.height = min(nativeImage.height, available.height)
```

This means:

- an ultra-wide image is capped in width but keeps its native height;
- a tall image is capped in height but keeps its native width;
- the window is no longer forced to fill the whole work area.

### 2.6 Fresh fit state on image switch

Upstream's `keepFitMode` can carry `FIT_ORIGINAL` across images. In the fork, every image switch resets:

- `imageFitMode` to `imageFitModeDefault`;
- saved viewport position to center.

This prevents a manual `1:1` action on one oversized image from affecting the next image. Window geometry is then recomputed by the normal auto-resize path.

## 3. `Focus in 1:1 mode`

Upstream offers `Top`, `Center`, and `At cursor`. The fork replaces these with:

| Option | Oversized 1:1 placement |
|---|---|
| `Top Center` | horizontally centered; top edge aligned; middle horizontal slice shown when wider |
| `Top Left` | top-left aligned; image starts from its upper-left corner |
| `Center` | image center aligned to viewport center |

Changes:

- `At cursor` was removed.
- `Center` keeps config value `1`, so existing configurations do not silently change meaning.
- The setting controls both automatic `FIT_ORIGINAL` rendering and the right-click `1:1` action.
- It applies even when `Crop oversized image (1:1)` is unchecked.
- Small images that fully fit the viewport are always centered, regardless of the focus option.

## 4. Mouse interaction model

### 4.1 Windowed mode

| Gesture | Fork behavior |
|---|---|
| Plain left drag | Move the window via `windowHandle()->startSystemMove()`; KDE snapping and borderless windows work |
| `Shift + left drag` | Drag out the file (DnD), preserving upstream functionality |
| Right drag | Zoom inside the viewport; throttle-driven window resize follows zoom |
| Left + right held together | Pan the image; both press orders are equivalent |
| Mouse wheel | In oversized `FIT_ORIGINAL`, pan vertically; at the image edge, pass the event on so MW changes image |
| `Shift + wheel` | Pan horizontally |
| Middle click | Close, using the existing upstream binding |

### 4.2 Fullscreen mode

- right drag still zooms;
- left drag pans only when the image exceeds the screen;
- `Shift + left drag` still drags out the file;
- middle click still closes.

### 4.3 Viewer geometry invariant

The fork uses a scene centered on the origin:

- scene origin = image center = viewport center;
- pixmap items use `pos(0,0)` and a centered offset;
- the scene rectangle was widened substantially to avoid edge/clamping artifacts.

This removes older workarounds based on `10000` offsets and a smaller scene rectangle.

## 5. Windowed info overlay

Upstream already has a fullscreen info overlay. The fork reuses that implementation for windowed mode with a second proxy instance and a distinct style.

### 5.1 Behavior

- Enabled by default.
- Shows file position, filename, resolution, and size.
- Hides after 5 seconds with an immediate, non-animated hide.
- Reappears on every image change.
- Re-starting the 5-second timer occurs on image change.
- `Always show windowed info bar` disables the timeout.

### 5.2 Position options

The dropdown offers six positions:

1. `Top Left` — default
2. `Top`
3. `Top Right`
4. `Bottom Left`
5. `Bottom`
6. `Bottom Right`

Right-side positions right-align the text. All positions use zero layout margin, so the overlay touches the chosen edge.

A non-breaking space (`U+00A0`) is added at both ends of the info text, providing visual padding without introducing word wrap.

### 5.3 Colors

Both background and text color have `Default` / `Custom` modes:

- empty config value means Default;
- custom values are stored as `#AARRGGBB`;
- the background color dialog enables alpha-channel selection;
- defaults are `rgba(70, 70, 70, 190)` and `#e8e8e8`;
- the same colors are injected into the QSS placeholders `%windowed_info_bg%` and `%windowed_info_text%`.

### 5.4 Related config keys

```ini
windowedInfoBar=true
windowedInfoBarAlways=false
windowedInfoBarPosition=0
windowedInfoBarBackground=
windowedInfoBarText=
```

Legacy position indices `6` / `7` are migrated to `Bottom` / `Bottom Right`; removed options fall back to `Top Left`.

## 6. Zoom badge

The zoom percentage badge was adjusted to avoid colliding with the windowed info overlay.

- Default position: bottom-left, flush with no bottom margin.
- If windowed info is set to `Bottom Left`, `Bottom`, or `Bottom Right`, the badge moves to the top-left corner.
- If windowed info is disabled, at the top, or the window is fullscreen, the badge stays bottom-left.
- The badge uses the same background and text colors as windowed info.

## 7. Context menu fit buttons

The first row of the right-click menu contains six image-size buttons. In visual order:

1. `Fit Window`
2. `Fit Width`
3. `Fit Window Stretch`
4. `1:1 / Original`
5. `Zoom In`
6. `Zoom Out`

A new setting is added:

```ini
closeContextMenuOnFitChange=true
```

When enabled (default), clicking any of the first four buttons closes the context menu immediately. `Zoom In`, `Zoom Out`, and all other menu entries keep their original behavior.

The UI label is:

> `Close context menu after fit change`

## 8. Build and deployment

The fork adds a local deployment helper, `deploy-system.sh`, which installs:

- `qimgv` to `/usr/lib/qimgv/qimgv`;
- `player_mpv.so` to `/usr/lib64/qimgv/player_mpv.so`;
- translations under `/usr/local/share/qimgv/translations`;
- `/usr/bin/qimgv` as a wrapper (native Wayland by default; `--xwayland` makes it export `QT_QPA_PLATFORM=xcb`);
- the KWin script `qimgv-centering` to `/usr/share/kwin/scripts/qimgv-centering`.

Native Wayland is the default. Wayland clients cannot move their own toplevels, so window centering is done by the KWin script, which re-centers the qimgv window after every image switch. `sudo ./deploy-system.sh --xwayland` installs the older XWayland wrapper and disables the script.

A native optimized build can be created with:

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

The deployed binary is tuned for the build machine's CPU (AVX2/FMA on the development host). It is not intended to be copied to unrelated machines.

## 9. Viewing images inside archives

Upstream only browses image files on disk. This fork can open an archive as an image source.

### 9.1 Scope

- Extensions: `zip`, `cbz`, `7z`, `cb7`, `rar`, `cbr`, `tar`.
- Backed by libarchive. Enabled only when libarchive is found at configure time, exposed as `USE_ARCHIVE`; the feature degrades safely otherwise.
- Only images are indexed: entries are filtered by the image suffixes Qt supports, so videos inside the archive (e.g. `mp4`) never appear in the list.

### 9.2 Virtual paths

Inner files use a virtual path:

```text
/absolute/path/archive.cbz::sub/inner.png
```

- `::` is the separator;
- the path doubles as `FSEntry.path`, cache key, `state.currentFilePath`, and `DocumentInfo::filePath()`;
- sorting and uniqueness use the full inner path; the UI shows only the basename.

### 9.3 UI behavior

- Opening an archive enters document view and browses with `[i/n]`;
- windowed info shows only the inner basename, index and uncompressed size — never the archive name;
- leaving the archive reuses `goUp` (parent / Backspace) and returns to the archive's directory.

### 9.4 Read-only policy

Only **file-level writes** are blocked:

- blocked: save, save as, delete, move to trash, rename, copy, move, drag-out (including `Shift` drag-out), `cropAndSave`;
- allowed but never persisted: crop, rotate, flip, resize (preview only);
- allowed: set as wallpaper, copy image to clipboard.

### 9.5 Passwords

- encrypted archives prompt for a password, up to 3 attempts;
- cancel or repeated failure exits with an error;
- the password is kept in memory only, never written to config or disk;
- archives using an undecryptable scheme report "encryption method not supported".

Tested support matrix (libarchive 3.8.7):

| Format / encryption | Password support |
|---|---|
| zip / cbz, ZipCrypto | yes |
| zip / cbz, AES-128/256 | yes |
| 7z / cb7, data encryption | no |
| 7z / cb7, header encryption (`-mhe=on`) | no |
| rar, RAR4 data/header encryption | no |
| rar5, encrypted | untested (theoretically supported by libarchive) |
| tar | no encryption concept |

Only the zip family can actually be unlocked with a password. For 7z and RAR4 the limitation is in libarchive, not in this fork.

No external-extractor fallback is used: Ark is only a GUI shell over `7z`/`unrar`/libarchive with no reusable library API; driving `7z` CLI adds processes, text parsing, and exposes the password via the command line (`/proc/<pid>/cmdline`), while Fedora's 7-Zip build drops RAR support anyway.

### 9.6 Caching

- up to 4 archive indexes, invalidated by source `mtime`/size;
- extracted image data uses a 3-entry LRU;
- libarchive reads are serialized to avoid concurrent reads of the same archive.

## 10. Documentation added

- `ARCHITECTURE.md` — concise onboarding architecture overview.
- `WINDOW_SCALING_CONSENSUS.md` — behavior contract for window sizing, fit modes, focus placement, and panning.
- `WINDOW_POSITIONING_NOTES.md` — design discussion notes that led to the window positioning rewrite.
- `FORK_DIFFERENCES.md` and `FORK_DIFFERENCES.zh-CN.md` — this document.

## 11. Non-goals and limitations

- No custom titlebar or window buttons were added.
- KDE's **No Titlebar and Frame** rule is expected to provide the borderless look.
- The native build is machine-specific because of `-march=native`.
- Qt itself is not rebuilt; compiler optimizations affect qimgv and its plugin, not system Qt.
- This fork is a personal KDE-focused customization, not an upstream merge proposal.
