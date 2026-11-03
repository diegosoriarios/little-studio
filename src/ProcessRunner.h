#ifndef LITE_STUDIO_PROCESS_RUNNER_H
#define LITE_STUDIO_PROCESS_RUNNER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

// Runs one command at a time, streaming merged stdout/stderr as lines.
// No threads: everything is driven by the event loop. Cancelling sends
// SIGTERM to the child's process group, then SIGKILL after a grace period,
// so build-tool children (Gradle, xcodebuild) do not survive.
class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    explicit ProcessRunner(QObject *parent = nullptr);
    ~ProcessRunner() override;

    void start(const QString &program, const QStringList &arguments,
               const QString &workingDirectory = QString(),
               const QProcessEnvironment &environment = QProcessEnvironment());
    void stop();

    bool isRunning() const;
    qint64 processId() const;

signals:
    void started();
    void outputLine(const QString &line);
    void finished(int exitCode, bool crashed, qint64 elapsedMs);

private:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onKillTimeout();
    void terminateProcess(bool force);

    static constexpr int kKillGraceMs = 2000;
    static constexpr int kMaxPendingBytes = 256 * 1024;

    QProcess m_process;
    QTimer m_killTimer;
    QElapsedTimer m_clock;
    QByteArray m_pending;
    bool m_finishedEmitted = false;
};

#endif
