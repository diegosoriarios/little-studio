#ifndef LITE_STUDIO_APPLE_TOOLING_H
#define LITE_STUDIO_APPLE_TOOLING_H

#include <QList>
#include <QString>
#include <QStringList>

// Pure, dependency-injected Apple/Xcode helpers. The tool commands only work
// on macOS, but all parsing and discovery logic is testable on any platform.
namespace AppleTooling {

struct Tools {
    QString xcrun;
    QString xcodebuild;

    bool isValid() const { return !xcrun.isEmpty() && !xcodebuild.isEmpty(); }
};

struct Simulator {
    QString udid;
    QString name;
    QString runtimeIdentifier; // com.apple.CoreSimulator.SimRuntime.iOS-17-2
    QString runtime;           // display: "iOS 17.2"
    QString state;             // Booted, Shutdown, ...
    bool isAvailable = true;

    bool isBooted() const
    {
        return state.compare(QLatin1String("Booted"), Qt::CaseInsensitive) == 0;
    }
};

struct BuildSettings {
    QString appPath;
    QString bundleId;
};

// Searches an explicit PATH-style list for xcrun/xcodebuild.
Tools discoverTools(const QString &pathEnvironment);
Tools discoverToolsFromEnvironment();

// Parses `xcrun simctl list devices available --json`.
QList<Simulator> parseSimctlDevices(const QString &json);

// Parses `xcodebuild -list -json` (workspace or project shape).
QStringList parseXcodebuildSchemes(const QString &json);

// Parses `xcodebuild ... -showBuildSettings -json` for the app path and id.
BuildSettings parseBuildSettings(const QString &json);

// Preferred build container inside a project root: .xcworkspace, then
// .xcodeproj; empty when neither exists.
QString findWorkspaceOrProject(const QString &rootPath);

} // namespace AppleTooling

#endif
