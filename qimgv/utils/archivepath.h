#pragma once

#include <QString>

// Virtual paths for files inside archives:
//   /abs/archive.cbz::inner/path.jpg
namespace ArchivePath {

// true if the real file path has an archive suffix
bool isArchiveExtension(const QString &path);

// true if path is a virtual path pointing inside an archive
bool isArchivePath(const QString &path);

QString join(const QString &archivePath, const QString &innerPath);

bool split(const QString &virtualPath, QString &archivePath, QString &innerPath);

QString innerPath(const QString &virtualPath);

QString archivePath(const QString &virtualPath);

// basename of the entry inside the archive
QString displayName(const QString &virtualPath);

}
