#ifndef LITE_STUDIO_MAIN_WINDOW_H
#define LITE_STUDIO_MAIN_WINDOW_H

#include <QFont>
#include <QMainWindow>
#include <QStringList>
#include <QTimer>

#include "EditorConfig.h"
#include "ProjectDetector.h"
#include "TaskPresets.h"
#include "Theme.h"

class QAction;
class AndroidPanel;
class ApplePanel;
class GitPanel;
class ConsolePanel;
class EditorTab;
class FileTreeView;
class FindBar;
class QLabel;
class QDockWidget;
class QMenu;
class QSplitter;
class QTabWidget;
class ProjectSearchPanel;
class QuickOpen;
class FileCollector;
class QListWidget;
class QTabBar;
class QMouseEvent;
class QTimer;
#ifdef LITE_STUDIO_HAVE_QTERMWIDGET
class TerminalPanel;
#endif

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct DefinitionCandidate {
        QString path;
        int line = 0;
        int index = 0;
        QString snippet;
    };
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    // Opens a folder, or a file plus its parent folder (CLI argument).
    void openPath(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void createMenus();
    void createCentralWidgets();
    void createStatusBar();
    void loadSettings();
    void saveSettings();

    void openFolder();
    void openFolderFromPath(const QString &path);
    void openFile(const QString &path);
    void saveCurrent();
    void saveAll();
    bool saveWithConflictCheck(EditorTab *editor);
    bool reloadEditor(EditorTab *editor);
    void reloadCurrent();
    bool closeTab(int index);
    void closeOtherTabs(EditorTab *keep);
    void closeAllTabs();
    void goToLine();
    bool maybeSave(EditorTab *editor);
    void applyTheme(Theme::Mode mode);

    void applyFontToEditors();
    QFont editorFont() const;
    void updateAndroidActions();
    void updateAppleActions();
    void updateGitActions();
    void showSymbolOutline();
    void saveSession();
    void restoreSessionFor(const QString &root);
    void applyAutoReload();
    void goToDefinition();
    void goBackInEditor();
    void completeWordInEditor();
    void handleDefinitionRequested(EditorTab *editor, const QString &word,
                                   int line, int index);
    void goToDefinitionFor(EditorTab *editor, const QString &word);
    void jumpToSymbol(EditorTab *origin, const QString &path, int line,
                      int index);
    void showDefinitionPopup(
        EditorTab *origin,
        const QList<DefinitionCandidate> &candidates);
    QList<DefinitionCandidate> findDefinitionCandidates(
        EditorTab *editor, const QString &word) const;
    void toggleCommentInEditor();
    void duplicateLineInEditor();
    void moveLinesInEditor(bool up);
    void matchingBraceInEditor();
    void toggleBookmarkInEditor();
    void nextBookmarkInEditor();
    void previousBookmarkInEditor();
    void reopenClosedTab();
    void applyEditorVisuals();
    void pickEditorFont();

    void addRecentFolder(const QString &path);
    void addRecentFile(const QString &path);
    void rebuildRecentsMenus();
    void loadTasks();
    void rebuildTasksMenu();

    void startFind();
    void findNextInEditor();
    void findPrevInEditor();
    void performReplace();
    void performReplaceAll();
    void applyFindHighlights();
    void closeFindBar();
    void focusEditor();

    void updateTabTitle(EditorTab *editor);
    void updateStatus();
    void updateActions();
    void updateWindowTitle();
    EditorTab *currentEditor() const;

    FileTreeView *m_fileTree = nullptr;
    QTabWidget *m_tabs = nullptr;
    QSplitter *m_splitter = nullptr;
    QString m_lastFolder;
    Theme::Mode m_theme = Theme::Mode::Dark;
    bool m_wordWrap = false;
    QStringList m_recentFolders;
    QStringList m_recentFiles;

    QAction *m_saveAction = nullptr;
    QAction *m_closeTabAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_selectAllAction = nullptr;
    QAction *m_darkAction = nullptr;
    QAction *m_lightAction = nullptr;
    QAction *m_wordWrapAction = nullptr;
    QAction *m_showIgnoredAction = nullptr;
    QAction *m_consoleAction = nullptr;
    QAction *m_syncTreeAction = nullptr;

    QMenu *m_recentFoldersMenu = nullptr;
    QMenu *m_recentFilesMenu = nullptr;
    QMenu *m_androidMenu = nullptr;
    QMenu *m_iosMenu = nullptr;
    QMenu *m_tasksMenu = nullptr;
    QList<TaskPreset> m_tasks;
    FileCollector *m_definitionCollector = nullptr;
    QAction *m_completeAction = nullptr;
    QAction *m_gotoDefAction = nullptr;
    QAction *m_goBackAction = nullptr;
    QAction *m_toggleCommentAction = nullptr;
    QAction *m_duplicateLineAction = nullptr;
    QAction *m_moveUpAction = nullptr;
    QAction *m_moveDownAction = nullptr;
    QAction *m_matchingBraceAction = nullptr;
    QAction *m_toggleBookmarkAction = nullptr;
    QAction *m_nextBookmarkAction = nullptr;
    QAction *m_prevBookmarkAction = nullptr;
    QAction *m_trimAction = nullptr;
    QAction *m_ensureNewlineAction = nullptr;
    QAction *m_whitespaceAction = nullptr;
    QAction *m_indentGuidesAction = nullptr;
    QAction *m_reopenTabAction = nullptr;
    QMenu *m_lineEndingsMenu = nullptr;
    QStringList m_closedTabs;
    QAction *m_symbolAction = nullptr;
    QAction *m_restoreSessionAction = nullptr;
    QAction *m_autoReloadAction = nullptr;
    QAction *m_autoSaveAction = nullptr;
    QTimer *m_recoveryTimer = nullptr;

    FindBar *m_findBar = nullptr;
    QDockWidget *m_projectDock = nullptr;
    ProjectSearchPanel *m_projectSearchPanel = nullptr;
    QDockWidget *m_consoleDock = nullptr;
    ConsolePanel *m_console = nullptr;
    QDockWidget *m_androidDock = nullptr;
    AndroidPanel *m_androidPanel = nullptr;
    QAction *m_androidRefreshAction = nullptr;
    QAction *m_androidBuildAction = nullptr;
    QAction *m_androidInstallAction = nullptr;
    QAction *m_androidRunAction = nullptr;
    QAction *m_androidStopDaemonsAction = nullptr;
    QDockWidget *m_appleDock = nullptr;
    ApplePanel *m_applePanel = nullptr;
    QDockWidget *m_gitDock = nullptr;
    GitPanel *m_gitPanel = nullptr;
    QMenu *m_gitMenu = nullptr;
    QAction *m_gitRefreshAction = nullptr;
    QAction *m_gitWorkingTreeAction = nullptr;
    QAction *m_gitStagedAction = nullptr;
    QAction *m_appleRefreshAction = nullptr;
    QAction *m_appleBuildAction = nullptr;
    QAction *m_appleInstallAction = nullptr;
    QAction *m_appleRunAction = nullptr;
#ifdef LITE_STUDIO_HAVE_QTERMWIDGET
    QDockWidget *m_terminalDock = nullptr;
    TerminalPanel *m_terminal = nullptr;
    QAction *m_terminalAction = nullptr;
#endif
    ProjectInfo m_projectInfo;
    EditorConfig::Settings m_editorConfig;
    QuickOpen *m_quickOpen = nullptr;
    bool m_findMatchCase = false;
    bool m_findWholeWord = false;
    QFont m_baseFont;
    int m_fontDelta = 0;

    QLabel *m_languageLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
    QLabel *m_indentLabel = nullptr;
    QLabel *m_eolLabel = nullptr;
    QLabel *m_modifiedLabel = nullptr;
};

#endif
