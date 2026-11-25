#pragma once

#include <QString>
#include <QDateTime>
#include <QList>
#include <QByteArray>
#include <QHash>
#include <QStringList>
#include <QMutex>

struct ArchiveEntry {
    QString path;      // full path inside the archive
    qint64 size = 0;   // uncompressed size
    QDateTime modified;
    bool isDir = false;
    bool encrypted = false;
};

// Reads archives through libarchive. All operations are serialized.
// Metadata is indexed once per archive; only recently read entries are kept
// in memory (no extraction to disk).
class ArchiveBackend {
public:
    static ArchiveBackend *instance();

    QList<ArchiveEntry> listEntries(const QString &archivePath);
    bool entryInfo(const QString &archivePath, const QString &innerPath, ArchiveEntry &out);
    bool readEntry(const QString &archivePath, const QString &innerPath, QByteArray &out);

    // Passwords are kept in memory only.
    void setPassword(const QString &archivePath, const QString &password);
    void clearPassword(const QString &archivePath);
    bool hasPassword(const QString &archivePath) const;
    bool passwordRequired(const QString &archivePath) const;
    bool unsupportedEncryption(const QString &archivePath) const;
    bool hasEncryptedEntries(const QString &archivePath) const;

    void clearCache(const QString &archivePath);

private:
    ArchiveBackend();
    Q_DISABLE_COPY(ArchiveBackend)

    struct IndexCache {
        qint64 fileSize = 0;
        qint64 mtimeMs = -1;
        QList<ArchiveEntry> entries;
        QHash<QString, int> byPath;
        bool hasEncrypted = false;
        bool passwordRequired = false;
        bool unsupportedEncryption = false;
    };

    void touchIndex(const QString &key);
    void touchRead(const QString &key);
    void storeRead(const QString &key, const QByteArray &data);
    void markPasswordRequired(const QString &archivePath);
    void markUnsupportedEncryption(const QString &archivePath);

    mutable QMutex m_mutex;
    QHash<QString, IndexCache> m_indexes;
    QStringList m_indexOrder;
    QHash<QString, QString> m_passwords;
    QHash<QString, QByteArray> m_readCache;
    QStringList m_readOrder;
};
