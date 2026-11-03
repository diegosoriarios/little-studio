#ifndef LITE_STUDIO_GIT_PANEL_H
#define LITE_STUDIO_GIT_PANEL_H

#include <QList>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "GitTooling.h"

class QLabel;
class QsciScintilla;
class QsciLexerDiff;

class ProcessOutputView;
class ProcessRunner;
class QComboBox;
class QLabel;
class QPushButton;
class QsciScintilla;
class QsciLexerDiff;

// Git diff dock: working-tree and staged diffs for the project or a single
// file, rendered with the QScintilla diff lexer. Streams through
// ProcessRunner; no threads.
class GitPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GitPanel(QWidget *parent = nullptr);

    void setProject(const QString &rootPath, bool enabled);
    bool isAvailable() const { return m_gitFound; }
    QList<GitTooling::ChangedFile> changedFiles() const { return m_changedFiles; }
    QString diffText() const;
    QString projectRoot() const { return m_root; }

    void refreshTargets();
    void diffWorkingTree();
    void diffStaged();

    void setTheme(int themeMode); // Theme::Mode as int (avoids include)

signals:
    void busyChanged(bool busy, const QString &description);

private:
    enum class DiffMode {
        WorkingTree,
        Staged,
    };

    enum class ProbeKind {
        None,
        NameStatus,
    };

    void startProbe();
    void onProbeFinished(int exitCode, bool crashed, qint64 elapsedMs);
    void runDiff();
    void onDiffFinished(int exitCode, bool crashed, qint64 elapsedMs);
    void flushDiff();
    void updateActions();
    void appendDiffLine(const QString &line);
    QString currentFileFilter() const;
    QString diffArgsBase() const; // "--cached" when staged mode

    ProcessRunner *m_probe = nullptr;
    ProcessRunner *m_diff = nullptr;
    QLabel *m_statusLabel = nullptr;
    QsciScintilla *m_diffView = nullptr;
    QsciLexerDiff *m_diffLexer = nullptr;

    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_fileCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_stopButton = nullptr;

    QTimer m_flushTimer;
    QStringList m_logBuffer;
    bool m_truncated = false;

    ProbeKind m_probeKind = ProbeKind::None;
    QStringList m_probeLines;

    QString m_root;
    QString m_git;
    bool m_outputNotAvailableMessageSent = false;
    bool m_gitFound = false;
    QList<GitTooling::ChangedFile> m_changedFiles;
    bool m_enabled = false;
    bool m_diffRunning = false;

    static constexpr int kMaxDiffBytes = 2 * 1024 * 1024;
};

#endif
