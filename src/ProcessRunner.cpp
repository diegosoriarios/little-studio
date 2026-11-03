#include "ProcessRunner.h"

#include "ProcessUtil.h"

#include <QTimer>

#ifdef Q_OS_UNIX
#include <csignal>
#include <unistd.h>
#endif

ProcessRunner::ProcessRunner(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_killTimer.setSingleShot(true);

    connect(&m_killTimer, &QTimer::timeout, this, &ProcessRunner::onKillTimeout);
    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &ProcessRunner::onReadyRead);
    connect(&m_process, &QProcess::finished,
            this, &ProcessRunner::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred,
            this, &ProcessRunner::onProcessError);

#ifdef Q_OS_UNIX
    // Put the child in its own process group so a cancel can signal the
    // whole tree (Gradle/xcodebuild spawn helper processes).
    m_process.setChildProcessModifier([]() { ::setsid(); });
#endif
}

ProcessRunner::~ProcessRunner()
{
    if (isRunning()) {
        terminateProcess(true);
        m_process.waitForFinished(500);
    }
}

void ProcessRunner::start(const QString &program, const QStringList &arguments,
                          const QString &workingDirectory,
                          const QProcessEnvironment &environment)
{
    if (isRunning())
        return;

    m_pending.clear();
    m_finishedEmitted = false;
    m_process.setWorkingDirectory(workingDirectory);
    m_process.setProgram(program);
    m_process.setArguments(arguments);

    // Empty environment means "inherit from the app" (QProcess default).
    // Non-empty must be a complete environment built by the caller.
    if (!environment.isEmpty())
        m_process.setProcessEnvironment(environment);
    else
        m_process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    m_clock.start();
    m_process.start();
    emit started();
}

void ProcessRunner::stop()
{
    if (!isRunning())
        return;
    terminateProcess(false);
    m_killTimer.start(kKillGraceMs);
}

bool ProcessRunner::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

qint64 ProcessRunner::processId() const
{
    return m_process.processId();
}

void ProcessRunner::onReadyRead()
{
    m_pending.append(m_process.readAllStandardOutput());

    int start = 0;
    const int size = m_pending.size();
    for (int i = 0; i < size; ++i) {
        const char c = m_pending.at(i);
        if (c != '\n' && c != '\r')
            continue;
        // A trailing CR might be the first half of a CRLF split across reads.
        if (c == '\r' && i == size - 1)
            break;

        const QByteArray raw = m_pending.mid(start, i - start);
        if (c == '\r' && i + 1 < size && m_pending.at(i + 1) == '\n')
            ++i; // skip the LF of CRLF
        start = i + 1;

        emit outputLine(ProcessUtil::stripAnsi(QString::fromUtf8(raw)));
    }

    if (start > 0)
        m_pending.remove(0, start);

    // Pathological output without newlines: flush instead of growing forever.
    if (m_pending.size() > kMaxPendingBytes) {
        const QString line =
            ProcessUtil::stripAnsi(QString::fromUtf8(m_pending));
        m_pending.clear();
        emit outputLine(line);
    }
}

void ProcessRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_killTimer.stop();

    if (!m_pending.isEmpty()) {
        const QString line =
            ProcessUtil::stripAnsi(QString::fromUtf8(m_pending));
        m_pending.clear();
        emit outputLine(line);
    }

    if (m_finishedEmitted)
        return;
    m_finishedEmitted = true;
    emit finished(exitCode, status == QProcess::CrashExit, m_clock.elapsed());
}

void ProcessRunner::onProcessError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart || m_finishedEmitted)
        return;
    m_finishedEmitted = true;
    emit outputLine(tr("Failed to start: %1").arg(m_process.errorString()));
    emit finished(-1, true, m_clock.elapsed());
}

void ProcessRunner::onKillTimeout()
{
    terminateProcess(true);
}

void ProcessRunner::terminateProcess(bool force)
{
#ifdef Q_OS_UNIX
    const qint64 pid = m_process.processId();
    if (pid > 0) {
        // Negative pid = the child's process group (see setsid above).
        if (::kill(-static_cast<pid_t>(pid), force ? SIGKILL : SIGTERM) == 0)
            return;
    }
#endif
    if (force)
        m_process.kill();
    else
        m_process.terminate();
}
