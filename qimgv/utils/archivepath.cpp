#include "archivepath.h"

#include <QFileInfo>
#include <QStringList>

namespace {

const char *kSeparator = "::";

QString normalizeInner(QString inner) {
    inner.replace('\\', '/');
    while(inner.startsWith("./"))
        inner.remove(0, 2);
    while(inner.startsWith('/'))
        inner.remove(0, 1);
    return inner;
}

}

namespace ArchivePath {

bool isArchiveExtension(const QString &path) {
    static const QStringList extensions = {
        "zip", "cbz", "7z", "cb7", "rar", "cbr", "tar"
    };
    return extensions.contains(QFileInfo(path).suffix().toLower());
}

bool isArchivePath(const QString &path) {
    return path.contains(kSeparator);
}

QString join(const QString &archivePath, const QString &innerPath) {
    return archivePath + kSeparator + normalizeInner(innerPath);
}

bool split(const QString &virtualPath, QString &archivePath, QString &innerPath) {
    int index = virtualPath.indexOf(kSeparator);
    if(index < 0)
        return false;
    archivePath = virtualPath.left(index);
    innerPath = virtualPath.mid(index + 2);
    return !archivePath.isEmpty() && !innerPath.isEmpty();
}

QString innerPath(const QString &virtualPath) {
    QString archive, inner;
    split(virtualPath, archive, inner);
    return inner;
}

QString archivePath(const QString &virtualPath) {
    QString archive, inner;
    split(virtualPath, archive, inner);
    return archive;
}

QString displayName(const QString &virtualPath) {
    return QFileInfo(innerPath(virtualPath)).fileName();
}

}
