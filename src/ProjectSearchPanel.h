#ifndef LITE_STUDIO_PROJECT_SEARCH_PANEL_H
#define LITE_STUDIO_PROJECT_SEARCH_PANEL_H

#include <QStringList>
#include <QTimer>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class FileCollector;

// Project-wide search over the currently open folder.
// Chunked and single-threaded: indexing and scanning each run in small
// time-boxed batches per event loop tick, keeping the UI responsive.
// No background threads.
class ProjectSearchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectSearchPanel(QWidget *parent = nullptr);

    void openForRoot(const QString &rootPath);
    void stopSearch();
    void replaceInResults();

signals:
    void fileActivated(const QString &path, int line, int index);

private:
    void startSearch();
    void onIndexed();
    void searchStep();
    void searchFile(const QString &path);
    void updateActions();
    void addResult(const QString &path, int line, int index,
                   const QString &lineText);
    void updateStatus();

    QLineEdit *m_input = nullptr;
    QLineEdit *m_replaceInput = nullptr;
    QCheckBox *m_caseBox = nullptr;
    QCheckBox *m_wordBox = nullptr;
    QPushButton *m_button = nullptr;
    QPushButton *m_replaceButton = nullptr;
    QLabel *m_status = nullptr;
    QListWidget *m_results = nullptr;

    QString m_rootPath;
    QStringList m_files;      // snapshot after indexing finishes
    FileCollector *m_collector = nullptr;
    int m_fileIndex = 0;
    int m_matchCount = 0;
    bool m_searching = false;
    QTimer m_timer;

    static constexpr int kMaxMatches = 500;
    static constexpr int kMaxFiles = 50000;
    static constexpr qint64 kMaxSearchBytes = 2 * 1024 * 1024;
};

#endif
