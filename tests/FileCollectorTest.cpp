#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>

#include "FileCollector.h"

// Headless tests for the chunked project walker: ordering, caps, ignored
// directories, and symlink-cycle safety (the walk must always terminate).

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

bool writeFile(const QString &path, const QByteArray &content)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath()))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(content) == content.size();
}

int countBasename(const QStringList &files, const QString &name)
{
    int count = 0;
    for (const QString &file : files) {
        if (QFileInfo(file).fileName() == name)
            ++count;
    }
    return count;
}

struct WalkResult
{
    QStringList files;
    bool timedOut = false;
};

WalkResult runWalk(const QString &root, int maxFiles)
{
    WalkResult result;
    FileCollector collector;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        result.timedOut = true;
        loop.quit();
    });
    QObject::connect(&collector, &FileCollector::finished,
                     &loop, &QEventLoop::quit);
    timeout.start(8000);
    collector.start(root, maxFiles);
    loop.exec();
    result.files = collector.files();
    return result;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString base = dir.path();

    // 1. Nested tree: every file found once, sorted.
    {
        const QString root = base + "/nested";
        CHECK(writeFile(root + "/f1.txt", "1\n"));
        CHECK(writeFile(root + "/a/f2.txt", "2\n"));
        CHECK(writeFile(root + "/a/b/f3.txt", "3\n"));

        const WalkResult result = runWalk(root, 10000);
        CHECK(!result.timedOut);
        CHECK(result.files.size() == 3);
        CHECK(countBasename(result.files, "f1.txt") == 1);
        CHECK(countBasename(result.files, "f2.txt") == 1);
        CHECK(countBasename(result.files, "f3.txt") == 1);
    }

    // 2. Ignored directories are skipped.
    {
        const QString root = base + "/ignored";
        CHECK(writeFile(root + "/keep.txt", "k\n"));
        CHECK(writeFile(root + "/build/skip.txt", "s\n"));
        CHECK(writeFile(root + "/.git/objects/skip2.txt", "s\n"));

        const WalkResult result = runWalk(root, 10000);
        CHECK(!result.timedOut);
        CHECK(result.files.size() == 1);
        CHECK(result.files.first().endsWith(QStringLiteral("/keep.txt")));
    }

    // 3. maxFiles is honored.
    {
        const QString root = base + "/cap";
        CHECK(writeFile(root + "/a.txt", "a\n"));
        CHECK(writeFile(root + "/b.txt", "b\n"));
        CHECK(writeFile(root + "/c.txt", "c\n"));

        const WalkResult result = runWalk(root, 1);
        CHECK(!result.timedOut);
        CHECK(result.files.size() == 1);
    }

    // 4. A missing root terminates with no files.
    {
        const WalkResult result = runWalk(base + "/does-not-exist", 1000);
        CHECK(!result.timedOut);
        CHECK(result.files.isEmpty());
    }

#if defined(Q_OS_UNIX)
    // 5. A symlink back to the root must not loop.
    {
        const QString root = base + "/cycle-root";
        CHECK(writeFile(root + "/sub/file.txt", "f\n"));
        CHECK(QFile::link(root, root + "/sub/up"));
        CHECK(QFile::link(root, root + "/self"));

        const WalkResult result = runWalk(root, 10000);
        CHECK(!result.timedOut);
        CHECK(result.files.size() == 1);
        CHECK(countBasename(result.files, "file.txt") == 1);
    }

    // 6. Mutual/ancestor cycles across two levels terminate.
    {
        const QString root = base + "/cycle-deep";
        CHECK(writeFile(root + "/a/b/file.txt", "f\n"));
        CHECK(QFile::link(root + "/a", root + "/a/b/back"));
        CHECK(QFile::link(root + "/a/b", root + "/top"));

        const WalkResult result = runWalk(root, 10000);
        CHECK(!result.timedOut);
        CHECK(countBasename(result.files, "file.txt") == 1);
    }

    // 7. Two links to the same directory do not duplicate its files.
    {
        const QString root = base + "/dedup";
        CHECK(writeFile(root + "/real/inner.txt", "i\n"));
        CHECK(QFile::link(root + "/real", root + "/alias"));

        const WalkResult result = runWalk(root, 10000);
        CHECK(!result.timedOut);
        CHECK(countBasename(result.files, "inner.txt") == 1);
    }

    // 8. Symlinks pointing outside the project are still followed.
    {
        const QString outside = base + "/outside";
        CHECK(writeFile(outside + "/out.txt", "o\n"));
        const QString root = base + "/external";
        CHECK(writeFile(root + "/inside.txt", "i\n"));
        CHECK(QFile::link(outside, root + "/ext"));

        const WalkResult result = runWalk(root, 10000);
        CHECK(!result.timedOut);
        CHECK(countBasename(result.files, "inside.txt") == 1);
        CHECK(countBasename(result.files, "out.txt") == 1);
    }
#endif

    if (failures == 0) {
        std::printf("ALL FILECOLLECTOR TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FILECOLLECTOR TEST(S) FAILED\n", failures);
    return 1;
}
