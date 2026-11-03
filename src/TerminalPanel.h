#ifndef LITE_STUDIO_TERMINAL_PANEL_H
#define LITE_STUDIO_TERMINAL_PANEL_H

#include <QWidget>

class QTermWidget;

// Optional embedded terminal backed by qtermwidget. The terminal widget (and
// its shell process) is created lazily on first use; this class is only
// compiled when qtermwidget is available.
class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);

    void setProjectRoot(const QString &root);
    void startShell();
    void restartShell();
    void focusTerminal();

    bool isStarted() const { return m_started; }

private:
    QTermWidget *m_terminal = nullptr;
    QString m_root;
    bool m_started = false;
};

#endif
