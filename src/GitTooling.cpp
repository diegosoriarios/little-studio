#include "GitTooling.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <cstdio>
#include <cstdlib>

namespace {

#ifdef Q_OS_WIN
constexpr const char *kExecutableSuffix = ".exe";
#else
constexpr const char *kExecutableSuffix = "";
#endif

// git quotes paths containing special characters; strip the quotes and undo
// the two most common escapes.
QString unquotePath(QString path)
{
    if (path.length() >= 2 && path.startsWith(QLatin1Char('"'))
        && path.endsWith(QLatin1Char('"'))) {
        path = path.mid(1, path.size() - 2);
        path.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        path.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    }
    return path;
}

// "diff --git a/x.txt b/x.txt"; without prefixes when diff.noprefix is set.
QString pathFromDiffHeader(const QString &line)
{
    const QString body = line.mid(QStringLiteral("diff --git ").size()).trimmed();
    const QStringList parts = body.split(QLatin1Char(' '));
    // The destination path is the last token (paths never contain spaces
    // unless quoted, and quoted paths are unquoted by the caller).
    if (parts.size() >= 2)
        return unquotePath(parts.last());
    return unquotePath(body);
}

// Strips the "--- "/"+++ " marker and optional a/ or b/ prefix.
QString pathFromMarkerLine(const QString &line)
{
    QString path = line.mid(4).trimmed(); // "--- " or "+++ " = 4 chars
    if (path.startsWith(QStringLiteral("a/"))
        || path.startsWith(QStringLiteral("b/")))
        path.remove(0, 2);
    return unquotePath(path);
}

} // namespace

namespace GitTooling {

QString findGit(const QString &pathEnvironment)
{
    const QStringList dirs =
        pathEnvironment.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString &dir : dirs) {
        const QString candidate =
            dir + QStringLiteral("/git") + QLatin1String(kExecutableSuffix);
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    return QString();
}

bool isGitRepository(const QString &rootPath)
{
    if (rootPath.isEmpty())
        return false;
    return QFileInfo::exists(rootPath + QStringLiteral("/.git"));
}

QList<ChangedFile> parseNameStatus(const QString &output)
{
    QList<ChangedFile> files;
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('*'))
            || trimmed.startsWith(QLatin1Char('('))
            || trimmed.startsWith(QStringLiteral("warning:"))
            || trimmed.startsWith(QStringLiteral("adb "), Qt::CaseInsensitive))
            continue;

        const QStringList tokens =
            trimmed.split(QRegularExpression(QStringLiteral("\\t")),
                          Qt::SkipEmptyParts);
        if (tokens.isEmpty())
            continue;

        // Status is the first tab-separated token; the new path is the last.
        // Rename/copy rows have two paths (old then new).
        ChangedFile file;
        file.status = tokens.first();
        if (tokens.size() >= 2)
            file.path = unquotePath(tokens.last());
        if (file.path.isEmpty())
            continue;
        files.append(file);
    }
    return files;
}

QList<SectionRange> diffSectionOffsets(const QString &diffText)
{
    QList<SectionRange> sections;
    static const QRegularExpression diffHeader(
        QStringLiteral("^diff --git "));

    int start = -1;
    QString currentPath;

    int offset = 0;
    const QStringList lines = diffText.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const int lineStart = offset;
        offset += line.size() + 1; // + the newline removed by split

        const QRegularExpressionMatch headerMatch =
            diffHeader.match(line);
        if (qEnvironmentVariableIsSet("GT_DEBUG"))
            std::fprintf(stderr, "DBG line %d start=%d off=%d hdr=%d: %s\n",
                         i, lineStart, offset, headerMatch.hasMatch() ? 1 : 0,
                         line.toUtf8().constData());
        if (headerMatch.hasMatch()) {
            if (start >= 0) {
                SectionRange section;
                section.path = currentPath;
                section.start = start;
                section.end = lineStart;
                if (!section.path.isEmpty())
                    sections.append(section);
            }
            start = lineStart;
            currentPath = pathFromDiffHeader(line);
            continue;
        }

        // Track --- / +++ lines for robust path extraction (deleted files
        // have +++ /dev/null; renames keep the +++ b/ destination).
        if (start >= 0) {
            if (line.startsWith(QStringLiteral("+++ "))
                || line.startsWith(QStringLiteral("--- "))) {
                const QString candidate = pathFromMarkerLine(line);
                if (!candidate.isEmpty()
                    && candidate != QStringLiteral("/dev/null"))
                    currentPath = candidate;
            }
        }
    }

    if (start >= 0) {
        SectionRange section;
        section.path = currentPath;
        section.start = start;
        section.end = offset;
        if (!section.path.isEmpty())
            sections.append(section);
    }
    return sections;
}

} // namespace GitTooling
