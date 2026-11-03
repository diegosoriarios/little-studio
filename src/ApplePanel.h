#ifndef LITE_STUDIO_APPLE_PANEL_H
#define LITE_STUDIO_APPLE_PANEL_H

#include <QList>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "AppleTooling.h"

class ProcessOutputView;
class ProcessRunner;
class QComboBox;
class QPushButton;

// iOS simulator panel. Commands require macOS + Xcode; on other platforms the
// panel reports that the toolchain is unavailable.
class ApplePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ApplePanel(QWidget *parent = nullptr);

    void setProject(const QString &appleRoot, bool enabled);
    bool isAvailable() const { return m_tools.isValid(); }
    QString appleRoot() const { return m_appleRoot; }

    bool canBuild() const;
    bool canInstall() const;
    bool canRun() const;

    void refreshTargets();
    void bootSelectedSimulator();
    void shutdownSelectedSimulator();
    void buildDebug();
    void installApp();
    void runApp();
    void toggleLogs();

signals:
    void capabilitiesChanged();
    void busyChanged(bool busy, const QString &description);

private:
    enum class ProbeKind {
        None,
        Simulators,
        Schemes,
        BuildSettings,
    };

    void detectTools();
    void startProbe(ProbeKind kind, const QString &program,
                    const QStringList &args);
    void onProbeFinished(int exitCode, bool crashed, qint64 elapsedMs);

    void startJob(const QString &program, const QStringList &args,
                  const QString &label);
    void onJobFinished(int exitCode, bool crashed, qint64 elapsedMs);

    void onBootFinished(int exitCode, bool crashed, qint64 elapsedMs);
    void stopLogs();
    void onLogsFinished(int exitCode, bool crashed, qint64 elapsedMs);
    void flushLogs();

    void updateActions();
    QString selectedSimulator() const;
    QString currentScheme() const;
    QString sourceFile() const;
    bool sourceIsWorkspace() const;
    QStringList sourceSelector() const;
    QString derivedDataPath() const;
    QString destinationSpecifier() const;

    ProcessRunner *m_probe = nullptr;
    ProcessRunner *m_job = nullptr;
    ProcessRunner *m_boot = nullptr;
    ProcessRunner *m_logs = nullptr;
    ProcessOutputView *m_output = nullptr;
    ProcessOutputView *m_logView = nullptr;

    QComboBox *m_simulatorCombo = nullptr;
    QComboBox *m_schemeCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_bootButton = nullptr;
    QPushButton *m_shutdownButton = nullptr;
    QPushButton *m_buildButton = nullptr;
    QPushButton *m_installButton = nullptr;
    QPushButton *m_runButton = nullptr;
    QPushButton *m_logsButton = nullptr;

    QTimer m_logFlushTimer;
    QStringList m_logBuffer;

    ProbeKind m_probeKind = ProbeKind::None;
    QStringList m_probeLines;

    QString m_appleRoot;
    QString m_workspace;
    QString m_scheme;
    QString m_appPath;
    QString m_bundleId;
    QString m_currentJobLabel;
    QList<AppleTooling::Simulator> m_simulators;
    AppleTooling::Tools m_tools;
    bool m_enabled = false;
    bool m_jobRunning = false;
    bool m_lastJobWasBuild = false;
};

#endif
