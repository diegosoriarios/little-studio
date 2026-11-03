#ifndef LITE_STUDIO_RECOVERY_STORE_H
#define LITE_STUDIO_RECOVERY_STORE_H

#include <QList>
#include <QString>

// Crash-safe snapshots of unsaved editor buffers, stored in the app's cache
// directory. Snapshot format: first line is the original file path, the rest
// is the recovered text (always UTF-8).
namespace RecoveryStore {

struct Snapshot {
    QString originalPath;
    QString text;
};

QString recoveryDir();
QString snapshotPath(const QString &originalPath);

bool writeSnapshot(const QString &originalPath, const QString &text);
QList<Snapshot> listSnapshots();
void removeSnapshot(const QString &originalPath);
void clearSnapshots();

} // namespace RecoveryStore

#endif
