#ifndef LITE_STUDIO_QUICK_OPEN_H
#define LITE_STUDIO_QUICK_OPEN_H

#include <QDialog>
#include <QStringList>
#include <QTimer>

class FileCollector;
class QLabel;
class QLineEdit;
class QListWidget;

// VS Code style quick file open (Ctrl+P): type to fuzzy-filter the
// project's files, Enter/double-click opens the selection.
// Indexing is chunked via FileCollector; no background threads.
class QuickOpen : public QDialog
{
    Q_OBJECT

public:
    explicit QuickOpen(QWidget *parent = nullptr);

    void openForRoot(const QString &rootPath);

signals:
    void fileActivated(const QString &path);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void refilter();
    void showFileList();

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;

    FileCollector *m_collector = nullptr;
    QString m_rootPath;
    QStringList m_files;
};

#endif
