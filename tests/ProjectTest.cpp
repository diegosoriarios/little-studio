#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdio>

#include "ProjectDetector.h"

// Headless tests for project detection. Builds small fixture trees; no SDK,
// device or GUI required.

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

bool makeDir(const QString &path)
{
    return QDir().mkpath(path);
}

QString path(const QString &root, const char *relative)
{
    return root + QLatin1Char('/') + QString::fromLatin1(relative);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString base = dir.path();

    // 1. Native Android (Groovy DSL).
    {
        const QString root = path(base, "android-groovy");
        CHECK(writeFile(path(root, "settings.gradle"), "include ':app'\n"));
        CHECK(writeFile(path(root, "gradlew"), "#!/bin/sh\n"));
        CHECK(writeFile(path(root, "app/src/main/AndroidManifest.xml"),
                        "<manifest/>\n"));
        CHECK(writeFile(path(root, "app/build.gradle"),
                        "plugins { id 'com.android.application' }\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Android);
        CHECK(info.hasAndroid);
        CHECK(!info.hasApple);
        CHECK(info.androidRoot == root);
        CHECK(info.summary() == QStringLiteral("Android"));
        CHECK(!info.evidence.isEmpty());
    }

    // 2. Native Android (Kotlin DSL, plugin in root build file).
    {
        const QString root = path(base, "android-kts");
        CHECK(writeFile(path(root, "settings.gradle.kts"), "include(\":app\")\n"));
        CHECK(writeFile(path(root, "build.gradle.kts"),
                        "plugins { id(\"com.android.application\") }\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Android);
        CHECK(info.hasAndroid);
        CHECK(info.androidRoot == root);
    }

    // 3. Native iOS project + Podfile.
    {
        const QString root = path(base, "ios-native");
        CHECK(makeDir(path(root, "MyApp.xcodeproj")));
        CHECK(writeFile(path(root, "Podfile"), "platform :ios\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Apple);
        CHECK(info.hasApple);
        CHECK(!info.hasAndroid);
        CHECK(info.appleRoot == root);
        CHECK(info.summary() == QStringLiteral("iOS"));
    }

    // 4. iOS workspace only.
    {
        const QString root = path(base, "ios-workspace");
        CHECK(makeDir(path(root, "MyApp.xcworkspace")));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Apple);
        CHECK(info.hasApple);
        CHECK(info.appleRoot == root);
    }

    // 5. Flutter app (android + ios subprojects).
    {
        const QString root = path(base, "flutter-app");
        CHECK(writeFile(path(root, "pubspec.yaml"), "name: demo\n"));
        CHECK(writeFile(path(root, ".metadata"),
                        "version:\n  revision: abc\nproject_type: app\n"));
        CHECK(writeFile(path(root, "android/settings.gradle"), "\n"));
        CHECK(writeFile(path(root, "android/gradlew"), "#!/bin/sh\n"));
        CHECK(makeDir(path(root, "ios/Runner.xcodeproj")));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Flutter);
        CHECK(info.hasAndroid);
        CHECK(info.hasApple);
        CHECK(info.androidRoot == path(root, "android"));
        CHECK(info.appleRoot == path(root, "ios"));
        CHECK(info.summary() == QStringLiteral("Flutter (android, ios)"));
    }

    // 6. Flutter package (no platform projects).
    {
        const QString root = path(base, "flutter-package");
        CHECK(writeFile(path(root, "pubspec.yaml"), "name: pkg\n"));
        CHECK(writeFile(path(root, ".metadata"), "project_type: package\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Flutter);
        CHECK(!info.hasAndroid);
        CHECK(!info.hasApple);
        CHECK(info.androidRoot.isEmpty());
        CHECK(info.appleRoot.isEmpty());
    }

    // 7. React Native (dependencies) with native subprojects.
    {
        const QString root = path(base, "react-native");
        CHECK(writeFile(path(root, "package.json"),
                        "{\"dependencies\":{\"react-native\":\"0.74.0\"}}\n"));
        CHECK(writeFile(path(root, "android/settings.gradle"), "\n"));
        CHECK(makeDir(path(root, "ios/App.xcodeproj")));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::ReactNative);
        CHECK(info.hasAndroid);
        CHECK(info.hasApple);
        CHECK(info.androidRoot == path(root, "android"));
        CHECK(info.appleRoot == path(root, "ios"));
        CHECK(info.summary() == QStringLiteral("React Native (android, ios)"));
    }

    // 8. React Native in devDependencies is still detected.
    {
        const QString root = path(base, "react-native-devdeps");
        CHECK(writeFile(path(root, "package.json"),
                        "{\"devDependencies\":{\"react-native\":\"0.74.0\"}}\n"));
        CHECK(writeFile(path(root, "android/settings.gradle"), "\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::ReactNative);
        CHECK(info.hasAndroid);
    }

    // 9. Expo managed (no native folders).
    {
        const QString root = path(base, "expo-managed");
        CHECK(writeFile(path(root, "package.json"),
                        "{\"dependencies\":{\"expo\":\"~50.0.0\"}}\n"));
        CHECK(writeFile(path(root, "app.json"), "{}\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Expo);
        CHECK(!info.hasAndroid);
        CHECK(!info.hasApple);
        CHECK(info.summary() == QStringLiteral("Expo (managed)"));
    }

    // 10. Expo with prebuild dirs.
    {
        const QString root = path(base, "expo-prebuilt");
        CHECK(writeFile(path(root, "package.json"),
                        "{\"dependencies\":{\"expo\":\"~50.0.0\"}}\n"));
        CHECK(writeFile(path(root, "android/settings.gradle"), "\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Expo);
        CHECK(info.hasAndroid);
        CHECK(info.androidRoot == path(root, "android"));
    }

    // 11. Monorepo with native Android and iOS at the root.
    {
        const QString root = path(base, "monorepo");
        CHECK(writeFile(path(root, "settings.gradle"), "\n"));
        CHECK(makeDir(path(root, "NativeApp.xcodeproj")));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Android);
        CHECK(info.hasAndroid);
        CHECK(info.hasApple);
        CHECK(info.androidRoot == root);
        CHECK(info.appleRoot == root);
        CHECK(info.summary() == QStringLiteral("Android (ios)"));
    }

    // 12. Unknown and missing folders.
    {
        const QString root = path(base, "plain-folder");
        CHECK(makeDir(root));
        CHECK(writeFile(path(root, "notes.txt"), "hello\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Unknown);
        CHECK(!info.hasAndroid);
        CHECK(!info.hasApple);
        CHECK(info.summary().isEmpty());

        const ProjectInfo missing = ProjectDetector::detect(path(base, "nope"));
        CHECK(missing.kind == ProjectInfo::Kind::Unknown);
        CHECK(missing.summary().isEmpty());
    }

    // 13. package.json with a misleading substring is not a false positive.
    {
        const QString root = path(base, "not-rn");
        CHECK(writeFile(path(root, "package.json"),
                        "{\"description\":\"react-native is not a dependency\"}\n"));

        const ProjectInfo info = ProjectDetector::detect(root);
        CHECK(info.kind == ProjectInfo::Kind::Unknown);
    }

    if (failures == 0) {
        std::printf("ALL PROJECT TESTS PASSED\n");
        return 0;
    }
    std::printf("%d PROJECT TEST(S) FAILED\n", failures);
    return 1;
}
