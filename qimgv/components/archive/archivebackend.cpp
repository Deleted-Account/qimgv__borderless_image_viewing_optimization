#include "archivebackend.h"

#include <QFileInfo>
#include <QFile>
#include <QMutexLocker>
#include <QDebug>
#include <QStringList>
#include <algorithm>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#else
#include <QTextCodec>
#endif

#ifdef USE_ARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace {

const int kMaxIndexes = 4;
const int kMaxReadCache = 3;

QString normalizeEntryPath(QString path) {
    path.replace('\\', '/');
    while(path.startsWith("./"))
        path.remove(0, 2);
    while(path.startsWith('/'))
        path.remove(0, 1);
    return path;
}

QString decodeEntryName(const char *raw, const char *utf8) {
    if(utf8 && *utf8)
        return QString::fromUtf8(utf8);
    if(!raw)
        return QString();
    QString name = QString::fromUtf8(raw);
    if(name.contains(QChar::ReplacementCharacter)) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringDecoder decoder("GB18030");
        if(decoder.isValid()) {
            QString alt = decoder(QByteArray(raw));
            if(!alt.contains(QChar::ReplacementCharacter))
                name = alt;
        }
#else
        QTextCodec *codec = QTextCodec::codecForName("GB18030");
        if(codec) {
            QString alt = codec->toUnicode(raw);
            if(!alt.contains(QChar::ReplacementCharacter))
                name = alt;
        }
#endif
    }
    return name;
}

#ifdef USE_ARCHIVE
bool looksLikePasswordError(const char *message) {
    if(!message)
        return false;
    const QString msg = QString::fromUtf8(message).toLower();
    return msg.contains("passphrase") || msg.contains("password") || msg.contains("encrypt");
}

bool looksLikeUnsupportedEncryption(const char *message) {
    if(!message)
        return false;
    const QString msg = QString::fromUtf8(message).toLower();
    return msg.contains("not supported") || msg.contains("unsupported");
}

// Prevents libarchive from prompting on the terminal when no password is cached.
const char *passphraseCallback(struct archive *, void *clientData) {
    const QByteArray *password = static_cast<const QByteArray*>(clientData);
    if(!password || password->isEmpty())
        return nullptr;
    return password->constData();
}
#endif

}

ArchiveBackend *ArchiveBackend::instance() {
    static ArchiveBackend backend;
    return &backend;
}

ArchiveBackend::ArchiveBackend() {
}

// ##############################################################
// ####################### PUBLIC METHODS #######################
// ##############################################################

QList<ArchiveEntry> ArchiveBackend::listEntries(const QString &archivePath) {
    QMutexLocker locker(&m_mutex);

    const QString key = QFileInfo(archivePath).absoluteFilePath();
    QFileInfo info(key);
    if(!info.isFile())
        return {};

    const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
    const qint64 fileSize = info.size();
    auto cached = m_indexes.find(key);
    if(cached != m_indexes.end() && cached->mtimeMs == mtimeMs
            && cached->fileSize == fileSize && !cached->passwordRequired) {
        touchIndex(key);
        return cached->entries;
    }

#ifdef USE_ARCHIVE
    struct archive *a = archive_read_new();
    if(!a)
        return {};
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    QByteArray passwordBytes = m_passwords.value(key).toUtf8();
    archive_read_set_passphrase_callback(a, &passwordBytes, passphraseCallback);
    if(!passwordBytes.isEmpty())
        archive_read_add_passphrase(a, passwordBytes.constData());

    const QByteArray encodedPath = QFile::encodeName(key);
    bool passwordRequired = false;
    bool unsupportedEncryption = false;
    int result = archive_read_open_filename(a, encodedPath.constData(), 65536);
    if(result != ARCHIVE_OK) {
        const char *message = archive_error_string(a);
        unsupportedEncryption = looksLikeUnsupportedEncryption(message);
        if(!unsupportedEncryption)
            passwordRequired = looksLikePasswordError(message);
        qDebug() << "[ArchiveBackend] cannot open archive:" << key << message;
        archive_read_free(a);
        if(passwordRequired || unsupportedEncryption) {
            IndexCache failed;
            failed.fileSize = fileSize;
            failed.mtimeMs = mtimeMs;
            failed.passwordRequired = passwordRequired;
            failed.unsupportedEncryption = unsupportedEncryption;
            m_indexes.insert(key, failed);
            touchIndex(key);
        }
        return {};
    }

    IndexCache cache;
    cache.fileSize = fileSize;
    cache.mtimeMs = mtimeMs;

    struct archive_entry *entry = nullptr;
    while((result = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const QString inner = normalizeEntryPath(
            decodeEntryName(archive_entry_pathname(entry), archive_entry_pathname_utf8(entry)));
        if(inner.isEmpty())
            continue;

        const bool isDir = (archive_entry_filetype(entry) == AE_IFDIR);
        if(isDir)
            continue;

        ArchiveEntry item;
        item.path = inner;
        item.size = archive_entry_size(entry);
        if(item.size < 0)
            item.size = 0;
        item.modified = QDateTime::fromSecsSinceEpoch(archive_entry_mtime(entry));
        item.encrypted = archive_entry_is_encrypted(entry) == 1;

        if(item.encrypted)
            cache.hasEncrypted = true;

        cache.byPath.insert(item.path, cache.entries.size());
        cache.entries.append(item);
    }

    if(result != ARCHIVE_EOF && result < ARCHIVE_OK) {
        const char *message = archive_error_string(a);
        unsupportedEncryption = looksLikeUnsupportedEncryption(message);
        if(!unsupportedEncryption)
            passwordRequired = looksLikePasswordError(message);
        qDebug() << "[ArchiveBackend] error while indexing:" << key << message;
    }
    archive_read_free(a);

    cache.passwordRequired = passwordRequired;
    cache.unsupportedEncryption = unsupportedEncryption;
    m_indexes.insert(key, cache);
    touchIndex(key);
    return cache.entries;
#else
    return {};
#endif
}

bool ArchiveBackend::entryInfo(const QString &archivePath, const QString &innerPath, ArchiveEntry &out) {
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    listEntries(archivePath);
    QMutexLocker locker(&m_mutex);
    auto cached = m_indexes.find(key);
    if(cached == m_indexes.end())
        return false;
    auto it = cached->byPath.find(normalizeEntryPath(innerPath));
    if(it == cached->byPath.end())
        return false;
    out = cached->entries.at(it.value());
    return true;
}

bool ArchiveBackend::readEntry(const QString &archivePath, const QString &innerPath, QByteArray &out) {
    QMutexLocker locker(&m_mutex);

    const QString absPath = QFileInfo(archivePath).absoluteFilePath();
    const QString target = normalizeEntryPath(innerPath);
    const QString key = absPath + "::" + target;

    auto cached = m_readCache.find(key);
    if(cached != m_readCache.end()) {
        touchRead(key);
        out = cached.value();
        return true;
    }

#ifdef USE_ARCHIVE
    struct archive *a = archive_read_new();
    if(!a)
        return false;
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    QByteArray passwordBytes = m_passwords.value(absPath).toUtf8();
    archive_read_set_passphrase_callback(a, &passwordBytes, passphraseCallback);
    if(!passwordBytes.isEmpty())
        archive_read_add_passphrase(a, passwordBytes.constData());

    const QByteArray encodedPath = QFile::encodeName(absPath);
    if(archive_read_open_filename(a, encodedPath.constData(), 65536) != ARCHIVE_OK) {
        const char *message = archive_error_string(a);
        if(looksLikeUnsupportedEncryption(message))
            markUnsupportedEncryption(absPath);
        else if(looksLikePasswordError(message))
            markPasswordRequired(absPath);
        qDebug() << "[ArchiveBackend] cannot open archive for reading:" << absPath << message;
        archive_read_free(a);
        return false;
    }

    bool found = false;
    struct archive_entry *entry = nullptr;
    int result = ARCHIVE_OK;
    while((result = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const QString name = normalizeEntryPath(
            decodeEntryName(archive_entry_pathname(entry), archive_entry_pathname_utf8(entry)));
        if(name == target) {
            found = true;
            break;
        }
        archive_read_data_skip(a);
    }

    if(!found) {
        const char *message = archive_error_string(a);
        if(looksLikeUnsupportedEncryption(message))
            markUnsupportedEncryption(absPath);
        else if(looksLikePasswordError(message))
            markPasswordRequired(absPath);
        qDebug() << "[ArchiveBackend] entry not found:" << key << message;
        archive_read_free(a);
        return false;
    }

    QByteArray data;
    const qint64 expectedSize = archive_entry_size(entry);
    if(expectedSize > 0)
        data.reserve(static_cast<int>(expectedSize));

    char buffer[65536];
    for(;;) {
        const la_ssize_t bytes = archive_read_data(a, buffer, sizeof(buffer));
        if(bytes > 0)
            data.append(buffer, static_cast<int>(bytes));
        else if(bytes == 0)
            break;
        else {
            const char *message = archive_error_string(a);
            if(looksLikeUnsupportedEncryption(message))
                markUnsupportedEncryption(absPath);
            else if(looksLikePasswordError(message))
                markPasswordRequired(absPath);
            qDebug() << "[ArchiveBackend] cannot read entry:" << key << message;
            archive_read_free(a);
            return false;
        }
    }
    archive_read_free(a);

    storeRead(key, data);
    out = data;
    return true;
#else
    Q_UNUSED(target)
    return false;
#endif
}

void ArchiveBackend::setPassword(const QString &archivePath, const QString &password) {
    QMutexLocker locker(&m_mutex);
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    m_passwords.insert(key, password);
    // The index may have been built without the password (or with a wrong one),
    // so it must be rebuilt on the next call.
    m_indexes.remove(key);
    m_indexOrder.removeAll(key);
    const QString prefix = key + "::";
    for(auto it = m_readCache.begin(); it != m_readCache.end();) {
        if(it.key().startsWith(prefix))
            it = m_readCache.erase(it);
        else
            ++it;
    }
    m_readOrder.erase(std::remove_if(m_readOrder.begin(), m_readOrder.end(),
                                     [&prefix](const QString &k) { return k.startsWith(prefix); }),
                      m_readOrder.end());
}

void ArchiveBackend::clearPassword(const QString &archivePath) {
    QMutexLocker locker(&m_mutex);
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    m_passwords.remove(key);
    m_indexes.remove(key);
    m_indexOrder.removeAll(key);
}

bool ArchiveBackend::hasPassword(const QString &archivePath) const {
    QMutexLocker locker(&m_mutex);
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    return m_passwords.contains(key);
}

bool ArchiveBackend::passwordRequired(const QString &archivePath) const {
    QMutexLocker locker(&m_mutex);
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    auto cached = m_indexes.constFind(key);
    return cached != m_indexes.constEnd() && cached->passwordRequired;
}

bool ArchiveBackend::unsupportedEncryption(const QString &archivePath) const {
    QMutexLocker locker(&m_mutex);
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    auto cached = m_indexes.constFind(key);
    return cached != m_indexes.constEnd() && cached->unsupportedEncryption;
}

bool ArchiveBackend::hasEncryptedEntries(const QString &archivePath) const {
    QMutexLocker locker(&m_mutex);
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    auto cached = m_indexes.constFind(key);
    return cached != m_indexes.constEnd() && cached->hasEncrypted;
}

void ArchiveBackend::clearCache(const QString &archivePath) {
    QMutexLocker locker(&m_mutex);
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    m_indexes.remove(key);
    m_indexOrder.removeAll(key);
    const QString prefix = key + "::";
    for(auto it = m_readCache.begin(); it != m_readCache.end();) {
        if(it.key().startsWith(prefix))
            it = m_readCache.erase(it);
        else
            ++it;
    }
    m_readOrder.erase(std::remove_if(m_readOrder.begin(), m_readOrder.end(),
                                     [&prefix](const QString &k) { return k.startsWith(prefix); }),
                      m_readOrder.end());
}

// ##############################################################
// ####################### PRIVATE METHODS ######################
// ##############################################################

void ArchiveBackend::touchIndex(const QString &key) {
    m_indexOrder.removeAll(key);
    m_indexOrder.append(key);
    while(m_indexOrder.size() > kMaxIndexes) {
        const QString oldest = m_indexOrder.takeFirst();
        m_indexes.remove(oldest);
    }
}

void ArchiveBackend::touchRead(const QString &key) {
    m_readOrder.removeAll(key);
    m_readOrder.append(key);
}

void ArchiveBackend::storeRead(const QString &key, const QByteArray &data) {
    m_readCache.insert(key, data);
    touchRead(key);
    while(m_readOrder.size() > kMaxReadCache) {
        const QString oldest = m_readOrder.takeFirst();
        m_readCache.remove(oldest);
    }
}

void ArchiveBackend::markPasswordRequired(const QString &archivePath) {
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    auto cached = m_indexes.find(key);
    if(cached != m_indexes.end())
        cached->passwordRequired = true;
}

void ArchiveBackend::markUnsupportedEncryption(const QString &archivePath) {
    const QString key = QFileInfo(archivePath).absoluteFilePath();
    auto cached = m_indexes.find(key);
    if(cached != m_indexes.end()) {
        cached->unsupportedEncryption = true;
        cached->passwordRequired = false;
    }
}
