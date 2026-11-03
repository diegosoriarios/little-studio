#ifndef LITE_STUDIO_GIT_TOOLING_H
#define LITE_STUDIO_GIT_TOOLING_H

#include <QList>
#include <QString>

// Pure, dependency-injected git helpers. All parsing is testable on any
// platform; no repository scanning beyond a single .git existence check.
namespace GitTooling {

QString findGit(const QString &pathEnvironment);

// True when rootPath contains a .git directory (or file, for worktrees).
bool isGitRepository(const QString &rootPath);

struct ChangedFile {
    QString status; // M, A, D, R100, C75, ...
    QString path;   // new path for renames/copies
};

// Parses `git diff [--cached] --name-status` output.
QList<ChangedFile> parseNameStatus(const QString &output);

struct SectionRange {
    QString path;  // path without a/ b/ prefixes
    int start = 0; // byte offset of the "diff --git" line
    int end = 0;   // exclusive byte offset (next section or end of text)
};

// Maps per-file sections inside a unified diff, so the panel can show a
// single file's slice. Paths have a/ b/ prefixes stripped defensively
// (the user may have diff.noprefix configured).
QList<SectionRange> diffSectionOffsets(const QString &diffText);

} // namespace GitTooling

#endif
