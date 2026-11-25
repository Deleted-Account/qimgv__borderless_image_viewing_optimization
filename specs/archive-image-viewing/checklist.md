# Checklist: 压缩包内看图 v1

- [x] 检测到 libarchive 时链接并定义 `USE_ARCHIVE`；未检测到时编译降级
- [x] 虚拟路径统一使用 `::`，`ArchivePath` 的 join/split/innerPath/archivePath/displayName 行为正确
- [x] `ArchiveBackend` 建立索引并标记加密条目；密码只保存在内存
- [x] `ArchiveBackend::readEntry` 带读取缓存与串行保护
- [x] `DocumentInfo` 虚拟路径构造、格式/动画检测、getter 均走 archive 分支
- [x] `ImageStatic` / `ImageAnimated` 从内存数据解码，`save()` 对 archive 返回 false
- [x] `Thumbnailer` 对 archive 使用内存数据生成缩略图
- [x] `DirectoryManager::mountArchive` 只收录图片，排除包内视频
- [x] `Core::loadArchive` 密码最多尝试 3 次，取消即退出并给出提示
- [x] `Core::loadPath` 支持压缩包真实路径与 `archive::inner` 虚拟路径
- [x] `Core::loadParentDir` 从压缩包退出到其所在目录
- [x] `Core::updateInfoString` 只显示包内 basename、`[i/n]` 与解压后大小
- [x] 只读守卫拦截文件级写操作，编辑类操作允许但不落盘
- [x] 打开对话框包含 Archives 过滤器
- [x] 文档更新（FORK_DIFFERENCES 中英文、ARCHITECTURE）
- [x] 加密格式支持矩阵已实测并写入文档；明确不引入外部解压器兜底
- [ ] rar/cbr 用真实样本实测
- [ ] `sudo ./deploy-system.sh` 部署并在桌面实测
