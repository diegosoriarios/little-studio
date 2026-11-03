#include "SimpleLexer.h"

#include <Qsci/qsciscintilla.h>

SimpleLexer::SimpleLexer(Dialect dialect, QObject *parent)
    : QsciLexerCustom(parent)
{
    if (dialect == Kotlin) {
        m_language = QByteArrayLiteral("Kotlin");
        m_keywords = kotlinKeywords();
    } else {
        m_language = QByteArrayLiteral("Swift");
        m_keywords = swiftKeywords();
    }
}

const char *SimpleLexer::language() const
{
    return m_language.constData();
}

QString SimpleLexer::description(int style) const
{
    switch (style) {
    case Default:
        return tr("Default");
    case Comment:
        return tr("Comment");
    case LineComment:
        return tr("Line comment");
    case String:
        return tr("String");
    case Number:
        return tr("Number");
    case Keyword:
        return tr("Keyword");
    case Function:
        return tr("Function");
    case Bracket:
        return tr("Bracket");
    case Operator:
        return tr("Operator");
    case Annotation:
        return tr("Annotation");
    default:
        break;
    }
    return QString();
}

namespace {

// Scintilla styling lengths are byte counts, QString indexes are chars.
int tokenBytes(const QString &token)
{
    return token.toUtf8().size();
}

} // namespace

void SimpleLexer::styleText(int start, int end)
{
    if (!editor() || end < start)
        return;
    if (start <= 0)
        m_state = Normal;

    // Byte positions in, byte lengths out: text() decodes the exact
    // byte range, and each token's UTF-8 size preserves the alignment.
    const QString text = editor()->text(start, end);
    const int n = text.size();
    startStyling(start, 0x1f);

    int i = 0;
    while (i < n) {
        const QChar c = text.at(i);

        // Continuation of a multi-line construct opened in an earlier chunk.
        if (m_state == InBlockComment) {
            const int j = text.indexOf(QStringLiteral("*/"), i);
            int len;
            if (j < 0) {
                len = n - i;
            } else {
                len = j + 2 - i;
                m_state = Normal;
            }
            styleToken(tokenBytes(text.mid(i, len)), Comment);
            i += len;
            continue;
        }
        if (m_state == InTripleString) {
            const int j = text.indexOf(QStringLiteral("\"\"\""), i);
            int len;
            if (j < 0) {
                len = n - i;
            } else {
                len = j + 3 - i;
                m_state = Normal;
            }
            styleToken(tokenBytes(text.mid(i, len)), String);
            i += len;
            continue;
        }

        if (c == QLatin1Char('/') && i + 1 < n) {
            const QChar next = text.at(i + 1);
            if (next == QLatin1Char('/')) {
                int j = text.indexOf(QLatin1Char('\n'), i);
                if (j < 0)
                    j = n;
                styleToken(tokenBytes(text.mid(i, j - i)), LineComment);
                i = j;
                continue;
            }
            if (next == QLatin1Char('*')) {
                const int j = text.indexOf(QStringLiteral("*/"), i + 2);
                if (j < 0) {
                    styleToken(tokenBytes(text.mid(i)), Comment);
                    m_state = InBlockComment;
                    i = n;
                } else {
                    styleToken(tokenBytes(text.mid(i, j + 2 - i)), Comment);
                    i = j + 2;
                }
                continue;
            }
        }

        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            // Triple-quoted (multi-line) strings.
            if (c == QLatin1Char('"') && i + 2 < n
                && text.at(i + 1) == QLatin1Char('"')
                && text.at(i + 2) == QLatin1Char('"')) {
                const int j = text.indexOf(QStringLiteral("\"\"\""), i + 3);
                if (j < 0) {
                    styleToken(tokenBytes(text.mid(i)), String);
                    m_state = InTripleString;
                    i = n;
                } else {
                    styleToken(tokenBytes(text.mid(i, j + 3 - i)), String);
                    i = j + 3;
                }
                continue;
            }
            int j = i + 1;
            while (j < n) {
                const QChar t = text.at(j);
                if (t == QLatin1Char('\\')) {
                    j += 2;
                    continue;
                }
                if (t == c || t == QLatin1Char('\n'))
                    break;
                ++j;
            }
            const int len = qMin(j + 1, n) - i;
            styleToken(tokenBytes(text.mid(i, len)), String);
            i += len;
            continue;
        }

        if (c.isDigit()) {
            int j = i + 1;
            while (j < n) {
                const QChar t = text.at(j);
                if (t.isLetterOrNumber() || t == QLatin1Char('_')
                    || t == QLatin1Char('.'))
                    ++j;
                else
                    break;
            }
            styleToken(tokenBytes(text.mid(i, j - i)), Number);
            i = j;
            continue;
        }

        if (c.isLetter() || c == QLatin1Char('_')) {
            int j = i + 1;
            while (j < n) {
                const QChar t = text.at(j);
                if (t.isLetterOrNumber() || t == QLatin1Char('_'))
                    ++j;
                else
                    break;
            }
            const QString word = text.mid(i, j - i);
            if (m_keywords.contains(word)) {
                styleToken(tokenBytes(word), Keyword);
            } else {
                int k = j;
                while (k < n && text.at(k).isSpace())
                    ++k;
                if (k < n && text.at(k) == QLatin1Char('('))
                    styleToken(tokenBytes(word), Function);
                else
                    styleToken(tokenBytes(word), Default);
            }
            i = j;
            continue;
        }

        if (c == QLatin1Char('@')) {
            int j = i + 1;
            while (j < n && (text.at(j).isLetterOrNumber()
                             || text.at(j) == QLatin1Char('_')))
                ++j;
            styleToken(tokenBytes(text.mid(i, j - i)), Annotation);
            i = j;
            continue;
        }

        if (c == QLatin1Char('(') || c == QLatin1Char(')')
            || c == QLatin1Char('[') || c == QLatin1Char(']')
            || c == QLatin1Char('{') || c == QLatin1Char('}')) {
            styleToken(1, Bracket);
            ++i;
            continue;
        }

        // Whitespace and any other character (including multibyte ones).
        const Style style = (c.isSpace() || c.unicode() >= 0x80)
            ? Default : Operator;
        styleToken(tokenBytes(text.mid(i, 1)), style);
        ++i;
    }
}

void SimpleLexer::styleToken(int byteLength, Style style)
{
    if (byteLength > 0)
        setStyling(byteLength, style);
}

QSet<QString> SimpleLexer::kotlinKeywords()
{
    return QSet<QString>({
        QStringLiteral("abstract"), QStringLiteral("actual"),
        QStringLiteral("annotation"), QStringLiteral("as"),
        QStringLiteral("break"), QStringLiteral("by"),
        QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("companion"), QStringLiteral("const"),
        QStringLiteral("constructor"), QStringLiteral("continue"),
        QStringLiteral("crossinline"), QStringLiteral("data"),
        QStringLiteral("do"), QStringLiteral("dynamic"),
        QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("expect"), QStringLiteral("external"),
        QStringLiteral("false"), QStringLiteral("field"),
        QStringLiteral("final"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("fun"),
        QStringLiteral("get"), QStringLiteral("if"),
        QStringLiteral("import"), QStringLiteral("in"),
        QStringLiteral("infix"), QStringLiteral("init"),
        QStringLiteral("inline"), QStringLiteral("inner"),
        QStringLiteral("interface"), QStringLiteral("internal"),
        QStringLiteral("is"), QStringLiteral("it"),
        QStringLiteral("lateinit"), QStringLiteral("null"),
        QStringLiteral("object"), QStringLiteral("open"),
        QStringLiteral("operator"), QStringLiteral("out"),
        QStringLiteral("override"), QStringLiteral("package"),
        QStringLiteral("param"), QStringLiteral("private"),
        QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("reified"), QStringLiteral("return"),
        QStringLiteral("sealed"), QStringLiteral("set"),
        QStringLiteral("super"), QStringLiteral("suspend"),
        QStringLiteral("tailrec"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("true"),
        QStringLiteral("try"), QStringLiteral("typealias"),
        QStringLiteral("typeof"), QStringLiteral("val"),
        QStringLiteral("var"), QStringLiteral("vararg"),
        QStringLiteral("when"), QStringLiteral("where"),
        QStringLiteral("while"),
    });
}

QSet<QString> SimpleLexer::swiftKeywords()
{
    return QSet<QString>({
        QStringLiteral("actor"), QStringLiteral("any"),
        QStringLiteral("Any"), QStringLiteral("as"),
        QStringLiteral("associatedtype"), QStringLiteral("async"),
        QStringLiteral("await"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"),
        QStringLiteral("class"), QStringLiteral("continue"),
        QStringLiteral("convenience"), QStringLiteral("default"),
        QStringLiteral("defer"), QStringLiteral("deinit"),
        QStringLiteral("do"), QStringLiteral("dynamic"),
        QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("extension"), QStringLiteral("fallthrough"),
        QStringLiteral("false"), QStringLiteral("final"),
        QStringLiteral("for"), QStringLiteral("func"),
        QStringLiteral("get"), QStringLiteral("guard"),
        QStringLiteral("if"), QStringLiteral("import"),
        QStringLiteral("in"), QStringLiteral("infix"),
        QStringLiteral("init"), QStringLiteral("inout"),
        QStringLiteral("internal"), QStringLiteral("is"),
        QStringLiteral("lazy"), QStringLiteral("let"),
        QStringLiteral("mutating"), QStringLiteral("nil"),
        QStringLiteral("nonmutating"), QStringLiteral("open"),
        QStringLiteral("operator"), QStringLiteral("optional"),
        QStringLiteral("override"), QStringLiteral("postfix"),
        QStringLiteral("prefix"), QStringLiteral("private"),
        QStringLiteral("protocol"), QStringLiteral("public"),
        QStringLiteral("repeat"), QStringLiteral("required"),
        QStringLiteral("rethrows"), QStringLiteral("return"),
        QStringLiteral("self"), QStringLiteral("Self"),
        QStringLiteral("set"), QStringLiteral("some"),
        QStringLiteral("static"), QStringLiteral("struct"),
        QStringLiteral("subscript"), QStringLiteral("super"),
        QStringLiteral("switch"), QStringLiteral("throw"),
        QStringLiteral("throws"), QStringLiteral("true"),
        QStringLiteral("try"), QStringLiteral("typealias"),
        QStringLiteral("unowned"), QStringLiteral("var"),
        QStringLiteral("weak"), QStringLiteral("where"),
        QStringLiteral("while"), QStringLiteral("willSet"),
    });
}
