#include "ApplePanel.h"

#include "ProcessOutputView.h"
#include "ProcessRunner.h"

#include <QCryptographicHash>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QString shellQuote(const QString &value)
{
    QString quoted = value;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

} // namespace

ApplePanel::ApplePanel(QWidget *parent)
    : QWidget(parent)
{
    m_probe = new ProcessRunner(this);
    m_job = new ProcessRunner(this);
    m_boot = new ProcessRunner(this);
    m_logs = new ProcessRunner(this);

    m_simulatorCombo = new QComboBox(this);
    m_simulatorCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_schemeCombo = new QComboBox(this);
    m_schemeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    m_bootButton = new QPushButton(tr("&Boot"), this);
    m_shutdownButton = new QPushButton(tr("S&hutdown"), this);
    m_refreshButton = new QPushButton(tr("&Refresh"), this);
    m_buildButton = new QPushButton(tr("&Build Debug"), this);
    m_installButton = new QPushButton(tr("&Install"), this);
    m_runButton = new QPushButton(tr("&Run"), this);
    m_logsButton = new QPushButton(tr("Start &Logs"), this);

    m_output = new ProcessOutputView(this);
    m_logView = new ProcessOutputView(this);

    auto *row1 = new QHBoxLayout;
    row1->addWidget(new QLabel(tr("Simulator:"), this));
    row1->addWidget(m_simulatorCombo, 1);
    row1->addWidget(m_bootButton);
    row1->addWidget(m_shutdownButton);
    row1->addWidget(m_refreshButton);

    auto *row2 = new QHBoxLayout;
    row2->addWidget(new QLabel(tr("Scheme:"), this));
    row2->addWidget(m_schemeCombo, 1);
    row2->addWidget(m_buildButton);
    row2->addWidget(m_installButton);
    row2->addWidget(m_runButton);
    row2->addWidget(m_logsButton);

    auto *tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    tabs->addTab(m_output, tr("Output"));
    tabs->addTab(m_logView, tr("Logs"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addLayout(row1);
    layout->addLayout(row2);
    layout->addWidget(tabs, 1);
    setLayout(layout);

    connect(m_refreshButton, &QPushButton::clicked,
            this, &ApplePanel::refreshTargets);
    connect(m_bootButton, &QPushButton::clicked,
            this, &ApplePanel::bootSelectedSimulator);
    connect(m_shutdownButton, &QPushButton::clicked,
            this, &ApplePanel::shutdownSelectedSimulator);
    connect(m_buildButton, &QPushButton::clicked,
            this, &ApplePanel::buildDebug);
    connect(m_installButton, &QPushButton::clicked,
            this, &ApplePanel::installApp);
    connect(m_runButton, &QPushButton::clicked,
            this, &ApplePanel::runApp);
    connect(m_logsButton, &QPushButton::clicked,
            this, &ApplePanel::toggleLogs);
    connect(m_simulatorCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &) { updateActions(); });
    connect(m_schemeCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &) {
        m_scheme = currentScheme();
        updateActions();
    });

    connect(m_probe, &ProcessRunner::outputLine, this,
            [this](const QString &line) { m_probeLines.append(line); });
    connect(m_probe, &ProcessRunner::finished,
            this, &ApplePanel::onProbeFinished);

    connect(m_job, &ProcessRunner::outputLine,
            m_output, &ProcessOutputView::appendLine);
    connect(m_job, &ProcessRunner::finished,
            this, &ApplePanel::onJobFinished);

    connect(m_boot, &ProcessRunner::outputLine,
            m_output, &ProcessOutputView::appendLine);
    connect(m_boot, &ProcessRunner::finished,
            this, &ApplePanel::onBootFinished);

    connect(m_output, &ProcessOutputView::stopRequested, this, [this]() {
        if (m_job->isRunning())
            m_job->stop();
    });
    connect(m_logView, &ProcessOutputView::stopRequested,
            this, &ApplePanel::stopLogs);

    m_logFlushTimer.setInterval(120);
    m_logFlushTimer.setSingleShot(true);
    connect(&m_logFlushTimer, &QTimer::timeout, this, &ApplePanel::flushLogs);
    connect(m_logs, &ProcessRunner::outputLine, this,
            [this](const QString &line) {
        m_logBuffer.append(line);
        if (!m_logFlushTimer.isActive())
            m_logFlushTimer.start();
    });
    connect(m_logs, &ProcessRunner::finished,
            this, &ApplePanel::onLogsFinished);

    updateActions();
}

void ApplePanel::setProject(const QString &appleRoot, bool enabled)
{
    const bool rootChanged = appleRoot != m_appleRoot;
    if (rootChanged) {
        if (m_job->isRunning())
            m_job->stop();
        if (m_boot->isRunning())
            m_boot->stop();
        if (m_logs->isRunning())
            stopLogs();
    }

    m_appleRoot = appleRoot;
    m_enabled = enabled && !appleRoot.isEmpty();

    if (!m_enabled) {
        m_simulatorCombo->clear();
        m_schemeCombo->clear();
        m_simulators.clear();
        m_tools = AppleTooling::Tools();
        m_workspace.clear();
        m_scheme.clear();
        m_appPath.clear();
        m_bundleId.clear();
        updateActions();
        return;
    }

    detectTools();
    updateActions();
    refreshTargets();
}

void ApplePanel::detectTools()
{
    m_tools = AppleTooling::discoverToolsFromEnvironment();
    m_workspace = AppleTooling::findWorkspaceOrProject(m_appleRoot);
    m_scheme.clear();
    m_appPath.clear();
    m_bundleId.clear();

    if (!m_tools.isValid()) {
        m_output->appendLine(tr("Xcode tools not found — iOS support requires "
                                "macOS with Xcode installed."));
    } else if (m_workspace.isEmpty()) {
        m_output->appendLine(tr("No .xcworkspace or .xcodeproj found in %1.")
            .arg(QDir::toNativeSeparators(m_appleRoot)));
    }
}

void ApplePanel::refreshTargets()
{
    if (!m_enabled) {
        updateActions();
        return;
    }
    if (!m_tools.isValid() || m_workspace.isEmpty()) {
        updateActions();
        return;
    }

    m_simulatorCombo->clear();
    m_simulators.clear();
    startProbe(ProbeKind::Simulators, m_tools.xcrun,
               { QStringLiteral("simctl"), QStringLiteral("list"),
                 QStringLiteral("devices"), QStringLiteral("available"),
                 QStringLiteral("--json") });
}

void ApplePanel::startProbe(ProbeKind kind, const QString &program,
                            const QStringList &args)
{
    if (m_probe->isRunning() || program.isEmpty())
        return;
    m_probeKind = kind;
    m_probeLines.clear();
    m_probe->start(program, args, m_appleRoot);
}

void ApplePanel::onProbeFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(crashed);
    Q_UNUSED(elapsedMs);

    const ProbeKind finishedKind = m_probeKind;
    m_probeKind = ProbeKind::None;
    const QString output = m_probeLines.join(QLatin1Char('\n'));

    if (finishedKind == ProbeKind::Simulators) {
        m_simulators.clear();
        for (const AppleTooling::Simulator &simulator :
             AppleTooling::parseSimctlDevices(output)) {
            if (!simulator.isAvailable)
                continue;
            // iOS simulators only (skip watchOS/tvOS/visionOS entries).
            if (!simulator.runtimeIdentifier.contains(
                    QStringLiteral(".iOS-")))
                continue;
            m_simulators.append(simulator);
            m_simulatorCombo->addItem(
                QStringLiteral("%1 (%2) [%3]").arg(
                    simulator.name, simulator.runtime, simulator.state),
                simulator.udid);
        }
        if (m_simulators.isEmpty())
            m_simulatorCombo->addItem(tr("(no iOS simulators)"), QString());
        for (int i = 0; i < m_simulators.size(); ++i) {
            if (m_simulators.at(i).isBooted()) {
                m_simulatorCombo->setCurrentIndex(i);
                break;
            }
        }

        startProbe(ProbeKind::Schemes, m_tools.xcodebuild,
                   sourceSelector()
                   + QStringList{ QStringLiteral("-list"),
                                  QStringLiteral("-json") });
        return;
    }

    if (finishedKind == ProbeKind::Schemes) {
        m_schemeCombo->clear();
        const QStringList schemes =
            AppleTooling::parseXcodebuildSchemes(output);
        for (const QString &scheme : schemes)
            m_schemeCombo->addItem(scheme, scheme);
        if (schemes.isEmpty())
            m_schemeCombo->addItem(tr("(no schemes)"), QString());
        m_scheme = currentScheme();
        updateActions();
        return;
    }

    if (finishedKind == ProbeKind::BuildSettings) {
        const AppleTooling::BuildSettings settings =
            AppleTooling::parseBuildSettings(output);
        if (!settings.appPath.isEmpty()) {
            m_appPath = settings.appPath;
            m_bundleId = settings.bundleId;
            m_output->appendLine(tr("app: %1").arg(m_appPath));
            m_output->appendLine(tr("bundle id: %1").arg(m_bundleId));
        } else {
            m_output->appendLine(tr("Could not read build settings "
                                    "(build the app first)."));
        }
        updateActions();
    }
}

void ApplePanel::bootSelectedSimulator()
{
    const QString udid = selectedSimulator();
    if (udid.isEmpty() || !m_tools.isValid())
        return;

    m_output->appendLine(QString());
    m_output->appendLine(tr("$ simctl boot %1; open Simulator; wait for boot").arg(udid));
    m_output->setStatusText(tr("booting simulator…"));

    // The wait loop runs inside the child; no host-side polling.
    const QString command = QStringLiteral(
        "%1 boot %2 || true; open -a Simulator; %1 bootstatus %2 -b")
        .arg(shellQuote(m_tools.xcrun), shellQuote(udid));
    m_boot->start(QStringLiteral("/bin/sh"),
                  { QStringLiteral("-c"), command }, m_appleRoot);
    updateActions();
}

void ApplePanel::shutdownSelectedSimulator()
{
    const QString udid = selectedSimulator();
    if (udid.isEmpty() || !m_tools.isValid())
        return;
    m_output->appendLine(tr("$ simctl shutdown %1").arg(udid));
    m_boot->start(m_tools.xcrun,
                  { QStringLiteral("simctl"), QStringLiteral("shutdown"),
                    udid },
                  m_appleRoot);
    updateActions();
}

void ApplePanel::onBootFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    Q_UNUSED(elapsedMs);
    m_output->appendLine(tr("[boot %1, exit %2]")
        .arg(crashed ? tr("terminated") : tr("done"))
        .arg(exitCode));
    m_output->setStatusText(QString());
    refreshTargets();
    updateActions();
}

void ApplePanel::buildDebug()
{
    const QString scheme = currentScheme();
    if (scheme.isEmpty() || !m_tools.isValid() || m_workspace.isEmpty())
        return;

    QStringList args = sourceSelector();
    args << QStringLiteral("-scheme") << scheme
         << QStringLiteral("-configuration") << QStringLiteral("Debug")
         << QStringLiteral("-sdk") << QStringLiteral("iphonesimulator")
         << QStringLiteral("-destination") << destinationSpecifier()
         << QStringLiteral("-derivedDataPath") << derivedDataPath()
         << QStringLiteral("build");

    m_lastJobWasBuild = true;
    startJob(m_tools.xcodebuild, args, tr("build %1").arg(scheme));
}

void ApplePanel::installApp()
{
    const QString udid = selectedSimulator();
    if (udid.isEmpty() || m_appPath.isEmpty() || !m_tools.isValid())
        return;
    m_lastJobWasBuild = false;
    startJob(m_tools.xcrun,
             { QStringLiteral("simctl"), QStringLiteral("install"), udid,
               m_appPath },
             tr("install %1").arg(QFileInfo(m_appPath).fileName()));
}

void ApplePanel::runApp()
{
    const QString udid = selectedSimulator();
    if (udid.isEmpty() || m_bundleId.isEmpty() || !m_tools.isValid())
        return;
    m_lastJobWasBuild = false;
    startJob(m_tools.xcrun,
             { QStringLiteral("simctl"), QStringLiteral("launch"), udid,
               m_bundleId },
             tr("launch %1").arg(m_bundleId));
}

void ApplePanel::toggleLogs()
{
    if (m_logs->isRunning()) {
        stopLogs();
        return;
    }

    const QString udid = selectedSimulator();
    const QString scheme = currentScheme();
    if (udid.isEmpty() || scheme.isEmpty() || !m_tools.isValid())
        return;

    m_logBuffer.clear();
    m_logView->clear();
    m_logView->setBusy(true, tr("logs: %1").arg(udid));
    m_logsButton->setText(tr("Stop Lo&gs"));

    const QString predicate = QStringLiteral(
        "processImagePath CONTAINS \"%1.app\"").arg(scheme);
    m_logs->start(m_tools.xcrun,
                  { QStringLiteral("simctl"), QStringLiteral("spawn"), udid,
                    QStringLiteral("log"), QStringLiteral("stream"),
                    QStringLiteral("--style"), QStringLiteral("compact"),
                    QStringLiteral("--level"), QStringLiteral("debug"),
                    QStringLiteral("--predicate"), predicate },
                  m_appleRoot);
    updateActions();
}

void ApplePanel::stopLogs()
{
    if (m_logs->isRunning()) {
        m_logs->stop();
        return;
    }
    m_logFlushTimer.stop();
    flushLogs();
    m_logView->setBusy(false);
    m_logsButton->setText(tr("Start &Logs"));
    updateActions();
}

void ApplePanel::onLogsFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    Q_UNUSED(elapsedMs);
    m_logFlushTimer.stop();
    flushLogs();
    m_logView->appendLine(tr("[logs %1, exit %2]")
        .arg(crashed ? tr("terminated") : tr("stopped"))
        .arg(exitCode));
    m_logView->setBusy(false);
    m_logsButton->setText(tr("Start &Logs"));
    updateActions();
}

void ApplePanel::flushLogs()
{
    if (m_logBuffer.isEmpty())
        return;
    const QStringList batch = m_logBuffer;
    m_logBuffer.clear();
    m_logView->appendLines(batch);
}

void ApplePanel::startJob(const QString &program, const QStringList &args,
                          const QString &label)
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
    m_job->start(program, args, m_appleRoot);
    updateActions();
}

void ApplePanel::onJobFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    m_jobRunning = false;
    emit busyChanged(false, m_currentJobLabel);
    m_output->appendLine(tr("[%1, exit %2, %3 ms]")
        .arg(crashed ? tr("terminated") : tr("done"))
        .arg(exitCode)
        .arg(elapsedMs));
    m_output->setBusy(false);

    if (m_lastJobWasBuild && exitCode == 0 && !m_probe->isRunning()) {
        // Read the built app path and bundle id for Install/Run.
        startProbe(ProbeKind::BuildSettings, m_tools.xcodebuild,
                   sourceSelector()
                   + QStringList{ QStringLiteral("-scheme"), currentScheme(),
                                  QStringLiteral("-configuration"),
                                  QStringLiteral("Debug"),
                                  QStringLiteral("-sdk"),
                                  QStringLiteral("iphonesimulator"),
                                  QStringLiteral("-destination"),
                                  destinationSpecifier(),
                                  QStringLiteral("-derivedDataPath"),
                                  derivedDataPath(),
                                  QStringLiteral("-showBuildSettings"),
                                  QStringLiteral("-json") });
    }
    m_lastJobWasBuild = false;
    updateActions();
}

void ApplePanel::updateActions()
{
    const QString udid = selectedSimulator();
    const QString scheme = currentScheme();
    const bool tools = m_enabled && m_tools.isValid();

    m_refreshButton->setEnabled(m_enabled && m_tools.isValid()
        && !m_workspace.isEmpty() && !m_probe->isRunning());
    m_bootButton->setEnabled(tools && !udid.isEmpty() && !m_boot->isRunning());
    m_shutdownButton->setEnabled(tools && !udid.isEmpty()
        && !m_boot->isRunning());
    m_buildButton->setEnabled(canBuild());
    m_installButton->setEnabled(canInstall());
    m_runButton->setEnabled(canRun());
    m_logsButton->setEnabled(tools && (!udid.isEmpty() || m_logs->isRunning()));

    m_buildButton->setToolTip(scheme.isEmpty()
        ? tr("select a scheme") : QString());
    if (m_appPath.isEmpty())
        m_installButton->setToolTip(tr("build the app first"));
    else if (udid.isEmpty())
        m_installButton->setToolTip(tr("select a simulator"));
    else
        m_installButton->setToolTip(QString());
    if (m_bundleId.isEmpty())
        m_runButton->setToolTip(tr("build the app first"));
    else if (udid.isEmpty())
        m_runButton->setToolTip(tr("select a simulator"));
    else
        m_runButton->setToolTip(QString());

    emit capabilitiesChanged();
}

bool ApplePanel::canBuild() const
{
    return m_enabled && m_tools.isValid() && !m_workspace.isEmpty()
        && !currentScheme().isEmpty() && !m_jobRunning;
}

bool ApplePanel::canInstall() const
{
    return canBuild() && !m_appPath.isEmpty()
        && !selectedSimulator().isEmpty();
}

bool ApplePanel::canRun() const
{
    return canInstall() && !m_bundleId.isEmpty();
}

QString ApplePanel::selectedSimulator() const
{
    if (!m_simulatorCombo)
        return QString();
    return m_simulatorCombo->currentData().toString();
}

QString ApplePanel::currentScheme() const
{
    if (!m_schemeCombo)
        return QString();
    return m_schemeCombo->currentData().toString();
}

QString ApplePanel::sourceFile() const
{
    return m_workspace;
}

bool ApplePanel::sourceIsWorkspace() const
{
    return m_workspace.endsWith(QStringLiteral(".xcworkspace"),
                                Qt::CaseInsensitive);
}

QStringList ApplePanel::sourceSelector() const
{
    if (m_workspace.isEmpty())
        return QStringList();
    return { sourceIsWorkspace() ? QStringLiteral("-workspace")
                                 : QStringLiteral("-project"),
             m_workspace };
}

QString ApplePanel::derivedDataPath() const
{
    QString cacheRoot =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty())
        cacheRoot = QDir::tempPath() + QStringLiteral("/lite-studio");
    const QByteArray hash = QCryptographicHash::hash(
        m_appleRoot.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return cacheRoot + QStringLiteral("/DerivedData-")
        + QString::fromLatin1(hash);
}

QString ApplePanel::destinationSpecifier() const
{
    const QString udid = selectedSimulator();
    if (udid.isEmpty())
        return QStringLiteral("generic/platform=iOS Simulator");
    return QStringLiteral("platform=iOS Simulator,id=") + udid;
}
