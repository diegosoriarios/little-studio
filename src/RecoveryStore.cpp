#include "RecoveryStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

QString headerPrefix()
{
    return QStringLiteral("lite-studio-recovery: ");
}

} // namespace

namespace RecoveryStore {

QString recoveryDir()
{
    QString cache = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation);
    if (cache.isEmpty())
        cache = QDir::tempPath() + QStringLiteral("/lite-studio");
    return cache + QStringLiteral("/recovery");
}

QString snapshotPath(const QString &originalPath)
{
    const QByteArray hash = QCryptographicHash::hash(
        originalPath.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return recoveryDir() + QStringLiteral("/")
        + QString::fromLatin1(hash) + QStringLiteral(".txt");
}

bool writeSnapshot(const QString &originalPath, const QString &text)
{
    if (originalPath.isEmpty())
        return false;
    QDir().mkpath(recoveryDir());

    QFile file(snapshotPath(originalPath));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray header = (headerPrefix() + originalPath
        + QLatin1Char('\n')).toUtf8();
    const QByteArray body = text.toUtf8();
    return file.write(header) == header.size()
        && file.write(body) == body.size();
}

QList<Snapshot> listSnapshots()
{
    QList<Snapshot> snapshots;
    QDir dir(recoveryDir());
    const QStringList files = dir.entryList(
        { QStringLiteral("*.txt") }, QDir::Files, QDir::Name);
    for (const QString &name : files) {
        QFile file(dir.absoluteFilePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = file.read(4 * 1024 * 1024 + 4096);
        const QString content = QString::fromUtf8(data);
        const QString header = headerPrefix();
        if (!content.startsWith(header))
            continue;
        const int firstLine = content.indexOf(QLatin1Char('\n'));
        if (firstLine < 0)
            continue;
        Snapshot snapshot;
        snapshot.originalPath = content.mid(header.size(),
                                            firstLine - header.size());
        snapshot.text = content.mid(firstLine + 1);
        if (!snapshot.originalPath.isEmpty())
            snapshots.append(snapshot);
    }
    return snapshots;
}

void removeSnapshot(const QString &originalPath)
{
    QFile::remove(snapshotPath(originalPath));
}

void clearSnapshots()
{
    QDir dir(recoveryDir());
    const QStringList files = dir.entryList(
        { QStringLiteral("*.txt") }, QDir::Files);
    for (const QString &name : files)
        dir.remove(name);
}

} // namespace RecoveryStore
