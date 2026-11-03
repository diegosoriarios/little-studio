#include "FuzzyMatch.h"

namespace {

// True when every character of term appears in path, in order.
bool isSubsequence(const QString &path, const QString &term)
{
    int pos = 0;
    for (const QChar c : term) {
        pos = path.indexOf(c, pos);
        if (pos < 0)
            return false;
        ++pos;
    }
    return true;
}

} // namespace

int fuzzyScore(const QString &path, const QString &term)
{
    if (term.isEmpty())
        return 0;

    const QString lowerPath = path.toLower();
    const QString lowerTerm = term.toLower();
    const int slash = lowerPath.lastIndexOf(QLatin1Char('/'));
    const QString base = lowerPath.mid(slash + 1);

    if (base.startsWith(lowerTerm))
        return 100;
    if (base.contains(lowerTerm))
        return 80;
    if (lowerPath.contains(lowerTerm))
        return 60;
    if (isSubsequence(lowerPath, lowerTerm))
        return 40;
    return -1;
}
