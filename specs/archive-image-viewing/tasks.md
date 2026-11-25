# Tasks: 压缩包内看图 v1

状态：实施完成，待部署。按顺序执行，每阶段增量构建。

## T1. 构建接入
- [x] 根 `CMakeLists.txt`：`find_package(LibArchive QUIET)` + 状态日志
- [x] `qimgv/CMakeLists.txt`：`LibArchive::LibArchive` 链接 + `USE_ARCHIVE`
- [x] `qimgv/utils/CMakeLists.txt`：加入 `archivepath.cpp`
- [x] `qimgv/components/CMakeLists.txt`：加入 `archive/archivebackend.cpp`
- 验证：`cmake --build build-full -j$(nproc)` 通过

## T2. 虚拟路径工具
- [x] 新建 `qimgv/utils/archivepath.h/.cpp`
- [x] 实现 `isArchiveExtension / isArchivePath / join / split / innerPath / archivePath / displayName`
- 验证：编译通过

## T3. ArchiveBackend
- [x] 新建 `qimgv/components/archive/archivebackend.h/.cpp`
- [x] libarchive 索引、UTF-8/GB18030 解码、索引缓存
- [x] `archive_entry_is_encrypted` 标记 + `passwordRequired` 探测
- [x] 内存密码：`setPassword/clearPassword/hasPassword` + `archive_read_add_passphrase`
- [x] `readEntry` + 3 条 LRU + QMutex 串行
- [x] `USE_ARCHIVE` 未定义时安全降级
- 验证：编译通过

## T4. DocumentInfo 虚拟路径
- [x] 构造分支 `ArchivePath::isArchivePath`
- [x] `initArchive` / `detectFormatFromData` / 数据版动画检测
- [x] getter archive 分支（filePath/fileName/baseName/fileSize/lastModified/directoryPath/refresh）
- [x] `loadExifOrientation` 用 QBuffer
- [x] `loadExifTags` 用 `ImageFactory::open(data,size)`
- 验证：编译通过

## T5. ImageStatic
- [x] `load()` 对 archive 走 loadGeneric（ICO 也用 QImageReader）
- [x] `loadGeneric()` QBuffer 分支
- [x] `save()` archive 返回 false
- 验证：编译通过

## T6. ImageAnimated
- [x] `mData` + `mBuffer` 成员
- [x] `loadMovie()` archive 用 `setDevice`
- [x] `getPixmap/getImage` archive 用 QImageReader+QBuffer
- [x] `save()` archive 返回 false
- 验证：编译通过

## T7. Thumbnailer
- [x] `createThumbnailFromData` + `generate()` archive 分支
- 验证：编译通过

## T8. DirectoryManager 挂载
- [x] `SOURCE_ARCHIVE`
- [x] `mountArchive`
- [x] archive 图片扩展名过滤（排除视频）
- [x] `archiveHasEncryptedEntries()` + `mArchiveEncrypted`
- [x] `directoryPath()` / `lastModified()` 适配
- 验证：编译通过

## T9. DirectoryModel
- [x] `mountArchive` → `cache.clear()` + `dirManager.mountArchive`
- [x] `archiveHasEncryptedEntries()` 透传
- 验证：编译通过

## T10. Core 加载、密码与退出
- [x] `loadArchive`
- [x] 密码流程：`QInputDialog` 最多 3 次、取消返回、错误提示
- [x] 加密条目探测（首图 `readEntry`）
- [x] `loadPath` 压缩包/虚拟路径分支
- [x] `loadParentDir` 退出压缩包
- [x] `updateInfoString` 显示 basename
- 验证：编译通过

## T11. Core 只读拦截（仅文件级写操作）
- [x] `archiveReadOnly()`
- [x] 拦截：保存/另存/删除/重命名/复制/移动/拖出/cropAndSave
- [x] 不拦截：crop/rotate/flip/resize/setWallpaper/复制图片
- 验证：编译通过

## T12. 打开对话框过滤器
- [x] `MW::showOpenDialog` 追加 Archives 过滤器
- 验证：编译通过

## T13. 完整构建与运行验证
- [x] `cmake --build build-full -j$(nproc)`
- [x] zip/cbz/7z/tar 实测（DocumentInfo 逐项解码 OK；GUI offscreen 启动无错误）
- [ ] rar/cbr 实测（本机无 rar 生成工具；已确认 libarchive 含 `archive_read_support_format_rar` / `rar5`，读取路径与其它格式一致）
- [x] 中文名、嵌套目录、gif、含视频包（视频被过滤，Qt 图片后缀不含 mp4）
- [x] 加密包：正确密码可解码；错误密码要求重试；7z 头加密/数据加密识别为不支持
- [x] 密码支持矩阵实测：zip ZipCrypto/AES 可读；7z 数据/头加密不支持；RAR4 数据/头加密不支持（libarchive 官方样本）
- [x] 只读：保存/删除/重命名/复制/移动/拖出拦截；裁剪旋转允许（代码级验证）
- [x] `goUp` 退出（代码级验证：`loadParentDir` 走 archive 分支）
- [x] 普通目录看图回归（`--gen-thumbs` 正常生成缩略图）

## T14. 部署
- [ ] `sudo ./deploy-system.sh`（需用户本机 sudo）
- [ ] 启动实测

## T15. 文档
- [x] `FORK_DIFFERENCES.zh-CN.md` 增加压缩包看图说明
- [x] `FORK_DIFFERENCES.md` 对应英文
- [x] `ARCHITECTURE.md` 增加压缩包数据流
- [x] `README.zh-CN.md` 文案由用户决定，未改动
