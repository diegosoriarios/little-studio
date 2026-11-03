#ifndef LITE_STUDIO_TASK_PRESETS_H
#define LITE_STUDIO_TASK_PRESETS_H

#include <QList>
#include <QString>
#include <QStringList>

// One command preset from <project>/.lite-studio/tasks.json.
struct TaskPreset
{
    QString name;
    QString command;
    QStringList args;
    QString workingDirectory; // absolute, defaults to the project root
    bool useShell = false;    // run command through the shell
};

namespace TaskPresets {

QString configPath(const QString &projectRoot);

// Loads and validates presets. Missing file -> empty list, no error.
// Malformed JSON -> empty list plus a human-readable error.
QList<TaskPreset> load(const QString &projectRoot, QString *error = nullptr);

} // namespace TaskPresets

#endif
