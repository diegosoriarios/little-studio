#ifndef LITE_STUDIO_CODE_TOOLS_H
#define LITE_STUDIO_CODE_TOOLS_H

#include <QList>
#include <QString>

// Lightweight, heuristic code navigation/completion helpers. Everything is
// derived from the text itself - no language servers, no type information.
namespace CodeTools {

enum class Language {
    Generic,
    Kotlin,
    Swift,
    Java,
    TsJs,
    Dart,
};

Language languageForExtension(const QString &suffix);

// Unique, sorted identifiers written as `<prefix>.member` anywhere in the
// text (heuristic member completion). Results are capped.
QStringList collectMembers(const QString &text, const QString &prefix,
                           int cap = 200);

// Identifier around character `index` within `lineText` ("" when none).
// Clicking just after an identifier resolves to that identifier.
QString identifierAt(const QString &lineText, int index);

struct SymbolMatch {
    int line = 0;    // 0-based
    int index = 0;   // character index of the word start within the line
    QString name;    // matched identifier
    QString snippet; // trimmed line text
};

// Heuristic declaration search: per-language declaration patterns
// (fun/func/class/val/let/Type x(...)...), comment lines skipped, plain
// call statements excluded. Ordered by line.
QList<SymbolMatch> findDeclarations(const QString &contents,
                                    const QString &word, Language lang);

struct ReplaceResult {
    int count = 0;
    QString text;
};

// Replace-all with the same matching semantics as the search panel
// (case sensitivity + whole-word). Count includes every replacement.
ReplaceResult replaceAll(const QString &text, const QString &term,
                         const QString &replacement, bool caseSensitive,
                         bool wholeWord);

// All declarations in the file (any identifier), for the symbol outline.
QList<SymbolMatch> listSymbols(const QString &contents, Language lang);

} // namespace CodeTools

#endif
