# qimgv Architecture

> Quick onboarding for agents / new sessions. Paths are repository-relative unless noted.

## 1. Positioning & core goals

qimgv is a GPL-3.0, cross-platform (Linux/BSD/Windows/macOS) image viewer built on Qt Widgets. Its goals are a minimal uncluttered UI, fast folder navigation, fully configurable shortcuts/themes, high-quality scaling, light editing (crop / resize / rotate / flip), quick copy/move, and optional mpv video playback.

There are two top-level modes:

- **Document view** — one image at a time with zoom/fit, overlays and editing.
- **Folder view** — grid/tree browser with bookmarks, sorting and multi-select file operations.

## 2. Technology choices

| Choice | Role |
| --- | --- |
| C++17 (app), C++11 (`player_mpv` plugin) | Core implementation. |
| Qt 5.12+ or Qt 6: Widgets, `QGraphicsView`, `QSettings`, `QThreadPool`, `QTranslator`, `QLibrary` | UI, event/action dispatch, persistence, concurrency, i18n, runtime plugin loading. |
| `std::filesystem` | Directory iteration and file metadata (`FSEntry`). |
| OpenCV (optional) | High-quality scaling filters (`ImageLib::scaled_CV`, unsharp mask). |
| Exiv2 (optional) | EXIF orientation/tag reading and writing (`DocumentInfo`). |
| libmpv via dynamic plugin | Video decoding/playback; host stays independent of mpv. |
| libarchive (optional, fork addition) | Reading images from `zip`/`cbz`/`7z`/`cb7`/`rar`/`cbr`/`tar` (`ArchiveBackend`; compile flag `USE_ARCHIVE`). |
| KF6WindowSystem (optional) | KDE blur-behind effect. |
| CMake | Root options + app target + separate plugin project + install rules. |

Root CMake switches: `EXIV2`, `VIDEO_SUPPORT`, `OPENCV_SUPPORT`, `KDE_SUPPORT`, `USE_QT5`. They become compile definitions `USE_EXIV2`, `USE_MPV`, `USE_OPENCV`, `USE_KDE_BLUR`.

## 3. High-level structure & dependency direction

```text
main.cpp
 ├─ process-wide singletons: settings, inputMap, appActions,
 │                            scriptManager, actionManager, shrRes
 └─ Core                         central controller / wiring hub
     ├─ MW                       main window + all GUI widgets
     ├─ shared_ptr<DirectoryModel>
     │   ├─ DirectoryManager     scan / sort / mutate FSEntry lists
     │   │   ├─ DirectoryWatcher (inotify / Windows / dummy)
     │   │   └─ ArchiveBackend   (fork) index/read archive entries via libarchive
     │   ├─ Loader               async decode (QThreadPool)
     │   ├─ Cache                in-memory shared_ptr<Image>
     │   └─ Scaler               coalesced HQ scaling (QThreadPool)
     ├─ DirectoryPresenter × 2   model <-> view adapters
     │   ├─ IDirectoryView       ThumbnailView / FolderView
     │   └─ Thumbnailer          disk cache + async thumbnails
     └─ slideshow / shuffle / drag state
```

Dependency rules:

- **Core is the hub.** Most component-to-component traffic is wired in `Core::connectComponents()` and `Core::initActions()`.
- `DirectoryModel` is a **facade/controller**, *not* a `QAbstractItemModel`. `DirectoryPresenter` bridges it to `IDirectoryView` implementations.
- UI sends user intent upward via `ActionManager` signals; `Core` executes domain operations and pushes results back into `MW`.
- Heavy work (load / scale / thumbnails) runs in `QThreadPool`s; results return to the GUI thread through queued signals. `ImageViewerV2 → MW → Core → Scaler` is the HQ-scaling relay.
- Cross-cutting services are `Xxx::getInstance()` singletons with `extern` globals, all created in `main()`.

## 4. Main modules & responsibilities

| Module | Key files | Responsibility |
| --- | --- | --- |
| Build | `CMakeLists.txt`, `qimgv/CMakeLists.txt`, `plugins/player_mpv/CMakeLists.txt` | Options, dependency detection, targets, translations, install rules. |
| App shell | `qimgv/main.cpp`, `appversion.*`, `macosapplication.*`, `proxystyle.*` | startup, HiDPI/platform tweaks, CLI options, global singleton creation. |
| Controller | `qimgv/core.*` | Owns GUI/model/presenters; navigation, editing, file ops, slideshow, drag/drop, i18n. |
| Config/services | `qimgv/settings.*`, `themestore.*`, `shortcutbuilder.*`, `sharedresources.*`, `qimgv/utils/*` | preferences/session state/theme, shortcut encoding, shared resources, helpers. |
| Directory data | `qimgv/components/directorymodel.*`, `directorypresenter.*`, `directorymanager/`, `directorymanager/watchers/` | file listing, filtering, sorting, change propagation, model/view adaptation. |
| Loading & processing | `qimgv/components/{loader,scaler,cache,thumbnailer}/`, `qimgv/sourcecontainers/`, `qimgv/utils/imagefactory.*`, `imagelib.*`, `3rdparty/QtOpenCV/` | image/animation/video containers, EXIF, async decode, cache, scaling, thumbnail generation. |
| Actions/scripts | `qimgv/components/actionmanager/`, `components/scriptmanager/`, `utils/actions.*`, `utils/inputmap.*`, `utils/script.*` | input → named actions, shortcut persistence, user shell scripts. |
| UI | `qimgv/gui/` | `MW`, viewer, thumbnail panel, folder view, dialogs, overlays, custom widgets. |
| Video plugin | `plugins/player_mpv/src/` | `VideoPlayerMpv`, `MpvWidget`, exported `CreatePlayerWidget()`. |
| Resources/packaging | `qimgv/resources.qrc`, `qimgv/res/`, `qimgv/distrib/`, `scripts/`, `.github/workflows/` | icons, QSS, translations, desktop/AppData/icons, Windows packaging/CI. |

## 5. Key data flows & execution sequences

### Startup

```text
main()
  env/HiDPI setup -> QApplication + ProxyStyle -> org/app identity
  -> create singletons -> Core core
  -> Core ctor: loadTranslation, initGui, initComponents,
                connectComponents, initActions, readSettings
  -> optional CLI path / default folder view -> Core::loadPath()
  -> Core::showGui() (deferred setupFullUi) -> a.exec()
```

### Open path → displayed image

```text
Core::loadPath(path)                       (file, dir, drop, open dialog, clipboard)
  -> DirectoryModel::setDirectory()        (cache cleared)
  -> DirectoryManager::setDirectory()      (std::filesystem scan, regex filter, sort, watcher)
  -> indexOfFile(); MIME fallback force-insert if path is outside current dir
  -> Core::loadFileIndex()
       -> model->unloadExcept() + load() [+ preload neighbours]
       -> Loader -> ImageFactory::createImage()
            -> DocumentInfo (MIME/type/EXIF orientation)
            -> ImageStatic / ImageAnimated / Video
       -> Cache -> DirectoryModel::imageReady
  -> Core::onModelItemReady() -> Core::guiSetImage()
  -> MW -> ViewerWidget -> ImageViewerV2::showImage()/showAnimation()/showVideo()
```

`Core::State::delayModel` defers loading a new directory until the requested file is already visible, avoiding an empty first frame.

### Archive viewing (fork addition)

```text
Core::loadPath("archive.cbz" | "archive.cbz::inner.png")
  -> Core::loadArchive()                  (password prompt loop, max 3 tries)
  -> DirectoryModel::mountArchive()       (cache cleared)
  -> DirectoryManager::mountArchive()
       -> ArchiveBackend::listEntries()   (mtime/size-cached index, image suffixes only)
       -> FSEntry.path = "archive.cbz::inner.png"
  -> Core::loadFileIndex()
       -> Loader -> ImageFactory::createImage()
            -> DocumentInfo(virtual path) -> ArchiveBackend::entryInfo()/readEntry()
            -> ImageStatic / ImageAnimated (decode from QBuffer, no temp files)
```

Key invariants:

- `::` is the only virtual-path separator (`utils/archivepath.*`). The full inner path is used for uniqueness/sorting; `Core::updateInfoString()` shows the basename.
- `Core::archiveReadOnly()` blocks file-level writes (save/delete/rename/copy/move/drag-out/`cropAndSave`) whenever `state.currentFilePath` is virtual or `model->source() == SOURCE_ARCHIVE`.
- `Core::loadParentDir()` leaves an archive by reloading its containing directory.
- Passwords are memory-only (`ArchiveBackend::setPassword`); encrypted 7z (header or data) is reported as unsupported because libarchive cannot decrypt it.

### High-quality scaling

```text
ImageViewerV2::requestScaling(QSize, filter)
  -> MW::scalingRequested -> Core::scalingRequest()
  -> Scaler::requestScaled()          latest-wins; pins in-use cache items
  -> ScalerRunnable -> ImageLib::scaled() (Qt or OpenCV)
  -> scalingFinished -> Core::onScalingFinished() (drops stale paths)
  -> MW -> ViewerWidget -> ImageViewerV2::setScaledPixmap()
```

### Thumbnails / folder view

```text
IDirectoryView::thumbnailsRequested(list, size, crop, force)
  -> DirectoryPresenter::generateThumbnails()
  -> Thumbnailer -> ThumbnailerRunnable -> ThumbnailCache (MD5 id + mtime check)
  -> thumbnailReady -> view->setThumbnail(index, Thumbnail)
```

`ThumbnailView` requests only visible items plus offscreen margins. Clicking an item emits `itemActivated` → `DirectoryPresenter::fileActivated` / `dirActivated` → `Core`.

### Input → action

```text
QInputEvent -> MW::keyPressEvent/wheelEvent/mousePressEvent
  -> ActionManager::processEvent()
  -> ShortcutBuilder::fromEvent() -> "<shortcut>=<action>"
  -> ActionManager::invokeAction()
  -> QMetaObject::invokeMethod(this, actionName)   // action name == signal name
  -> Core::initActions() connection -> Core or MW slot
```

Scripts are registered as actions named `s:<scriptName>` and dispatched to `ScriptManager` with the current image.

### Editing / save

`Core::edit_template()` applies `ImageLib` operations (crop/resize/rotate/flip), calls `DirectoryModel::updateImage()`, and shows `SaveConfirmOverlay`. Folder view auto-saves immediately. Save/save-as goes back through `Core` → `DirectoryModel::saveFile()` → `Image::save()` → `DocumentInfo::refresh()`.

### Video

`Core::guiSetImage()` detects `DocumentType::VIDEO` → `MW::showVideo()` → `ViewerWidget::enableVideoPlayer()` → `VideoPlayerInitProxy::showVideo()`. The proxy lazily resolves `CreatePlayerWidget` via `QLibrary`, then forwards the `VideoPlayer` interface to `VideoPlayerMpv`.

### Settings propagation

`SettingsDialog::saveSettings()` writes all setters and emits `settingsChanged`, which `Settings::sendChangeNotification()` re-emits globally. Consumers (`Core`, `MW`, `ImageViewerV2`, `DirectoryManager`, overlays, proxies, …) run their own `readSettings()`. Language changes are the exception: translation is loaded once in the `Core` constructor, so a restart is required.

## 6. Directory structure

```text
.
├── CMakeLists.txt                  root build options / subdirectories
├── README.md, LICENSE
├── scripts/                        Windows/MSYS2 packaging helper
├── .github/workflows/              CI build
├── plugins/player_mpv/             independent CMake project for the video plugin
│   ├── CMakeLists.txt
│   └── src/                        VideoPlayerMpv, MpvWidget, qthelper, nogl fallback
└── qimgv/                          main application
    ├── CMakeLists.txt              executable, translations, install/uninstall
    ├── main.cpp                    process entry
    ├── core.*                      central controller
    ├── settings.*, themestore.*, shortcutbuilder.*, sharedresources.*
    ├── resources.qrc               embedded icons + QSS
    ├── 3rdparty/QtOpenCV/          QImage <-> cv::Mat bridge
    ├── components/                 backend services (model, presenters, loader, cache,
    │   │                            scaler, thumbnailer, directory manager/watchers)
    │   ├── actionmanager/, scriptmanager/
    │   ├── archive/                ArchiveBackend (fork, libarchive)
    │   └── directorymanager/watchers/{linux,windows}
    ├── sourcecontainers/           FSEntry, DocumentInfo, Image*, Thumbnail, Video
    ├── utils/                      actions, inputmap, script, imagefactory, imagelib,
    │                               archivepath (fork), fileoperations, cmdoptionsrunner, ...
    ├── gui/                        MW, CentralWidget, idirectoryview
    │   ├── viewers/                ImageViewerV2, ViewerWidget, DocumentWidget, video proxies
    │   ├── customwidgets/          ThumbnailView, ThumbnailWidget, buttons, menus, ...
    │   ├── panels/                 MainPanel/ThumbnailStrip, CropPanel, SidePanel, InfoBar
    │   ├── folderview/             FolderView, FolderGridView, tree, bookmarks, proxies
    │   ├── overlays/               crop, copy, rename, info, video controls, etc.
    │   └── dialogs/                settings (large .ui), resize, scripts, shortcuts, print
    ├── res/                        icons, styles/style-template.qss, theme-presets, translations
    ├── distrib/                    desktop file, AppData, hicolor icons, PKGBUILD, plist, MIME
    └── tests/                      standalone Qt6 test target (not added by root CMake)
```

## 7. Important entry & config files

| File | Why it matters |
| --- | --- |
| `CMakeLists.txt` | Feature switches and dependency detection; start here for build changes. |
| `qimgv/CMakeLists.txt` | `qimgv` target sources, feature macros, translation list, install rules. |
| `qimgv/main.cpp` | Process entry, global singletons, CLI options (`--gen-thumbs`, `--build-options`). |
| `qimgv/core.*` | Central controller; first stop for navigation/action/file-op behavior. |
| `qimgv/settings.*` | All preferences, session state, cache paths and global change signal. |
| `qimgv/components/actionmanager/actionmanager.*` | Input event → action signal mapping. |
| `qimgv/gui/idirectoryview.h` | Contract implemented by both thumbnail strip and folder view. |
| `qimgv/gui/dialogs/settingsdialog.*` | User-facing settings; writes almost every preference. |
| `qimgv/gui/viewers/videoplayerinitproxy.cpp` | Runtime plugin discovery/loading. |
| `qimgv/resources.qrc`, `qimgv/res/styles/style-template.qss` | Embedded assets and stylesheet placeholders. |
| `qimgv/distrib/qimgv.desktop`, `qimgv/distrib/qimgv.appdata.xml` | Desktop integration. |
| `plugins/player_mpv/CMakeLists.txt` | Separate plugin build/install. |

Runtime config (Linux/FreeBSD, via `QSettings` NativeFormat): `~/.config/qimgv/qimgv.conf` (preferences + `[Controls]` + `[Scripts]`), `savedState.conf` (window geometry, paths, bookmarks, print state), `theme.conf` (`[Colors]`). On Windows/macOS these become `<appdir>/conf/{qimgv,savedState,theme}.ini`. Thumbnail cache lives under `QStandardPaths::GenericCacheLocation/qimgv/thumbnails`.

## 8. Extension & modification notes

### Where to add things

| Goal | Primary files |
| --- | --- |
| New preference/session value | `settings.*`; expose in `settingsdialog.*`; consume via `settingsChanged()`. |
| New action/shortcut | `utils/actions.cpp` → `actionmanager.*` (signal + default) → `Core::initActions()`. |
| New input event type | `shortcutbuilder.*` (+ `utils/inputmap.cpp` key maps). |
| New script feature | `components/scriptmanager/`, `utils/script.*`, `Settings::readScripts/saveScripts`. |
| New player capability/backend | **both** copies of `videoplayer.h`, `videoplayerinitproxy.cpp`, `VideoPlayerMpv`. |
| New theme / QSS token | `themestore.*`, settings dialog selector, `style-template.qss`, `Settings::loadStylesheet()`. |
| New image format | Qt imageformat plugin + `Settings::supportedFormats()/supportedMimeTypes()`; Windows packaging script. |
| New locale | `res/translations/*.ts`, `TS_FILES` in `qimgv/CMakeLists.txt`, language list in `settingsdialog.cpp`. |
| New installed/embedded asset | `resources.qrc` or `qimgv/distrib/` + an `install()` rule. |

### Gotchas

- `Core`, not `MW`, owns application logic. Adding behavior only in a widget usually bypasses actions, shortcuts and state handling.
- `DirectoryModel` is not an item model; do not look for `rowCount()`/`data()`. Index translation (including folder offsets when `mShowDirs`) lives in `DirectoryPresenter`.
- Many UI classes are lazy *proxy + state buffer* pairs (e.g. `FolderViewProxy`, `ThumbnailStripProxy`, overlay proxies). State changes before the real widget exists must go through the proxy.
- `ActionManager` dispatch is name-based (`QMetaObject::invokeMethod`). A new action needs a matching signal name, not just an enum entry.
- The `VideoPlayer` interface header is duplicated in `qimgv/gui/viewers/` and `plugins/player_mpv/src/`; keep both binary-compatible. Plugin search directories are currently hardcoded in `VideoPlayerInitProxy`; the CMake macro `_QIMGV_PLUGIN_DIR` is defined but unused.
- Scaling/thumbnail results can be stale. `Core::onScalingFinished()` checks the path; thumbnail delivery checks index/size. Preserve these guards.
- `Settings::settingsChanged()` is live for most options, but **language requires restart** and theme presets under `res/theme-presets/` are stale reference files (the active themes come from `ThemeStore`).
- `res/translations/en_US.ts` is the source template and is not compiled into `.qm`; add new locales to `TS_FILES`.
- `qimgv/tests/` exists but is not wired into the root build; add `add_subdirectory(tests)` and `enable_testing()` if you want it in the default build.
