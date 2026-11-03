#include "Theme.h"

#include <QApplication>
#include <QPalette>

#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qsciscintilla.h>

#include "EditorTab.h"

#include "SimpleLexer.h"

namespace {

// One palette struct keeps dark/light definitions side by side.
struct Colors {
    // Application palette.
    QColor window;
    QColor windowText;
    QColor base;
    QColor alternateBase;
    QColor text;
    QColor button;
    QColor buttonText;
    QColor toolTipBase;
    QColor toolTipText;
    QColor highlight;
    QColor highlightedText;
    QColor link;
    QColor placeholder;
    QColor disabled;

    // Editor chrome.
    QColor paper;
    QColor fg;
    QColor marginBg;
    QColor marginFg;
    QColor caret;
    QColor caretLine;
    QColor selectionBg;
    QColor selectionFg;
    QColor braceBg;
    QColor braceFg;
    QColor unmatched;

    // Syntax roles.
    QColor keyword;
    QColor type;
    QColor string;
    QColor comment;
    QColor number;
    QColor function;
    QColor op;
    QColor bracket;
    QColor annotation;
    QColor preprocessor;
    QColor error;
};

Colors darkColors()
{
    Colors c;
    c.window = QColor(0x25, 0x25, 0x26);
    c.windowText = QColor(0xcc, 0xcc, 0xcc);
    c.base = QColor(0x1e, 0x1e, 0x1e);
    c.alternateBase = QColor(0x2d, 0x2d, 0x30);
    c.text = QColor(0xd4, 0xd4, 0xd4);
    c.button = QColor(0x2d, 0x2d, 0x30);
    c.buttonText = QColor(0xcc, 0xcc, 0xcc);
    c.toolTipBase = QColor(0x1e, 0x1e, 0x1e);
    c.toolTipText = QColor(0xcc, 0xcc, 0xcc);
    c.highlight = QColor(0x26, 0x4f, 0x78);
    c.highlightedText = QColor(0xff, 0xff, 0xff);
    c.link = QColor(0x37, 0x94, 0xff);
    c.placeholder = QColor(0x80, 0x80, 0x80);
    c.disabled = QColor(0x70, 0x70, 0x70);

    c.paper = QColor(0x1e, 0x1e, 0x1e);
    c.fg = QColor(0xd4, 0xd4, 0xd4);
    c.marginBg = QColor(0x25, 0x25, 0x26);
    c.marginFg = QColor(0x85, 0x85, 0x85);
    c.caret = QColor(0xae, 0xaf, 0xad);
    c.caretLine = QColor(0x2a, 0x2d, 0x2e);
    c.selectionBg = QColor(0x26, 0x4f, 0x78);
    c.selectionFg = QColor(0xff, 0xff, 0xff);
    c.braceBg = QColor(0x3a, 0x3d, 0x41);
    c.braceFg = QColor(0xff, 0xd7, 0x00);
    c.unmatched = QColor(0xf1, 0x4c, 0x4c);

    c.keyword = QColor(0x56, 0x9c, 0xd6);
    c.type = QColor(0x4e, 0xc9, 0xb0);
    c.string = QColor(0xce, 0x91, 0x78);
    c.comment = QColor(0x6a, 0x99, 0x55);
    c.number = QColor(0xb5, 0xce, 0xa8);
    c.function = QColor(0xdc, 0xdc, 0xaa);
    c.op = QColor(0xd4, 0xd4, 0xd4);
    c.bracket = QColor(0xff, 0xd7, 0x00);
    c.annotation = QColor(0xd7, 0xba, 0x7d);
    c.preprocessor = QColor(0xc5, 0x86, 0xc0);
    c.error = QColor(0xf1, 0x4c, 0x4c);
    return c;
}

Colors lightColors()
{
    Colors c;
    c.window = QColor(0xf3, 0xf3, 0xf3);
    c.windowText = QColor(0x1f, 0x1f, 0x1f);
    c.base = QColor(0xff, 0xff, 0xff);
    c.alternateBase = QColor(0xf7, 0xf7, 0xf7);
    c.text = QColor(0x1f, 0x1f, 0x1f);
    c.button = QColor(0xf0, 0xf0, 0xf0);
    c.buttonText = QColor(0x1f, 0x1f, 0x1f);
    c.toolTipBase = QColor(0xff, 0xff, 0xff);
    c.toolTipText = QColor(0x1f, 0x1f, 0x1f);
    c.highlight = QColor(0x00, 0x78, 0xd4);
    c.highlightedText = QColor(0xff, 0xff, 0xff);
    c.link = QColor(0x00, 0x66, 0xcc);
    c.placeholder = QColor(0x90, 0x90, 0x90);
    c.disabled = QColor(0xa0, 0xa0, 0xa0);

    c.paper = QColor(0xff, 0xff, 0xff);
    c.fg = QColor(0x1f, 0x1f, 0x1f);
    c.marginBg = QColor(0xf3, 0xf3, 0xf3);
    c.marginFg = QColor(0x6e, 0x76, 0x81);
    c.caret = QColor(0x1f, 0x1f, 0x1f);
    c.caretLine = QColor(0xf7, 0xf7, 0xf7);
    c.selectionBg = QColor(0xad, 0xd6, 0xff);
    c.selectionFg = QColor(0x1f, 0x1f, 0x1f);
    c.braceBg = QColor(0xe5, 0xeb, 0xf1);
    c.braceFg = QColor(0x7a, 0x3e, 0x00);
    c.unmatched = QColor(0xd1, 0x1a, 0x1a);

    c.keyword = QColor(0x00, 0x00, 0xff);
    c.type = QColor(0x26, 0x7f, 0x99);
    c.string = QColor(0xa3, 0x15, 0x15);
    c.comment = QColor(0x00, 0x80, 0x00);
    c.number = QColor(0x09, 0x86, 0x58);
    c.function = QColor(0x79, 0x5e, 0x26);
    c.op = QColor(0x1f, 0x1f, 0x1f);
    c.bracket = QColor(0x7a, 0x3e, 0x00);
    c.annotation = QColor(0xaf, 0x00, 0xdb);
    c.preprocessor = QColor(0xaf, 0x00, 0xdb);
    c.error = QColor(0xd1, 0x1a, 0x1a);
    return c;
}

const Colors &colorsFor(Theme::Mode mode)
{
    static const Colors dark = darkColors();
    static const Colors light = lightColors();
    return mode == Theme::Mode::Dark ? dark : light;
}

void applyPalette(const Colors &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.windowText);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.alternateBase);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.button);
    p.setColor(QPalette::ButtonText, c.buttonText);
    p.setColor(QPalette::ToolTipBase, c.toolTipBase);
    p.setColor(QPalette::ToolTipText, c.toolTipText);
    p.setColor(QPalette::Highlight, c.highlight);
    p.setColor(QPalette::HighlightedText, c.highlightedText);
    p.setColor(QPalette::Link, c.link);
    p.setColor(QPalette::PlaceholderText, c.placeholder);
    p.setColor(QPalette::BrightText, QColor(0xff, 0x00, 0x00));
    p.setColor(QPalette::Disabled, QPalette::Text, c.disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.disabled);
    QApplication::setPalette(p);
}

void applyCppColors(QsciLexer *lexer, const Colors &c)
{
    using L = QsciLexerCPP;
    lexer->setColor(c.comment, L::Comment);
    lexer->setColor(c.comment, L::CommentLine);
    lexer->setColor(c.comment, L::CommentDoc);
    lexer->setColor(c.comment, L::CommentLineDoc);
    lexer->setColor(c.comment, L::CommentDocKeyword);
    lexer->setColor(c.comment, L::CommentDocKeywordError);
    lexer->setColor(c.comment, L::TaskMarker);
    lexer->setColor(c.comment, L::PreProcessorComment);
    lexer->setColor(c.string, L::DoubleQuotedString);
    lexer->setColor(c.string, L::SingleQuotedString);
    lexer->setColor(c.string, L::VerbatimString);
    lexer->setColor(c.string, L::RawString);
    lexer->setColor(c.string, L::TripleQuotedVerbatimString);
    lexer->setColor(c.string, L::HashQuotedString);
    lexer->setColor(c.string, L::EscapeSequence);
    lexer->setColor(c.string, L::UnclosedString);
    lexer->setColor(c.string, L::Regex);
    lexer->setColor(c.number, L::Number);
    lexer->setColor(c.number, L::UserLiteral);
    lexer->setColor(c.keyword, L::Keyword);
    lexer->setColor(c.type, L::KeywordSet2);
    lexer->setColor(c.type, L::GlobalClass);
    lexer->setColor(c.preprocessor, L::PreProcessor);
    lexer->setColor(c.op, L::Operator);
    lexer->setColor(c.fg, L::Identifier);
}

void applyJsonColors(QsciLexer *lexer, const Colors &c)
{
    using L = QsciLexerJSON;
    lexer->setColor(c.number, L::Number);
    lexer->setColor(c.string, L::String);
    lexer->setColor(c.string, L::UnclosedString);
    lexer->setColor(c.string, L::EscapeSequence);
    lexer->setColor(c.type, L::Property);
    lexer->setColor(c.type, L::IRI);
    lexer->setColor(c.type, L::IRICompact);
    lexer->setColor(c.keyword, L::Keyword);
    lexer->setColor(c.keyword, L::KeywordLD);
    lexer->setColor(c.comment, L::CommentLine);
    lexer->setColor(c.comment, L::CommentBlock);
    lexer->setColor(c.op, L::Operator);
    lexer->setColor(c.error, L::Error);
}

void applyMarkdownColors(QsciLexer *lexer, const Colors &c)
{
    using L = QsciLexerMarkdown;
    lexer->setColor(c.comment, L::Special);
    lexer->setColor(c.fg, L::StrongEmphasisAsterisks);
    lexer->setColor(c.fg, L::StrongEmphasisUnderscores);
    lexer->setColor(c.fg, L::EmphasisAsterisks);
    lexer->setColor(c.fg, L::EmphasisUnderscores);
    lexer->setColor(c.keyword, L::Header1);
    lexer->setColor(c.keyword, L::Header2);
    lexer->setColor(c.keyword, L::Header3);
    lexer->setColor(c.keyword, L::Header4);
    lexer->setColor(c.keyword, L::Header5);
    lexer->setColor(c.keyword, L::Header6);
    lexer->setColor(c.fg, L::Prechar);
    lexer->setColor(c.fg, L::UnorderedListItem);
    lexer->setColor(c.fg, L::OrderedListItem);
    lexer->setColor(c.comment, L::BlockQuote);
    lexer->setColor(c.comment, L::StrikeOut);
    lexer->setColor(c.comment, L::HorizontalRule);
    lexer->setColor(c.function, L::Link);
    lexer->setColor(c.string, L::CodeBackticks);
    lexer->setColor(c.string, L::CodeDoubleBackticks);
    lexer->setColor(c.string, L::CodeBlock);
}

void applyXmlColors(QsciLexer *lexer, const Colors &c)
{
    using L = QsciLexerHTML;
    lexer->setColor(c.keyword, L::Tag);
    lexer->setColor(c.error, L::UnknownTag);
    lexer->setColor(c.annotation, L::Attribute);
    lexer->setColor(c.annotation, L::UnknownAttribute);
    lexer->setColor(c.number, L::HTMLNumber);
    lexer->setColor(c.string, L::HTMLDoubleQuotedString);
    lexer->setColor(c.string, L::HTMLSingleQuotedString);
    lexer->setColor(c.fg, L::OtherInTag);
    lexer->setColor(c.comment, L::HTMLComment);
    lexer->setColor(c.function, L::Entity);
    lexer->setColor(c.keyword, L::XMLTagEnd);
    lexer->setColor(c.string, L::CDATA);
    lexer->setColor(c.fg, L::HTMLValue);
    lexer->setColor(c.keyword, L::SGMLCommand);
    lexer->setColor(c.annotation, L::SGMLParameter);
    lexer->setColor(c.string, L::SGMLDoubleQuotedString);
    lexer->setColor(c.string, L::SGMLSingleQuotedString);
    lexer->setColor(c.error, L::SGMLError);
    lexer->setColor(c.function, L::SGMLSpecial);
    lexer->setColor(c.function, L::SGMLEntity);
    lexer->setColor(c.comment, L::SGMLComment);
    lexer->setColor(c.comment, L::JavaScriptComment);
    lexer->setColor(c.comment, L::JavaScriptCommentLine);
    lexer->setColor(c.number, L::JavaScriptNumber);
    lexer->setColor(c.keyword, L::JavaScriptKeyword);
    lexer->setColor(c.string, L::JavaScriptDoubleQuotedString);
    lexer->setColor(c.string, L::JavaScriptSingleQuotedString);
}

void applySimpleColors(QsciLexer *lexer, const Colors &c)
{
    using L = SimpleLexer;
    lexer->setColor(c.fg, L::Default);
    lexer->setColor(c.comment, L::Comment);
    lexer->setColor(c.comment, L::LineComment);
    lexer->setColor(c.string, L::String);
    lexer->setColor(c.number, L::Number);
    lexer->setColor(c.keyword, L::Keyword);
    lexer->setColor(c.function, L::Function);
    lexer->setColor(c.bracket, L::Bracket);
    lexer->setColor(c.op, L::Operator);
    lexer->setColor(c.annotation, L::Annotation);
}

} // namespace

void Theme::applyApplication(Mode mode)
{
    applyPalette(colorsFor(mode));
}

void Theme::applyEditor(QsciScintilla *editor, Mode mode)
{
    const Colors &c = colorsFor(mode);

    editor->setColor(c.fg);
    editor->setPaper(c.paper);
    editor->setCaretForegroundColor(c.caret);
    editor->setCaretLineVisible(true);
    editor->setCaretLineBackgroundColor(c.caretLine);
    editor->setSelectionForegroundColor(c.selectionFg);
    editor->setSelectionBackgroundColor(c.selectionBg);
    editor->setMatchedBraceForegroundColor(c.braceFg);
    editor->setMatchedBraceBackgroundColor(c.braceBg);
    editor->setUnmatchedBraceForegroundColor(c.unmatched);
    editor->setUnmatchedBraceBackgroundColor(c.paper);
    editor->setMarginsBackgroundColor(c.marginBg);
    editor->setMarginsForegroundColor(c.marginFg);
    editor->setIndentationGuidesBackgroundColor(c.paper);
    editor->setIndentationGuidesForegroundColor(c.marginFg);
    editor->setIndicatorForegroundColor(
        mode == Mode::Dark ? QColor(0xff, 0xd7, 0x00, 0x70)
                           : QColor(0xff, 0xa5, 0x00, 0x80),
        EditorTab::kFindIndicator);
}

void Theme::applyLexer(QsciLexer *lexer, Mode mode, const QFont &font)
{
    if (!lexer)
        return;
    const Colors &c = colorsFor(mode);

    lexer->setDefaultColor(c.fg);
    lexer->setDefaultPaper(c.paper);
    lexer->setDefaultFont(font);
    lexer->setFont(font);
    lexer->setColor(c.fg);
    lexer->setPaper(c.paper);

    if (auto *simple = qobject_cast<SimpleLexer *>(lexer)) {
        Q_UNUSED(simple);
        applySimpleColors(lexer, c);
    } else if (qobject_cast<QsciLexerXML *>(lexer)) {
        applyXmlColors(lexer, c);
    } else if (qobject_cast<QsciLexerJSON *>(lexer)) {
        applyJsonColors(lexer, c);
    } else if (qobject_cast<QsciLexerMarkdown *>(lexer)) {
        applyMarkdownColors(lexer, c);
    } else if (qobject_cast<QsciLexerCPP *>(lexer)) {
        applyCppColors(lexer, c);
    }
}
