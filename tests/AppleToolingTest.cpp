#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdio>

#include "AppleTooling.h"

// Headless tests for Apple tooling discovery and JSON parsing. Runs on any
// platform; the actual xcrun/xcodebuild commands remain macOS-only.

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

    // 1. Tool discovery from an explicit PATH list.
    {
        const QString tools = base + "/tools";
        CHECK(writeFile(tools + "/xcrun", "#!/bin/sh\n"));
        CHECK(writeFile(tools + "/xcodebuild", "#!/bin/sh\n"));

        const AppleTooling::Tools found =
            AppleTooling::discoverTools(tools + ":/usr/bin");
        CHECK(found.isValid());
        CHECK(found.xcrun == tools + "/xcrun");
        CHECK(found.xcodebuild == tools + "/xcodebuild");

        CHECK(!AppleTooling::discoverTools(base + "/empty").isValid());
    }

    // 2. simctl device list parsing (multiple runtimes).
    {
        const QString json = QStringLiteral(R"({
  "devices": {
    "com.apple.CoreSimulator.SimRuntime.iOS-17-2": [
      {"udid":"AAAA-1111","name":"iPhone 15","state":"Booted","isAvailable":true},
      {"udid":"BBBB-2222","name":"iPhone 15 Pro","state":"Shutdown","isAvailable":true},
      {"udid":"CCCC-3333","name":"Old","state":"Shutdown","isAvailable":false}
    ],
    "com.apple.CoreSimulator.SimRuntime.watchOS-10-0": [
      {"udid":"DDDD-4444","name":"Watch","state":"Shutdown","isAvailable":true}
    ]
  }
})");
        const QList<AppleTooling::Simulator> simulators =
            AppleTooling::parseSimctlDevices(json);
        CHECK(simulators.size() == 4);

        QHash<QString, AppleTooling::Simulator> byUdid;
        for (const AppleTooling::Simulator &simulator : simulators)
            byUdid.insert(simulator.udid, simulator);

        CHECK(byUdid.value(QStringLiteral("AAAA-1111")).isBooted());
        CHECK(byUdid.value(QStringLiteral("AAAA-1111")).runtime
              == QStringLiteral("iOS 17.2"));
        CHECK(byUdid.value(QStringLiteral("AAAA-1111")).name
              == QStringLiteral("iPhone 15"));
        CHECK(!byUdid.value(QStringLiteral("CCCC-3333")).isAvailable);
        CHECK(byUdid.value(QStringLiteral("DDDD-4444")).runtime
              == QStringLiteral("watchOS 10.0"));

        CHECK(AppleTooling::parseSimctlDevices(QStringLiteral("{}")).isEmpty());
        CHECK(AppleTooling::parseSimctlDevices(QStringLiteral("not json"))
              .isEmpty());
    }

    // 3. xcodebuild -list parsing (workspace and project shapes).
    {
        const QString workspaceJson = QStringLiteral(R"({
  "workspace": {"name":"App","schemes":["App","AppTests"]}
})");
        const QStringList workspaceSchemes =
            AppleTooling::parseXcodebuildSchemes(workspaceJson);
        CHECK(workspaceSchemes
              == QStringList({ QStringLiteral("App"),
                               QStringLiteral("AppTests") }));

        const QString projectJson = QStringLiteral(R"({
  "project": {"name":"App","schemes":["App"],"targets":["App"]}
})");
        CHECK(AppleTooling::parseXcodebuildSchemes(projectJson)
              == QStringList({ QStringLiteral("App") }));

        CHECK(AppleTooling::parseXcodebuildSchemes(QStringLiteral("{}"))
              .isEmpty());
    }

    // 4. Build settings parsing: skip entries without a bundle id.
    {
        const QString json = QStringLiteral(R"([
  {"target":"Pods-App","buildSettings":{
    "TARGET_BUILD_DIR":"/tmp/DD/Build/Products/Debug-iphonesimulator",
    "FULL_PRODUCT_NAME":"libPods-App.a"}},
  {"target":"App","buildSettings":{
    "TARGET_BUILD_DIR":"/tmp/DD/Build/Products/Debug-iphonesimulator",
    "FULL_PRODUCT_NAME":"App.app",
    "PRODUCT_BUNDLE_IDENTIFIER":"com.example.app"}}
])");
        const AppleTooling::BuildSettings settings =
            AppleTooling::parseBuildSettings(json);
        CHECK(settings.appPath
              == QStringLiteral("/tmp/DD/Build/Products/Debug-iphonesimulator/App.app"));
        CHECK(settings.bundleId == QStringLiteral("com.example.app"));
    }

    // 5. Workspace is preferred over project.
    {
        const QString root = base + "/ios-project";
        CHECK(QDir().mkpath(root + "/Both.xcworkspace"));
        CHECK(QDir().mkpath(root + "/Other.xcodeproj"));
        CHECK(AppleTooling::findWorkspaceOrProject(root)
              == root + "/Both.xcworkspace");

        const QString onlyProject = base + "/only-project";
        CHECK(QDir().mkpath(onlyProject + "/Solo.xcodeproj"));
        CHECK(AppleTooling::findWorkspaceOrProject(onlyProject)
              == onlyProject + "/Solo.xcodeproj");

        const QString empty = base + "/empty-ios";
        CHECK(QDir().mkpath(empty));
        CHECK(AppleTooling::findWorkspaceOrProject(empty).isEmpty());
    }

#ifndef Q_OS_MACOS
    // 6. On non-macOS hosts there is no Xcode toolchain.
    CHECK(!AppleTooling::discoverToolsFromEnvironment().isValid());
#endif

    if (failures == 0) {
        std::printf("ALL APPLE TESTS PASSED\n");
        return 0;
    }
    std::printf("%d APPLE TEST(S) FAILED\n", failures);
    return 1;
}
