#include "AppleTooling.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QString runtimeDisplayName(const QString &identifier)
{
    // com.apple.CoreSimulator.SimRuntime.iOS-17-2 -> "iOS 17.2"
    const QString tail = identifier.section(QLatin1Char('.'), -1);
    const int dash = tail.indexOf(QLatin1Char('-'));
    if (dash < 0)
        return tail;
    QString version = tail.mid(dash + 1);
    version.replace(QLatin1Char('-'), QLatin1Char('.'));
    return tail.left(dash) + QLatin1Char(' ') + version;
}

QJsonArray buildSettingsArray(const QJsonDocument &doc)
{
    if (doc.isArray())
        return doc.array();
    if (doc.isObject())
        return QJsonArray{ doc.object() };
    return QJsonArray();
}

} // namespace

namespace AppleTooling {

Tools discoverTools(const QString &pathEnvironment)
{
    Tools tools;
    const QChar separator = QDir::listSeparator();
    const QStringList dirs =
        pathEnvironment.split(separator, Qt::SkipEmptyParts);

    for (const QString &dir : dirs) {
        if (tools.xcrun.isEmpty()) {
            const QString candidate = dir + QStringLiteral("/xcrun");
            if (QFileInfo(candidate).isFile())
                tools.xcrun = candidate;
        }
        if (tools.xcodebuild.isEmpty()) {
            const QString candidate = dir + QStringLiteral("/xcodebuild");
            if (QFileInfo(candidate).isFile())
                tools.xcodebuild = candidate;
        }
        if (tools.isValid())
            break;
    }
    return tools;
}

Tools discoverToolsFromEnvironment()
{
    Tools tools = discoverTools(qEnvironmentVariable("PATH"));
    if (tools.xcrun.isEmpty())
        tools.xcrun = QStandardPaths::findExecutable(QStringLiteral("xcrun"));
    if (tools.xcodebuild.isEmpty())
        tools.xcodebuild = QStandardPaths::findExecutable(
            QStringLiteral("xcodebuild"));
    return tools;
}

QList<Simulator> parseSimctlDevices(const QString &json)
{
    QList<Simulator> simulators;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return simulators;

    const QJsonObject devices = doc.object().value(QStringLiteral("devices")).toObject();
    for (auto runtimeIt = devices.constBegin(); runtimeIt != devices.constEnd();
         ++runtimeIt) {
        const QString runtimeIdentifier = runtimeIt.key();
        const QJsonArray entries = runtimeIt.value().toArray();
        for (const QJsonValue &entry : entries) {
            const QJsonObject object = entry.toObject();
            Simulator simulator;
            simulator.udid = object.value(QStringLiteral("udid")).toString();
            simulator.name = object.value(QStringLiteral("name")).toString();
            simulator.state = object.value(QStringLiteral("state")).toString();
            simulator.isAvailable =
                object.value(QStringLiteral("isAvailable")).toBool(true);
            simulator.runtimeIdentifier = runtimeIdentifier;
            simulator.runtime = runtimeDisplayName(runtimeIdentifier);
            if (!simulator.udid.isEmpty())
                simulators.append(simulator);
        }
    }
    return simulators;
}

QStringList parseXcodebuildSchemes(const QString &json)
{
    QStringList schemes;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return schemes;

    const QJsonObject root = doc.object();
    for (const QString &key : { QStringLiteral("workspace"),
                                QStringLiteral("project") }) {
        const QJsonObject container = root.value(key).toObject();
        const QJsonArray list =
            container.value(QStringLiteral("schemes")).toArray();
        for (const QJsonValue &value : list) {
            const QString scheme = value.toString();
            if (!scheme.isEmpty() && !schemes.contains(scheme))
                schemes.append(scheme);
        }
    }
    return schemes;
}

BuildSettings parseBuildSettings(const QString &json)
{
    BuildSettings settings;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
        return settings;

    const QJsonArray entries = buildSettingsArray(doc);
    for (const QJsonValue &entry : entries) {
        const QJsonObject buildSettings =
            entry.toObject().value(QStringLiteral("buildSettings")).toObject();
        const QString directory =
            buildSettings.value(QStringLiteral("TARGET_BUILD_DIR")).toString();
        const QString product =
            buildSettings.value(QStringLiteral("FULL_PRODUCT_NAME")).toString();
        const QString bundleId = buildSettings
            .value(QStringLiteral("PRODUCT_BUNDLE_IDENTIFIER")).toString();

        if (!directory.isEmpty() && !product.isEmpty() && !bundleId.isEmpty()) {
            settings.appPath = directory + QLatin1Char('/') + product;
            settings.bundleId = bundleId;
            return settings;
        }
    }
    return settings;
}

QString findWorkspaceOrProject(const QString &rootPath)
{
    const QDir dir(rootPath);

    const QStringList workspaces = dir.entryList(
        { QStringLiteral("*.xcworkspace") },
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (!workspaces.isEmpty())
        return rootPath + QLatin1Char('/') + workspaces.first();

    const QStringList projects = dir.entryList(
        { QStringLiteral("*.xcodeproj") },
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (!projects.isEmpty())
        return rootPath + QLatin1Char('/') + projects.first();

    return QString();
}

} // namespace AppleTooling
