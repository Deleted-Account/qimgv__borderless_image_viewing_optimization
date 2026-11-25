# Spec: 压缩包内看图（Archive Image Viewing v1）

状态：待批准（Spec Mode）
分支：`kde-tweaks`
影响范围：仅在压缩包内，不改变现有文件夹/单图行为

## 1. 目标

让 qimgv 可以直接打开 `zip / cbz / 7z / cb7 / rar / cbr / tar`，把包内图片当成一个“只读目录”顺序浏览，复用现有文档视图、切图居中、浮动 info、fit/1:1 等全部逻辑。

## 2. 已达成共识（用户确认）

- 仅文档视图；不做文件夹视图封面、不做缩略图条改造（只为预加载服务）。
- 包内只索引图片，**排除视频**。
- 虚拟路径：`/abs/archive.cbz::inner/path.jpg`，同时作为：
  - `FSEntry.path`
  - Cache key
  - `state.currentFilePath`
  - `DocumentInfo::filePath()`
- 不把压缩包扩展名加入 `Settings::supportedFormats()`（否则 DirectoryManager 会把压缩包当图片）。
- info 显示：**只显示包内文件名 basename**、`[3/120]`、解压后大小；**不显示压缩包名**。
- 退出压缩包复用现有 `goUp`（parent directory）动作；Esc 不改。
- 包内文件只读，**仅拦截文件级写操作**：保存/另存/删除/重命名/复制/移动/拖出（含 Shift 拖出）。提示“压缩包内文件不可修改”。
- 允许的内存内编辑：裁剪、旋转、翻转、缩放（不保存），保存仍被拦截；`cropAndSave` 因包含保存而整体拦截。
- 设壁纸、复制图片到剪贴板属于读取，不拦截。
- 排序：用包内完整路径 + 现有 QCollator（数字模式）。
- `FSEntry.name` = 包内完整路径（保证唯一和排序）；显示名在 `Core::updateInfoString` 里取 basename。
- 包内文件名 UTF-8，解码失败回退 GB18030。
- 串行解压读取 + 最近 3 个解压条目 LRU。
- 索引只读元数据，**不落盘解压**。
- 加密包：支持密码输入（详见第 4.3 节）；取消或 3 次错误则放弃打开，保留当前图片。
- 损坏/不支持的包：提示错误，保留当前图片。
- CMake 可选 `find_package(LibArchive QUIET)`，有则定义 `USE_ARCHIVE` 并链接；没有 devel 也能正常构建（编译为无压缩包支持）。

## 3. 虚拟路径约定

- 分隔符：`::`
- `ArchivePath::join("/a/b.cbz", "img/1.jpg")` → `/a/b.cbz::img/1.jpg`
- 包内路径统一使用 `/`，去掉开头的 `./`。
- `ArchivePath::isArchivePath(p)`：`p.contains("::")`
- `ArchivePath::isArchiveExtension(p)`：后缀（小写）∈ `{zip, cbz, 7z, cb7, rar, cbr, tar}`
- `ArchivePath::innerPath(p)` / `archivePath(p)`：拆分

## 4. 新增文件

### 4.1 `qimgv/utils/archivepath.h` / `.cpp`

```cpp
namespace ArchivePath {
bool isArchiveExtension(const QString &path);
bool isArchivePath(const QString &path);
QString join(const QString &archivePath, const QString &innerPath);
bool split(const QString &virtualPath, QString &archivePath, QString &innerPath);
QString innerPath(const QString &virtualPath);
QString archivePath(const QString &virtualPath);
QString displayName(const QString &virtualPath); // 包内 basename
}
```

无任何 libarchive 依赖。

### 4.2 `qimgv/components/archive/archivebackend.h` / `.cpp`

```cpp
struct ArchiveEntry {
    QString path;      // 包内完整路径
    qint64 size;       // 解压后大小
    QDateTime modified;
    bool isDir;
    bool encrypted;    // archive_entry_is_encrypted()
};

class ArchiveBackend {
public:
    static ArchiveBackend *instance();
    QList<ArchiveEntry> listEntries(const QString &archivePath);   // 带索引缓存
    bool entryInfo(const QString &archivePath, const QString &innerPath,
                   ArchiveEntry &out);
    bool readEntry(const QString &archivePath, const QString &innerPath,
                   QByteArray &out);                              // 带 LRU

    // 密码（仅内存，绝不落盘/写日志）
    void setPassword(const QString &archivePath, const QString &password);
    void clearPassword(const QString &archivePath);
    bool hasPassword(const QString &archivePath) const;
    bool passwordRequired(const QString &archivePath) const;
    bool hasEncryptedEntries(const QString &archivePath) const;

    void clearCache(const QString &archivePath);
private:
    QMutex m_mutex; // 所有操作串行化（libarchive + 缓存）
};
```

实现要点：
- 头文件不包含 `<archive.h>`；`archivebackend.cpp` 内部 `#ifdef USE_ARCHIVE` 包含。
- 未定义 `USE_ARCHIVE` 时：`listEntries` 返回空，`readEntry/entryInfo` 返回 false。
- `listEntries`：
  - `archive_read_new()` + `archive_read_support_filter_all()` + `archive_read_support_format_all()`
  - 若该包已存密码：`archive_read_add_passphrase()`
  - `archive_read_open_filename(path, 65536)`
  - 遍历头，只收集 regular file
  - 名称解码：`archive_entry_pathname_utf8()`（可用时）/ `archive_entry_pathname()` 原始字节 → `QString::fromUtf8`；含替换字符则用 `QStringDecoder("GB18030")` 重解
  - 记录 `archive_entry_size()`（可能为 -1，按 0 处理）、`archive_entry_mtime()`、`archive_entry_is_encrypted()`
  - 若打开/读头失败且错误串包含 `passphrase` / `password` / `encrypted`（大小写不敏感），置 `passwordRequired=true`
  - 索引缓存：key = 压缩包绝对路径，value = { 源文件 mtime + size, entries, path→index, hasEncrypted, passwordRequired }，最多缓存 4 个包；命中时先校验源文件 mtime/size
  - 任何失败都返回空列表，并 `qDebug()` 记录 `archive_error_string()`（不含密码）
- `readEntry`：
  - LRU：最多 3 个 `QByteArray`，key = 虚拟路径；命中直接返回
  - 未命中：重新打开包，若已存密码则 `archive_read_add_passphrase()`
  - 按包内路径匹配，`archive_read_data` 循环读满 `archive_entry_size`
  - 大小未知（-1）时动态 append
  - 读失败且错误串指示密码问题时置 `passwordRequired=true`
  - 结果放入 LRU 后返回
- 线程安全：`QMutexLocker` 覆盖全部公开方法。
- 不写临时文件。

### 4.3 加密压缩包策略

- 密码只保存在 `ArchiveBackend` 内存中（按压缩包路径），不写配置、不写日志、不进 MRU。
- 打开流程（`Core::loadArchive`，GUI 线程）：
  1. 先按已有缓存/无密码尝试 `model->mountArchive(archivePath)`。
  2. 若失败且 `backend->passwordRequired(archivePath)` 为真，弹 `QInputDialog::getText`（密码模式，标题“压缩包需要密码”）。
  3. 用户取消 → 放弃打开，当前图片不变。
  4. 输入后 `setPassword` 并重试挂载；错误再弹，最多 3 次。
  5. 3 次仍失败 → `mw->showError(tr("密码错误或该压缩包不支持解密"))`，`clearPassword`，保留当前图片。
- 只加密部分条目时：`mountArchive` 成功但 `hasEncryptedEntries()` 为真，仍对首个图片条目做一次 `readEntry` 探测；失败则进入上述密码流程。
- 加密文件名（7z/RAR5 头加密）导致无法列目录时，由 `passwordRequired` 触发同一流程。
- 非加密的损坏包：`passwordRequired` 为假，直接报“无法打开压缩包”。
- 探测失败且 `passwordRequired` / `unsupportedEncryption` 均为假（如 RAR4 数据加密：能列目录但解不开）时，提示“该压缩包的加密方式暂不支持”，不弹密码框。

实测支持矩阵（libarchive 3.8.7）：

| 格式 / 加密方式 | 密码支持 |
|---|---|
| zip / cbz，ZipCrypto | 是 |
| zip / cbz，AES-128/256 | 是 |
| 7z / cb7，数据加密 | 否 |
| 7z / cb7，头加密（`-mhe=on`） | 否 |
| rar，RAR4 数据/头加密 | 否 |
| rar5，加密 | 未实测 |
| tar | 无加密概念 |

非目标：不引入外部解压器（Ark / 7z CLI / unrar）兜底，保持单一 libarchive 依赖。

## 5. 修改文件（精确到函数）

### 5.1 CMake

根 `CMakeLists.txt`：
- 在 `find_package` 区域新增：
  ```cmake
  find_package(LibArchive QUIET)
  if(LibArchive_FOUND)
      message(STATUS "libarchive: enabled")
  else()
      message(STATUS "libarchive: not found, archive viewing disabled")
  endif()
  ```
- 不新增 option（自动检测即可）。

`qimgv/CMakeLists.txt`：
- 在 EXIV2/KDE 链接块附近新增：
  ```cmake
  if(LibArchive_FOUND)
      target_link_libraries(qimgv PRIVATE LibArchive::LibArchive)
      target_compile_definitions(qimgv PRIVATE USE_ARCHIVE)
  endif()
  ```

`qimgv/utils/CMakeLists.txt`：加入 `archivepath.cpp`。

`qimgv/components/CMakeLists.txt`：加入 `archive/archivebackend.cpp`。

### 5.2 `qimgv/sourcecontainers/documentinfo.h/.cpp`

新增私有成员：
```cpp
bool mArchive = false;
QString mArchivePath;   // 真实压缩包
QString mInnerPath;     // 包内路径
QByteArray mArchiveData; // 解压内容（Qt 隐式共享，与 LRU 共享同一缓冲）
qint64 mArchiveSize = 0;
QDateTime mArchiveModified;
void initArchive(const QString &virtualPath);
void detectFormatFromData();
bool detectAPNGData();
bool detectAnimatedWebPData();
bool detectAnimatedAvifData();
bool detectAnimatedJxlData();
QByteArray headBytes(int n) const;
```
新增公开：
```cpp
bool isArchive() const;
QByteArray archiveData() const;
```

构造函数：
```cpp
if(ArchivePath::isArchivePath(path)) { initArchive(path); return; }
// 原 fileInfo 逻辑不变
```

`initArchive`：
- `ArchivePath::split`
- `ArchiveBackend::instance()->entryInfo(...)` → size/mtime
- `ArchiveBackend::instance()->readEntry(...)` → mArchiveData
- 失败则 `mDocumentType = NONE`、`qDebug()`，直接返回（保持“保留当前图片”由上层处理）
- `detectFormatFromData()`

`detectFormatFromData()`：
- `QMimeDatabase::mimeTypeForData(mArchiveData)` + 包内后缀
- 分支逻辑与现有 `detectFormat()` 完全一致：
  - jpeg→jpg/STATIC；png→apng 检测/STATIC；gif→ANIMATED；webp→动画位检测；jxl→动画检测（受 `settings->jxlAnimation()`）；avif→动画检测；bmp→STATIC
  - 兜底：后缀（jfif→jpg），STATIC
  - **不识别视频**：即使后缀命中视频也按 STATIC/兜底处理
- `loadExifOrientation()` 走 archive 分支

各 getter 的 archive 分支：
- `directoryPath()`：`QFileInfo(mArchivePath).absolutePath()`
- `filePath()`：虚拟路径
- `fileName()`：`QFileInfo(mInnerPath).fileName()`
- `baseName()`：`QFileInfo(mInnerPath).baseName()`
- `fileSize()`：`mArchiveSize`
- `lastModified()`：`mArchiveModified`
- `refresh()`：archive 直接 return
- `mimeType()`：由 `detectFormatFromData` 填充

动画检测函数改为“数据版”（文件版保留给非压缩包）：
- `detectAPNG` 文件分支不变；新增 `detectAPNGData`：`mArchiveData.left(120).contains("acTL")`
- WebP：检查 offset 12 起 `VP8X` 与 flags bit1
- AVIF：offset 4 起 8 字节 `ftypavis`
- JXL：`QBuffer` + `QImageReader(..., "jxl").supportsAnimation()`

`loadExifOrientation()`：
- archive：`QBuffer` 包住 `mArchiveData`，`QImageReader(&buf, format)`，取 `transformation()`

`loadExifTags()`：
- archive：`Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(mArchiveData.constData()), size_t(mArchiveData.size()))`
- 其余 EXIF 解析代码不变

### 5.3 `qimgv/sourcecontainers/imagestatic.cpp`

- `load()`：
  ```cpp
  if(!mDocInfo->isArchive() && mimeType == "image/vnd.microsoft.icon")
      loadICO();
  else
      loadGeneric();
  ```
- `loadGeneric()`：archive 分支
  ```cpp
  QByteArray data = mDocInfo->archiveData();
  QBuffer buf(&data);
  buf.open(QIODevice::ReadOnly);
  QImageReader r(&buf, format);
  ```
  其余 allocationLimit / exifRotated / Mono 转换逻辑不变。
- `save()` / `save(destPath)`：archive 直接 `return false;`（Core 层已拦截，双保险）。

### 5.4 `qimgv/sourcecontainers/imageanimated.h/.cpp`

- 新增成员：`QByteArray mData; std::unique_ptr<QBuffer> mBuffer;`
- `loadMovie()`：archive 分支
  ```cpp
  mData = mDocInfo->archiveData();
  mBuffer.reset(new QBuffer(&mData));
  mBuffer->open(QIODevice::ReadOnly);
  movie->setDevice(mBuffer.get());
  ```
  非 archive 保持 `setFileName`。
- `getPixmap()` / `getImage()`：archive 用 `QImageReader` + `QBuffer` 读当前帧；非 archive 不变。
- `save(...)`：archive 分支 `return false;`

### 5.5 `qimgv/components/thumbnailer/thumbnailerrunnable.cpp`

- 新增静态/私有辅助 `createThumbnailFromData(const QByteArray&, const char *format, int size, bool squared)`，逻辑复制自 `createThumbnail`，把 `QImageReader(path)` 换成 `QBuffer`。
- `generate()` 中：
  ```cpp
  if(imgInfo.isArchive())
      pair = createThumbnailFromData(imgInfo.archiveData(), format, size, crop);
  else if(imgInfo.type() == VIDEO) ...
  else pair = createThumbnail(...);
  ```
- 目的：保证任何缩略图路径不会对虚拟路径做文件 I/O。

### 5.6 `qimgv/components/directorymanager/directorymanager.h/.cpp`

- `enum FileListSource` 增加 `SOURCE_ARCHIVE`。
- 新增公开：`bool mountArchive(QString archivePath);`、`bool archiveHasEncryptedEntries() const;`
- 新增私有：`bool isArchiveImageEntry(const QString &innerPath) const;`、`bool mArchiveEncrypted = false;`
  - 用 `QImageReader::supportedImageFormats()` + `jfif` 构建（**不含视频**）
- `mountArchive`：
  1. `stopFileWatcher()`
  2. 校验源文件是 regular file
  3. `ArchiveBackend::instance()->listEntries(archivePath)`（自动使用已存密码）
  4. 空则返回 false
  5. `mArchiveEncrypted = backend->hasEncryptedEntries(archivePath)`
  6. `mListSource = SOURCE_ARCHIVE; mDirectoryPath = archivePath;`
  7. 清空 `fileEntryVec/dirEntryVec`
  8. 对每个非目录且 `isArchiveImageEntry` 的条目：
     - `path = ArchivePath::join(archivePath, e.path)`
     - `name = e.path`（包内完整路径）
     - `size = e.size`
  9. `sortEntryLists()`
  10. `emit loaded(archivePath)`
  11. `return true`
- `directoryPath()`：条件扩展到 `SOURCE_ARCHIVE`，返回 `mDirectoryPath`（压缩包真实路径）。
- `lastModified()`：虚拟路径走 `ArchiveBackend::entryInfo` 取 mtime；失败返回无效值。
- 其余 setDirectory / watcher / forceInsert 等对 archive 不适用，不做改动（不会被调用）。

### 5.7 `qimgv/components/directorymodel.h/.cpp`

- 新增 `bool mountArchive(QString path);`、`bool archiveHasEncryptedEntries() const;`
- 实现：
  ```cpp
  cache.clear();
  return dirManager.mountArchive(path);
  ```
- `setDirectory` 保持原样（切回真实目录会自然卸载压缩包并清缓存）。

### 5.8 `qimgv/core.h/.cpp`

`loadPath`：
- 在 `fileInfo.isFile()` 分支内，优先判断：
  ```cpp
  if(ArchivePath::isArchiveExtension(fileInfo.absoluteFilePath()))
      return loadArchive(fileInfo.absoluteFilePath());
  ```
- 新增私有 `bool loadArchive(QString archivePath);`：
  1. `stopSlideshow()`
  2. 首次 `model->mountArchive(archivePath)`
  3. 若失败且 `ArchiveBackend::instance()->passwordRequired(archivePath)`：
     - 循环最多 3 次：`QInputDialog::getText(this, tr("压缩包需要密码"), tr("密码："), QLineEdit::Password)`
     - 取消/空 → 返回 false（当前图不变）
     - `setPassword` → 重试 `mountArchive`
     - 成功则跳出
     - 3 次失败 → `showError(tr("密码错误或该压缩包不支持解密"))`、`clearPassword`、返回 false
  4. 若挂载成功但 `model->archiveHasEncryptedEntries()`，对第一个图片条目 `readEntry` 探测一次：
     - 失败且 `passwordRequired` → 走第 3 步密码流程
     - 失败且 `unsupportedEncryption`，或两者均为假 → `showError(tr("该压缩包的加密方式暂不支持"))`，返回 false
  5. 仍失败 → `showError(tr("无法打开压缩包：") + archivePath)`，返回 false
  6. `state.directoryPath = archivePath`
  7. `mw->setDirectoryPath(QFileInfo(archivePath).absolutePath())`
  8. `mw->enableDocumentView()`
  9. `fileCount() == 0` → `mw->showMessage(tr("压缩包内没有图片"))`，返回 false
  10. `return loadFileIndex(0, false, settings->usePreloader())`
- 额外支持直接传虚拟路径（CLI/剪贴板）：
  ```cpp
  if(ArchivePath::isArchivePath(path)) {
      QString a, inner;
      ArchivePath::split(path, a, inner);
      if(!loadArchive(a)) return false;
      int i = model->indexOfFile(path);
      return i >= 0 ? loadFileIndex(i, false, settings->usePreloader()) : false;
  }
  ```
- `loadParentDir` 退出压缩包时同时 `ArchiveBackend::instance()->clearPassword(model->directoryPath())`（可选，建议保留会话内密码，不做强制清除）。

`loadParentDir()`（`goUp`）：
- 新增压缩包分支（在现有 FOLDERVIEW 守卫之前）：
  ```cpp
  if(model->source() == SOURCE_ARCHIVE) {
      loadPath(QFileInfo(model->directoryPath()).absolutePath());
      return;
  }
  ```
- 其余文件夹逻辑不变。

只读拦截：新增私有
```cpp
bool Core::archiveReadOnly();
```
实现：若 `ArchivePath::isArchivePath(state.currentFilePath)` 或 `model->source() == SOURCE_ARCHIVE`，`mw->showMessage(tr("压缩包内文件不可修改"))` 并返回 true。

**仅文件级写操作**在以下函数入口调用并直接 return：
- `removePermanent()`
- `moveToTrash()`
- `renameCurrentSelection(...)`
- `copyCurrentFile(...)`
- `moveCurrentFile(...)`
- `copyPathsTo(...)`
- `movePathsTo(...)`
- `saveCurrentFile()`
- `saveCurrentFileAs(...)`
- `requestSavePath()`
- `cropAndSave(...)`（含保存）
- `onDraggedOut(...)` / `getMimeDataForImage(...)`（拖出=复制文件）
- `saveFile(...)` 双保险：archive 直接返回 false

**允许的操作（不拦截）**：
- `crop(...)`、`resize(...)`、`edit_template(...)` 的 flip/rotate（仅内存编辑，不落盘）
- `setWallpaper()`、复制图片到剪贴板
- 注意：内存编辑后 `info.edited` 会显示 `*`，但保存会被拦截，符合“只读”预期。

`updateInfoString()`：
- `const QString vp = state.currentFilePath;`
- 若 `ArchivePath::isArchivePath(vp)`，传给 `mw->setCurrentInfo` 的 fileName 用 `ArchivePath::displayName(vp)`，其余参数不变。
- `fileSize` 来自 `img->fileSize()`，即解压后大小。
- 不拼接压缩包名。

### 5.9 `qimgv/gui/mainwindow.cpp`

`MW::showOpenDialog`：
```cpp
imageFilter.append("Archives (*.zip *.cbz *.7z *.cb7 *.rar *.cbr *.tar)");
```
放在 “All Files” 之前。

不修改 `MW::onInfoUpdated`（文件名 basename + index + size 已满足要求）。

### 5.10 设置

- **不**改 `Settings::supportedFormats()` / `supportedFormatsRegex()` / `supportedMimeTypes()`。
- `supportedFormatsFilter()` 可保持不变；压缩包过滤器只在打开对话框追加。

## 6. 行为定义

| 场景 | 行为 |
|---|---|
| 打开压缩包 | 挂载索引，显示第一张图，文档视图 |
| 上/下一张 | 按包内完整路径排序，循环/到头行为沿用现有设置 |
| info | `[i/n]` + 包内 basename + 解压后大小 |
| 右键 1:1 / fit / 滚动 | 完全不改，走现有 Image |
| 预加载 | 正常预加载前后各一张 |
| 包内视频 | 索引阶段排除，不显示 |
| 加密包 | 弹密码框，最多 3 次；成功可看，取消/失败保留当前图 |
| 损坏/不支持 | 错误提示，当前图不变 |
| 保存/另存/删除/重命名/复制/移动/拖出 | 提示“压缩包内文件不可修改” |
| 裁剪/旋转/翻转/缩放（不保存） | 允许，内存内生效，`*` 标记 |
| 设壁纸/复制图片到剪贴板 | 允许 |
| goUp | 退出压缩包，回到压缩包所在文件夹（文件夹视图） |
| 关闭压缩包（Esc） | 不改，维持现有行为 |
| 无 libarchive 构建 | 能编译；打开压缩包提示无法打开 |

## 7. 验证

1. 构建：
   - `cmake --build build-full -j$(nproc)`
   - 期望：无编译错误；日志出现 `libarchive: enabled`
2. 运行验证（用测试包）：
   - `zip`：普通 jpg/png 混合，验证顺序、info、切图居中、1:1
   - `cbz`：验证扩展名大小写与中文文件名
   - `7z` / `rar`：验证可读
   - 嵌套目录：验证按完整路径排序
   - 含 gif：验证动画播放
   - 含视频文件：验证不出现
   - 加密包：正确密码可浏览；错误密码 3 次后提示；取消后当前图不变
   - 加密文件名包（7z/RAR5 头加密）：能触发密码框并挂载
   - 只读操作：保存/删除/重命名/复制/移动/拖出被拦截并提示
   - 内存编辑：裁剪/旋转可操作，保存被拦截
   - `goUp`：回到压缩包所在文件夹
3. 回归：
   - 普通文件夹看图、缩略图条、文件夹视图不受影响
   - 单图打开、排序、预加载不受影响
4. 无 libarchive 构建（可选）：
   - 临时用 `-DCMAKE_DISABLE_FIND_PACKAGE_LibArchive=ON` 配置一次，确认可编译

## 8. 不做（Out of scope）

- 压缩包内缩略图条/文件夹视图封面
- 写回压缩包
- 多卷/嵌套压缩包
- 压缩包内视频播放
- 修改中文 README 文案
- 改动显示缩放/公式逻辑

## 9. 已确认决策

- 支持全部格式：zip/cbz/7z/cb7/rar/cbr/tar。
- 虚拟路径分隔符：`::`。
- 只读拦截仅限文件级写操作；裁剪/旋转/翻转/缩放允许但不保存。
- 加密包：支持内存密码输入，最多 3 次，不落盘。

## 10. 待用户批准

- 密码框文案：标题“压缩包需要密码”，标签“密码：”，按钮沿用系统默认。
- 加密支持受 libarchive 后端限制（部分 RAR/7z 变体可能无法解密），失败按“密码错误或该压缩包不支持解密”提示。
- 批准后按 `tasks.md` 顺序实施。
