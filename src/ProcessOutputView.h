#ifndef LITE_STUDIO_PROCESS_OUTPUT_VIEW_H
#define LITE_STUDIO_PROCESS_OUTPUT_VIEW_H

#include <QStringList>
#include <QWidget>

#include "LineBuffer.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// Read-only, line-capped output view with a client-side filter, status line
// and Stop button. Shared by the console and the run panels.
class ProcessOutputView : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessOutputView(QWidget *parent = nullptr);

    void appendLine(const QString &line);
    void appendLines(const QStringList &lines);
    void clear();
    void setBusy(bool busy, const QString &status = QString());
    void setStatusText(const QString &text);

signals:
    void stopRequested();

private:
    void rebuildView();
    bool isAtBottom() const;

    static constexpr int kMaxLines = 5000;

    QPlainTextEdit *m_view = nullptr;
    QLineEdit *m_filter = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_stopButton = nullptr;
    LineBuffer m_lines{kMaxLines};
};

#endif
