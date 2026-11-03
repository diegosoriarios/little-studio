#include "MainWindow.h"

#include "AndroidPanel.h"
#include "ApplePanel.h"
#include "CodeTools.h"
#ifdef LITE_STUDIO_HAVE_QTERMWIDGET
#include "TerminalPanel.h"
#endif
#include "CodeTools.h"
#include "ConsolePanel.h"
#include "EditorConfig.h"
#include "EditorTab.h"
#include "FileTreeView.h"
#include "FileCollector.h"
#include "FindBar.h"
#include "GitPanel.h"
#include "GitTooling.h"
#include "ProjectSearchPanel.h"
#include "QuickOpen.h"
#include "RecoveryStore.h"
#include "TaskPresets.h"

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFormLayout>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontComboBox>
#include <QGuiApplication>
#include <QInputDialog>
#include <QListWidget>
#include <QKeySequence>
#include <QLabel>
#include <QTabBar>
#include <QMouseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QMouseEvent>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_baseFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_baseFont.setPointSize(10);

    createCentralWidgets();
    createStatusBar();
    createMenus();

    m_projectSearchPanel = new ProjectSearchPanel(this);
    m_projectDock = new QDockWidget(tr("Find in Project"), this);
    m_projectDock->setWidget(m_projectSearchPanel);
    m_projectDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_projectDock);
    m_projectDock->hide();

    m_console = new ConsolePanel(this);
    m_consoleDock = new QDockWidget(tr("Console"), this);
    m_consoleDock->setWidget(m_console);
    m_consoleDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
    tabifyDockWidget(m_projectDock, m_consoleDock);
    m_consoleDock->hide();
    connect(m_consoleDock, &QDockWidget::visibilityChanged,
            m_consoleAction, &QAction::setChecked);

    m_androidPanel = new AndroidPanel(this);
    m_androidDock = new QDockWidget(tr("Android"), this);
    m_androidDock->setWidget(m_androidPanel);
    m_androidDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_androidDock);
    tabifyDockWidget(m_projectDock, m_androidDock);
    m_androidDock->hide();
    connect(m_androidPanel, &AndroidPanel::capabilitiesChanged,
            this, &MainWindow::updateAndroidActions);

    m_applePanel = new ApplePanel(this);
    m_appleDock = new QDockWidget(tr("iOS"), this);
    m_appleDock->setWidget(m_applePanel);
    m_appleDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_appleDock);
    tabifyDockWidget(m_projectDock, m_appleDock);
    m_appleDock->hide();
    connect(m_applePanel, &ApplePanel::capabilitiesChanged,
            this, &MainWindow::updateAppleActions);
    connect(m_applePanel, &ApplePanel::busyChanged, this,
            [this](bool busy, const QString &description) {
        if (busy)
            statusBar()->showMessage(tr("iOS: %1").arg(description));
        else
            statusBar()->showMessage(tr("iOS: %1 — done").arg(description),
                                     3000);
    });
    connect(m_androidPanel, &AndroidPanel::busyChanged, this,
            [this](bool busy, const QString &description) {
        if (busy)
            statusBar()->showMessage(tr("Android: %1").arg(description));
        else
            statusBar()->showMessage(tr("Android: %1 — done").arg(description),
                                     3000);
    });

#ifdef LITE_STUDIO_HAVE_QTERMWIDGET
    m_terminal = new TerminalPanel(this);
    m_terminalDock = new QDockWidget(tr("Terminal"), this);
    m_terminalDock->setWidget(m_terminal);
    m_terminalDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
    tabifyDockWidget(m_projectDock, m_terminalDock);
    m_terminalDock->hide();
    connect(m_terminalDock, &QDockWidget::visibilityChanged,
            m_terminalAction, &QAction::setChecked);
    connect(m_terminalDock, &QDockWidget::visibilityChanged, this,
            [this](bool visible) {
        if (visible)
            m_terminal->startShell();
    });
#endif

    connect(m_projectDock, &QDockWidget::visibilityChanged, this,
            [this](bool visible) {
        if (!visible)
            m_projectSearchPanel->stopSearch();
    });
    connect(m_projectSearchPanel, &ProjectSearchPanel::fileActivated, this,
            [this](const QString &path, int line, int index) {
        openFile(path);
        if (EditorTab *editor = currentEditor()) {
            editor->setCursorPosition(line, index);
            editor->ensureLineVisible(line);
            focusEditor();
        }
    });

    m_gitPanel = new GitPanel(this);
    m_gitDock = new QDockWidget(tr("Git"), this);
    m_gitDock->setWidget(m_gitPanel);
    m_gitDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, m_gitDock);
    tabifyDockWidget(m_projectDock, m_gitDock);
    m_gitDock->hide();
    connect(m_gitPanel, &GitPanel::busyChanged, this,
            [this](bool busy, const QString &description) {
        if (busy)
            statusBar()->showMessage(tr("Git: %1").arg(description));
        else
            statusBar()->showMessage(tr("Git: %1 — done").arg(description),
                                     3000);
    });

    m_definitionCollector = new FileCollector(this);
    m_recoveryTimer = new QTimer(this);
    m_recoveryTimer->setInterval(30000);
    m_recoveryTimer->setSingleShot(true);
    connect(m_recoveryTimer, &QTimer::timeout, this, [this]() {
        if (!m_autoSaveAction || !m_autoSaveAction->isChecked())
            return;
        bool anyModified = false;
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i))) {
                if (editor->isModified() && !editor->filePath().isEmpty()) {
                    RecoveryStore::writeSnapshot(editor->filePath(),
                                                 editor->text());
                    anyModified = true;
                }
            }
        }
        if (anyModified)
            m_recoveryTimer->start();
    });

    // Startup recovery prompt (after the window is shown).
    QTimer::singleShot(0, this, [this]() {
        const QList<RecoveryStore::Snapshot> snapshots =
            RecoveryStore::listSnapshots();
        if (snapshots.isEmpty())
            return;
        QStringList names;
        for (int i = 0; i < snapshots.size() && i < 5; ++i)
            names << snapshots.at(i).originalPath;
        if (snapshots.size() > 5)
            names << tr("… and %1 more").arg(snapshots.size() - 5);
        QMessageBox box(QMessageBox::Question, tr("Recover Unsaved Files"),
            tr("Found %1 recovery snapshot(s) from a previous session:\n\n%2\n\n"
               "Reopen them with the recovered text?")
                .arg(snapshots.size())
                .arg(names.join(QStringLiteral("\n"))),
            QMessageBox::Yes | QMessageBox::No, this);
        box.setDefaultButton(QMessageBox::Yes);
        if (box.exec() == QMessageBox::Yes) {
            for (const RecoveryStore::Snapshot &snapshot : snapshots) {
                if (!QFileInfo(snapshot.originalPath).isFile())
                    continue;
                openFile(snapshot.originalPath);
                if (EditorTab *editor = currentEditor()) {
                    if (editor->filePath() == snapshot.originalPath) {
                        editor->setText(snapshot.text);
                        editor->setModified(true);
                    }
                }
            }
        }
        RecoveryStore::clearSnapshots();
    });

    m_quickOpen = new QuickOpen(this);
    m_quickOpen = new QuickOpen(this);
    m_quickOpen->hide();
    connect(m_quickOpen, &QuickOpen::fileActivated, this,
            [this](const QString &path) { openFile(path); });

    loadSettings();
    updateStatus();
    updateActions();
    updateWindowTitle();
}

void MainWindow::createCentralWidgets()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_fileTree = new FileTreeView;
    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->setMovable(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setElideMode(Qt::ElideRight);

    m_splitter->addWidget(m_fileTree);
    m_splitter->addWidget(m_tabs);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({ 260, 840 });

    m_findBar = new FindBar;
    m_findBar->setVisible(false);

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_findBar);
    centralLayout->addWidget(m_splitter);
    setCentralWidget(central);

    connect(m_fileTree, &FileTreeView::fileActivated, this, &MainWindow::openFile);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        updateStatus();
        updateActions();
        updateWindowTitle();
        if (!currentEditor()) {
            m_findBar->setVisible(false);
        } else if (m_findBar->isVisible()) {
            applyFindHighlights();
        }
        if (m_syncTreeAction->isChecked()) {
            if (EditorTab *editor = currentEditor())
                m_fileTree->revealFile(editor->filePath());
        }
    });

    connect(m_findBar, &FindBar::findNextRequested,
            this, &MainWindow::findNextInEditor);
    connect(m_findBar, &FindBar::findPreviousRequested,
            this, &MainWindow::findPrevInEditor);
    connect(m_findBar, &FindBar::replaceRequested,
            this, &MainWindow::performReplace);
    connect(m_findBar, &FindBar::replaceAllRequested,
            this, &MainWindow::performReplaceAll);
    connect(m_findBar, &FindBar::optionsChanged,
            this, &MainWindow::applyFindHighlights);
    connect(m_findBar, &FindBar::closed, this, &MainWindow::closeFindBar);

    m_tabs->tabBar()->installEventFilter(this);
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        const int index = m_tabs->tabBar()->tabAt(pos);
        if (index < 0)
            return;
        auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(index));
        if (!editor)
            return;

        QMenu menu(this);
        QAction *closeAction = menu.addAction(tr("&Close"));
        QAction *closeOthersAction = menu.addAction(tr("Close &Others"));
        QAction *closeAllAction = menu.addAction(tr("Close &All"));
        menu.addSeparator();
        QAction *copyPathAction = menu.addAction(tr("Copy &Path"));
        QAction *copyNameAction = menu.addAction(tr("Copy File &Name"));

        QAction *chosen = menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
        if (!chosen)
            return;
        if (chosen == closeAction) {
            closeTab(index);
        } else if (chosen == closeAllAction) {
            closeAllTabs();
        } else if (chosen == closeOthersAction) {
            closeOtherTabs(editor);
        } else if (chosen == copyPathAction) {
            QGuiApplication::clipboard()->setText(editor->filePath());
        } else if (chosen == copyNameAction) {
            QGuiApplication::clipboard()->setText(editor->displayName());
        }
    });
}

void MainWindow::createStatusBar()
{
    m_languageLabel = new QLabel(this);
    m_positionLabel = new QLabel(this);
    m_indentLabel = new QLabel(this);
    m_eolLabel = new QLabel(this);
    m_modifiedLabel = new QLabel(this);

    statusBar()->addWidget(m_languageLabel);
    statusBar()->addPermanentWidget(m_positionLabel);
    statusBar()->addPermanentWidget(m_indentLabel);
    statusBar()->addPermanentWidget(m_eolLabel);
    statusBar()->addPermanentWidget(m_modifiedLabel);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *openFolderAction = new QAction(tr("Open &Folder..."), this);
    openFolderAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openFolder);
    fileMenu->addAction(openFolderAction);

    QMenu *recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    m_recentFoldersMenu = recentMenu->addMenu(tr("&Folders"));
    m_recentFilesMenu = recentMenu->addMenu(tr("F&iles"));
    rebuildRecentsMenus();

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveCurrent);
    fileMenu->addAction(m_saveAction);

    QAction *saveAllAction = new QAction(tr("Save A&ll"), this);
    saveAllAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveAllAction, &QAction::triggered, this, &MainWindow::saveAll);
    fileMenu->addAction(saveAllAction);

    QAction *reloadAction = new QAction(tr("Re&load from Disk"), this);
    reloadAction->setShortcut(QKeySequence::Refresh);
    connect(reloadAction, &QAction::triggered, this, &MainWindow::reloadCurrent);
    fileMenu->addAction(reloadAction);

    m_trimAction = new QAction(tr("Trim Trailing &Whitespace"), this);
    connect(m_trimAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor()) {
            editor->trimTrailingWhitespace();
            editor->ensureFinalNewline();
        }
    });
    fileMenu->addAction(m_trimAction);

    m_ensureNewlineAction = new QAction(tr("Ensure &Final Newline"), this);
    connect(m_ensureNewlineAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->ensureFinalNewline();
    });
    fileMenu->addAction(m_ensureNewlineAction);

    QMenu *eolMenu = fileMenu->addMenu(tr("Convert Line &Endings"));
    QAction *toLfAction = eolMenu->addAction(tr("Convert to &LF (Unix)"));
    connect(toLfAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->convertEol(QsciScintilla::EolUnix);
    });
    QAction *toCrLfAction = eolMenu->addAction(tr("Convert to C&RLF (Windows)"));
    connect(toCrLfAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->convertEol(QsciScintilla::EolWindows);
    });
    QAction *toCrAction = eolMenu->addAction(tr("Convert to &CR (Classic Mac)"));
    connect(toCrAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->convertEol(QsciScintilla::EolMac);
    });
    m_lineEndingsMenu = eolMenu;

    m_closeTabAction = new QAction(tr("Close Ta&b"), this);
    m_closeTabAction->setShortcut(QKeySequence::Close);
    connect(m_closeTabAction, &QAction::triggered, this, [this]() {
        closeTab(m_tabs->currentIndex());
    });
    fileMenu->addAction(m_closeTabAction);

    fileMenu->addSeparator();

    QAction *quitAction = new QAction(tr("E&xit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);
    fileMenu->addAction(quitAction);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    m_undoAction = new QAction(tr("&Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->undo();
    });
    editMenu->addAction(m_undoAction);

    m_redoAction = new QAction(tr("&Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->redo();
    });
    editMenu->addAction(m_redoAction);

    editMenu->addSeparator();

    m_cutAction = new QAction(tr("Cu&t"), this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    connect(m_cutAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->cut();
    });
    editMenu->addAction(m_cutAction);

    m_copyAction = new QAction(tr("&Copy"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->copy();
    });
    editMenu->addAction(m_copyAction);

    m_pasteAction = new QAction(tr("&Paste"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->paste();
    });
    editMenu->addAction(m_pasteAction);

    editMenu->addSeparator();

    m_selectAllAction = new QAction(tr("Select &All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->selectAll();
    });
    editMenu->addAction(m_selectAllAction);

    editMenu->addSeparator();

    QAction *findAction = new QAction(tr("&Find..."), this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::startFind);
    editMenu->addAction(findAction);

    QAction *findNextAction = new QAction(tr("Find &Next"), this);
    findNextAction->setShortcut(QKeySequence("F3"));
    connect(findNextAction, &QAction::triggered, this, &MainWindow::findNextInEditor);
    editMenu->addAction(findNextAction);

    QAction *findPrevAction = new QAction(tr("Find Pre&vious"), this);
    findPrevAction->setShortcut(QKeySequence("Shift+F3"));
    connect(findPrevAction, &QAction::triggered, this, &MainWindow::findPrevInEditor);
    editMenu->addAction(findPrevAction);

    QAction *goToLineAction = new QAction(tr("&Go to Line..."), this);
    goToLineAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(goToLineAction, &QAction::triggered, this, &MainWindow::goToLine);
    editMenu->addAction(goToLineAction);

    m_completeAction = new QAction(tr("Complete &Word"), this);
    m_completeAction->setShortcut(QKeySequence("Ctrl+Space"));
    connect(m_completeAction, &QAction::triggered, this, [this]() {
        if (EditorTab *editor = currentEditor())
            editor->autoCompleteFromDocument();
    });
    editMenu->addAction(m_completeAction);

    m_symbolAction = new QAction(tr("Go to &Symbol in File..."), this);
    m_symbolAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(m_symbolAction, &QAction::triggered, this, [this]() {
        showSymbolOutline();
    });
    editMenu->addAction(m_symbolAction);

    m_gotoDefAction = new QAction(tr("Go to &Definition"), this);
    m_gotoDefAction->setShortcut(QKeySequence("F12"));
    connect(m_gotoDefAction, &QAction::triggered, this, &MainWindow::goToDefinition);
    editMenu->addAction(m_gotoDefAction);

    m_goBackAction = new QAction(tr("Go &Back"), this);
    m_goBackAction->setShortcut(QKeySequence("Alt+Left"));
    connect(m_goBackAction, &QAction::triggered, this, &MainWindow::goBackInEditor);
    editMenu->addAction(m_goBackAction);

    editMenu->addSeparator();

    m_toggleCommentAction = new QAction(tr("Toggle &Comment"), this);
    m_toggleCommentAction->setShortcut(QKeySequence("Ctrl+/"));
    connect(m_toggleCommentAction, &QAction::triggered,
            this, &MainWindow::toggleCommentInEditor);
    editMenu->addAction(m_toggleCommentAction);

    m_duplicateLineAction = new QAction(tr("&Duplicate Line"), this);
    m_duplicateLineAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(m_duplicateLineAction, &QAction::triggered,
            this, &MainWindow::duplicateLineInEditor);
    editMenu->addAction(m_duplicateLineAction);

    m_moveUpAction = new QAction(tr("Move Lines &Up"), this);
    m_moveUpAction->setShortcut(QKeySequence("Alt+Up"));
    connect(m_moveUpAction, &QAction::triggered, this, [this]() {
        moveLinesInEditor(true);
    });
    editMenu->addAction(m_moveUpAction);

    m_moveDownAction = new QAction(tr("Move Lines Do&wn"), this);
    m_moveDownAction->setShortcut(QKeySequence("Alt+Down"));
    connect(m_moveDownAction, &QAction::triggered, this, [this]() {
        moveLinesInEditor(false);
    });
    editMenu->addAction(m_moveDownAction);

    m_matchingBraceAction = new QAction(tr("Go to Matching &Brace"), this);
    m_matchingBraceAction->setShortcut(QKeySequence("Ctrl+Shift+\\"));
    connect(m_matchingBraceAction, &QAction::triggered,
            this, &MainWindow::matchingBraceInEditor);
    editMenu->addAction(m_matchingBraceAction);

    editMenu->addSeparator();

    m_toggleBookmarkAction = new QAction(tr("&Toggle Bookmark"), this);
    m_toggleBookmarkAction->setShortcut(QKeySequence("Ctrl+F2"));
    connect(m_toggleBookmarkAction, &QAction::triggered,
            this, &MainWindow::toggleBookmarkInEditor);
    editMenu->addAction(m_toggleBookmarkAction);

    m_nextBookmarkAction = new QAction(tr("&Next Bookmark"), this);
    m_nextBookmarkAction->setShortcut(QKeySequence("F2"));
    connect(m_nextBookmarkAction, &QAction::triggered,
            this, &MainWindow::nextBookmarkInEditor);
    editMenu->addAction(m_nextBookmarkAction);

    m_prevBookmarkAction = new QAction(tr("&Previous Bookmark"), this);
    m_prevBookmarkAction->setShortcut(QKeySequence("Shift+F2"));
    connect(m_prevBookmarkAction, &QAction::triggered,
            this, &MainWindow::previousBookmarkInEditor);
    editMenu->addAction(m_prevBookmarkAction);

    editMenu->addSeparator();

    m_reopenTabAction = new QAction(tr("Reopen Closed Ta&b"), this);
    m_reopenTabAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(m_reopenTabAction, &QAction::triggered,
            this, &MainWindow::reopenClosedTab);
    editMenu->addAction(m_reopenTabAction);

    QAction *findInProjectAction = new QAction(tr("Find in &Project..."), this);
    findInProjectAction->setShortcut(QKeySequence("Ctrl+Shift+F"));
    connect(findInProjectAction, &QAction::triggered, this, [this]() {
        if (m_fileTree->rootPath().isEmpty()) {
            statusBar()->showMessage(tr("Open a folder first"), 2000);
            return;
        }
        m_projectDock->show();
        m_projectDock->raise();
        m_projectSearchPanel->openForRoot(m_fileTree->rootPath());
    });
    editMenu->addAction(findInProjectAction);

    QAction *quickOpenAction = new QAction(tr("&Quick Open File..."), this);
    quickOpenAction->setShortcut(QKeySequence("Ctrl+P"));
    connect(quickOpenAction, &QAction::triggered, this, [this]() {
        if (m_fileTree->rootPath().isEmpty()) {
            statusBar()->showMessage(tr("Open a folder first"), 2000);
            return;
        }
        m_quickOpen->openForRoot(m_fileTree->rootPath());
    });
    editMenu->addAction(quickOpenAction);

    m_androidMenu = menuBar()->addMenu(tr("&Android"));

    QAction *showAndroidAction = m_androidMenu->addAction(tr("Show Android &Panel"));
    connect(showAndroidAction, &QAction::triggered, this, [this]() {
        m_androidDock->show();
        m_androidDock->raise();
    });

    m_androidMenu->addSeparator();

    m_androidRefreshAction = m_androidMenu->addAction(tr("&Refresh Devices"));
    connect(m_androidRefreshAction, &QAction::triggered, this, [this]() {
        m_androidPanel->refreshTargets();
    });

    m_androidBuildAction = m_androidMenu->addAction(tr("&Build Debug"));
    connect(m_androidBuildAction, &QAction::triggered, this, [this]() {
        m_androidDock->show();
        m_androidDock->raise();
        m_androidPanel->buildDebug();
    });

    m_androidInstallAction = m_androidMenu->addAction(tr("&Install Debug"));
    connect(m_androidInstallAction, &QAction::triggered, this, [this]() {
        m_androidDock->show();
        m_androidDock->raise();
        m_androidPanel->installDebug();
    });

    m_androidRunAction = m_androidMenu->addAction(tr("&Run App"));
    connect(m_androidRunAction, &QAction::triggered, this, [this]() {
        m_androidDock->show();
        m_androidDock->raise();
        m_androidPanel->runApp();
    });

    m_androidMenu->addSeparator();

    m_androidStopDaemonsAction =
        m_androidMenu->addAction(tr("Stop Gradle &Daemons"));
    connect(m_androidStopDaemonsAction, &QAction::triggered, this, [this]() {
        m_androidPanel->stopGradleDaemons();
    });

    for (QAction *action : { m_androidRefreshAction, m_androidBuildAction,
                             m_androidInstallAction, m_androidRunAction,
                             m_androidStopDaemonsAction }) {
        action->setEnabled(false);
    }

    m_iosMenu = menuBar()->addMenu(tr("&iOS"));

    QAction *showIosAction = m_iosMenu->addAction(tr("Show &iOS Panel"));
    connect(showIosAction, &QAction::triggered, this, [this]() {
        m_appleDock->show();
        m_appleDock->raise();
    });

    m_iosMenu->addSeparator();

    m_appleRefreshAction = m_iosMenu->addAction(tr("&Refresh Simulators"));
    connect(m_appleRefreshAction, &QAction::triggered, this, [this]() {
        m_applePanel->refreshTargets();
    });

    m_appleBuildAction = m_iosMenu->addAction(tr("&Build Debug"));
    connect(m_appleBuildAction, &QAction::triggered, this, [this]() {
        m_appleDock->show();
        m_appleDock->raise();
        m_applePanel->buildDebug();
    });

    m_appleInstallAction = m_iosMenu->addAction(tr("&Install"));
    connect(m_appleInstallAction, &QAction::triggered, this, [this]() {
        m_appleDock->show();
        m_appleDock->raise();
        m_applePanel->installApp();
    });

    m_appleRunAction = m_iosMenu->addAction(tr("&Run App"));
    connect(m_appleRunAction, &QAction::triggered, this, [this]() {
        m_appleDock->show();
        m_appleDock->raise();
        m_applePanel->runApp();
    });

    for (QAction *action : { m_appleRefreshAction, m_appleBuildAction,
                             m_appleInstallAction, m_appleRunAction }) {
        action->setEnabled(false);
    }

    m_gitMenu = menuBar()->addMenu(tr("&Git"));
    QAction *showGitAction = m_gitMenu->addAction(tr("Show &Git Panel"));
    connect(showGitAction, &QAction::triggered, this, [this]() {
        m_gitDock->show();
        m_gitDock->raise();
    });
    m_gitMenu->addSeparator();
    m_gitRefreshAction = m_gitMenu->addAction(tr("&Refresh"));
    connect(m_gitRefreshAction, &QAction::triggered, this, [this]() {
        m_gitDock->show();
        m_gitDock->raise();
        m_gitPanel->refreshTargets();
    });
    m_gitWorkingTreeAction = m_gitMenu->addAction(tr("Diff &Working Tree"));
    connect(m_gitWorkingTreeAction, &QAction::triggered, this, [this]() {
        m_gitDock->show();
        m_gitDock->raise();
        m_gitPanel->diffWorkingTree();
    });
    m_gitStagedAction = m_gitMenu->addAction(tr("Diff &Staged (HEAD)"));
    connect(m_gitStagedAction, &QAction::triggered, this, [this]() {
        m_gitDock->show();
        m_gitDock->raise();
        m_gitPanel->diffStaged();
    });
    m_gitMenu->menuAction()->setVisible(false);

    m_tasksMenu = menuBar()->addMenu(tr("&Tasks"));
    rebuildTasksMenu();

    // Project-specific and task menus stay hidden until there is something
    // relevant to show for the opened folder.
    if (m_androidMenu)
        m_androidMenu->menuAction()->setVisible(false);
    if (m_iosMenu)
        m_iosMenu->menuAction()->setVisible(false);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    m_showIgnoredAction = new QAction(tr("Show &Ignored Folders"), this);
    m_showIgnoredAction->setCheckable(true);
    m_showIgnoredAction->setChecked(m_fileTree->showIgnored());
    connect(m_showIgnoredAction, &QAction::toggled,
            m_fileTree, &FileTreeView::setShowIgnored);
    viewMenu->addAction(m_showIgnoredAction);

    m_syncTreeAction = new QAction(tr("&Sync Tree to Editor"), this);
    m_syncTreeAction->setCheckable(true);
    m_syncTreeAction->setChecked(false);
    viewMenu->addAction(m_syncTreeAction);

    m_consoleAction = new QAction(tr("&Console"), this);
    m_consoleAction->setCheckable(true);
    m_consoleAction->setShortcut(QKeySequence("Ctrl+`"));
    connect(m_consoleAction, &QAction::toggled, this, [this](bool on) {
        if (on) {
            m_consoleDock->show();
            m_consoleDock->raise();
            m_console->focusInput();
        } else {
            m_consoleDock->hide();
        }
    });
    viewMenu->addAction(m_consoleAction);

#ifdef LITE_STUDIO_HAVE_QTERMWIDGET
    m_terminalAction = new QAction(tr("&Terminal"), this);
    m_terminalAction->setCheckable(true);
    m_terminalAction->setShortcut(QKeySequence("Ctrl+Shift+`"));
    connect(m_terminalAction, &QAction::toggled, this, [this](bool on) {
        if (on) {
            m_terminalDock->show();
            m_terminalDock->raise();
            m_terminal->startShell();
        } else {
            m_terminalDock->hide();
        }
    });
    viewMenu->addAction(m_terminalAction);

    QAction *restartTerminalAction = new QAction(tr("&Restart Terminal"), this);
    connect(restartTerminalAction, &QAction::triggered, this, [this]() {
        m_terminalDock->show();
        m_terminalDock->raise();
        m_terminal->restartShell();
    });
    viewMenu->addAction(restartTerminalAction);
#endif

    m_whitespaceAction = new QAction(tr("Show &Whitespace"), this);
    m_whitespaceAction->setCheckable(true);
    connect(m_whitespaceAction, &QAction::toggled, this, [this](bool on) {
        QSettings settings;
        settings.setValue(QStringLiteral("editor/whitespace"), on);
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i)))
                editor->setWhitespaceVisible(on);
        }
    });
    viewMenu->addAction(m_whitespaceAction);

    m_indentGuidesAction = new QAction(tr("Show &Indent Guides"), this);
    m_indentGuidesAction->setCheckable(true);
    connect(m_indentGuidesAction, &QAction::toggled, this, [this](bool on) {
        QSettings settings;
        settings.setValue(QStringLiteral("editor/indentGuides"), on);
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i)))
                editor->setIndentGuidesVisible(on);
        }
    });
    viewMenu->addAction(m_indentGuidesAction);

    m_restoreSessionAction = new QAction(tr("Restore Session on Open"), this);
    m_restoreSessionAction->setCheckable(true);
    viewMenu->addAction(m_restoreSessionAction);

    m_autoReloadAction = new QAction(tr("Auto-&Reload Unmodified Files"), this);
    m_autoReloadAction->setCheckable(true);
    connect(m_autoReloadAction, &QAction::toggled, this, [this](bool on) {
        if (on)
            applyAutoReload();
    });
    viewMenu->addAction(m_autoReloadAction);

    m_autoSaveAction = new QAction(tr("Auto-save &Recovery Copies"), this);
    m_autoSaveAction->setCheckable(true);
    connect(m_autoSaveAction, &QAction::toggled, this, [this](bool on) {
        QSettings settings;
        settings.setValue(QStringLiteral("recovery/autoSave"), on);
        if (on && m_recoveryTimer && !m_recoveryTimer->isActive())
            m_recoveryTimer->start();
    });
    viewMenu->addAction(m_autoSaveAction);

    m_wordWrapAction = new QAction(tr("&Word Wrap"), this);
    m_wordWrapAction->setCheckable(true);
    m_wordWrapAction->setChecked(m_wordWrap);
    connect(m_wordWrapAction, &QAction::toggled, this, [this](bool on) {
        m_wordWrap = on;
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i)))
                editor->setWordWrapEnabled(on);
        }
    });
    viewMenu->addAction(m_wordWrapAction);

    QAction *zoomInAction = new QAction(tr("Zoom &In"), this);
    zoomInAction->setShortcut(QKeySequence("Ctrl+="));
    connect(zoomInAction, &QAction::triggered, this, [this]() {
        m_fontDelta = qMin(8, m_fontDelta + 1);
        applyFontToEditors();
    });
    viewMenu->addAction(zoomInAction);

    QAction *zoomOutAction = new QAction(tr("Zoom &Out"), this);
    zoomOutAction->setShortcut(QKeySequence("Ctrl+-"));
    connect(zoomOutAction, &QAction::triggered, this, [this]() {
        m_fontDelta = qMax(-4, m_fontDelta - 1);
        applyFontToEditors();
    });
    viewMenu->addAction(zoomOutAction);

    QAction *zoomResetAction = new QAction(tr("Zoom &Reset"), this);
    zoomResetAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(zoomResetAction, &QAction::triggered, this, [this]() {
        m_fontDelta = 0;
        applyFontToEditors();
    });
    viewMenu->addAction(zoomResetAction);

    QAction *fontAction = new QAction(tr("Editor &Font..."), this);
    connect(fontAction, &QAction::triggered, this, [this]() {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Editor Font"));
        auto *familyCombo = new QFontComboBox(&dialog);
        familyCombo->setCurrentFont(m_baseFont);
        auto *sizeSpin = new QSpinBox(&dialog);
        sizeSpin->setRange(6, 24);
        sizeSpin->setValue(qMax(6, m_baseFont.pointSize() + m_fontDelta));
        auto *form = new QFormLayout;
        form->addRow(tr("Family:"), familyCombo);
        form->addRow(tr("Size:"), sizeSpin);
        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        auto *dialogLayout = new QVBoxLayout(&dialog);
        dialogLayout->addLayout(form);
        dialogLayout->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted)
            return;
        m_baseFont = familyCombo->currentFont();
        m_fontDelta = 0;
        const int pointSize = sizeSpin->value();
        m_baseFont.setPointSize(qMax(6, pointSize));
        QSettings settings;
        settings.setValue(QStringLiteral("editor/fontFamily"),
                          m_baseFont.family());
        settings.setValue(QStringLiteral("editor/fontSize"),
                          m_baseFont.pointSize());
        settings.setValue(QStringLiteral("ui/fontdelta"), 0);
        applyFontToEditors();
    });
    viewMenu->addAction(fontAction);

    QMenu *themeMenu = viewMenu->addMenu(tr("&Theme"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    m_darkAction = new QAction(tr("&Dark"), this);
    m_darkAction->setCheckable(true);
    m_darkAction->setChecked(m_theme == Theme::Mode::Dark);
    m_lightAction = new QAction(tr("&Light"), this);
    m_lightAction->setCheckable(true);
    themeGroup->addAction(m_darkAction);
    themeGroup->addAction(m_lightAction);
    themeMenu->addAction(m_darkAction);
    themeMenu->addAction(m_lightAction);

    connect(m_darkAction, &QAction::triggered, this, [this]() {
        applyTheme(Theme::Mode::Dark);
    });
    connect(m_lightAction, &QAction::triggered, this, [this]() {
        applyTheme(Theme::Mode::Light);
    });
}

void MainWindow::applyTheme(Theme::Mode mode)
{
    m_theme = mode;
    Theme::applyApplication(mode);
    m_darkAction->setChecked(mode == Theme::Mode::Dark);
    m_lightAction->setChecked(mode == Theme::Mode::Light);
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i)))
            editor->setTheme(mode);
    }
    m_gitPanel->setTheme(static_cast<int>(mode));
}

void MainWindow::openFolder()
{
    const QString start = m_lastFolder.isEmpty() ? QDir::homePath() : m_lastFolder;
    const QString path = QFileDialog::getExistingDirectory(this, tr("Open Folder"), start);
    if (path.isEmpty())
        return;
    openFolderFromPath(path);
}

void MainWindow::openPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return;
    if (info.isDir()) {
        openFolderFromPath(info.canonicalFilePath());
        return;
    }
    if (info.isFile()) {
        openFolderFromPath(info.dir().absolutePath());
        openFile(path);
    }
}

void MainWindow::openFolderFromPath(const QString &path)
{
    saveSession(); // persist the previous project's session first
    m_fileTree->setRootPath(path);
    m_lastFolder = path;
    addRecentFolder(path);

    // Cheap, bounded detection: root + one level, small capped reads.
    m_projectInfo = ProjectDetector::detect(path);
    m_editorConfig = EditorConfig::loadFor(path, QStringLiteral("*"));
    m_console->setProjectRoot(path);
    m_androidPanel->setProject(m_projectInfo.androidRoot,
                               m_projectInfo.hasAndroid);
    m_applePanel->setProject(m_projectInfo.appleRoot,
                             m_projectInfo.hasApple);

    updateAndroidActions();
    updateAppleActions();
    const bool gitOk = GitTooling::isGitRepository(path)
        && !GitTooling::findGit(qEnvironmentVariable("PATH")).isEmpty();
    m_gitPanel->setProject(path, gitOk);
    if (m_gitMenu)
        m_gitMenu->menuAction()->setVisible(gitOk);
    if (!gitOk)
        m_gitDock->hide();
    loadTasks();
    restoreSessionFor(path);
#ifdef LITE_STUDIO_HAVE_QTERMWIDGET
    m_terminal->setProjectRoot(path);
#endif
    if (!m_projectInfo.hasAndroid)
        m_androidDock->hide();

    updateWindowTitle();
    const QString summary = m_projectInfo.summary();
    statusBar()->showMessage(summary.isEmpty()
        ? path
        : tr("%1  ·  %2").arg(path, summary));
}

void MainWindow::openFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return;
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty())
        return;

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *existing = qobject_cast<EditorTab *>(m_tabs->widget(i));
        if (existing && existing->filePath() == canonical) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }

    auto *editor = new EditorTab;
    QString error;
    switch (editor->load(canonical, &error)) {
    case EditorTab::LoadStatus::Binary:
        QMessageBox::warning(this, tr("Binary File"),
            tr("\"%1\" appears to be a binary file and cannot be opened as text.")
                .arg(info.fileName()));
        delete editor;
        return;
    case EditorTab::LoadStatus::Error:
        QMessageBox::warning(this, tr("Open Failed"), error);
        delete editor;
        return;
    case EditorTab::LoadStatus::Ok:
        break;
    }

    const EditorConfig::Settings config = EditorConfig::loadFor(
        m_fileTree->rootPath(), info.fileName());
    editor->applyIndentOverride(config.indentStyleSet, config.useTabs,
        config.indentSizeSet, config.indentSize, config.tabWidthSet,
        config.tabWidth);

    editor->setTheme(m_theme);
    editor->setWordWrapEnabled(m_wordWrap);
    editor->applyFont(editorFont());
    addRecentFile(canonical);

    connect(editor, &QsciScintilla::modificationChanged, this,
            [this, editor](bool) {
        updateTabTitle(editor);
        updateStatus();
        updateActions();
    });
    connect(editor, &QsciScintilla::cursorPositionChanged, this,
            [this](int, int) { updateStatus(); });
    connect(editor, &QsciScintilla::textChanged, this, [this, editor]() {
        updateActions();
        if (m_findBar->isVisible())
            applyFindHighlights();
        if (m_autoSaveAction && m_autoSaveAction->isChecked()
            && editor->isModified() && m_recoveryTimer
            && !m_recoveryTimer->isActive()) {
            m_recoveryTimer->start();
        }
    });
    connect(editor, &EditorTab::definitionRequested, this,
            [this, editor](const QString &word, int line, int index) {
        handleDefinitionRequested(editor, word, line, index);
    });

    const int index = m_tabs->addTab(editor, editor->displayName());
    m_tabs->setTabToolTip(index, canonical);
    m_tabs->setCurrentIndex(index);
}

void MainWindow::saveCurrent()
{
    EditorTab *editor = currentEditor();
    if (!editor)
        return;
    saveWithConflictCheck(editor);
}

bool MainWindow::saveWithConflictCheck(EditorTab *editor)
{
    if (editor->changedOnDisk()) {
        QMessageBox box(QMessageBox::Warning, tr("File Changed on Disk"),
            tr("\"%1\" has changed on disk since it was opened.\n"
               "Reload it, overwrite it, or cancel?")
                .arg(editor->displayName()),
            QMessageBox::NoButton, this);
        QPushButton *reloadButton =
            box.addButton(tr("&Reload"), QMessageBox::DestructiveRole);
        QPushButton *overwriteButton =
            box.addButton(tr("&Overwrite"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(overwriteButton);
        box.exec();

        if (box.clickedButton() == reloadButton)
            return reloadEditor(editor);
        if (box.clickedButton() != overwriteButton)
            return false;
    }

    QString error;
    if (!editor->save(&error)) {
        QMessageBox::warning(this, tr("Save Failed"), error);
        return false;
    }
    return true;
}

bool MainWindow::reloadEditor(EditorTab *editor)
{
    QString error;
    const EditorTab::LoadStatus status = editor->load(editor->filePath(), &error);
    if (status != EditorTab::LoadStatus::Ok) {
        QMessageBox::warning(this, tr("Reload Failed"), error);
        return false;
    }
    editor->setWordWrapEnabled(m_wordWrap);
    editor->applyFont(editorFont());
    updateTabTitle(editor);
    updateStatus();
    updateActions();
    applyFindHighlights();
    return true;
}

void MainWindow::reloadCurrent()
{
    EditorTab *editor = currentEditor();
    if (!editor) {
        statusBar()->showMessage(tr("No file open"), 2000);
        return;
    }
    if (editor->isModified()) {
        const QMessageBox::StandardButton ret = QMessageBox::question(this,
            tr("Reload"),
            tr("Discard unsaved changes in \"%1\" and reload?")
                .arg(editor->displayName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;
    }
    reloadEditor(editor);
}

bool MainWindow::closeTab(int index)
{
    auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(index));
    if (!editor)
        return false;
    if (!maybeSave(editor))
        return false;
    m_closedTabs.removeAll(editor->filePath());
    m_closedTabs.prepend(editor->filePath());
    while (m_closedTabs.size() > 10)
        m_closedTabs.removeFirst();
    m_tabs->removeTab(index);
    editor->deleteLater();
    return true;
}

void MainWindow::closeOtherTabs(EditorTab *keep)
{
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        if (qobject_cast<EditorTab *>(m_tabs->widget(i)) == keep)
            continue;
        if (!closeTab(i))
            break;
    }
}

void MainWindow::closeAllTabs()
{
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        if (!closeTab(i))
            break;
    }
}

void MainWindow::reopenClosedTab()
{
    if (m_closedTabs.isEmpty()) {
        statusBar()->showMessage(tr("No recently closed tab"), 2000);
        return;
    }
    const QString path = m_closedTabs.takeFirst();
    openFile(path);
}

void MainWindow::saveSession()
{
    if (!m_restoreSessionAction)
        return;
    const QString root = m_fileTree->rootPath();
    if (root.isEmpty())
        return;
    QStringList files;
    QStringList cursors;
    const int count = qMin(20, m_tabs->count());
    for (int i = 0; i < count; ++i) {
        if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i))) {
            if (editor->filePath().isEmpty())
                continue;
            int line = 0;
            int index = 0;
            editor->getCursorPosition(&line, &index);
            files << editor->filePath();
            cursors << QStringLiteral("%1,%2").arg(line).arg(index);
        }
    }
    QSettings settings;
    settings.setValue(QStringLiteral("session/root"), root);
    settings.setValue(QStringLiteral("session/files"), files);
    settings.setValue(QStringLiteral("session/cursors"), cursors);
}

void MainWindow::restoreSessionFor(const QString &root)
{
    if (!m_restoreSessionAction || !m_restoreSessionAction->isChecked())
        return;
    QSettings settings;
    const QString savedRoot = settings.value(QStringLiteral("session/root")).toString();
    if (savedRoot != root)
        return;
    const QStringList files =
        settings.value(QStringLiteral("session/files")).toStringList();
    const QStringList cursors =
        settings.value(QStringLiteral("session/cursors")).toStringList();
    for (int i = 0; i < files.size() && i < cursors.size(); ++i) {
        if (!QFileInfo(files.at(i)).isFile())
            continue;
        openFile(files.at(i));
        for (int t = 0; t < m_tabs->count(); ++t) {
            auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(t));
            if (editor && editor->filePath() == files.at(i)) {
                const QStringList parts = cursors.at(i).split(QLatin1Char(','));
                if (parts.size() == 2) {
                    editor->setCursorPosition(parts.at(0).toInt(),
                                              parts.at(1).toInt());
                    editor->ensureLineVisible(parts.at(0).toInt());
                }
                break;
            }
        }
    }
}

void MainWindow::applyAutoReload()
{
    if (!m_autoReloadAction || !m_autoReloadAction->isChecked())
        return;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i))) {
            if (!editor->isModified() && editor->changedOnDisk())
                reloadEditor(editor);
        }
    }
}

void MainWindow::toggleCommentInEditor()
{
    if (EditorTab *editor = currentEditor())
        editor->toggleComment();
}

void MainWindow::duplicateLineInEditor()
{
    if (EditorTab *editor = currentEditor())
        editor->duplicateLine();
}

void MainWindow::moveLinesInEditor(bool up)
{
    if (EditorTab *editor = currentEditor()) {
        if (up)
            editor->moveLinesUp();
        else
            editor->moveLinesDown();
    }
}

void MainWindow::matchingBraceInEditor()
{
    EditorTab *editor = currentEditor();
    if (!editor)
        return;
    if (!editor->goToMatchingBrace())
        statusBar()->showMessage(tr("No matching brace"), 2000);
}

void MainWindow::toggleBookmarkInEditor()
{
    if (EditorTab *editor = currentEditor())
        editor->toggleBookmark();
}

void MainWindow::nextBookmarkInEditor()
{
    if (EditorTab *editor = currentEditor())
        editor->nextBookmark();
}

void MainWindow::previousBookmarkInEditor()
{
    if (EditorTab *editor = currentEditor())
        editor->previousBookmark();
}

void MainWindow::applyEditorVisuals()
{
    QSettings settings;
    const bool whitespace =
        settings.value(QStringLiteral("editor/whitespace"), false).toBool();
    const bool guides =
        settings.value(QStringLiteral("editor/indentGuides"), true).toBool();
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i))) {
            editor->setWhitespaceVisible(whitespace);
            editor->setIndentGuidesVisible(guides);
        }
    }
}

void MainWindow::saveAll()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i));
        if (!editor || !editor->isModified())
            continue;
        if (!saveWithConflictCheck(editor))
            return;
    }
}

void MainWindow::goToLine()
{
    EditorTab *editor = currentEditor();
    if (!editor) {
        statusBar()->showMessage(tr("No file open"), 2000);
        return;
    }
    int currentLine = 0;
    int index = 0;
    editor->getCursorPosition(&currentLine, &index);
    bool ok = false;
    const int line = QInputDialog::getInt(this, tr("Go to Line"),
        tr("Line number (1-%1):").arg(editor->lines()), currentLine + 1,
        1, editor->lines(), 1, &ok);
    if (!ok)
        return;
    editor->setCursorPosition(line - 1, 0);
    editor->ensureLineVisible(line - 1);
    focusEditor();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabs->tabBar()
        && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            const int index = m_tabs->tabBar()->tabAt(mouseEvent->position().toPoint());
            if (index >= 0)
                closeTab(index);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowActivate)
        applyAutoReload();
}

void MainWindow::goToDefinition()
{
    EditorTab *editor = currentEditor();
    if (!editor || editor->filePath().isEmpty()) {
        statusBar()->showMessage(tr("No file open"), 2000);
        return;
    }
    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    QString lineText = editor->text(line);
    while (lineText.endsWith(QLatin1Char('\n'))
           || lineText.endsWith(QLatin1Char('\r')))
        lineText.chop(1);
    const QString word = CodeTools::identifierAt(lineText, index);
    if (word.isEmpty()) {
        statusBar()->showMessage(tr("No symbol under cursor"), 2000);
        return;
    }
    handleDefinitionRequested(editor, word, line, index);
    focusEditor();
}

void MainWindow::goBackInEditor()
{
    EditorTab *editor = currentEditor();
    if (!editor)
        return;
    if (!editor->goBack())
        statusBar()->showMessage(tr("Nothing to go back to"), 2000);
}

void MainWindow::completeWordInEditor()
{
    EditorTab *editor = currentEditor();
    if (!editor)
        return;
    editor->autoCompleteFromDocument();
    focusEditor();
}

void MainWindow::handleDefinitionRequested(EditorTab *editor,
                                           const QString &word,
                                           int line, int index)
{
    Q_UNUSED(line);
    Q_UNUSED(index);
    goToDefinitionFor(editor, word);
}

void MainWindow::goToDefinitionFor(EditorTab *editor, const QString &word)
{
    if (!editor || word.isEmpty())
        return;
    editor->pushCurrentPosition();

    const CodeTools::Language lang = CodeTools::languageForExtension(
        QFileInfo(editor->filePath()).suffix());
    const QList<CodeTools::SymbolMatch> inFile =
        CodeTools::findDeclarations(editor->text(), word, lang);

    QList<DefinitionCandidate> candidates;
    for (const CodeTools::SymbolMatch &match : inFile) {
        candidates.append({ editor->filePath(), match.line, match.index,
                            match.snippet });
    }

    if (candidates.isEmpty()) {
        const QList<DefinitionCandidate> projectMatches =
            findDefinitionCandidates(editor, word);
        candidates.append(projectMatches);
    }

    if (candidates.isEmpty()) {
        statusBar()->showMessage(
            tr("No declaration of \"%1\" found").arg(word), 3000);
        editor->goBack(); // nothing jumped; don't leave a stale entry
        return;
    }

    if (candidates.size() == 1) {
        jumpToSymbol(editor, candidates.first().path, candidates.first().line,
                     candidates.first().index);
        return;
    }
    showDefinitionPopup(editor, candidates);
}

QList<MainWindow::DefinitionCandidate> MainWindow::findDefinitionCandidates(
    EditorTab *editor, const QString &word) const
{
    QList<DefinitionCandidate> candidates;
    const QString root = m_fileTree->rootPath();
    if (root.isEmpty() || m_definitionCollector->isRunning())
        return candidates;

    // Bounded, event-loop-driven project walk (no threads), 2 s guard.
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(m_definitionCollector, &FileCollector::finished, &loop,
            &QEventLoop::quit);
    guard.start(2000);
    m_definitionCollector->start(root, 2000);
    loop.exec();
    m_definitionCollector->stop();

    const QStringList files = m_definitionCollector->files();
    int scanned = 0;
    for (const QString &path : files) {
        if (candidates.size() >= 25 || scanned >= 500)
            break;
        if (path == editor->filePath())
            continue;
        const QFileInfo info(path);
        if (info.size() > EditorTab::kMaxHighlightBytes)
            continue;
        const FileIO::LoadResult result = FileIO::readFile(path);
        if (!result.ok || result.binary)
            continue;
        ++scanned;
        const CodeTools::Language lang = CodeTools::languageForExtension(
            info.suffix());
        const QList<CodeTools::SymbolMatch> found =
            CodeTools::findDeclarations(result.text, word, lang);
        for (const CodeTools::SymbolMatch &match : found) {
            candidates.append({ path, match.line, match.index, match.snippet });
            if (candidates.size() >= 25)
                break;
        }
    }
    return candidates;
}

void MainWindow::jumpToSymbol(EditorTab *origin, const QString &path,
                              int line, int index)
{
    EditorTab *target = origin;
    if (origin->filePath() != path) {
        openFile(path);
        target = nullptr;
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *tab = qobject_cast<EditorTab *>(m_tabs->widget(i));
            if (tab && tab->filePath() == path) {
                target = tab;
                break;
            }
        }
        if (!target)
            return;
    }
    target->setCursorPosition(line, index);
    target->ensureLineVisible(line);
    if (m_tabs->indexOf(target) >= 0)
        m_tabs->setCurrentIndex(m_tabs->indexOf(target));
    focusEditor();
}

void MainWindow::showDefinitionPopup(
    EditorTab *origin, const QList<DefinitionCandidate> &candidates)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Go to Declaration"));
    dialog.resize(600, 300);

    auto *list = new QListWidget(&dialog);
    for (const DefinitionCandidate &candidate : candidates) {
        const QString rel = QDir(m_fileTree->rootPath()).relativeFilePath(
            candidate.path);
        auto *item = new QListWidgetItem(list);
        item->setText(QStringLiteral("%1:%2  %3")
            .arg(rel).arg(candidate.line + 1).arg(candidate.snippet));
        item->setData(Qt::UserRole, candidate.path);
        item->setData(Qt::UserRole + 1, candidate.line);
        item->setData(Qt::UserRole + 2, candidate.index);
    }

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(list);
    list->setCurrentRow(0);

    QString chosenPath;
    int chosenLine = 0;
    int chosenIndex = 0;
    connect(list, &QListWidget::itemActivated, &dialog,
            [&](QListWidgetItem *item) {
        chosenPath = item->data(Qt::UserRole).toString();
        chosenLine = item->data(Qt::UserRole + 1).toInt();
        chosenIndex = item->data(Qt::UserRole + 2).toInt();
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted)
        return;
    jumpToSymbol(origin, chosenPath, chosenLine, chosenIndex);
}

void MainWindow::showSymbolOutline()
{
    EditorTab *editor = currentEditor();
    if (!editor || editor->filePath().isEmpty())
        return;

    const CodeTools::Language lang = CodeTools::languageForExtension(
        QFileInfo(editor->filePath()).suffix());
    const QList<CodeTools::SymbolMatch> symbols =
        CodeTools::listSymbols(editor->text(), lang);
    if (symbols.isEmpty()) {
        statusBar()->showMessage(tr("No symbols found in this file"), 2000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Go to Symbol in File"));
    dialog.resize(560, 340);

    auto *input = new QLineEdit(&dialog);
    input->setPlaceholderText(tr("Filter symbols"));
    input->setClearButtonEnabled(true);
    auto *list = new QListWidget(&dialog);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(input);
    layout->addWidget(list, 1);

    connect(input, &QLineEdit::textChanged, &dialog,
            [&symbols, list, input](const QString &filter) {
        list->clear();
        for (const CodeTools::SymbolMatch &symbol : symbols) {
            if (!filter.isEmpty()
                && !symbol.name.contains(filter, Qt::CaseInsensitive))
                continue;
            auto *item = new QListWidgetItem(list);
            item->setText(QStringLiteral("%1  ·  %2")
                .arg(symbol.name, symbol.snippet));
            item->setData(Qt::UserRole, symbol.line);
            item->setData(Qt::UserRole + 1, symbol.index);
        }
        if (list->count() > 0)
            list->setCurrentRow(0);
    });
    emit input->textChanged(input->text());

    EditorTab *origin = editor;
    connect(list, &QListWidget::itemActivated, &dialog,
            [&](QListWidgetItem *item) {
        origin->pushCurrentPosition();
        origin->setCursorPosition(item->data(Qt::UserRole).toInt(),
                                  item->data(Qt::UserRole + 1).toInt());
        origin->ensureLineVisible(item->data(Qt::UserRole).toInt());
        dialog.accept();
    });

    input->setFocus();
    dialog.exec();
    focusEditor();
}

void MainWindow::applyFontToEditors()
{
    const QFont font = editorFont();
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i)))
            editor->applyFont(font);
    }
}

QFont MainWindow::editorFont() const
{
    QFont font = m_baseFont;
    font.setPointSize(qMax(6, m_baseFont.pointSize() + m_fontDelta));
    return font;
}

void MainWindow::updateAndroidActions()
{
    if (m_androidMenu)
        m_androidMenu->menuAction()->setVisible(m_projectInfo.hasAndroid);

    const bool detected = m_projectInfo.hasAndroid && m_androidPanel;
    if (!detected) {
        for (QAction *action : { m_androidRefreshAction, m_androidBuildAction,
                                 m_androidInstallAction, m_androidRunAction,
                                 m_androidStopDaemonsAction }) {
            action->setEnabled(false);
        }
        return;
    }

    m_androidRefreshAction->setEnabled(true);
    m_androidBuildAction->setEnabled(m_androidPanel->canBuild());
    m_androidInstallAction->setEnabled(m_androidPanel->canInstall());
    m_androidRunAction->setEnabled(m_androidPanel->canRun());
    m_androidStopDaemonsAction->setEnabled(m_androidPanel->canBuild());
}

void MainWindow::updateAppleActions()
{
    const bool available = m_projectInfo.hasApple && m_applePanel
        && m_applePanel->isAvailable();

    if (m_iosMenu)
        m_iosMenu->menuAction()->setVisible(available);

    if (!available) {
        if (m_appleDock)
            m_appleDock->hide();
        for (QAction *action : { m_appleRefreshAction, m_appleBuildAction,
                                 m_appleInstallAction, m_appleRunAction }) {
            action->setEnabled(false);
        }
        return;
    }

    m_appleRefreshAction->setEnabled(true);
    m_appleBuildAction->setEnabled(m_applePanel->canBuild());
    m_appleInstallAction->setEnabled(m_applePanel->canInstall());
    m_appleRunAction->setEnabled(m_applePanel->canRun());
}

bool MainWindow::maybeSave(EditorTab *editor)
{
    if (!editor->isModified())
        return true;

    QMessageBox box(QMessageBox::Warning, tr("Unsaved Changes"),
        tr("\"%1\" has unsaved changes. Save before closing?")
            .arg(editor->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setDefaultButton(QMessageBox::Save);

    const int ret = box.exec();
    if (ret == QMessageBox::Discard)
        return true;
    if (ret != QMessageBox::Save)
        return false;
    return saveWithConflictCheck(editor);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *editor = qobject_cast<EditorTab *>(m_tabs->widget(i));
        if (editor && !maybeSave(editor)) {
            event->ignore();
            return;
        }
    }
    saveSettings();
    event->accept();
}

void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("ui/geometry").toByteArray());
    m_splitter->restoreState(settings.value("ui/splitter").toByteArray());

    applyTheme(settings.value("ui/theme").toString() == QStringLiteral("light")
        ? Theme::Mode::Light : Theme::Mode::Dark);

    m_wordWrapAction->setChecked(
        settings.value("ui/wordwrap", false).toBool());
    m_showIgnoredAction->setChecked(
        settings.value("ui/showignored", false).toBool());
    m_syncTreeAction->setChecked(
        settings.value("ui/synctree", false).toBool());

    m_findMatchCase = settings.value("find/matchcase", false).toBool();
    m_findWholeWord = settings.value("find/wholeword", false).toBool();
    m_findBar->setMatchCase(m_findMatchCase);
    m_findBar->setWholeWord(m_findWholeWord);

    m_fontDelta = settings.value("ui/fontdelta", 0).toInt();
    applyFontToEditors();

    m_recentFolders = settings.value("recent/folders").toStringList();
    m_recentFiles = settings.value("recent/files").toStringList();
    rebuildRecentsMenus();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("ui/geometry", saveGeometry());
    settings.setValue("ui/splitter", m_splitter->saveState());
    settings.setValue("ui/theme", m_theme == Theme::Mode::Dark
        ? QStringLiteral("dark") : QStringLiteral("light"));
    settings.setValue("ui/wordwrap", m_wordWrap);
    settings.setValue("ui/showignored", m_fileTree->showIgnored());
    settings.setValue("ui/synctree", m_syncTreeAction->isChecked());
    settings.setValue("find/matchcase", m_findBar->matchCase());
    settings.setValue("find/wholeword", m_findBar->wholeWord());
    settings.setValue("ui/fontdelta", m_fontDelta);
    settings.setValue("recent/folders", m_recentFolders);
    settings.setValue("recent/files", m_recentFiles);
}

void MainWindow::addRecentFolder(const QString &path)
{
    m_recentFolders.removeAll(path);
    m_recentFolders.prepend(path);
    while (m_recentFolders.size() > 10)
        m_recentFolders.removeLast();
    rebuildRecentsMenus();
}

void MainWindow::addRecentFile(const QString &path)
{
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 10)
        m_recentFiles.removeLast();
    rebuildRecentsMenus();
}

void MainWindow::rebuildRecentsMenus()
{
    if (!m_recentFoldersMenu || !m_recentFilesMenu)
        return;

    m_recentFoldersMenu->clear();
    for (const QString &path : m_recentFolders) {
        QAction *action = new QAction(path, m_recentFoldersMenu);
        connect(action, &QAction::triggered, this, [this, path]() {
            openFolderFromPath(path);
        });
        m_recentFoldersMenu->addAction(action);
    }
    m_recentFoldersMenu->setEnabled(!m_recentFolders.isEmpty());

    m_recentFilesMenu->clear();
    for (const QString &path : m_recentFiles) {
        QAction *action = new QAction(path, m_recentFilesMenu);
        connect(action, &QAction::triggered, this, [this, path]() {
            openPath(path);
        });
        m_recentFilesMenu->addAction(action);
    }
    m_recentFilesMenu->setEnabled(!m_recentFiles.isEmpty());
}

void MainWindow::loadTasks()
{
    QString error;
    m_tasks = TaskPresets::load(m_lastFolder, &error);
    if (!error.isEmpty())
        statusBar()->showMessage(error, 5000);
    rebuildTasksMenu();
}

void MainWindow::rebuildTasksMenu()
{
    if (!m_tasksMenu)
        return;

    m_tasksMenu->menuAction()->setVisible(!m_tasks.isEmpty());
    m_tasksMenu->clear();
    for (const TaskPreset &task : m_tasks) {
        QAction *action = m_tasksMenu->addAction(task.name);
        connect(action, &QAction::triggered, this, [this, task]() {
            m_consoleDock->show();
            m_consoleDock->raise();
            m_console->runTask(task);
        });
    }

    if (m_tasks.isEmpty()) {
        QAction *empty = m_tasksMenu->addAction(
            tr("(no .lite-studio/tasks.json)"));
        empty->setEnabled(false);
    }

    m_tasksMenu->addSeparator();
    QAction *reload = m_tasksMenu->addAction(tr("&Reload Tasks"));
    reload->setEnabled(!m_lastFolder.isEmpty());
    connect(reload, &QAction::triggered, this, &MainWindow::loadTasks);
}

void MainWindow::startFind()
{
    EditorTab *editor = currentEditor();
    if (!editor) {
        statusBar()->showMessage(tr("No file open"), 2000);
        return;
    }
    if (editor->hasSelectedText()) {
        const QString selection = editor->selectedText();
        if (!selection.contains(QLatin1Char('\n')))
            m_findBar->setSearchText(selection);
    }
    m_findBar->setVisible(true);
    m_findBar->focusSearchField();
    applyFindHighlights();
}

void MainWindow::findNextInEditor()
{
    EditorTab *editor = currentEditor();
    if (!editor) {
        statusBar()->showMessage(tr("No file open"), 2000);
        return;
    }
    m_findBar->setVisible(true);
    const bool found = editor->findNextMatch(m_findBar->searchText(), true,
        m_findBar->matchCase(), m_findBar->wholeWord());
    m_findBar->showMessage(found ? QString() : tr("Not found"));
}

void MainWindow::findPrevInEditor()
{
    EditorTab *editor = currentEditor();
    if (!editor) {
        statusBar()->showMessage(tr("No file open"), 2000);
        return;
    }
    m_findBar->setVisible(true);
    const bool found = editor->findNextMatch(m_findBar->searchText(), false,
        m_findBar->matchCase(), m_findBar->wholeWord());
    m_findBar->showMessage(found ? QString() : tr("Not found"));
}

void MainWindow::performReplace()
{
    EditorTab *editor = currentEditor();
    if (!editor)
        return;
    const bool replaced = editor->replaceCurrent(m_findBar->searchText(),
        m_findBar->replaceText(), true, m_findBar->matchCase(),
        m_findBar->wholeWord());
    m_findBar->showMessage(replaced ? QString() : tr("Not found"));
    applyFindHighlights();
}

void MainWindow::performReplaceAll()
{
    EditorTab *editor = currentEditor();
    if (!editor)
        return;
    const int count = editor->replaceAllMatches(m_findBar->searchText(),
        m_findBar->replaceText(), m_findBar->matchCase(),
        m_findBar->wholeWord());
    m_findBar->showMessage(tr("%n replacement(s)", "", count));
    applyFindHighlights();
}

void MainWindow::applyFindHighlights()
{
    EditorTab *editor = currentEditor();
    if (!editor)
        return;
    const int count = editor->highlightMatches(m_findBar->searchText(),
        m_findBar->matchCase(), m_findBar->wholeWord());
    if (m_findBar->searchText().isEmpty())
        m_findBar->showMessage(QString());
    else
        m_findBar->showMessage(count == 0 ? tr("Not found")
            : tr("%n match(es)", "", count));
}

void MainWindow::closeFindBar()
{
    m_findBar->setVisible(false);
    if (EditorTab *editor = currentEditor())
        editor->clearMatches();
    focusEditor();
}

void MainWindow::focusEditor()
{
    if (EditorTab *editor = currentEditor())
        editor->setFocus();
}

void MainWindow::updateTabTitle(EditorTab *editor)
{
    const int index = m_tabs->indexOf(editor);
    if (index < 0)
        return;
    const QString star = editor->isModified() ? QStringLiteral(" *") : QString();
    m_tabs->setTabText(index, editor->displayName() + star);
}

void MainWindow::updateStatus()
{
    EditorTab *editor = currentEditor();
    if (!editor) {
        m_languageLabel->setText(tr("No file open"));
        m_positionLabel->setText(QString());
        m_indentLabel->setText(QString());
        m_eolLabel->setText(QString());
        m_modifiedLabel->setText(QString());
        return;
    }

    int line = 0;
    int column = 0;
    editor->getCursorPosition(&line, &column);

    m_languageLabel->setText(QStringLiteral(" %1 ").arg(editor->languageName()));
    m_positionLabel->setText(QStringLiteral("Ln %1, Col %2 ").arg(line + 1).arg(column + 1));
    m_indentLabel->setText(QStringLiteral("%1 ").arg(editor->indentationName()));
    m_eolLabel->setText(QStringLiteral("%1 ").arg(editor->lineEndingName()));
    const QString state = editor->isReadOnlyFile() ? tr("Read-only")
        : (editor->isModified() ? tr("Modified") : tr("Saved"));
    m_modifiedLabel->setText(QStringLiteral("%1 ").arg(state));
}

void MainWindow::updateActions()
{
    EditorTab *editor = currentEditor();
    const bool hasEditor = editor != nullptr;

    m_saveAction->setEnabled(hasEditor && editor->isModified());
    m_closeTabAction->setEnabled(m_tabs->count() > 0);
    m_undoAction->setEnabled(hasEditor && editor->isUndoAvailable());
    m_redoAction->setEnabled(hasEditor && editor->isRedoAvailable());
    m_cutAction->setEnabled(hasEditor && editor->hasSelectedText());
    m_copyAction->setEnabled(hasEditor && editor->hasSelectedText());
    m_pasteAction->setEnabled(hasEditor);
    m_selectAllAction->setEnabled(hasEditor);
    m_completeAction->setEnabled(hasEditor);
    m_gotoDefAction->setEnabled(hasEditor);
    m_goBackAction->setEnabled(hasEditor && editor->canGoBack());
    const bool canComment = hasEditor && !editor->lineCommentToken().isEmpty();
    m_toggleCommentAction->setEnabled(canComment);
    if (m_toggleCommentAction->isEnabled() == false)
        m_toggleCommentAction->setToolTip(tr("No line comments for this file type"));
    m_duplicateLineAction->setEnabled(hasEditor);
    m_moveUpAction->setEnabled(hasEditor);
    m_moveDownAction->setEnabled(hasEditor);
    m_matchingBraceAction->setEnabled(hasEditor);
    m_toggleBookmarkAction->setEnabled(hasEditor);
    m_nextBookmarkAction->setEnabled(hasEditor);
    m_prevBookmarkAction->setEnabled(hasEditor);
    m_trimAction->setEnabled(hasEditor);
    m_ensureNewlineAction->setEnabled(hasEditor);
    if (m_lineEndingsMenu)
        m_lineEndingsMenu->setEnabled(hasEditor);
    m_reopenTabAction->setEnabled(!m_closedTabs.isEmpty());
}

void MainWindow::updateWindowTitle()
{
    if (EditorTab *editor = currentEditor()) {
        setWindowTitle(tr("Lite Studio — %1").arg(editor->displayName()));
        return;
    }
    const QString root = m_fileTree->rootPath();
    if (!root.isEmpty()) {
        setWindowTitle(tr("Lite Studio — %1").arg(QFileInfo(root).fileName()));
        return;
    }
    setWindowTitle(tr("Lite Studio"));
}

EditorTab *MainWindow::currentEditor() const
{
    return qobject_cast<EditorTab *>(m_tabs->currentWidget());
}
