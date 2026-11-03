#ifndef LITE_STUDIO_EDITOR_TAB_H
#define LITE_STUDIO_EDITOR_TAB_H

#include <QDateTime>
#include <Qsci/qsciscintilla.h>
#include <QVector>

#include "FileIO.h"
#include "Theme.h"

class QMouseEvent;

// One open file: a QScintilla editor plus file state
// (path, encoding, line endings, indentation style).
class EditorTab : public QsciScintilla
{
    Q_OBJECT

public:
    enum class LoadStatus {
        Ok,
        Binary,
        Error,
    };

    explicit EditorTab(QWidget *parent = nullptr);

    LoadStatus load(const QString &path, QString *error);
    bool save(QString *error);

    QString filePath() const { return m_filePath; }
    QString displayName() const;
    QString languageName() const;
    QString encodingName() const;
    QString lineEndingName() const;
    QString indentationName() const;
    bool isLargeFile() const { return m_largeFile; }
    bool isReadOnlyFile() const { return m_readOnlyFile; }
    bool changedOnDisk() const;

    void pushCurrentPosition();
    bool goBack();
    bool canGoBack() const { return !m_backStack.isEmpty(); }

    // Editing quick actions (Stage 14). Comment token is "" for file types
    // without line comments (XML, JSON, plain text).
    void toggleComment();
    QString lineCommentToken() const;
    void duplicateLine();
    void moveLinesUp();
    void moveLinesDown();
    bool goToMatchingBrace();
    void toggleBookmark();
    void nextBookmark();
    void previousBookmark();
    void convertEol(QsciScintilla::EolMode mode);
    void trimTrailingWhitespace();
    void ensureFinalNewline();
    void setWhitespaceVisible(bool on);
    void setIndentGuidesVisible(bool on);

    void setWordWrapEnabled(bool enabled);
    void setTheme(Theme::Mode mode);
    void applyFont(const QFont &font);
    void applyIndentOverride(bool styleSet, bool useTabs, bool sizeSet,
                             int indentSize, bool widthSet, int tabWidth);

    // Search support (find bar). Positions/handles use byte-safe QScintilla
    // APIs internally.
    int highlightMatches(const QString &expr, bool caseSensitive,
                         bool wholeWord);
    void clearMatches();
    bool findNextMatch(const QString &expr, bool forward, bool caseSensitive,
                       bool wholeWord);
    bool replaceCurrent(const QString &expr, const QString &replacement,
                        bool forward, bool caseSensitive, bool wholeWord);
    int replaceAllMatches(const QString &expr, const QString &replacement,
                          bool caseSensitive, bool wholeWord);

    static constexpr int kFindIndicator = 8;
    static constexpr int kBookmarkMarker = 1;
    static constexpr qint64 kMaxHighlightBytes = 2 * 1024 * 1024; // used app-wide

signals:
    void definitionRequested(const QString &word, int line, int index);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void configureEditor();
    bool detectIndentation(const QString &text);
    void updateLineNumberWidth();
    void showMemberCompletion();

    QString m_filePath;
    QDateTime m_lastModified;
    FileIO::Encoding m_encoding = FileIO::Encoding::Utf8;
    FileIO::LineEnding m_eol = FileIO::LineEnding::LF;
    QString m_language;
    bool m_largeFile = false;
    bool m_readOnlyFile = false;
    bool m_useTabs = false;
    QVector<int> m_backStack;
    int m_indentWidth = 4;
    Theme::Mode m_theme = Theme::Mode::Dark;
};

#endif
