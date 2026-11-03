#ifndef LITE_STUDIO_ANDROID_TOOLING_H
#define LITE_STUDIO_ANDROID_TOOLING_H

#include <QList>
#include <QString>
#include <QStringList>

// Pure, dependency-injected Android helpers. All environment/path inputs are
// parameters so the parsing and discovery logic can be tested headlessly.
namespace AndroidTooling {

struct Sdk {
    QString sdkPath;
    QString adb;
    QString emulator;

    bool hasAdb() const { return !adb.isEmpty(); }
    bool hasEmulator() const { return !emulator.isEmpty(); }
    bool isValid() const { return hasAdb() || hasEmulator(); }
};

struct Device {
    QString serial;
    QString state; // device, offline, unauthorized, ...
    QString model;

    bool isReady() const { return state == QLatin1String("device"); }
};

// Picks the first root that contains adb or emulator.
Sdk discoverSdk(const QString &envHome, const QStringList &fallbackRoots);

// Reads ANDROID_HOME / ANDROID_SDK_ROOT plus common install locations.
Sdk discoverSdkFromEnvironment();

QStringList parseAvdList(const QString &output);
QList<Device> parseAdbDevices(const QString &output);
QList<int> parsePids(const QString &output);

// applicationId from build.gradle(.kts); falls back to namespace.
QString extractApplicationId(const QString &contents,
                             const QString &fileName = QString());

// Path to ./gradlew (or gradlew.bat); empty when absent.
QString findGradleWrapper(const QString &projectRoot);

// ---------------------------------------------------------------------------
// Resource limits for build/test jobs.
// ---------------------------------------------------------------------------

struct LimitedCommand {
    QString program;
    QStringList arguments;
};

// Gradle flags capping daemon heap and worker parallelism (all platforms).
QStringList gradleLimitArgs(int maxMemoryGB, int maxWorkers);

// Prefix for `systemd-run --user --scope` capping the whole process tree
// (Gradle daemon, Kotlin daemon, test workers). Empty when nothing is capped.
// Low priority is applied with `nice` *inside* the scope (scope units have no
// Nice property).
QStringList systemdScopePrefix(int maxCpus, int maxMemoryGB);

// `nice` prefix used when systemd is unavailable. Empty when unchanged.
QStringList nicePrefix(bool lowPriority);

// Path to systemd-run, or empty when the tool is unavailable.
QString findSystemdRun();

// Wraps a command with the best available limiting strategy:
// systemd scope when available, else nice, else unchanged.
LimitedCommand wrapCommand(const QString &program, const QStringList &args,
                           int maxCpus, int maxMemoryGB, bool lowPriority,
                           const QString &systemdRunPath);

} // namespace AndroidTooling

#endif
