# qimgv Window / Scaling / 1:1 Consensus

> This document records the agreed behavior before implementation.
> No source code has been changed for this redesign.

## 1. Scope

This redesign replaces the current combination of:

- `Automatic window resize`
- `Screen area limit for auto resize`
- old image-fit modes (`fit in window`, `fit width`, `fit in window stretch`)

with one coherent model:

- the image/window size is controlled by `Expand image`;
- the screen boundary is always the **available work area**;
- `1:1` decides whether an oversized image is cropped/panned or shrunk to fit.

## 2. Settings model

### 2.1 Removed settings

Remove from the Settings dialog and from persisted configuration handling:

- `Automatic window resize`
- `Screen area limit for auto resize`

The screen limit is fixed at **100% of available work area** (`QScreen::availableGeometry()`), excluding panels/taskbars.

### 2.2 Expand slider

`Expand image` no longer has a checkbox. It is a slider only.

- `-` = no expansion / no scaling.
- Integer stops are always available: `2x`, `3x`, `4x`, …
- The next stop after `-` is `2x`.

Half-step stops depend on `always expand`:

- **unchecked**: half-stops are hidden/skipped; slider stops are `-`, `2x`, `3x`, `4x`, …
- **checked**: half-stops are added; slider stops are `-`, `1.5x`, `2x`, `2.5x`, `3x`, `3.5x`, …

### 2.3 `always expand` checkbox

A new checkbox under the Expand slider:

- **unchecked**: only **integer multiples** are allowed; half-stops are not selectable.
- **checked**: half-stops (`1.5x`, `2.5x`, `3.5x`, …) become available, and the final scale may also be fractional because it is capped by the available work area.

## 3. Size calculation

Definitions:

- `A` = available work area size.
- `I` = image logical size (`QPixmap::deviceIndependentSize()`, DPR-corrected).
- `N` = Expand slider value; `-` means no expansion.

### 3.1 Expand = `-`

No expansion. Desired image size is `I`.

### 3.2 `always expand` unchecked

Integer-only scale. Hard constraint: when `always expand` is unchecked, `k` must be an integer; half-stops such as `1.5x` / `2.5x` / `3.5x` are not selectable and must never be applied.

```text
k = max(1, min(floor(N), floor(A.width / I.width), floor(A.height / I.height)))
desired = I * k
```

Examples, assuming available area 3840×2160 and `Expand = 3x`:

- 1920×1080 → max `2x` → 3840×2160
- 1000×900  → max `2x` → 2000×1800

Even though the slider is 3x, integer-only mode stops at the largest integer multiple that still fits.

### 3.3 `always expand` checked

Fractional scale allowed:

```text
k = max(1, min(N, A.width / I.width, A.height / I.height))
desired = I * k
```

Examples, assuming available area 3840×2160:

- `Expand = 2.5x`, image 1000×900 → `min(2.5, 3.84, 2.4) = 2.4` → 2400×2160
- `Expand = 3.5x`, image 1000×900 → still `2.4` → 2400×2160, because the screen height cap wins
- `Expand = 3x`, image 1200×600 → `min(3, 3.2, 3.6) = 3` → 3600×1800
- `Expand = 3x`, image 1920×1080 → `min(3, 2, 2) = 2` → 3840×2160

For large images where `k < 1`, see section 5.

## 4. `1:1` display mode

The remaining image-fit checkbox should be renamed in the UI because its meaning is no longer literal 1:1 pixel scale. Suggested label:

> **Do not shrink to fit screen (crop / pan oversized image)**

Internal name can remain `1:1`.

### 4.1 `1:1` enabled

The image is displayed at the size calculated in section 3. It is **not shrunk to fit** the available work area.

Placement of an image that exceeds the viewport is controlled by `Focus in 1:1 mode` in Preferences → View:

| Option | Horizontal placement | Vertical placement |
|---|---|---|
| `Top Center` | centered (middle part is shown when wider) | top-aligned |
| `Top Left` | left-aligned | top-aligned |
| `Center` | centered | centered |

If the whole image fits in the viewport, it is centered regardless of the focus option. The anchor applies only when at least one axis overflows; in that case leftover space on a fitting axis stays on the opposite side. The window itself never exceeds the available work area.

This setting applies both to the automatic `1:1` rendering on image switch and to the right-click `1:1` action. Right-click `1:1` always renders at native 1x and uses this focus setting even when `Crop oversized image (1:1)` is unchecked.

"Clipped" does **not** mean inaccessible. The hidden parts must be reachable by panning.

### 4.2 Panning an oversized image in `1:1` mode

Agreed panning methods:

- left and right buttons held together, in either press order: drag to pan.
- mouse wheel: pan when the image exceeds the viewport.

If the image does not exceed the viewport, wheel keeps its existing behavior (next / previous image).

### 4.3 `1:1` disabled

The image is shrunk to fit the available work area if needed:

```text
fit = min(1, A.width / desired.width, A.height / desired.height)
final = desired * fit
```

The window and image are then centered in the available work area.

## 5. Images larger than the screen

### 5.1 `1:1` enabled

Do not shrink. Apply the `Focus in 1:1 mode` placement from section 4.1:

- clip to screen;
- pan with right+left drag or wheel to inspect hidden areas.

### 5.2 `1:1` disabled

Scale down to available work area (section 4.3), then center.

## 6. Mouse interaction contract (preserved)

### 6.1 Windowed mode

- plain left drag → move window
- `Shift + left drag` → drag out file (DnD)
- right drag → zoom image inside the viewport; does **not** resize window
- right → left, both held → pan image
- middle click → close

### 6.2 Fullscreen

- right drag → zoom image
- left drag → pan when image exceeds the screen; no-op when it fits
- `Shift + left drag` → drag out file
- middle click → close

## 7. Relationship between settings

- `Expand` / `always expand`: decide the **desired image and window size**.
- `1:1`: decides what happens when that desired size **does not fit the screen**: crop + pan, or shrink to fit.
- `Focus in 1:1 mode`: decides where an oversized 1:1 image is anchored (`Top Center` / `Top Left` / `Center`); it also applies to right-click `1:1` regardless of the `1:1` checkbox.
- Right-drag zoom stays an in-viewport operation and does not resize the window.

## 8. Platform / deployment notes

- The system build must run through XWayland (`QT_QPA_PLATFORM=xcb`), because native Wayland clients cannot reposition already-mapped windows reliably. `/usr/bin/qimgv` is a wrapper that sets this variable.
- Wayland first-paint DPR race remains handled by the installed `QEvent::DevicePixelRatioChange` filter.
- When `autoResizeWindow` is enabled, saved window geometry and maximized state are not restored at startup; the first image's auto-resize decides the geometry instead. A maximize triggered by the user during the session is still respected.
- This document is a behavior consensus; see section 9 for implementation status.

## 9. Implementation status

Implemented in source and compiled into `build-full`:

- Settings UI no longer shows `Automatic window resize` or `Screen area limit for auto resize`.
- `Expand image` is slider-only, stored as `expandScaleX2` in half-step units (`0` = `-`, `3` = `1.5x`, `4` = `2x`, …).
- `alwaysExpand` controls whether half-stops are selectable and whether the final scale may be fractional.
- Fit UI is reduced to one `Crop oversized image (1:1)` checkbox; it maps to `FIT_ORIGINAL` vs `FIT_WINDOW`.
- Window sizing on image switch follows the formulas in section 3.
- A single `displayScale` computed in `MW` is authoritative for image display. With crop off, `MW` applies the section 3 shrink factor to the scale itself; with crop on, the scale is left uncropped and the window is clamped instead. `FIT_WINDOW` uses this scale during automatic image switching instead of recomputing `viewport / pixmap` in the viewer. The old viewer-side `fitWindowScale` remains only for manual `Fit in window` actions.
- Manual zoom / Fit actions and right-click `1:1` switch to a manual scale source. Fullscreen transitions, window resizes, settings changes and GIF frame-size changes recompute the automatic scale only while the auto source is active.
- `FIT_ORIGINAL` uses the externally computed display scale and the `Focus in 1:1 mode` placement from section 4.1.
- Preferences → View offers `Top Center`, `Top Left`, `Center` for `Focus in 1:1 mode`; `At cursor` was removed. Right-click `1:1` uses this setting at native 1x even when the crop checkbox is off.
- Mouse wheel pans when `FIT_ORIGINAL` image exceeds the viewport.
- Full-feature build and an XWayland startup smoke test passed. Real-world behavior still needs user verification before re-running `deploy-system.sh`.
- Follow-up interaction fixes: `Shift + wheel` pans horizontally; in `FIT_ORIGINAL`, wheel pans until the image edge, then the event is ignored so MW applies next/prev image; user-invoked 1:1 (`setFitOriginal`) resets `mDisplayScale` to 1.0 so the image renders at native size. If the native 1x image exceeds the available work area, `nativeFitRequested` makes MW resize the window by clamping each axis independently to the native image size, so only the overflowing axis is capped and the window is not forced to fill the whole work area. Automatic initial display scale from `Expand` is still applied on image switch.
