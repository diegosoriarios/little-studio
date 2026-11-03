#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdio>

#include "AndroidTooling.h"

// Headless tests for Android tool discovery and output parsing.

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

    // 1. SDK discovery: env home wins, fallbacks used when needed.
    {
        const QString sdkA = base + "/sdk-a";
        const QString sdkB = base + "/sdk-b";
        CHECK(writeFile(sdkA + "/platform-tools/adb", "#fake\n"));
        CHECK(writeFile(sdkB + "/platform-tools/adb", "#fake\n"));
        CHECK(writeFile(sdkB + "/emulator/emulator", "#fake\n"));

        const AndroidTooling::Sdk fromEnv =
            AndroidTooling::discoverSdk(sdkA, { sdkB });
        CHECK(fromEnv.isValid());
        CHECK(fromEnv.hasAdb());
        CHECK(!fromEnv.hasEmulator());
        CHECK(fromEnv.adb == sdkA + "/platform-tools/adb");

        const AndroidTooling::Sdk fromFallback =
            AndroidTooling::discoverSdk(base + "/missing", { sdkB });
        CHECK(fromFallback.hasAdb());
        CHECK(fromFallback.hasEmulator());
        CHECK(fromFallback.emulator == sdkB + "/emulator/emulator");

        CHECK(!AndroidTooling::discoverSdk(base + "/nope", {}).isValid());
    }

    // 2. AVD list parsing.
    {
        const QStringList avds = AndroidTooling::parseAvdList(
            QStringLiteral("Pixel_3a_XL\nMedium_Phone_API_35\n\n  Wear_OS  \n"));
        CHECK(avds.size() == 3);
        CHECK(avds.at(0) == QStringLiteral("Pixel_3a_XL"));
        CHECK(avds.at(2) == QStringLiteral("Wear_OS"));
    }

    // 3. adb devices -l parsing, including long format and noise lines.
    {
        const QString output = QStringLiteral(
            "* daemon started successfully *\n"
            "List of devices attached\n"
            "emulator-5554\tdevice\n"
            "R58M1234\tunauthorized\n"
            "10.0.0.2:5555\toffline\n"
            "SM_G991B\tdevice product:r8qxxx model:SM_G991B device:r8q transport_id:3\n"
            "adb server version (41) doesn't match this client (40); killing...\n");
        const QList<AndroidTooling::Device> devices =
            AndroidTooling::parseAdbDevices(output);
        CHECK(devices.size() == 4);
        if (devices.size() == 4) {
            CHECK(devices.at(0).serial == QStringLiteral("emulator-5554"));
            CHECK(devices.at(0).isReady());
            CHECK(!devices.at(1).isReady());
            CHECK(devices.at(1).state == QStringLiteral("unauthorized"));
            CHECK(devices.at(2).state == QStringLiteral("offline"));
            CHECK(devices.at(3).model == QStringLiteral("SM G991B"));
        }
    }

    // 4. pidof parsing.
    {
        const QList<int> pids = AndroidTooling::parsePids(
            QStringLiteral("1234 5678\n"));
        CHECK(pids == QList<int>({ 1234, 5678 }));
        CHECK(AndroidTooling::parsePids(QStringLiteral("no pids\n")).isEmpty());
    }

    // 5. applicationId extraction (Groovy, KTS, namespace fallback, comments).
    {
        CHECK(AndroidTooling::extractApplicationId(QStringLiteral(
            "android {\n  defaultConfig {\n    applicationId \"com.example.app\"\n  }\n}\n"))
            == QStringLiteral("com.example.app"));

        CHECK(AndroidTooling::extractApplicationId(QStringLiteral(
            "android {\n  defaultConfig {\n    applicationId = \"com.example.kts\"\n  }\n}\n"))
            == QStringLiteral("com.example.kts"));

        CHECK(AndroidTooling::extractApplicationId(QStringLiteral(
            "android {\n  namespace = \"com.example.ns\"\n}\n"))
            == QStringLiteral("com.example.ns"));

        CHECK(AndroidTooling::extractApplicationId(QStringLiteral(
            "// applicationId \"com.old.id\"\nnamespace = \"com.example.new\"\n"))
            == QStringLiteral("com.example.new"));

        CHECK(AndroidTooling::extractApplicationId(
            QStringLiteral("plugins { id 'com.android.application' }\n"))
            .isEmpty());
    }

    // 6. Gradle wrapper discovery.
    {
        const QString root = base + "/gradle-project";
        CHECK(writeFile(root + "/gradlew", "#!/bin/sh\n"));
        const QString wrapper = AndroidTooling::findGradleWrapper(root);
        CHECK(wrapper == root + "/gradlew");

        CHECK(AndroidTooling::findGradleWrapper(base + "/empty").isEmpty());
    }

    // 7. Gradle resource-limit flags.
    {
        CHECK(AndroidTooling::gradleLimitArgs(0, 0).isEmpty());
        CHECK(AndroidTooling::gradleLimitArgs(4, 0)
              == QStringList({ QStringLiteral("-Dorg.gradle.jvmargs=-Xmx4g") }));
        CHECK(AndroidTooling::gradleLimitArgs(4, 2)
              == QStringList({ QStringLiteral("-Dorg.gradle.jvmargs=-Xmx4g"),
                               QStringLiteral("-Dorg.gradle.workers.max=2") }));
    }

    // 8. systemd scope prefix.
    {
        CHECK(AndroidTooling::systemdScopePrefix(0, 0).isEmpty());
        CHECK(AndroidTooling::systemdScopePrefix(2, 0)
              == QStringList({ QStringLiteral("--user"),
                               QStringLiteral("--scope"),
                               QStringLiteral("--quiet"),
                               QStringLiteral("-p"),
                               QStringLiteral("CPUQuota=200%") }));
        CHECK(AndroidTooling::systemdScopePrefix(2, 4)
              == QStringList({ QStringLiteral("--user"),
                               QStringLiteral("--scope"),
                               QStringLiteral("--quiet"),
                               QStringLiteral("-p"),
                               QStringLiteral("CPUQuota=200%"),
                               QStringLiteral("-p"),
                               QStringLiteral("MemoryMax=4G") }));
        CHECK(AndroidTooling::systemdScopePrefix(0, 0).isEmpty());
    }

    // 9. nice fallback prefix.
    {
        CHECK(AndroidTooling::nicePrefix(false).isEmpty());
        CHECK(AndroidTooling::nicePrefix(true)
              == QStringList({ QStringLiteral("nice"), QStringLiteral("-n"),
                               QStringLiteral("19") }));
    }

    // 10. wrapCommand strategy selection.
    {
        const AndroidTooling::LimitedCommand scope = AndroidTooling::wrapCommand(
            QStringLiteral("/x/gradlew"), { QStringLiteral("assembleDebug") },
            2, 4, true, QStringLiteral("/usr/bin/systemd-run"));
        CHECK(scope.program == QStringLiteral("/usr/bin/systemd-run"));
        CHECK(scope.arguments
              == QStringList({ QStringLiteral("--user"),
                               QStringLiteral("--scope"),
                               QStringLiteral("--quiet"),
                               QStringLiteral("-p"),
                               QStringLiteral("CPUQuota=200%"),
                               QStringLiteral("-p"),
                               QStringLiteral("MemoryMax=4G"),
                               QStringLiteral("nice"),
                               QStringLiteral("-n"),
                               QStringLiteral("19"),
                               QStringLiteral("/x/gradlew"),
                               QStringLiteral("assembleDebug") }));

        const AndroidTooling::LimitedCommand nice = AndroidTooling::wrapCommand(
            QStringLiteral("/bin/sh"),
            { QStringLiteral("-c"), QStringLiteral("ls") },
            0, 0, true, QString());
        CHECK(nice.program == QStringLiteral("nice"));
        CHECK(nice.arguments
              == QStringList({ QStringLiteral("-n"), QStringLiteral("19"),
                               QStringLiteral("/bin/sh"),
                               QStringLiteral("-c"),
                               QStringLiteral("ls") }));

        const AndroidTooling::LimitedCommand plain = AndroidTooling::wrapCommand(
            QStringLiteral("/bin/sh"),
            { QStringLiteral("-c"), QStringLiteral("ls") },
            0, 0, false, QString());
        CHECK(plain.program == QStringLiteral("/bin/sh"));
        CHECK(plain.arguments
              == QStringList({ QStringLiteral("-c"), QStringLiteral("ls") }));
    }

    if (failures == 0) {
        std::printf("ALL ANDROID TESTS PASSED\n");
        return 0;
    }
    std::printf("%d ANDROID TEST(S) FAILED\n", failures);
    return 1;
}
