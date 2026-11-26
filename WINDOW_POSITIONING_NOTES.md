# qimgv Window Positioning & Auto-Resize Notes

> Discussion memo. No source code has been changed for this topic.
> User intent: in **non-fullscreen, non-maximized** state, every image switch leaves the window (and the image inside it) centered on screen, with no accumulated offset and no off-screen rendering. KDE will later enable "No Titlebar and Frame" for qimgv.

## 1. Relevant current configuration (this machine)

From `~/.config/qimgv/qimgv.conf`:

```ini
autoResizeWindow=true
autoResizeLimit=90        ; user may change to 100
expandImage=true
expandLimit=3
keepFitMode=true
focusPointIn1to1Mode=1
```

`defaultFitMode` is unset, so `Settings::imageFitMode()` returns `0` = `FIT_WINDOW`, and with `keepFitMode=true` the viewer stays in `FIT_WINDOW` across image changes.

`autoResizeLimit` is **configurable**, not hard-coded: `Settings::autoResizeLimit()` (`settings.cpp:1094-1102`) accepts 30–100 and falls back to 90 only if out of range; the Settings dialog exposes it as a slider in 5% steps (`settingsdialog.cpp:252-253, 403`). The limit applies to `availableGeometry()` (screen area excluding taskbar/panels).

## 2. Final interaction contract (agreed)

### 2.1 Windowed (`Qt::WindowNoState`, includes KDE borderless windows; also applies when merely `Maximized`)

- Image display size follows window client size; zoom resizes both together, capped by `autoResizeLimit` (user may set 100%).
- **Left drag** = move window via `window()->windowHandle()->startSystemMove()`.
- **Right drag** = zoom, **anchored at the mouse cursor** (current behavior preserved). Window follows the zoom in real time (throttled), growing/shrinking around the anchored point.
- Once the cap is reached, the window stops growing and the image may overflow the viewport. In that state, panning is done with **right button first, then left button, drag with both held**.
- **Shift + left drag** = drag out file (DnD).
- **Middle click** = close (existing `exit` binding).
- No custom window buttons are added.

### 2.2 Fullscreen (`isFullScreen() == true`: double-click image / `F` / `F11`, no title bar)

- Window size is fixed; only the image zooms.
- **Right drag** = zoom (mouse anchor, unchanged).
- **Left drag** = pan image only when the image exceeds the viewport (`!scaledImageFits()`); when the image fits, left drag does nothing.
- **Shift + left drag** = DnD.
- **Middle click** = close.

### 2.3 Both-button pan gesture

Left and right held together enter pan, **regardless of press order**.

1. If `mousePressEvent()` sees both `LeftButton` and `RightButton` in `event->buttons()`, enter `MOUSE_PAN_BOTH` and record the cursor position.
2. Mouse move with both buttons held: pan the image.
3. Releasing either button exits pan and resets all interaction state. The remaining button must be released before starting a new gesture — it must not immediately resume zoom or window move.
4. Neither left-first-then-right nor right-first-then-left has special meaning; both are pan.

Implementation note: `mouseMoveEvent()` checks `MOUSE_PAN_BOTH` before the single-button branches; `mousePressEvent()` checks the combined button mask before assigning zoom/window-move state.

### 2.4 Zoom inputs

Right drag is the primary gesture. As an implementation principle (not a new user decision), other zoom inputs (`+` / `-`, `Ctrl+Wheel`, cursor zoom) should reuse the same "resize window for new scale" path so the window never drifts out of sync with the image.

## 3. Two independent positioning systems

### 3.1 Window geometry — `MW::preShowResize()` in `qimgv/gui/mainwindow.cpp:225-249`

Called from `MW::showImage()` / `showAnimation()` / `showVideo()` **before** the viewer receives new content; also to be reused by the zoom path.

Guards: current implementation only runs when `windowState() == Qt::WindowNoState`.

Algorithm:

1. `decorationSize = frameGeometry().height() - height()`
2. `maxSz = screens.at(currentDisplay)->availableGeometry().size() * (autoResizeLimit/100)`
3. `maxSz.setHeight(maxSz.height() - decorationSize)`
4. If `sz > maxSz`: `sz.scale(maxSz, Qt::KeepAspectRatio)`; if `sz` is empty, use `maxSz` (video).
5. `QRect newGeom(0, 0, sz.width(), sz.height())`
6. `newGeom.moveCenter(availableGeom.center())`
7. `newGeom.translate(0, decorationSize / 2)`
8. If visible: `setGeometry(newGeom)`; if hidden: `settings->setWindowGeometry(newGeom)`
9. `qApp->processEvents()`

Source comment at `mainwindow.cpp:223-224` already admits the problem:

```text
// todo: fix flicker somehow
// ideally it should change img & resize in one go
```

### 3.2 View content — `ImageViewerV2` in `qimgv/gui/viewers/imageviewerv2.cpp`

- Scene rect is centered on the origin: `(-10000000, -10000000, 20000000, 20000000)`. The invariant is **scene origin = image center = viewport center**. Both pixmap items keep `pos(0,0)` and `transformOriginPoint(0,0)`; their `offset` is set to `-imageSize / 2` whenever a pixmap is installed. `reset()` calls `centerOn(0, 0)`.
- `dpr` from `devicePixelRatioF()`; corrected on `DevicePixelRatioChange` on Wayland (`103-130`).
- `showImage()` (`315-343`): sets pixmap, `setDevicePixelRatio(dpr)`, `updateMinScale()`, resets fit mode if needed, `applyFitMode()`, `requestScaling()`.
- `applyFitMode()` → `fitWindow()` under current config. `fitWindow()` calls `centerOnPixmap()` synchronously.
- `centerOnPixmap()` is now simply `centerOn(0, 0)`; `centerIfNecessary()` also uses this when the image fits. The old scrollbar arithmetic and `scrollBarWorkaround` have been removed.
- `centerIfNecessary()` (`1222-1232`) similar, only when scaled image <= viewport.
- `resizeEvent()` (`933-954`): when parent visible, `updateMinScale()`, then `centerIfNecessary()` + `snapToEdges()` for `FIT_FREE`/`FIT_ORIGINAL`, else `applyFitMode()`; starts `scaleTimer` and calls `saveViewportPos()`.
- `saveViewportPos()` / `applySavedViewportPos()` (`1199-1220`) only act when `mViewLock == LOCK_ALL`.
- Right-drag zoom: `mouseMoveZoom()` (`724-740`) → `zoomAnchored()` (`1068-1078`) → `doZoom()` (`1255-1267`) which clamps to `[minScale, 500]` and emits `scaleChanged`.
- Unit asymmetry already present: `imageFits()` (`479-484`) compares `pixmap->width()` to `viewport()->width() * dpr`; `scaledImageFits()` (`486-492`) compares `scaledSizeR()` to `viewport()->width()` without `dpr`.

## 4. Call chains

### Subsequent image (next / previous, thumbnail click)

```text
Core::onModelItemReady()            core.cpp:1458
  -> Core::guiSetImage()            core.cpp:1494
     -> MW::showImage()             mainwindow.cpp:251
        -> MW::preShowResize()      changes window geometry, emits resize events
        -> ViewerWidget::showImage() viewerwidget.cpp:212
           -> ImageViewerV2::showImage()  resets state, applies fit mode
        -> scaling request -> Core::onScalingFinished() -> setScaledPixmap()
```

### First image / image from another directory (`delayModel`)

```text
Core::loadPath() sets state.delayModel
  -> image ready -> guiSetImage() -> MW::showImage()
     -> preShowResize() sees hidden window, only settings->setWindowGeometry()
  -> Core::showGui() -> MW::showDefault() -> MW::showWindowed()
     -> restoreWindowGeometry() -> setGeometry(settings->windowGeometry())  mainwindow.cpp:407-412
     -> show() -> resizeEvent() -> fit again
```

### Zoom (new path to implement)

```text
ImageViewerV2::mouseMoveZoom()
  -> compute new scale around cursor anchor
  -> request window resize for displayed size (real time, throttled)
  -> doZoom(newScale) -> scaleChanged
```

Must avoid a feedback loop: window resize triggers `resizeEvent()` → `applyFitMode()` → `doZoom()` → `scaleChanged` → another resize.

### Fullscreen zoom

Same `mouseMoveZoom()` / `doZoom()` path, but **no** window resize; panning uses left drag when `!scaledImageFits()`.

## 5. Diagnosis of the observed symptoms

**D1 — Resize and image assignment were originally two separate steps (root cause).** The old `preShowResize()` ran before `ImageViewerV2::showImage()`; `setGeometry()` triggered `resizeEvent()` with the previous pixmap, then `showImage()` reset and fit again. The current code wraps both in one transaction with viewer fit suppressed, then calls `applyCurrentFitMode()` once.

**D2 — `decorationSize` is a fragile hack.** `frameGeometry().height() - height()` depends on KWin having applied frame extents. With "No Titlebar and Frame" it may be non-zero on one image and zero on the next; only half is applied vertically (`/ 2`, integer division); width is never compensated. Produces position derived from the previous window state.

**D3 — Forced re-centering on every switch.** `moveCenter(availableGeom.center())` discards user window placement on every image. Combined with D2, the window jumps.

**D4 — DPR mismatch (fixed).** The old `preShowResize()` fed device pixels into `setGeometry()`. The current `applyAutoResize()` uses `pixmap->deviceIndependentSize()`; `updateFitWindowScale()` now resolves to about 1.0 when the window matches the image.

**D5 — Large images can exceed the screen.** `maxSz` subtracts `decorationSize` from height only; stale frame extents can make the frame exceed `availableGeometry()`. Viewport is smaller than window client area, so "window = image size" and "fit image to viewport" are a fixed-point problem; stale scroll position can push a large image off-screen.

**D6 — `currentDisplay` can be stale.** `preShowResize()` uses `screens.at(currentDisplay)` and returns early if out of range.

**D7 — First-show double application.** Hidden-window branch writes geometry into settings; `showWindowed()` later restores it and fits again.

## 6. Target behavior / acceptance criteria

1. In `Qt::WindowNoState`, after any image change, the window frame (or client rect when borderless) is centered on the screen containing the window; error <= 1 px.
2. Ten next/prev operations with alternating small / large / portrait images produce no accumulating offset.
3. Window client size = image logical size, capped at `autoResizeLimit` of `availableGeometry()`, aspect preserved, never larger than available area including frame.
4. Image content centered in viewport when scaled size <= viewport; when capped it is also centered with no inherited scrollbar offset.
5. No part of the image renders outside the screen.
6. Works with normal KDE decorations and with "No Titlebar and Frame" (`decorationSize == 0`).
7. First-open and subsequent-switch paths converge to the same state.
8. Window follows right-drag zoom in real time without visible lag or runaway resize/scaling loops.
9. With left and right held together, both-button pan works in either press order.
10. Shift+left drag still performs DnD in both windowed and fullscreen.

## 7. Fix principles (when coding starts)

- **One transaction.** Compute target geometry from the new image/scale, assign pixmap and geometry in the same event-loop turn, suppress or ignore the resize event caused by that assignment until after initial `applyFitMode()`. Add an `isApplyingAutoResize` re-entrancy guard instead of relying on `qApp->processEvents()`.
- **Correct units.** Use `pixmap->deviceIndependentSize()` (or `size() / dpr`) for `setGeometry()`.
- **Real frame margins.** Derive offsets from `frameGeometry()` vs `geometry()`; remove the `decorationSize / 2` guess; expect 0 when borderless.
- **Single source of truth.** Window-level code owns screen positioning; `ImageViewerV2` only centers content in its viewport. Do not let `saveViewportPos()` / `applySavedViewportPos()` override fresh centering unless the user explicitly enabled `LOCK_ALL`.
- **Unconditional final centering.** With the origin-centered invariant, `centerOnPixmap()` is just `centerOn(0, 0)`; no scrollbar arithmetic or one-time workaround remains.
- **Fresh screen.** Use `mw->screen()` / refresh `currentDisplay` instead of the stored index.
- **First show.** Either let `showWindowed()` restore geometry computed for the new image, or skip `preShowResize()` while hidden and let the first visible `resizeEvent()` do the single positioning pass — not both.
- **Zoom path.** Throttle window resize requests (e.g. coalesce to one per event-loop turn or ~16 ms); route all zoom inputs through one helper; never resize window in fullscreen.
- **Pan state machine.** Any-order left+right enters pan; reset on any release.

## 8. Verification plan

- Matrix: 800x600, 1920x1080, 7680x2160, tall portrait; dpr 1.0 and 1.5/2.0; decorations on and KDE borderless; `autoResizeLimit` 90 and 100.
- For each, capture window `frameGeometry().center()` and image `scaledRectR()` center after each of 10 next/prev operations; assert no drift.
- Confirm `windowGeometry` in `qimgv.conf` does not accumulate offsets across restarts.
- Confirm a large image never produces a window frame larger than `availableGeometry()`.
- Zoom test: right-drag until image overflows; verify left+right pan works in either press order and Shift+left still DnD.
- Fullscreen test: `F` after overflow; verify right-drag zoom, left-drag pan only when overflowing, Shift+left DnD, middle-click close.

## 9. Implementation notes (after coding)

- Replaced `MW::preShowResize()` with `MW::applyAutoResize(QSize logicalSize)`, based on `deviceIndependentSize()` / `frameGeometry()` margins and centered on `mw->screen()->availableGeometry()`. The old `decorationSize / 2` and stored `currentDisplay` lookup are gone.
- Image/animation/video swap is wrapped in a single fit transaction: viewer fit updates are suppressed while geometry and pixmap change, then `applyCurrentFitMode()` is called once. First-open uses a pending size applied in `showWindowed()` after the window becomes visible.
- Zoom: right-drag and keyboard zoom are back to the original in-viewport scaling behavior; they no longer resize the window. The `zoomResizeRequested` path is kept only for DPR correction (Wayland first-paint race). Because the image can now overflow the viewport after zooming, both-button pan works in either press order. `Expand images, up to:` follows the original `fitWindow()` / `expandLimit` code path and only takes effect when the viewport is larger than the image; with `autoResizeWindow` sizing the window to the image's logical dimensions it is intentionally a no-op.
- Mouse state machine: `MOUSE_WINDOW_MOVE_BEGIN` (plain left drag), `MOUSE_DRAG_BEGIN` (Shift+left drag/DnD), `MOUSE_PAN_BOTH` (right then left only), existing `MOUSE_PAN` in fullscreen, and `MOUSE_ZOOM` for right drag. Releasing one of the both-pan buttons locks input until all buttons are released.
- `dragsEnabled` no longer gates DnD; the click-zone event filter still consumes edge presses before the viewer sees them.
- Terminal refactor applied: scene rect centered on origin; both pixmap items kept at `pos(0,0)` / `transformOriginPoint(0,0)` with `offset = -imageSize/2`; `doZoom()` no longer rewrites offset; `centerOnPixmap()` and `centerIfNecessary()` collapse to `centerOn(0,0)`; `scrollBarWorkaround` and all hand-written scrollbar centering removed. Window geometry still centers the frame on the screen; the image is centered inside it by construction.
- Important unit fixes: (1) `offset` must use `QPixmap::deviceIndependentSize()` (logical pixels), not `width()`/`height()` (device pixels); using device pixels at dpr=2 produced an exact `-1/4` shift. (2) `MW::showImage()` must call `pixmap->setDevicePixelRatio(dpr)` **before** computing `deviceIndependentSize()`, otherwise window geometry is computed at dpr=1 while the viewer uses dpr=2. Both fixed in `syncPixmapItemOffset()`, `setScaledPixmap()`, and `MW::showImage()`. A Qt probe confirmed `QGraphicsView::centerOn(0,0)` and `QGraphicsPixmapItem` bounding rect use logical pixels and center correctly.
- Wayland DPR race (root cause of the remaining first-image offset): the real session reports dpr=2.00 before the first paint and dpr=1.50 afterwards. `ImageViewerV2::eventFilter()` handled `QEvent::DevicePixelRatioChange` but was **never installed**. Now installed on both the view and its viewport; `onDPRChanged()` also emits `zoomResizeRequested(pixmap->deviceIndependentSize())` so MW re-centers and re-sizes the window with the corrected dpr. Verified in the real Wayland session: first image is recalculated from dpr=2 to dpr=1.5 and subsequent switches use dpr=1.5 consistently.
- Wayland reposition constraint: real screen capture proved that `QWidget::setGeometry()` on an already-mapped **native Wayland** window changes the size but **does not move the window** — the compositor keeps the previous top-left, so each new image expands down/right from the old corner. Hiding/re-showing forced re-placement but produced an unacceptable close/open animation on every switch. Decision at that time: **run qimgv via XWayland** (`QT_QPA_PLATFORM=xcb`), where `setGeometry()` moves and resizes normally. Verified by three successive screenshots under XWayland: window stays centered across image switches with no remap animation. The checkerboard seen around `blueFontTest.png` is the image's own alpha channel, not a positioning bug. **Superseded by section 10 on branch `kde-wayland-native`.**
- Local debug build used: `build-local` (EXIV2/VIDEO off). Final system build: `build-full` with `EXIV2=ON`, `VIDEO_SUPPORT=ON`, `OPENCV_SUPPORT=ON`, `USE_QT5=OFF`; dev headers/libs were extracted into `.build-deps/prefix` without root. `deploy-system.sh` installs the full binary to `/usr/lib/qimgv/qimgv`, the mpv plugin to `/usr/lib64/qimgv/player_mpv.so`, translations to `/usr/local/share/qimgv/translations`, and replaces `/usr/bin/qimgv` with a wrapper exporting `QT_QPA_PLATFORM=xcb`.

## 10. Native Wayland + KWin script (branch `kde-wayland-native`)

- Supersedes the XWayland decision in section 9.
- Root cause unchanged: `xdg-shell` gives clients no way to position a mapped toplevel. `setGeometry()` applies size only; the compositor keeps the previous top-left, so the window grows down/right and its center drifts.
- Fix: `kde/kwin-scripts/qimgv-centering`, a self-contained KWin script. It matches the qimgv main window by `resourceClass` / `resourceName` / `desktopFileName`, and on `frameGeometryChanged` re-centers the frame inside `workspace.clientArea(KWin.WorkArea, win)`.
- Only size changes trigger centering (last size tracked per `internalId`), so manual window moves via `startSystemMove()` are not hijacked.
- Fullscreen, maximized, tiled, transient/modal windows and non-normal windows are ignored.
- Opacity is set to 0 and restored around the move so the intermediate frame at the old position is not visible.
- No qimgv C++ change is required: `MW::applyAutoResize()` still requests the correct client size; Wayland only ignores the position part.
- Latency: a client size change needs a compositor round trip (`xdg_toplevel` min==max → configure → ack), so an image switch costs about one extra vsync compared with XWayland. The script's move runs in the compositor process and is negligible.
- Verified on Plasma 6.7.5 / KWin 6.7.5 (7680x2160 @ 240 Hz, scale 1.5): first open, real next/prev switches, compositor-initiated resizes and 10x rapid switching all leave the frame center at the work-area center; manual moves stay put; fullscreen/maximize restore unchanged.
- Deployment: `deploy-system.sh` installs the script to `/usr/share/kwin/scripts/qimgv-centering` and enables it in the user's `kwinrc`; `--xwayland` installs the old wrapper and disables the script.
- Optional follow-up only if a one-frame "new image at old window size" is ever visible before the configure round trip completes: defer the pixmap application in `MW::showImage()` until the resize is confirmed. Not observed at 240 Hz during testing.
- Context-menu placement on Wayland: never use `QCursor::pos()`. It lags and lives in a different coordinate space than Qt's cached window geometry, which the compositor-side centering move does not update, so menus landed far from the click. The menu now takes the right-button release event position (`MW::mContextMenuPos`) and maps it with `mapToGlobal()`, which uses the same cached parent position as Qt's popup positioner, so the offset cancels. `ContextMenu::setGeometry()` also clamps against the target rect and `screenAt(geom.topLeft())` instead of the cursor.
