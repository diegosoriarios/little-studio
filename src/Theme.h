#ifndef LITE_STUDIO_THEME_H
#define LITE_STUDIO_THEME_H

#include <QColor>
#include <QFont>

class QsciLexer;
class QsciScintilla;

// Readable dark and light themes: application palette, editor chrome
// (margins, caret, selection, braces) and per-lexer syntax colors.
class Theme
{
public:
    enum class Mode {
        Dark,
        Light,
    };

    static void applyApplication(Mode mode);
    static void applyEditor(QsciScintilla *editor, Mode mode);
    static void applyLexer(QsciLexer *lexer, Mode mode, const QFont &font);

private:
    Theme() = default;
};

#endif
