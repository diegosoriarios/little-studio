#ifndef LITE_STUDIO_CONSOLE_PANEL_H
#define LITE_STUDIO_CONSOLE_PANEL_H

#include <QStringList>
#include <QWidget>

class ProcessOutputView;
class ProcessRunner;
class QCheckBox;
class QLineEdit;
struct TaskPreset;

// Minimal command console: runs shell commands in the project root and
// streams output. No TTY, so interactive full-screen programs are not
// supported; stdin-driven prompts (y/n) are not forwarded either.
class ConsolePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget *parent = nullptr);

    void setProjectRoot(const QString &root);
    void focusInput();
    void runTask(const TaskPreset &task);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void runCommand();
    void stopCommand();
    void applyLimits(QString &program, QStringList &arguments) const;
    void onFinished(int exitCode, bool crashed, qint64 elapsedMs);

    ProcessRunner *m_runner = nullptr;
    ProcessOutputView *m_output = nullptr;
    QLineEdit *m_input = nullptr;
    QCheckBox *m_limitsCheck = nullptr;

    QString m_root;
    QStringList m_history;
    int m_historyIndex = 0;
    QString m_draft;
    bool m_applyLimits = true;

    static constexpr int kMaxHistory = 100;
};

#endif
