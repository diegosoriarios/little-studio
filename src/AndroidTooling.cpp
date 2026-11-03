#include "AndroidTooling.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace {

QString toolPath(const QString &root, const QString &relative)
{
    QString path = root + QLatin1Char('/') + relative;
#ifdef Q_OS_WIN
    path += QStringLiteral(".exe");
#endif
    return QFileInfo(path).isFile() ? path : QString();
}

// Finds `key = "value"`, `key "value"` or `key: "value"` on non-comment lines.
QString findStringAssignment(const QString &contents, const QString &key)
{
    const QRegularExpression re(
        QStringLiteral("\\b%1\\s*[:=]?\\s*[\"']([^\"']+)[\"']").arg(key));
    const QStringList lines = contents.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("//"))
            || trimmed.startsWith(QLatin1Char('*')))
            continue;
        const QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch())
            return match.captured(1).trimmed();
    }
    return QString();
}

} // namespace

namespace AndroidTooling {

Sdk discoverSdk(const QString &envHome, const QStringList &fallbackRoots)
{
    QStringList roots;
    if (!envHome.isEmpty())
        roots.append(envHome);
    roots.append(fallbackRoots);

    QSet<QString> seen;
    for (const QString &root : roots) {
        if (root.isEmpty() || seen.contains(root))
            continue;
        seen.insert(root);

        Sdk sdk;
        sdk.sdkPath = root;
        sdk.adb = toolPath(root, QStringLiteral("platform-tools/adb"));
        sdk.emulator = toolPath(root, QStringLiteral("emulator/emulator"));
        if (sdk.isValid())
            return sdk;
    }
    return Sdk();
}

Sdk discoverSdkFromEnvironment()
{
    QString envHome = qEnvironmentVariable("ANDROID_HOME");
    if (envHome.isEmpty())
        envHome = qEnvironmentVariable("ANDROID_SDK_ROOT");

    const QStringList fallbacks = {
        QDir::homePath() + QStringLiteral("/Android/Sdk"),
        QStringLiteral("/usr/lib/android-sdk"),
        QStringLiteral("/opt/android-sdk"),
    };
    return discoverSdk(envHome, fallbacks);
}

QStringList parseAvdList(const QString &output)
{
    QStringList avds;
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            avds.append(trimmed);
    }
    return avds;
}

QList<Device> parseAdbDevices(const QString &output)
{
    QList<Device> devices;
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()
            || trimmed.startsWith(QLatin1Char('*'))
            || trimmed.startsWith(QStringLiteral("adb "), Qt::CaseInsensitive)
            || trimmed.startsWith(QStringLiteral("List of devices"),
                                  Qt::CaseInsensitive))
            continue;

        const QStringList tokens =
            trimmed.split(whitespace, Qt::SkipEmptyParts);
        if (tokens.size() < 2)
            continue;

        Device device;
        device.serial = tokens.at(0);
        device.state = tokens.at(1);

        for (int i = 2; i < tokens.size(); ++i) {
            const QString &token = tokens.at(i);
            if (token.startsWith(QStringLiteral("model:"))) {
                device.model = token.mid(6);
                device.model.replace(QLatin1Char('_'), QLatin1Char(' '));
            }
        }
        devices.append(device);
    }
    return devices;
}

QList<int> parsePids(const QString &output)
{
    QList<int> pids;
    const QStringList tokens = output.split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        bool ok = false;
        const int pid = token.toInt(&ok);
        if (ok && pid > 0)
            pids.append(pid);
    }
    return pids;
}

QString extractApplicationId(const QString &contents, const QString &fileName)
{
    Q_UNUSED(fileName);
    const QString id = findStringAssignment(contents, QStringLiteral("applicationId"));
    if (!id.isEmpty())
        return id;
    return findStringAssignment(contents, QStringLiteral("namespace"));
}

QString findGradleWrapper(const QString &projectRoot)
{
    const QStringList names = { QStringLiteral("gradlew"),
                                QStringLiteral("gradlew.bat") };
    for (const QString &name : names) {
        const QString candidate = projectRoot + QLatin1Char('/') + name;
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    return QString();
}

QStringList gradleLimitArgs(int maxMemoryGB, int maxWorkers)
{
    QStringList args;
    if (maxMemoryGB > 0) {
        args.append(QStringLiteral("-Dorg.gradle.jvmargs=-Xmx%1g")
                        .arg(maxMemoryGB));
    }
    if (maxWorkers > 0) {
        args.append(QStringLiteral("-Dorg.gradle.workers.max=%1")
                        .arg(maxWorkers));
    }
    return args;
}

QStringList systemdScopePrefix(int maxCpus, int maxMemoryGB)
{
    QStringList prefix;
    if (maxCpus <= 0 && maxMemoryGB <= 0)
        return prefix;

    prefix << QStringLiteral("--user") << QStringLiteral("--scope")
           << QStringLiteral("--quiet");
    if (maxCpus > 0) {
        prefix << QStringLiteral("-p")
               << QStringLiteral("CPUQuota=%1%").arg(maxCpus * 100);
    }
    if (maxMemoryGB > 0) {
        prefix << QStringLiteral("-p")
               << QStringLiteral("MemoryMax=%1G").arg(maxMemoryGB);
    }
    return prefix;
}

QStringList nicePrefix(bool lowPriority)
{
    if (!lowPriority)
        return QStringList();
    return { QStringLiteral("nice"), QStringLiteral("-n"),
             QStringLiteral("19") };
}

QString findSystemdRun()
{
    return QStandardPaths::findExecutable(QStringLiteral("systemd-run"));
}

LimitedCommand wrapCommand(const QString &program, const QStringList &args,
                           int maxCpus, int maxMemoryGB, bool lowPriority,
                           const QString &systemdRunPath)
{
    LimitedCommand result{ program, args };

    if (!systemdRunPath.isEmpty()) {
        const QStringList prefix =
            systemdScopePrefix(maxCpus, maxMemoryGB);
        if (!prefix.isEmpty()) {
            result.program = systemdRunPath;
            result.arguments = prefix;
            if (lowPriority) {
                // Scope units have no Nice property; nice the command inside.
                result.arguments << QStringLiteral("nice")
                                 << QStringLiteral("-n")
                                 << QStringLiteral("19");
            }
            result.arguments.append(program);
            result.arguments.append(args);
            return result;
        }
    }

#ifdef Q_OS_UNIX
    if (lowPriority) {
        result.program = QStringLiteral("nice");
        result.arguments = { QStringLiteral("-n"), QStringLiteral("19"),
                             program };
        result.arguments.append(args);
    }
#else
    Q_UNUSED(lowPriority);
#endif
    return result;
}

} // namespace AndroidTooling
