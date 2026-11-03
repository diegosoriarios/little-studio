#include "ProjectDetector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr qint64 kGradleReadBytes = 16 * 1024;
constexpr qint64 kJsonReadBytes = 64 * 1024;

bool pathExists(const QString &path)
{
    return QFileInfo::exists(path);
}

bool pathIsDir(const QString &path)
{
    return QFileInfo(path).isDir();
}

QString readUpTo(const QString &path, qint64 maxBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.read(maxBytes));
}

bool gradleMentionsAndroid(const QString &dir)
{
    const QStringList names = { QStringLiteral("build.gradle"),
                                QStringLiteral("build.gradle.kts") };
    for (const QString &name : names) {
        const QString path = dir + QLatin1Char('/') + name;
        if (!pathExists(path))
            continue;
        const QString text = readUpTo(path, kGradleReadBytes);
        if (text.contains(QStringLiteral("com.android.application"))
            || text.contains(QStringLiteral("com.android.library"))
            || text.contains(QStringLiteral("com.android.dynamic-feature")))
            return true;
    }
    return false;
}

void addSignal(QStringList *evidence, const QString &signal)
{
    if (evidence && !evidence->contains(signal))
        evidence->append(signal);
}

bool isAndroidProject(const QString &dir, QStringList *evidence)
{
    if (!pathIsDir(dir))
        return false;

    bool found = false;

    if (pathExists(dir + QStringLiteral("/settings.gradle"))
        || pathExists(dir + QStringLiteral("/settings.gradle.kts"))) {
        addSignal(evidence, QStringLiteral("settings.gradle"));
        found = true;
    }

    if (pathExists(dir + QStringLiteral("/gradlew"))
        || pathExists(dir + QStringLiteral("/gradlew.bat"))) {
        addSignal(evidence, QStringLiteral("gradlew"));
        found = true;
    }

    if (pathExists(dir + QStringLiteral("/app/src/main/AndroidManifest.xml"))) {
        addSignal(evidence, QStringLiteral("AndroidManifest.xml"));
        found = true;
    }

    if (gradleMentionsAndroid(dir)
        || gradleMentionsAndroid(dir + QStringLiteral("/app"))) {
        addSignal(evidence, QStringLiteral("build.gradle (com.android)"));
        found = true;
    }

    return found;
}

bool isAppleProject(const QString &dir, QStringList *evidence)
{
    if (!pathIsDir(dir))
        return false;

    bool found = false;

    const QStringList containers = QDir(dir).entryList(
        { QStringLiteral("*.xcodeproj"), QStringLiteral("*.xcworkspace") },
        QDir::Dirs | QDir::NoDotAndDotDot);
    if (!containers.isEmpty()) {
        addSignal(evidence, containers.first());
        found = true;
    }

    if (pathExists(dir + QStringLiteral("/Podfile"))) {
        addSignal(evidence, QStringLiteral("Podfile"));
        found = true;
    }

    if (pathExists(dir + QStringLiteral("/project.yml"))) {
        addSignal(evidence, QStringLiteral("project.yml"));
        found = true;
    }

    return found;
}

bool packageHasDependency(const QString &packageJson, const QString &name)
{
    QJsonParseError error{};
    const QJsonDocument doc =
        QJsonDocument::fromJson(packageJson.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject root = doc.object();
        const QStringList sections = { QStringLiteral("dependencies"),
                                       QStringLiteral("devDependencies"),
                                       QStringLiteral("peerDependencies") };
        for (const QString &section : sections) {
            const QJsonObject deps = root.value(section).toObject();
            if (deps.contains(name))
                return true;
        }
        return false;
    }
    // Tolerate non-standard package.json (comments, templates).
    return packageJson.contains(QLatin1Char('"') + name + QLatin1Char('"'));
}

} // namespace

QString ProjectInfo::kindName() const
{
    switch (kind) {
    case Kind::Android:
        return QStringLiteral("Android");
    case Kind::Apple:
        return QStringLiteral("iOS");
    case Kind::Flutter:
        return QStringLiteral("Flutter");
    case Kind::ReactNative:
        return QStringLiteral("React Native");
    case Kind::Expo:
        return QStringLiteral("Expo (managed)");
    case Kind::Unknown:
        break;
    }
    return QString();
}

QString ProjectInfo::summary() const
{
    const QString base = kindName();
    if (base.isEmpty())
        return QString();

    QStringList extras;
    if (kind == Kind::Android || kind == Kind::Apple) {
        if (kind == Kind::Android && hasApple)
            extras << QStringLiteral("ios");
        if (kind == Kind::Apple && hasAndroid)
            extras << QStringLiteral("android");
    } else {
        if (hasAndroid)
            extras << QStringLiteral("android");
        if (hasApple)
            extras << QStringLiteral("ios");
    }

    if (extras.isEmpty())
        return base;
    return QStringLiteral("%1 (%2)").arg(base, extras.join(QStringLiteral(", ")));
}

ProjectInfo ProjectDetector::detect(const QString &rootPath)
{
    ProjectInfo info;
    if (!pathIsDir(rootPath))
        return info;

    QStringList evidence;

    // --- Flutter (highest priority: also contains android/ and ios/ dirs) ---
    if (pathExists(rootPath + QStringLiteral("/pubspec.yaml"))) {
        info.kind = ProjectInfo::Kind::Flutter;
        addSignal(&evidence, QStringLiteral("pubspec.yaml"));

        const QString metadata =
            readUpTo(rootPath + QStringLiteral("/.metadata"), kJsonReadBytes);
        if (metadata.contains(QStringLiteral("project_type: app")))
            addSignal(&evidence, QStringLiteral(".metadata (project_type: app)"));

        QStringList sub;
        if (isAndroidProject(rootPath + QStringLiteral("/android"), &sub)) {
            info.hasAndroid = true;
            info.androidRoot = rootPath + QStringLiteral("/android");
            addSignal(&evidence, QStringLiteral("android/"));
        }
        if (isAppleProject(rootPath + QStringLiteral("/ios"), &sub)) {
            info.hasApple = true;
            info.appleRoot = rootPath + QStringLiteral("/ios");
            addSignal(&evidence, QStringLiteral("ios/"));
        }

        info.evidence = evidence;
        return info;
    }

    // --- React Native / Expo (also contains android/ and ios/ dirs) ---
    const QString packagePath = rootPath + QStringLiteral("/package.json");
    if (pathExists(packagePath)) {
        const QString packageJson = readUpTo(packagePath, kJsonReadBytes);
        const bool reactNative =
            packageHasDependency(packageJson, QStringLiteral("react-native"));
        const bool expo =
            packageHasDependency(packageJson, QStringLiteral("expo"));

        if (reactNative) {
            addSignal(&evidence, QStringLiteral("package.json (react-native)"));

            QStringList sub;
            if (isAndroidProject(rootPath + QStringLiteral("/android"), &sub)) {
                info.hasAndroid = true;
                info.androidRoot = rootPath + QStringLiteral("/android");
                addSignal(&evidence, QStringLiteral("android/"));
            }
            if (isAppleProject(rootPath + QStringLiteral("/ios"), &sub)) {
                info.hasApple = true;
                info.appleRoot = rootPath + QStringLiteral("/ios");
                addSignal(&evidence, QStringLiteral("ios/"));
            }

            if (!info.hasAndroid && !info.hasApple && expo)
                info.kind = ProjectInfo::Kind::Expo;
            else
                info.kind = ProjectInfo::Kind::ReactNative;

            info.evidence = evidence;
            return info;
        }

        if (expo) {
            info.kind = ProjectInfo::Kind::Expo;
            addSignal(&evidence, QStringLiteral("package.json (expo)"));

            QStringList sub;
            if (isAndroidProject(rootPath + QStringLiteral("/android"), &sub)) {
                info.hasAndroid = true;
                info.androidRoot = rootPath + QStringLiteral("/android");
                addSignal(&evidence, QStringLiteral("android/"));
            }
            if (isAppleProject(rootPath + QStringLiteral("/ios"), &sub)) {
                info.hasApple = true;
                info.appleRoot = rootPath + QStringLiteral("/ios");
                addSignal(&evidence, QStringLiteral("ios/"));
            }

            info.evidence = evidence;
            return info;
        }
    }

    // --- Native Android and/or native iOS at the project root ---
    if (isAndroidProject(rootPath, &evidence)) {
        info.hasAndroid = true;
        info.androidRoot = rootPath;
        info.kind = ProjectInfo::Kind::Android;
    }
    if (isAppleProject(rootPath, &evidence)) {
        info.hasApple = true;
        info.appleRoot = rootPath;
        if (info.kind == ProjectInfo::Kind::Unknown)
            info.kind = ProjectInfo::Kind::Apple;
    }

    info.evidence = evidence;
    return info;
}
