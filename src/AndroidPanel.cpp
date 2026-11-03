#include "AndroidPanel.h"

#include "ProcessOutputView.h"
#include "ProcessRunner.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QString readSmallFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.read(64 * 1024));
}

} // namespace

AndroidPanel::AndroidPanel(QWidget *parent)
    : QWidget(parent)
{
    m_probe = new ProcessRunner(this);
    m_job = new ProcessRunner(this);
    m_emulator = new ProcessRunner(this);
    m_bootWait = new ProcessRunner(this);
    m_logcat = new ProcessRunner(this);

    m_avdCombo = new QComboBox(this);
    m_avdCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    m_startButton = new QPushButton(tr("&Start"), this);
    m_stopButton = new QPushButton(tr("St&op"), this);
    m_refreshButton = new QPushButton(tr("&Refresh"), this);
    m_buildButton = new QPushButton(tr("&Build Debug"), this);
    m_testButton = new QPushButton(tr("&Test Debug"), this);
    m_installButton = new QPushButton(tr("&Install Debug"), this);
    m_runButton = new QPushButton(tr("&Run"), this);
    m_logcatButton = new QPushButton(tr("Start &Logcat"), this);
    m_output = new ProcessOutputView(this);

    m_cpuLimitCombo = new QComboBox(this);
    m_cpuLimitCombo->addItem(tr("All CPUs"), 0);
    m_cpuLimitCombo->addItem(tr("1 CPU"), 1);
    m_cpuLimitCombo->addItem(tr("2 CPUs"), 2);
    m_cpuLimitCombo->addItem(tr("4 CPUs"), 4);
    m_cpuLimitCombo->setToolTip(tr(
        "Caps the whole build process tree via a systemd scope "
        "(and Gradle worker parallelism)."));
    m_memoryLimitCombo = new QComboBox(this);
    m_memoryLimitCombo->addItem(tr("Unlimited RAM"), 0);
    for (int gb : { 2, 4, 6, 8 })
        m_memoryLimitCombo->addItem(tr("%1 GB").arg(gb), gb);
    m_memoryLimitCombo->setToolTip(tr(
        "Gradle daemon heap limit (auto-restarts the daemon when changed)."));
    m_lowPriorityCheck = new QCheckBox(tr("Low &priority"), this);
    m_lowPriorityCheck->setToolTip(tr(
        "Run builds at low CPU priority (nice 19) so the desktop stays responsive."));
    m_logView = new ProcessOutputView(this);

    auto *row1 = new QHBoxLayout;
    row1->addWidget(new QLabel(tr("Emulator:"), this));
    row1->addWidget(m_avdCombo, 1);
    row1->addWidget(m_startButton);
    row1->addWidget(m_stopButton);
    row1->addWidget(m_refreshButton);

    auto *row2 = new QHBoxLayout;
    row2->addWidget(new QLabel(tr("Device:"), this));
    row2->addWidget(m_deviceCombo, 1);
    row2->addWidget(m_buildButton);
    row2->addWidget(m_testButton);
    row2->addWidget(m_installButton);
    row2->addWidget(m_runButton);
    row2->addWidget(m_logcatButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addLayout(row1);
    layout->addLayout(row2);
    auto *tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    tabs->addTab(m_output, tr("Output"));
    tabs->addTab(m_logView, tr("Logcat"));
    layout->addWidget(tabs, 1);
    setLayout(layout);

    connect(m_startButton, &QPushButton::clicked,
            this, &AndroidPanel::startSelectedEmulator);
    connect(m_stopButton, &QPushButton::clicked,
            this, &AndroidPanel::stopEmulator);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &AndroidPanel::refreshTargets);
    connect(m_buildButton, &QPushButton::clicked,
            this, &AndroidPanel::buildDebug);
    connect(m_testButton, &QPushButton::clicked,
            this, &AndroidPanel::testDebug);
    connect(m_installButton, &QPushButton::clicked,
            this, &AndroidPanel::installDebug);
    connect(m_runButton, &QPushButton::clicked,
            this, &AndroidPanel::runApp);
    connect(m_logcatButton, &QPushButton::clicked,
            this, &AndroidPanel::toggleLogcat);
    connect(m_avdCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &) { updateActions(); });
    connect(m_deviceCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &) {
        const QString serial = selectedSerial();
        if (m_logcat->isRunning() && !serial.isEmpty()
            && serial != m_logcatSerial) {
            stopLogcat();
        }
        updateActions();
    });

    connect(m_probe, &ProcessRunner::outputLine, this,
            [this](const QString &line) { m_probeLines.append(line); });
    connect(m_probe, &ProcessRunner::finished,
            this, &AndroidPanel::onProbeFinished);

    connect(m_job, &ProcessRunner::outputLine,
            m_output, &ProcessOutputView::appendLine);
    connect(m_job, &ProcessRunner::finished,
            this, &AndroidPanel::onJobFinished);

    connect(m_emulator, &ProcessRunner::outputLine,
            m_output, &ProcessOutputView::appendLine);
    connect(m_emulator, &ProcessRunner::finished, this,
            [this](int exitCode, bool, qint64) {
        m_output->appendLine(tr("[emulator stopped, exit %1]").arg(exitCode));
        updateActions();
        refreshTargets();
    });

    connect(m_bootWait, &ProcessRunner::finished, this,
            [this](int, bool, qint64) { refreshTargets(); });

    m_logFlushTimer.setInterval(120);
    m_logFlushTimer.setSingleShot(true);
    connect(&m_logFlushTimer, &QTimer::timeout,
            this, &AndroidPanel::flushLogcat);
    connect(m_logcat, &ProcessRunner::outputLine, this,
            [this](const QString &line) {
        m_logBuffer.append(line);
        if (!m_logFlushTimer.isActive())
            m_logFlushTimer.start();
    });
    connect(m_logcat, &ProcessRunner::finished,
            this, &AndroidPanel::onLogcatFinished);
    connect(m_logView, &ProcessOutputView::stopRequested,
            this, &AndroidPanel::stopLogcat);

    connect(m_output, &ProcessOutputView::stopRequested, this, [this]() {
        if (m_job->isRunning())
            m_job->stop();
        else if (m_emulator->isRunning())
            m_emulator->stop();
    });

    loadLimitSettings();
    connect(m_cpuLimitCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { saveLimitSettings(); });
    connect(m_memoryLimitCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { saveLimitSettings(); });
    connect(m_lowPriorityCheck, &QCheckBox::toggled, this,
            [this](bool) { saveLimitSettings(); });

    updateActions();
}

void AndroidPanel::setProject(const QString &androidRoot, bool enabled)
{
    const bool rootChanged = androidRoot != m_androidRoot;
    if (rootChanged && m_job->isRunning())
        m_job->stop();
    if (rootChanged && m_logcat->isRunning())
        stopLogcat();

    m_androidRoot = androidRoot;
    m_enabled = enabled && !androidRoot.isEmpty();

    if (!m_enabled) {
        if (m_logcat->isRunning())
            stopLogcat();
        m_avdCombo->clear();
        m_deviceCombo->clear();
        m_devices.clear();
        m_sdk = AndroidTooling::Sdk();
        m_gradlew.clear();
        m_applicationId.clear();
        updateActions();
        return;
    }

    detectLocalTools();
    updateActions();
    refreshTargets();
}

void AndroidPanel::detectLocalTools()
{
    m_sdk = AndroidTooling::discoverSdkFromEnvironment();
    m_gradlew = AndroidTooling::findGradleWrapper(m_androidRoot);

    m_applicationId.clear();
    const QStringList candidates = {
        QStringLiteral("build.gradle"),
        QStringLiteral("build.gradle.kts"),
        QStringLiteral("app/build.gradle"),
        QStringLiteral("app/build.gradle.kts"),
    };
    for (const QString &relative : candidates) {
        const QString contents = readSmallFile(
            m_androidRoot + QLatin1Char('/') + relative);
        if (contents.isEmpty())
            continue;
        const QString id = AndroidTooling::extractApplicationId(contents, relative);
        if (!id.isEmpty()) {
            m_applicationId = id;
            break;
        }
    }

    if (!m_sdk.isValid()) {
        m_output->appendLine(tr("Android SDK not found (set ANDROID_HOME)."));
    } else if (m_gradlew.isEmpty()) {
        m_output->appendLine(tr("No gradlew found in the project root."));
    }
}

void AndroidPanel::refreshTargets()
{
    if (!m_enabled) {
        updateActions();
        return;
    }

    m_avdCombo->clear();
    m_deviceCombo->clear();
    m_devices.clear();

    if (m_sdk.hasEmulator()) {
        startProbe(ProbeKind::AvdList, m_sdk.emulator,
                   { QStringLiteral("-list-avds") });
    } else if (m_sdk.hasAdb()) {
        startProbe(ProbeKind::DeviceList, m_sdk.adb,
                   { QStringLiteral("devices"), QStringLiteral("-l") });
    } else {
        updateActions();
    }
}

void AndroidPanel::startProbe(ProbeKind kind, const QString &program,
                              const QStringList &args)
{
    if (m_probe->isRunning() || program.isEmpty())
        return;
    m_probeKind = kind;
    m_probeLines.clear();
    m_probe->start(program, args, m_androidRoot);
}

void AndroidPanel::onProbeFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(crashed);
    Q_UNUSED(elapsedMs);

    const ProbeKind finishedKind = m_probeKind;
    m_probeKind = ProbeKind::None;

    if (finishedKind == ProbeKind::AvdList) {
        const QStringList avds = AndroidTooling::parseAvdList(
            m_probeLines.join(QLatin1Char('\n')));
        for (const QString &avd : avds)
            m_avdCombo->addItem(avd, avd);
        if (avds.isEmpty())
            m_avdCombo->addItem(tr("(no AVDs)"), QString());

        if (m_sdk.hasAdb()) {
            startProbe(ProbeKind::DeviceList, m_sdk.adb,
                       { QStringLiteral("devices"), QStringLiteral("-l") });
        } else {
            updateActions();
        }
        return;
    }

    if (finishedKind == ProbeKind::DeviceList) {
        m_devices = AndroidTooling::parseAdbDevices(
            m_probeLines.join(QLatin1Char('\n')));
        for (const AndroidTooling::Device &device : m_devices) {
            QString label = device.serial;
            if (!device.model.isEmpty())
                label += QStringLiteral(" — ") + device.model;
            label += QStringLiteral(" [") + device.state + QLatin1Char(']');
            m_deviceCombo->addItem(label, device.serial);
        }
        if (m_devices.isEmpty())
            m_deviceCombo->addItem(tr("(no devices)"), QString());
        for (int i = 0; i < m_devices.size(); ++i) {
            if (m_devices.at(i).isReady()) {
                m_deviceCombo->setCurrentIndex(i);
                break;
            }
        }
        updateActions();
    }
}

void AndroidPanel::startSelectedEmulator()
{
    if (!m_enabled || !m_sdk.hasEmulator() || m_emulator->isRunning())
        return;
    const QString avd = m_avdCombo->currentData().toString();
    if (avd.isEmpty())
        return;

    m_output->appendLine(QString());
    m_output->appendLine(QStringLiteral("$ emulator -avd ") + avd);
    m_output->setStatusText(tr("starting emulator %1…").arg(avd));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("ANDROID_HOME"), m_sdk.sdkPath);
    env.insert(QStringLiteral("ANDROID_SDK_ROOT"), m_sdk.sdkPath);
    m_emulator->start(m_sdk.emulator, { QStringLiteral("-avd"), avd },
                      m_androidRoot, env);

    if (m_sdk.hasAdb() && !m_bootWait->isRunning())
        m_bootWait->start(m_sdk.adb, { QStringLiteral("wait-for-device") },
                          m_androidRoot);
    updateActions();
}

void AndroidPanel::stopEmulator()
{
    if (!m_emulator->isRunning())
        return;
    m_output->setStatusText(tr("stopping emulator…"));
    m_emulator->stop();
}

void AndroidPanel::buildDebug()
{
    startGradleJob({ QStringLiteral("assembleDebug"),
                     QStringLiteral("--console=plain") },
                   tr("build (assembleDebug)"));
}

void AndroidPanel::testDebug()
{
    startGradleJob({ QStringLiteral("testDebugUnitTest"),
                     QStringLiteral("--console=plain") },
                   tr("unit tests (testDebugUnitTest)"));
}

void AndroidPanel::startGradleJob(const QStringList &gradleArgs,
                                  const QString &label,
                                  const QProcessEnvironment &environment)
{
    if (m_gradlew.isEmpty() || m_jobRunning)
        return;

    QStringList args =
        AndroidTooling::gradleLimitArgs(m_limitMemoryGB, m_limitCpus);
    args.append(gradleArgs);

    const AndroidTooling::LimitedCommand command = AndroidTooling::wrapCommand(
        m_gradlew, args, m_limitCpus, m_limitMemoryGB, m_limitNice,
        m_systemdRun);
    startJob(command.program, command.arguments, label, environment);
}

void AndroidPanel::loadLimitSettings()
{
    QSettings settings;
    m_limitCpus = settings.value(QStringLiteral("android/limitCpus"), 2).toInt();
    m_limitMemoryGB =
        settings.value(QStringLiteral("android/limitMemGB"), 4).toInt();
    m_limitNice =
        settings.value(QStringLiteral("android/limitNice"), true).toBool();

    m_cpuLimitCombo->setCurrentIndex(
        qMax(0, m_cpuLimitCombo->findData(m_limitCpus)));
    m_memoryLimitCombo->setCurrentIndex(
        qMax(0, m_memoryLimitCombo->findData(m_limitMemoryGB)));
    m_lowPriorityCheck->setChecked(m_limitNice);

    m_systemdRun = AndroidTooling::findSystemdRun();
}

void AndroidPanel::saveLimitSettings()
{
    m_limitCpus = m_cpuLimitCombo->currentData().toInt();
    m_limitMemoryGB = m_memoryLimitCombo->currentData().toInt();
    m_limitNice = m_lowPriorityCheck->isChecked();

    QSettings settings;
    settings.setValue(QStringLiteral("android/limitCpus"), m_limitCpus);
    settings.setValue(QStringLiteral("android/limitMemGB"), m_limitMemoryGB);
    settings.setValue(QStringLiteral("android/limitNice"), m_limitNice);
}

void AndroidPanel::installDebug()
{
    if (m_gradlew.isEmpty())
        return;
    const QString serial = selectedSerial();
    if (serial.isEmpty())
        return;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("ANDROID_SERIAL"), serial);
    startGradleJob({ QStringLiteral("installDebug"),
                     QStringLiteral("--console=plain") },
                   tr("install (installDebug) → %1").arg(serial), env);
}

void AndroidPanel::runApp()
{
    const QString serial = selectedSerial();
    if (serial.isEmpty() || m_applicationId.isEmpty() || !m_sdk.hasAdb())
        return;

    startJob(m_sdk.adb,
             { QStringLiteral("-s"), serial,
               QStringLiteral("shell"), QStringLiteral("monkey"),
               QStringLiteral("-p"), m_applicationId,
               QStringLiteral("-c"),
               QStringLiteral("android.intent.category.LAUNCHER"),
               QStringLiteral("1") },
             tr("launch %1").arg(m_applicationId));
}

void AndroidPanel::stopGradleDaemons()
{
    if (m_gradlew.isEmpty())
        return;
    startJob(m_gradlew,
             { QStringLiteral("--stop"), QStringLiteral("--console=plain") },
             tr("stop Gradle daemons"));
}

void AndroidPanel::toggleLogcat()
{
    if (m_logcat->isRunning()) {
        stopLogcat();
        return;
    }

    const QString serial = selectedSerial();
    if (serial.isEmpty() || !m_sdk.hasAdb())
        return;

    m_logBuffer.clear();
    m_logView->clear();
    m_logView->setBusy(true, tr("logcat: %1").arg(serial));
    m_logcatSerial = serial;
    m_logcatButton->setText(tr("Stop Lo&gcat"));
    m_logcat->start(m_sdk.adb,
                    { QStringLiteral("-s"), serial,
                      QStringLiteral("logcat") },
                    m_androidRoot);
    updateActions();
}

void AndroidPanel::stopLogcat()
{
    if (m_logcat->isRunning()) {
        m_logcat->stop();
        return;
    }
    m_logFlushTimer.stop();
    flushLogcat();
    m_logcatSerial.clear();
    m_logView->setBusy(false);
    m_logcatButton->setText(tr("Start &Logcat"));
    updateActions();
}

void AndroidPanel::onLogcatFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    Q_UNUSED(elapsedMs);
    m_logFlushTimer.stop();
    flushLogcat();
    m_logcatSerial.clear();
    m_logView->appendLine(tr("[logcat %1, exit %2]")
        .arg(crashed ? tr("terminated") : tr("stopped"))
        .arg(exitCode));
    m_logView->setBusy(false);
    m_logcatButton->setText(tr("Start &Logcat"));
    updateActions();
}

void AndroidPanel::flushLogcat()
{
    if (m_logBuffer.isEmpty())
        return;
    const QStringList batch = m_logBuffer;
    m_logBuffer.clear();
    m_logView->appendLines(batch);
}

void AndroidPanel::startJob(const QString &program, const QStringList &args,
                            const QString &label,
                            const QProcessEnvironment &environment)
{
    if (m_jobRunning || program.isEmpty())
        return;

    m_jobRunning = true;
    m_currentJobLabel = label;
    emit busyChanged(true, label);
    m_output->appendLine(QString());
    m_output->appendLine(QStringLiteral("$ ") + QFileInfo(program).fileName()
        + QLatin1Char(' ') + args.join(QLatin1Char(' ')));
    m_output->setBusy(true, label);
    m_job->start(program, args, m_androidRoot, environment);
    updateActions();
}

void AndroidPanel::onJobFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    m_jobRunning = false;
    emit busyChanged(false, m_currentJobLabel);
    m_output->appendLine(tr("[%1, exit %2, %3 ms]")
        .arg(crashed ? tr("terminated") : tr("done"))
        .arg(exitCode)
        .arg(elapsedMs));
    m_output->setBusy(false);
    updateActions();
}

void AndroidPanel::updateActions()
{
    const QString avd = m_avdCombo->currentData().toString();
    const QString serial = selectedSerial();

    const bool tools = m_enabled && (m_sdk.hasAdb() || m_sdk.hasEmulator());
    m_refreshButton->setEnabled(tools && !m_probe->isRunning());
    m_startButton->setEnabled(m_enabled && m_sdk.hasEmulator()
        && !avd.isEmpty() && !m_emulator->isRunning());
    m_stopButton->setEnabled(m_emulator->isRunning());
    m_buildButton->setEnabled(m_enabled && !m_gradlew.isEmpty()
        && !m_jobRunning);
    m_testButton->setEnabled(m_enabled && !m_gradlew.isEmpty()
        && !m_jobRunning);
    m_installButton->setEnabled(m_enabled && m_sdk.hasAdb()
        && !m_gradlew.isEmpty() && !m_jobRunning && !serial.isEmpty());
    m_runButton->setEnabled(m_enabled && m_sdk.hasAdb() && !m_jobRunning
        && !serial.isEmpty() && !m_applicationId.isEmpty());
    m_logcatButton->setEnabled(m_enabled && m_sdk.hasAdb()
        && (!serial.isEmpty() || m_logcat->isRunning()));

    m_buildButton->setToolTip(m_gradlew.isEmpty()
        ? tr("gradlew not found in the project root") : QString());
    m_testButton->setToolTip(m_gradlew.isEmpty()
        ? tr("gradlew not found in the project root") : QString());
    m_installButton->setToolTip(serial.isEmpty()
        ? tr("no ready device") : QString());
    m_logcatButton->setToolTip(serial.isEmpty() && !m_logcat->isRunning()
        ? tr("no ready device") : QString());
    if (m_applicationId.isEmpty()) {
        m_runButton->setToolTip(
            tr("applicationId not found in build.gradle(.kts)"));
    } else {
        m_runButton->setToolTip(serial.isEmpty()
            ? tr("no ready device") : QString());
    }

    emit capabilitiesChanged();
}

QString AndroidPanel::selectedSerial() const
{
    const QString current = m_deviceCombo->currentData().toString();
    for (const AndroidTooling::Device &device : m_devices) {
        if (device.isReady() && device.serial == current)
            return device.serial;
    }
    for (const AndroidTooling::Device &device : m_devices) {
        if (device.isReady())
            return device.serial;
    }
    return QString();
}
