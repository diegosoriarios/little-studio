#ifndef LITE_STUDIO_ANDROID_PANEL_H
#define LITE_STUDIO_ANDROID_PANEL_H

#include <QList>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "AndroidTooling.h"

class ProcessOutputView;
class ProcessRunner;
class QCheckBox;
class QComboBox;
class QPushButton;

// Android run panel: emulator/device selection, debug build, install and
// launch, plus Gradle daemon cleanup. All work is explicit and streamed
// through ProcessRunner; no polling while idle.
class AndroidPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AndroidPanel(QWidget *parent = nullptr);

    void setProject(const QString &androidRoot, bool enabled);
    bool isAvailable() const { return m_sdk.hasAdb(); }
    QString androidRoot() const { return m_androidRoot; }

    bool canBuild() const { return m_enabled && !m_gradlew.isEmpty() && !m_jobRunning; }
    bool canInstall() const { return canBuild() && m_sdk.hasAdb() && !selectedSerial().isEmpty(); }
    bool canRun() const { return canInstall() && !m_applicationId.isEmpty(); }

    void refreshTargets();
    void startSelectedEmulator();
    void stopEmulator();
    void buildDebug();
    void testDebug();
    void installDebug();
    void runApp();
    void stopGradleDaemons();
    void toggleLogcat();

signals:
    void capabilitiesChanged();
    void busyChanged(bool busy, const QString &description);

private:
    enum class ProbeKind {
        None,
        AvdList,
        DeviceList,
    };

    void detectLocalTools();
    void startProbe(ProbeKind kind, const QString &program,
                    const QStringList &args);
    void onProbeFinished(int exitCode, bool crashed, qint64 elapsedMs);
    void startJob(const QString &program, const QStringList &args,
                  const QString &label,
                  const QProcessEnvironment &environment = QProcessEnvironment());
    void startGradleJob(const QStringList &gradleArgs, const QString &label,
                        const QProcessEnvironment &environment = QProcessEnvironment());
    void loadLimitSettings();
    void saveLimitSettings();
    void onJobFinished(int exitCode, bool crashed, qint64 elapsedMs);
    void updateActions();
    QString selectedSerial() const;

    void stopLogcat();
    void onLogcatFinished(int exitCode, bool crashed, qint64 elapsedMs);
    void flushLogcat();

    ProcessRunner *m_probe = nullptr;
    ProcessRunner *m_job = nullptr;
    ProcessRunner *m_emulator = nullptr;
    ProcessRunner *m_bootWait = nullptr;
    ProcessRunner *m_logcat = nullptr;
    ProcessOutputView *m_output = nullptr;
    ProcessOutputView *m_logView = nullptr;

    QComboBox *m_avdCombo = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_buildButton = nullptr;
    QPushButton *m_testButton = nullptr;
    QPushButton *m_installButton = nullptr;
    QPushButton *m_runButton = nullptr;
    QPushButton *m_logcatButton = nullptr;
    QComboBox *m_cpuLimitCombo = nullptr;
    QComboBox *m_memoryLimitCombo = nullptr;
    QCheckBox *m_lowPriorityCheck = nullptr;

    QTimer m_logFlushTimer;
    QStringList m_logBuffer;
    QString m_logcatSerial;
    QString m_currentJobLabel;

    ProbeKind m_probeKind = ProbeKind::None;
    QStringList m_probeLines;

    QString m_androidRoot;
    QString m_gradlew;
    QString m_applicationId;
    QString m_systemdRun;
    int m_limitCpus = 2;
    int m_limitMemoryGB = 4;
    bool m_limitNice = true;
    AndroidTooling::Sdk m_sdk;
    QList<AndroidTooling::Device> m_devices;
    bool m_enabled = false;
    bool m_jobRunning = false;
};

#endif
