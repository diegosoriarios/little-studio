#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

#include "RecoveryStore.h"

// Headless tests for crash-recovery snapshot storage.

namespace {

int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n",                      \
                         __FILE__, __LINE__, #cond);                      \
            ++failures;                                                   \
        }                                                                 \
    } while (false)

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 1. Empty store.
    RecoveryStore::clearSnapshots();
    CHECK(RecoveryStore::listSnapshots().isEmpty());

    // 2. Write + read back a snapshot (path + text preserved).
    RecoveryStore::clearSnapshots();
    {
        const QString original = QDir::tempPath()
            + QStringLiteral("/lite-recovery-test/main.cpp");
        QDir().mkpath(QFileInfo(original).absolutePath());
        QFile file(original);
        CHECK(file.open(QIODevice::WriteOnly));
        file.close();

        const QString text = QStringLiteral("hello\nworld\n");
        CHECK(RecoveryStore::writeSnapshot(original, text));
        CHECK(QFileInfo::exists(RecoveryStore::snapshotPath(original)));

        const QList<RecoveryStore::Snapshot> snapshots =
            RecoveryStore::listSnapshots();
        CHECK(snapshots.size() == 1);
        if (snapshots.size() == 1) {
            CHECK(snapshots.first().originalPath == original);
            CHECK(snapshots.first().text == text);
        }
    }

    // 3. Snapshot path is deterministic per original path.
    CHECK(RecoveryStore::snapshotPath(QStringLiteral("/a/b.txt"))
          == RecoveryStore::snapshotPath(QStringLiteral("/a/b.txt")));
    CHECK(RecoveryStore::snapshotPath(QStringLiteral("/a/b.txt"))
          != RecoveryStore::snapshotPath(QStringLiteral("/a/c.txt")));

    // 4. Multiple snapshots.
    RecoveryStore::clearSnapshots();
    {
        CHECK(RecoveryStore::writeSnapshot(QStringLiteral("/x/one.txt"),
                                           QStringLiteral("one")));
        CHECK(RecoveryStore::writeSnapshot(QStringLiteral("/x/two.txt"),
                                           QStringLiteral("two")));
        CHECK(RecoveryStore::listSnapshots().size() == 2);
    }

    // 5. Clear.
    {
        RecoveryStore::clearSnapshots();
        CHECK(RecoveryStore::listSnapshots().isEmpty());
    }

    if (failures == 0) {
        std::printf("ALL RECOVERY TESTS PASSED\n");
        return 0;
    }
    std::printf("%d RECOVERY TEST(S) FAILED\n", failures);
    return 1;
}
