#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdio>

#include "TaskPresets.h"

// Headless tests for the optional per-project task presets file.

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

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString base = dir.path();

    // 1. Missing file -> no presets, no error.
    {
        QString error = QStringLiteral("sentinel");
        const QList<TaskPreset> presets = TaskPresets::load(base, &error);
        CHECK(presets.isEmpty());
        CHECK(error.isEmpty());
        CHECK(TaskPresets::configPath(base)
              == base + "/.lite-studio/tasks.json");
    }

    // 2. Valid presets, including args, cwd and shell mode.
    {
        const QString root = base + "/project";
        CHECK(writeFile(TaskPresets::configPath(root), R"({
  "tasks": [
    {"name": "Build debug", "command": "./gradlew",
     "args": ["assembleDebug", "--console=plain"]},
    {"name": "List files", "command": "ls -la", "shell": true, "cwd": "src"},
    {"name": "", "command": "ignored"},
    {"name": "No command"}
  ]
})"));
        QString error;
        const QList<TaskPreset> presets = TaskPresets::load(root, &error);
        CHECK(error.isEmpty());
        CHECK(presets.size() == 2);

        if (presets.size() == 2) {
            CHECK(presets.at(0).name == QStringLiteral("Build debug"));
            CHECK(presets.at(0).command == QStringLiteral("./gradlew"));
            CHECK(presets.at(0).args
                  == QStringList({ QStringLiteral("assembleDebug"),
                                   QStringLiteral("--console=plain") }));
            CHECK(!presets.at(0).useShell);
            CHECK(presets.at(0).workingDirectory == root);

            CHECK(presets.at(1).name == QStringLiteral("List files"));
            CHECK(presets.at(1).useShell);
            CHECK(presets.at(1).workingDirectory == root + "/src");
        }
    }

    // 3. Malformed JSON reports an error.
    {
        const QString root = base + "/broken";
        CHECK(writeFile(TaskPresets::configPath(root),
                        QByteArrayLiteral("{ not json ")));
        QString error;
        const QList<TaskPreset> presets = TaskPresets::load(root, &error);
        CHECK(presets.isEmpty());
        CHECK(!error.isEmpty());
    }

    // 4. Non-object root is rejected.
    {
        const QString root = base + "/array-root";
        CHECK(writeFile(TaskPresets::configPath(root), QByteArrayLiteral("[]")));
        QString error;
        const QList<TaskPreset> presets = TaskPresets::load(root, &error);
        CHECK(presets.isEmpty());
        CHECK(!error.isEmpty());
    }

    if (failures == 0) {
        std::printf("ALL TASK TESTS PASSED\n");
        return 0;
    }
    std::printf("%d TASK TEST(S) FAILED\n", failures);
    return 1;
}
