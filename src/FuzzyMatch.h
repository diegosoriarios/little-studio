#ifndef LITE_STUDIO_FUZZY_MATCH_H
#define LITE_STUDIO_FUZZY_MATCH_H

#include <QString>

// Simple deterministic ranking for quick-open style file matching.
// Returns a score, or -1 when the path does not match the term.
// Higher is better; callers tie-break by shorter path, then alphabetically.
int fuzzyScore(const QString &path, const QString &term);

#endif
