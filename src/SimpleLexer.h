#ifndef LITE_STUDIO_SIMPLE_LEXER_H
#define LITE_STUDIO_SIMPLE_LEXER_H

#include <QByteArray>
#include <QSet>
#include <QString>

#include <Qsci/qscilexercustom.h>

// Minimal keyword-based lexer for languages QScintilla has no built-in
// support for (Kotlin, Swift). It colors keywords, strings, comments,
// numbers, function names, brackets and annotations.
//
// Scintilla positions are byte offsets, so every styling length is
// computed in UTF-8 bytes, not QString characters.
class SimpleLexer : public QsciLexerCustom
{
    Q_OBJECT

public:
    enum Dialect {
        Kotlin,
        Swift,
    };

    enum Style {
        Default = 0,
        Comment,      // /* ... */
        LineComment,  // // ...
        String,
        Number,
        Keyword,
        Function,     // identifier followed by '('
        Bracket,      // ( ) [ ] { }
        Operator,
        Annotation,   // @Name
    };

    SimpleLexer(Dialect dialect, QObject *parent = nullptr);

    const char *language() const override;
    QString description(int style) const override;
    void styleText(int start, int end) override;

private:
    enum State {
        Normal,
        InBlockComment,
        InTripleString,
    };

    static QSet<QString> kotlinKeywords();
    static QSet<QString> swiftKeywords();

    void styleToken(int byteLength, Style style);

    QByteArray m_language;
    QSet<QString> m_keywords;
    int m_state = Normal;
};

#endif
