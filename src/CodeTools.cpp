#include "CodeTools.h"

#include <QRegularExpression>
#include <QSet>

namespace {

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

bool isCommentLine(const QString &trimmed)
{
    return trimmed.startsWith(QStringLiteral("//"))
        || trimmed.startsWith(QStringLiteral("/*"))
        || trimmed.startsWith(QLatin1Char('*'));
}

// Statements like `run(1);` or `return foo(x);` are calls, not declarations.
bool looksLikeCallStatement(const QString &trimmed, const QString &word)
{
    static const QStringList controlPrefixes = {
        QStringLiteral("return"), QStringLiteral("if"), QStringLiteral("else"),
        QStringLiteral("while"), QStringLiteral("for"), QStringLiteral("do"),
        QStringLiteral("switch"), QStringLiteral("case"),
        QStringLiteral("catch"), QStringLiteral("throw"), QStringLiteral("new"),
        QStringLiteral("delete"), QStringLiteral("break"),
        QStringLiteral("continue"),
    };
    if (trimmed.startsWith(word + QStringLiteral("("))
        || trimmed.startsWith(word + QStringLiteral(" (")))
        return true;
    for (const QString &prefix : controlPrefixes) {
        if (trimmed.startsWith(prefix + QLatin1Char(' '))
            || trimmed.startsWith(prefix + QLatin1Char('(')))
            return true;
    }
    return false;
}

// Per-language declaration patterns. When `word` is non-empty only that
// identifier matches; otherwise any identifier matches. In every pattern the
// identifier is capture group 1 (all other groups are non-capturing).
QStringList declarationPatterns(CodeTools::Language lang, const QString &word)
{
    const bool specific = !word.isEmpty();
    const QString w = specific ? QRegularExpression::escape(word)
                               : QStringLiteral("(\\w+)");
    QStringList patterns;

    const QString cStyleFunction =
        QStringLiteral("[\\w\\]\\[>,\\*&\\s]+\\s+(%1)\\s*\\(").arg(w);
    const QString cStyleField =
        QStringLiteral("[\\w\\]\\[>,\\*&\\s]+\\s+(%1)\\s*[;=]").arg(w);

    switch (lang) {
    case CodeTools::Language::Kotlin:
        patterns << QStringLiteral("\\bfun\\s+(%1)\\b\\s*[<(]").arg(w)
                 << QStringLiteral("\\b(?:val|var)\\s+(?:<[^>]*>\\s+)?(%1)\\b").arg(w)
                 << QStringLiteral("\\b(?:enum\\s+)?class\\s+(%1)\\b").arg(w)
                 << QStringLiteral("\\b(?:interface|object)\\s+(%1)\\b").arg(w)
                 << QStringLiteral("\\btypealias\\s+(%1)\\b").arg(w);
        break;
    case CodeTools::Language::Swift:
        patterns << QStringLiteral("\\bfunc\\s+(%1)\\b\\s*[<(]").arg(w)
                 << QStringLiteral("\\b(?:weak\\s+|lazy\\s+)?(?:let|var)\\s+(?:.+\\s+)?(%1)\\b")
                    .arg(w)
                 << QStringLiteral("\\b(?:class|struct|enum|protocol|actor|extension)\\s+(%1)\\b")
                    .arg(w)
                 << QStringLiteral("\\btypealias\\s+(%1)\\b").arg(w);
        patterns << cStyleFunction << cStyleField;
        break;
    case CodeTools::Language::Java:
        patterns << QStringLiteral("\\b(?:class|interface|enum|record)\\s+(%1)\\b")
                    .arg(w)
                 << cStyleFunction << cStyleField;
        break;
    case CodeTools::Language::TsJs:
        patterns << QStringLiteral("\\bfunction\\s*+(%1)\\b\\s*\\(").arg(w)
                 << QStringLiteral("\\b(?:const|let|var)\\s+(%1)\\b\\s*=").arg(w)
                 << QStringLiteral("\\b(?:class|interface|enum)\\s+(%1)\\b").arg(w)
                 << QStringLiteral("\\btype\\s+(%1)\\b\\s*=").arg(w)
                 << cStyleFunction << cStyleField;
        break;
    case CodeTools::Language::Dart:
        patterns << QStringLiteral("\\b(?:class|enum|mixin|extension)\\s+(%1)\\b")
                    .arg(w)
                 << QStringLiteral("\\btypedef\\s+(?:.+\\s+)?(%1)\\b").arg(w)
                 << QStringLiteral("\\b(?:final|var|const|late)\\s+(?:[\\w<>?,\\s]+\\s+)?(%1)\\b\\s*(?:=|;)")
                    .arg(w)
                 << cStyleFunction << cStyleField;
        break;
    case CodeTools::Language::Generic:
    default:
        patterns << QStringLiteral("^\\s*#\\s*define\\s+(%1)\\b").arg(w)
                 << cStyleFunction << cStyleField;
        break;
    }
    return patterns;
}

// Extra guards for the loose C-style patterns: word preceded by '.' is a
// member use; a ';' after the parameter list (or after the word) means a call
// statement rather than a declaration signature.
bool cStyleFalsePositive(const QString &lineText, const QRegularExpressionMatch &match,
                         const QString &word)
{
    const int start = match.capturedStart(0);
    if (start > 0 && lineText.at(start - 1) == QLatin1Char('.'))
        return true;

    if (word.isEmpty())
        return false;

    const int wordStart = match.capturedStart(1);
    const int wordEnd = wordStart + word.size();
    const int parenEnd = lineText.indexOf(QLatin1Char(')'), wordEnd);
    if (parenEnd >= 0) {
        const int rest = lineText.indexOf(QLatin1Char(';'), parenEnd);
        if (rest == parenEnd + 1)
            return true;
    }
    return false;
}

} // namespace

namespace CodeTools {

Language languageForExtension(const QString &suffix)
{
    const QString s = suffix.toLower();
    if (s == QLatin1String("kt") || s == QLatin1String("kts"))
        return Language::Kotlin;
    if (s == QLatin1String("swift"))
        return Language::Swift;
    if (s == QLatin1String("java"))
        return Language::Java;
    if (s == QLatin1String("ts") || s == QLatin1String("tsx")
        || s == QLatin1String("js") || s == QLatin1String("jsx")
        || s == QLatin1String("mjs"))
        return Language::TsJs;
    if (s == QLatin1String("dart"))
        return Language::Dart;
    if (s == QLatin1String("c") || s == QLatin1String("h")
        || s == QLatin1String("cpp") || s == QLatin1String("hpp")
        || s == QLatin1String("cc") || s == QLatin1String("hh")
        || s == QLatin1String("m") || s == QLatin1String("mm"))
        return Language::Generic;
    return Language::Generic;
}

QStringList collectMembers(const QString &text, const QString &prefix,
                           int cap)
{
    if (prefix.isEmpty() || cap <= 0)
        return QStringList();

    const QRegularExpression re(
        QRegularExpression::escape(prefix) + QStringLiteral("\\.(\\w+)"));
    QSet<QString> members;

    auto it = re.globalMatch(text);
    while (it.hasNext() && members.size() < cap) {
        const QRegularExpressionMatch match = it.next();
        members.insert(match.captured(1));
    }

    QStringList list = members.values();
    list.sort();
    return list;
}

QString identifierAt(const QString &lineText, int index)
{
    if (lineText.isEmpty())
        return QString();

    if (index >= lineText.size() || !isWordChar(lineText.at(index))) {
        if (index > 0 && isWordChar(lineText.at(index - 1)))
            --index;
        else
            return QString();
    }

    int start = index;
    while (start > 0 && isWordChar(lineText.at(start - 1)))
        --start;
    int end = index;
    while (end < lineText.size() && isWordChar(lineText.at(end)))
        ++end;
    return lineText.mid(start, end - start);
}

QList<SymbolMatch> listSymbols(const QString &contents, Language lang)
{
    const QStringList patterns = declarationPatterns(lang, QString());
    QList<SymbolMatch> matches;
    const QStringList lines = contents.split(QLatin1Char('\n'));

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString lineText = lines.at(lineIndex);
        const QString trimmed = lineText.trimmed();
        if (trimmed.isEmpty() || isCommentLine(trimmed))
            continue;

        for (const QString &pattern : patterns) {
            const QRegularExpression re(pattern);
            const QRegularExpressionMatch match = re.match(lineText);
            if (!match.hasMatch())
                continue;
            if (match.capturedStart(1) < 0)
                continue;
            SymbolMatch symbol;
            symbol.line = lineIndex;
            symbol.index = match.capturedStart(1);
            symbol.name = match.captured(1);
            symbol.snippet = trimmed;
            matches.append(symbol);
            break;
        }
        if (matches.size() >= 200)
            break;
    }
    return matches;
}

ReplaceResult replaceAll(const QString &text, const QString &term,
                         const QString &replacement, bool caseSensitive,
                         bool wholeWord)
{
    ReplaceResult result;
    result.text = text;
    if (term.isEmpty())
        return result;

    QString pattern = QRegularExpression::escape(term);
    if (wholeWord)
        pattern = QStringLiteral("\\b") + pattern + QStringLiteral("\\b");
    QRegularExpression re(pattern);
    if (!caseSensitive)
        re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    QString out;
    int last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out += text.mid(last, match.capturedStart() - last);
        out += replacement;
        last = match.capturedEnd();
        ++result.count;
    }
    out += text.mid(last);
    result.text = out;
    return result;
}

QList<SymbolMatch> findDeclarations(const QString &contents,
                                    const QString &word, Language lang)
{
    QList<SymbolMatch> matches;
    if (word.isEmpty() || !isWordChar(word.at(0)))
        return matches;
    for (const QChar c : word) {
        if (!isWordChar(c))
            return matches;
    }

    const QStringList patterns = declarationPatterns(lang, word);
    const QStringList lines = contents.split(QLatin1Char('\n'));

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString lineText = lines.at(lineIndex);
        const QString trimmed = lineText.trimmed();
        if (trimmed.isEmpty() || isCommentLine(trimmed)
            || looksLikeCallStatement(trimmed, word))
            continue;

        for (const QString &pattern : patterns) {
            const QRegularExpression re(pattern);
            const QRegularExpressionMatch match = re.match(lineText);
            if (!match.hasMatch())
                continue;

            // C-style patterns can also hit call statements like
            // `obj.method(...)` or `= foo(...)`; filter those.
            const bool loose = pattern.contains(QLatin1String("\\s+%1\\s*\\("))
                || pattern.contains(QLatin1String("\\s+%1\\s*[;=]"));
            if (loose && cStyleFalsePositive(lineText, match, word))
                continue;

            if (match.capturedStart(1) < 0)
                continue;
            SymbolMatch symbol;
            symbol.line = lineIndex;
            symbol.index = match.capturedStart(1);
            symbol.name = match.captured(1);
            symbol.snippet = trimmed;
            matches.append(symbol);
            break; // one declaration guess per line
        }

        if (matches.size() >= 50)
            break;
    }
    return matches;
}

} // namespace CodeTools
