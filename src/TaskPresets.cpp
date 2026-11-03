#include "TaskPresets.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

namespace {

constexpr int kMaxTasks = 100;
constexpr qint64 kMaxConfigBytes = 256 * 1024;

} // namespace

namespace TaskPresets {

QString configPath(const QString &projectRoot)
{
    return projectRoot + QStringLiteral("/.lite-studio/tasks.json");
}

QList<TaskPreset> load(const QString &projectRoot, QString *error)
{
    if (error)
        error->clear();

    QList<TaskPreset> presets;
    if (projectRoot.isEmpty())
        return presets;

    const QString path = configPath(projectRoot);
    QFile file(path);
    if (!file.exists())
        return presets;

    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Cannot read %1: %2")
                .arg(QDir::toNativeSeparators(path), file.errorString());
        return presets;
    }

    const QByteArray data = file.read(kMaxConfigBytes);
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = QObject::tr("Invalid JSON in %1: %2")
                .arg(QDir::toNativeSeparators(path), parseError.errorString());
        return presets;
    }
    if (!doc.isObject()) {
        if (error)
            *error = QObject::tr("%1 must contain a JSON object.")
                .arg(QDir::toNativeSeparators(path));
        return presets;
    }

    const QJsonArray tasks = doc.object().value(QStringLiteral("tasks")).toArray();
    for (const QJsonValue &value : tasks) {
        if (presets.size() >= kMaxTasks)
            break;

        const QJsonObject object = value.toObject();
        TaskPreset preset;
        preset.name = object.value(QStringLiteral("name")).toString().trimmed();
        preset.command =
            object.value(QStringLiteral("command")).toString().trimmed();
        if (preset.name.isEmpty() || preset.command.isEmpty())
            continue;

        preset.useShell = object.value(QStringLiteral("shell")).toBool(false);

        const QJsonArray args = object.value(QStringLiteral("args")).toArray();
        for (const QJsonValue &arg : args)
            preset.args.append(arg.toString());

        const QString cwd = object.value(QStringLiteral("cwd")).toString().trimmed();
        preset.workingDirectory = cwd.isEmpty()
            ? projectRoot
            : QDir::cleanPath(projectRoot + QLatin1Char('/') + cwd);

        presets.append(preset);
    }
    return presets;
}

} // namespace TaskPresets
