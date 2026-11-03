#include "EditorTab.h"

#include <QFontDatabase>
#include <QFileInfo>
#include <QSettings>
#include <utility>
#include <QMouseEvent>

#include <Qsci/qsciscintillabase.h>

#include "CodeTools.h"
#include "LexerFactory.h"
#include "RecoveryStore.h"

EditorTab::EditorTab(QWidget *parent)
    : QsciScintilla(parent)
{
    configureEditor();
    connect(this, &QsciScintilla::textChanged,
            this, &EditorTab::updateLineNumberWidth);
    connect(this, &QsciScintillaBase::SCN_CHARADDED, this,
            [this](int ch) {
        if (ch == '.')
            showMemberCompletion();
    });
}

void EditorTab::configureEditor()
{
    // Preserve what the user typed: no auto-indent, no completion popups,
    // no call tips, no formatting of any kind.
    setUtf8(true);
    setAutoIndent(false);
    // Word completion from the current buffer while typing.
    setAutoCompletionSource(QsciScintilla::AcsDocument);
    setAutoCompletionThreshold(3);
    setCallTipsStyle(QsciScintilla::CallTipsNone);
    setFolding(QsciScintilla::NoFoldStyle);
    setEolVisibility(false);
    setWrapMode(QsciScintilla::WrapNone);
    setTabWidth(4);
    setIndentationWidth(4);

    setBraceMatching(QsciScintilla::SloppyBraceMatch);
    setCaretLineVisible(true);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    setFont(font);
    setMarginsFont(font);

    setMarginType(0, QsciScintilla::NumberMargin);
    setMarginWidth(0, QStringLiteral("000"));

    // Search-match highlighting (color set per theme in Theme::applyEditor).
    indicatorDefine(QsciScintilla::StraightBoxIndicator, kFindIndicator);

    // Bookmark margin marker (margin 1, next to the line numbers).
    markerDefine(QsciScintilla::Bookmark, kBookmarkMarker);
    setMarkerBackgroundColor(QColor(0xe5, 0xc0, 0x7b), kBookmarkMarker);

    QSettings settings;
    setWhitespaceVisibility(
        settings.value(QStringLiteral("editor/whitespace"), false).toBool()
            ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
    setIndentationGuides(
        settings.value(QStringLiteral("editor/indentGuides"), true).toBool());
}

EditorTab::LoadStatus EditorTab::load(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!info.isFile()) {
        if (error)
            *error = QObject::tr("Not a regular file: %1").arg(path);
        return LoadStatus::Error;
    }

    const FileIO::LoadResult result = FileIO::readFile(path);
    if (result.binary)
        return LoadStatus::Binary;
    if (!result.ok) {
        if (error)
            *error = result.error;
        return LoadStatus::Error;
    }

    m_filePath = info.canonicalFilePath();
    m_encoding = result.encoding;
    m_eol = result.eol;
    m_largeFile = info.size() > kMaxHighlightBytes;
    m_language = m_largeFile ? QObject::tr("Plain Text (large file)")
                             : LexerFactory::languageName(m_filePath);
    m_readOnlyFile = !info.isWritable();
    setReadOnly(m_readOnlyFile);
    m_lastModified = info.lastModified();

    // EOL mode must be set before setText so the document keeps the
    // file's line-ending style.
    switch (m_eol) {
    case FileIO::LineEnding::CRLF:
        setEolMode(QsciScintilla::EolWindows);
        break;
    case FileIO::LineEnding::CR:
        setEolMode(QsciScintilla::EolMac);
        break;
    case FileIO::LineEnding::LF:
        setEolMode(QsciScintilla::EolUnix);
        break;
    }

    m_useTabs = detectIndentation(result.text);
    setIndentationsUseTabs(m_useTabs);

    setText(result.text);
    SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
    setModified(false);
    setCursorPosition(0, 0);
    updateLineNumberWidth();

    // Files over the size limit open as plain text: no lexer, no coloring.
    setLexer(m_largeFile ? nullptr : LexerFactory::create(m_filePath, this));
    setTheme(m_theme);
    return LoadStatus::Ok;
}

bool EditorTab::save(QString *error)
{
    // Never rewrite an untouched file; this preserves the bytes exactly.
    if (!isModified())
        return true;
    if (m_filePath.isEmpty()) {
        if (error)
            *error = QObject::tr("The editor has no file path.");
        return false;
    }
    if (!FileIO::writeFile(m_filePath, text(), m_encoding, m_eol, error))
        return false;
    RecoveryStore::removeSnapshot(m_filePath);
    m_lastModified = QFileInfo(m_filePath).lastModified();
    setModified(false);
    return true;
}

QString EditorTab::displayName() const
{
    return QFileInfo(m_filePath).fileName();
}

QString EditorTab::languageName() const
{
    return m_language;
}

QString EditorTab::encodingName() const
{
    return FileIO::encodingToString(m_encoding);
}

QString EditorTab::lineEndingName() const
{
    return FileIO::eolToString(m_eol);
}

QString EditorTab::indentationName() const
{
    return m_useTabs ? QObject::tr("Tabs")
                     : QObject::tr("Spaces: %1").arg(m_indentWidth);
}

bool EditorTab::changedOnDisk() const
{
    if (m_filePath.isEmpty())
        return false;
    const QFileInfo info(m_filePath);
    if (!info.isFile())
        return true; // deleted on disk
    return info.lastModified() != m_lastModified;
}

void EditorTab::setWordWrapEnabled(bool enabled)
{
    setWrapMode(enabled ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
}

void EditorTab::showMemberCompletion()
{
    const int caret = static_cast<int>(
        SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS));
    int line = 0;
    int index = 0;
    lineIndexFromPosition(caret, &line, &index);
    QString lineText = text(line);
    while (lineText.endsWith(QLatin1Char('\n'))
           || lineText.endsWith(QLatin1Char('\r')))
        lineText.chop(1);

    // The dot sits at index - 1; collect the identifier before it.
    const QString before = lineText.left(qMax(0, index - 1));
    int start = before.length();
    while (start > 0
           && (before.at(start - 1).isLetterOrNumber()
               || before.at(start - 1) == QLatin1Char('_')))
        --start;
    const QString prefix = before.mid(start);
    if (prefix.isEmpty())
        return;

    const QStringList members = CodeTools::collectMembers(text(), prefix);
    if (members.isEmpty())
        return;

    const QByteArray listData = members.join(QLatin1Char(' ')).toUtf8();
    SendScintilla(QsciScintillaBase::SCI_AUTOCSHOW,
                  static_cast<unsigned long>(0), listData.constData());
}

void EditorTab::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && (event->modifiers() & Qt::ControlModifier)) {
        const QPoint p = event->position().toPoint();
        const long pos = SendScintilla(
            QsciScintillaBase::SCI_POSITIONFROMPOINTCLOSE,
            static_cast<unsigned long>(p.x()), static_cast<long>(p.y()));
        int line = 0;
        int index = 0;
        lineIndexFromPosition(static_cast<int>(pos), &line, &index);
        QString lineText = text(line);
        while (lineText.endsWith(QLatin1Char('\n'))
               || lineText.endsWith(QLatin1Char('\r')))
            lineText.chop(1);
        const QString word = CodeTools::identifierAt(lineText, index);
        if (!word.isEmpty()) {
            emit definitionRequested(word, line, index);
            return;
        }
    }
    QsciScintilla::mousePressEvent(event);
}

void EditorTab::pushCurrentPosition()
{
    const int pos = static_cast<int>(
        SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS));
    if (m_backStack.isEmpty() || m_backStack.back() != pos)
        m_backStack.append(pos);
    while (m_backStack.size() > 100)
        m_backStack.removeFirst();
}

bool EditorTab::goBack()
{
    if (m_backStack.isEmpty())
        return false;
    const int pos = m_backStack.takeLast();
    int line = 0;
    int index = 0;
    lineIndexFromPosition(pos, &line, &index);
    setCursorPosition(line, index);
    ensureLineVisible(line);
    return true;
}

void EditorTab::toggleComment()
{
    const QString token = lineCommentToken();
    if (token.isEmpty())
        return;

    int fromLine = 0;
    int fromIndex = 0;
    int toLine = 0;
    int toIndex = 0;
    if (hasSelectedText()) {
        getSelection(&fromLine, &fromIndex, &toLine, &toIndex);
    } else {
        getCursorPosition(&fromLine, &fromIndex);
        toLine = fromLine;
    }
    if (toLine < fromLine)
        std::swap(fromLine, toLine);

    beginUndoAction();

    bool allCommented = true;
    for (int line = fromLine; line <= toLine; ++line) {
        QString lineText = text(line);
        while (lineText.endsWith(QLatin1Char('\n'))
               || lineText.endsWith(QLatin1Char('\r')))
            lineText.chop(1);
        if (lineText.trimmed().isEmpty())
            continue;
        if (!lineText.trimmed().startsWith(token)) {
            allCommented = false;
            break;
        }
    }

    for (int line = fromLine; line <= toLine; ++line) {
        QString lineText = text(line);
        while (lineText.endsWith(QLatin1Char('\n'))
               || lineText.endsWith(QLatin1Char('\r')))
            lineText.chop(1);
        if (lineText.trimmed().isEmpty())
            continue;

        QString updated;
        if (allCommented) {
            const int pos = lineText.indexOf(token);
            updated = lineText;
            updated.remove(pos, token.size());
            if (updated.startsWith(QLatin1Char(' ')))
                updated.remove(0, 1);
        } else {
            int firstNonSpace = 0;
            while (firstNonSpace < lineText.length()
                   && lineText.at(firstNonSpace).isSpace())
                ++firstNonSpace;
            updated = lineText;
            updated.insert(firstNonSpace, token + QLatin1Char(' '));
        }

        setSelection(line, 0, line, lineText.length());
        replaceSelectedText(updated);
    }
    endUndoAction();
}

QString EditorTab::lineCommentToken() const
{
    switch (CodeTools::languageForExtension(
        QFileInfo(m_filePath).suffix())) {
    case CodeTools::Language::Kotlin:
    case CodeTools::Language::Swift:
    case CodeTools::Language::Java:
    case CodeTools::Language::TsJs:
    case CodeTools::Language::Dart:
    case CodeTools::Language::Generic:
        break;
    default:
        return QString();
    }

    // File types that do not support // comments.
    static const QStringList noComment = {
        QStringLiteral("txt"), QStringLiteral("md"),
        QStringLiteral("markdown"), QStringLiteral("json"),
        QStringLiteral("jsonc"), QStringLiteral("geojson"),
        QStringLiteral("log"), QStringLiteral("xml"),
        QStringLiteral("fxml"), QStringLiteral("plist"),
        QStringLiteral("storyboard"), QStringLiteral("xib"),
        QStringLiteral("svg"), QStringLiteral("properties"),
        QStringLiteral("ini"), QStringLiteral("cfg"),
        QStringLiteral("yaml"), QStringLiteral("yml"),
    };
    return noComment.contains(QFileInfo(m_filePath).suffix().toLower())
        ? QString() : QStringLiteral("//");
}

void EditorTab::duplicateLine()
{
    if (hasSelectedText())
        SendScintilla(QsciScintillaBase::SCI_SELECTIONDUPLICATE);
    else
        SendScintilla(QsciScintillaBase::SCI_LINEDUPLICATE);
}

void EditorTab::moveLinesUp()
{
    SendScintilla(QsciScintillaBase::SCI_MOVESELECTEDLINESUP);
}

void EditorTab::moveLinesDown()
{
    SendScintilla(QsciScintillaBase::SCI_MOVESELECTEDLINESDOWN);
}

bool EditorTab::goToMatchingBrace()
{
    long brace = static_cast<long>(
        SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS));
    long other = -1;
    bool found = findMatchingBrace(brace, other,
                                   QsciScintilla::SloppyBraceMatch);
    if (!found && brace > 0) {
        brace -= 1;
        found = findMatchingBrace(brace, other, QsciScintilla::SloppyBraceMatch);
    }
    if (!found)
        return false;

    int line = 0;
    int index = 0;
    lineIndexFromPosition(static_cast<int>(other), &line, &index);
    setCursorPosition(line, index);
    ensureLineVisible(line);
    return true;
}

void EditorTab::toggleBookmark()
{
    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    const unsigned mask = 1u << kBookmarkMarker;
    const unsigned markers = static_cast<unsigned>(
        SendScintilla(QsciScintillaBase::SCI_MARKERGET,
                      static_cast<unsigned long>(line)));
    if (markers & mask)
        markerDelete(line, kBookmarkMarker);
    else
        markerAdd(line, kBookmarkMarker);
}

void EditorTab::nextBookmark()
{
    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    const unsigned mask = 1u << kBookmarkMarker;
    int found = markerFindNext(line + 1, mask);
    if (found < 0)
        found = markerFindNext(0, mask);
    if (found < 0)
        return;
    setCursorPosition(found, 0);
    ensureLineVisible(found);
}

void EditorTab::previousBookmark()
{
    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    const unsigned mask = 1u << kBookmarkMarker;
    int found = markerFindPrevious(line - 1, mask);
    if (found < 0)
        found = markerFindPrevious(lines() - 1, mask);
    if (found < 0)
        return;
    setCursorPosition(found, 0);
    ensureLineVisible(found);
}

void EditorTab::convertEol(QsciScintilla::EolMode mode)
{
    SendScintilla(QsciScintillaBase::SCI_CONVERTEOLS,
                  static_cast<unsigned long>(mode));
    setEolMode(mode);
    switch (mode) {
    case QsciScintilla::EolWindows:
        m_eol = FileIO::LineEnding::CRLF;
        break;
    case QsciScintilla::EolMac:
        m_eol = FileIO::LineEnding::CR;
        break;
    case QsciScintilla::EolUnix:
        m_eol = FileIO::LineEnding::LF;
        break;
    }
    setModified(true);
}

void EditorTab::trimTrailingWhitespace()
{
    beginUndoAction();
    const int total = lines();
    for (int line = 0; line < total; ++line) {
        QString lineText = text(line);
        while (lineText.endsWith(QLatin1Char('\n'))
               || lineText.endsWith(QLatin1Char('\r')))
            lineText.chop(1);
        int trailing = 0;
        while (trailing < lineText.length()) {
            const QChar c = lineText.at(lineText.length() - 1 - trailing);
            if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
                break;
            ++trailing;
        }
        if (trailing == 0 || lineText.trimmed().isEmpty())
            continue;
        setSelection(line, lineText.length() - trailing,
                     line, lineText.length());
        replaceSelectedText(QString());
    }
    endUndoAction();
}

void EditorTab::ensureFinalNewline()
{
    if (length() == 0)
        return;
    const QString eol = eolMode() == QsciScintilla::EolWindows
        ? QStringLiteral("\r\n")
        : (eolMode() == QsciScintilla::EolMac ? QStringLiteral("\r")
                                              : QStringLiteral("\n"));
    if (!text().endsWith(eol))
        append(eol);
}

void EditorTab::setWhitespaceVisible(bool on)
{
    setWhitespaceVisibility(on ? QsciScintilla::WsVisible
                               : QsciScintilla::WsInvisible);
}

void EditorTab::setIndentGuidesVisible(bool on)
{
    setIndentationGuides(on);
}

void EditorTab::setTheme(Theme::Mode mode)
{
    m_theme = mode;
    Theme::applyEditor(this, mode);
    if (lexer())
        Theme::applyLexer(lexer(), mode, font());
}

void EditorTab::applyIndentOverride(bool styleSet, bool useTabs, bool sizeSet,
                                    int indentSize, bool widthSet, int tabWidth)
{
    if (styleSet) {
        m_useTabs = useTabs;
        setIndentationsUseTabs(useTabs);
    }
    if (sizeSet) {
        m_indentWidth = indentSize;
        setIndentationWidth(indentSize);
        setTabWidth(indentSize);
    } else if (widthSet) {
        setTabWidth(tabWidth);
    }
}

void EditorTab::applyFont(const QFont &font)
{
    setFont(font);
    setMarginsFont(font);
    if (lexer())
        Theme::applyLexer(lexer(), m_theme, font);
    updateLineNumberWidth();
}

bool EditorTab::detectIndentation(const QString &text)
{
    int tabLines = 0;
    int spaceLines = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    const int limit = qMin(lines.size(), 1000);
    for (int i = 0; i < limit; ++i) {
        const QString &line = lines.at(i);
        if (line.startsWith(QLatin1Char('\t')))
            ++tabLines;
        else if (line.startsWith(QLatin1Char(' ')))
            ++spaceLines;
    }
    return tabLines > spaceLines;
}

void EditorTab::updateLineNumberWidth()
{
    const int digits = qMax(3, static_cast<int>(QString::number(lines()).size()));
    setMarginWidth(0, QString(digits, QLatin1Char('0')));
}

void EditorTab::clearMatches()
{
    SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, kFindIndicator);
    SendScintilla(QsciScintillaBase::SCI_INDICATORCLEARRANGE, 0UL,
                  static_cast<long>(length()));
}

int EditorTab::highlightMatches(const QString &expr, bool caseSensitive,
                                bool wholeWord)
{
    clearMatches();
    if (expr.isEmpty())
        return 0;

    // Keep the caret and viewport where the user left them: search without
    // scrolling (show = false) and restore the caret after the pass.
    const int caret = static_cast<int>(
        SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS));

    int count = 0;
    bool found = findFirst(expr, false, caseSensitive, wholeWord,
                           false /*wrap*/, true /*forward*/, 0, 0,
                           false /*show*/);
    while (found && count < 10000) {
        const int startPos = static_cast<int>(
            SendScintilla(QsciScintillaBase::SCI_GETSELECTIONSTART));
        const int endPos = static_cast<int>(
            SendScintilla(QsciScintillaBase::SCI_GETSELECTIONEND));
        int fromLine = 0;
        int fromIndex = 0;
        int toLine = 0;
        int toIndex = 0;
        lineIndexFromPosition(startPos, &fromLine, &fromIndex);
        lineIndexFromPosition(endPos, &toLine, &toIndex);
        fillIndicatorRange(fromLine, fromIndex, toLine, toIndex,
                           kFindIndicator);
        ++count;
        found = findNext();
    }

    SendScintilla(QsciScintillaBase::SCI_SETSEL, caret, caret);
    return count;
}

bool EditorTab::findNextMatch(const QString &expr, bool forward,
                              bool caseSensitive, bool wholeWord)
{
    if (expr.isEmpty())
        return false;
    if (!forward) {
        // The caret sits at the end of the current match; move it to the
        // match start so a backward search does not re-find the same one.
        const int selStart = static_cast<int>(
            SendScintilla(QsciScintillaBase::SCI_GETSELECTIONSTART));
        int line = 0;
        int index = 0;
        lineIndexFromPosition(selStart, &line, &index);
        setSelection(line, index, line, index);
    }
    // line = -1: search starts at the current position; wrap = true.
    return findFirst(expr, false, caseSensitive, wholeWord, true, forward);
}

bool EditorTab::replaceCurrent(const QString &expr, const QString &replacement,
                               bool forward, bool caseSensitive,
                               bool wholeWord)
{
    if (!findNextMatch(expr, forward, caseSensitive, wholeWord))
        return false;
    replace(replacement);
    return true;
}

int EditorTab::replaceAllMatches(const QString &expr, const QString &replacement,
                                 bool caseSensitive, bool wholeWord)
{
    if (expr.isEmpty())
        return 0;

    int count = 0;
    bool found = findFirst(expr, false, caseSensitive, wholeWord,
                           false /*wrap*/, true /*forward*/, 0, 0);
    while (found && count < 100000) {
        replace(replacement);
        ++count;
        found = findNext();
    }
    return count;
}
