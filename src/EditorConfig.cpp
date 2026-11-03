#include "EditorConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

struct Section {
    QList<QRegularExpression> globs;
    QHash<QString, QString> properties;
};

QString trimmed(const QString &value)
{
    return value.trimmed();
}

bool matchesAny(const QList<QRegularExpression> &globs, const QString &name)
{
    for (const QRegularExpression &glob : globs) {
        if (glob.match(name).hasMatch())
            return true;
    }
    return false;
}

} // namespace

namespace EditorConfig {

Settings loadFor(const QString &rootPath, const QString &fileName)
{
    Settings settings;
    const QString path = QDir::cleanPath(rootPath) + QStringLiteral("/.editorconfig");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return settings;

    const QString content = QString::fromUtf8(file.read(64 * 1024));
    const QString baseName = QFileInfo(fileName).fileName();

    QList<Section> sections;
    QList<QRegularExpression> currentGlobs;
    bool inSection = false;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))
            || line.startsWith(QLatin1Char(';')))
            continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentGlobs.clear();
            const QStringList patterns =
                line.mid(1, line.size() - 2).split(QLatin1Char(','));
            for (const QString &patternRaw : patterns) {
                const QString pattern = trimmed(patternRaw);
                if (!pattern.isEmpty()) {
                    currentGlobs.append(QRegularExpression::fromWildcard(
                        pattern, Qt::CaseInsensitive));
                }
            }
            Section section;
            section.globs = currentGlobs;
            sections.append(section);
            inSection = true;
            continue;
        }

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = trimmed(line.left(eq)).toLower();
        const QString value = trimmed(line.mid(eq + 1));
        if (inSection && !key.isEmpty() && !sections.isEmpty())
            sections.last().properties.insert(key, value);
    }

    for (const Section &section : sections) {
        if (!matchesAny(section.globs, baseName))
            continue;
        const QString style = section.properties.value(QStringLiteral("indent_style"));
        if (style == QStringLiteral("tab")) {
            settings.indentStyleSet = true;
            settings.useTabs = true;
        } else if (style == QStringLiteral("space")) {
            settings.indentStyleSet = true;
            settings.useTabs = false;
        }
        const QString indentSize = section.properties.value(QStringLiteral("indent_size"));
        bool ok = false;
        const int size = indentSize.toInt(&ok);
        if (ok && size > 0) {
            settings.indentSizeSet = true;
            settings.indentSize = size;
        }
        const QString tabWidth = section.properties.value(QStringLiteral("tab_width"));
        const int width = tabWidth.toInt(&ok);
        if (ok && width > 0) {
            settings.tabWidthSet = true;
            settings.tabWidth = width;
        }
    }
    return settings;
}

} // namespace EditorConfig
