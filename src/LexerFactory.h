#ifndef LITE_STUDIO_LEXER_FACTORY_H
#define LITE_STUDIO_LEXER_FACTORY_H

#include <QString>

class QsciLexer;

// Maps file names/extensions to language names and lexers.
// Built-in QScintilla lexers are used where they exist; Kotlin and
// Swift use the project's SimpleLexer.
class LexerFactory
{
public:
    static QString languageName(const QString &path);
    static QsciLexer *create(const QString &path, QObject *parent);
};

#endif
