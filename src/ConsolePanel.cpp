#include "ConsolePanel.h"

#include "AndroidTooling.h"
#include "ProcessOutputView.h"
#include "ProcessRunner.h"
#include "TaskPresets.h"

#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSettings>
#include <QVBoxLayout>

ConsolePanel::ConsolePanel(QWidget *parent)
    : QWidget(parent)
{
    m_runner = new ProcessRunner(this);
    m_output = new ProcessOutputView(this);
    m_input = new QLineEdit(this);
    m_input->setClearButtonEnabled(true);
    m_input->installEventFilter(this);

    m_limitsCheck = new QCheckBox(tr("&Limits"), this);
    m_limitsCheck->setToolTip(tr(
        "Run console commands with the Android panel's CPU/RAM/priority "
        "limits (systemd scope when available)."));

    connect(m_input, &QLineEdit::returnPressed,
            this, &ConsolePanel::runCommand);
    connect(m_output, &ProcessOutputView::stopRequested,
            this, &ConsolePanel::stopCommand);
    connect(m_runner, &ProcessRunner::outputLine,
            m_output, &ProcessOutputView::appendLine);
    connect(m_runner, &ProcessRunner::finished,
            this, &ConsolePanel::onFinished);

    auto *inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->setSpacing(4);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_limitsCheck);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addWidget(m_output, 1);
    layout->addLayout(inputRow);
    setLayout(layout);

    QSettings settings;
    m_applyLimits =
        settings.value(QStringLiteral("console/applyLimits"), true).toBool();
    m_limitsCheck->setChecked(m_applyLimits);
    connect(m_limitsCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_applyLimits = on;
        QSettings settings;
        settings.setValue(QStringLiteral("console/applyLimits"), on);
    });

    setProjectRoot(QString());
}

void ConsolePanel::setProjectRoot(const QString &root)
{
    m_root = root;
    const bool hasRoot = !root.isEmpty();
    m_input->setEnabled(hasRoot);
    m_input->setPlaceholderText(hasRoot
        ? tr("Command (in %1)").arg(QDir::toNativeSeparators(root))
        : tr("Open a folder to use the console"));
}

void ConsolePanel::focusInput()
{
    if (m_input->isEnabled())
        m_input->setFocus();
}

void ConsolePanel::applyLimits(QString &program, QStringList &arguments) const
{
    if (!m_applyLimits)
        return;

    QSettings settings;
    const int cpus =
        settings.value(QStringLiteral("android/limitCpus"), 2).toInt();
    const int memoryGB =
        settings.value(QStringLiteral("android/limitMemGB"), 4).toInt();
    const bool lowPriority =
        settings.value(QStringLiteral("android/limitNice"), true).toBool();
    const AndroidTooling::LimitedCommand limited =
        AndroidTooling::wrapCommand(program, arguments, cpus, memoryGB,
                                    lowPriority,
                                    AndroidTooling::findSystemdRun());
    program = limited.program;
    arguments = limited.arguments;
}

void ConsolePanel::runCommand()
{
    if (m_runner->isRunning())
        return;

    const QString command = m_input->text().trimmed();
    if (command.isEmpty() || m_root.isEmpty())
        return;

    m_history.removeAll(command);
    m_history.append(command);
    while (m_history.size() > kMaxHistory)
        m_history.removeFirst();
    m_historyIndex = m_history.size();
    m_draft.clear();

    m_output->appendLine(QString());
    m_output->appendLine(QStringLiteral("$ ") + command);
    m_output->setBusy(true, tr("running: %1").arg(command));

#ifdef Q_OS_WIN
    QString program = QStringLiteral("cmd.exe");
    QStringList arguments = { QStringLiteral("/c"), command };
#else
    QString program = QStringLiteral("/bin/sh");
    QStringList arguments = { QStringLiteral("-c"), command };
#endif

    applyLimits(program, arguments);
    m_runner->start(program, arguments, m_root);

    m_input->clear();
}

void ConsolePanel::runTask(const TaskPreset &task)
{
    if (m_runner->isRunning()) {
        m_output->appendLine(tr("Console is busy - stop the running command first."));
        return;
    }
    if (task.command.isEmpty())
        return;

    QString shown = task.command;
    if (!task.args.isEmpty())
        shown += QLatin1Char(' ') + task.args.join(QLatin1Char(' '));

    m_output->appendLine(QString());
    m_output->appendLine(QStringLiteral("$ ") + task.name
        + QStringLiteral("  (") + shown + QLatin1Char(')'));
    m_output->setBusy(true, tr("task: %1").arg(task.name));

    QString program;
    QStringList arguments;
    if (task.useShell) {
#ifdef Q_OS_WIN
        program = QStringLiteral("cmd.exe");
        arguments = { QStringLiteral("/c"), task.command };
#else
        program = QStringLiteral("/bin/sh");
        arguments = { QStringLiteral("-c"), task.command };
#endif
    } else {
        program = task.command;
        arguments = task.args;
    }

    applyLimits(program, arguments);
    m_runner->start(program, arguments, task.workingDirectory);
}

void ConsolePanel::stopCommand()
{
    if (!m_runner->isRunning())
        return;
    m_output->setStatusText(tr("stopping…"));
    m_runner->stop();
}

void ConsolePanel::onFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    m_output->appendLine(tr("[%1, exit %2, %3 ms]")
        .arg(crashed ? tr("terminated") : tr("done"))
        .arg(exitCode)
        .arg(elapsedMs));
    m_output->setBusy(false);
    m_output->setStatusText(QString());
}

bool ConsolePanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (m_historyIndex > 0) {
                if (m_historyIndex == m_history.size())
                    m_draft = m_input->text();
                --m_historyIndex;
                m_input->setText(m_history.at(m_historyIndex));
                m_input->end(false);
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            if (m_historyIndex < m_history.size()) {
                ++m_historyIndex;
                m_input->setText(m_historyIndex == m_history.size()
                    ? m_draft : m_history.at(m_historyIndex));
                m_input->end(false);
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
