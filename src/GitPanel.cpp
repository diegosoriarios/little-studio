#include "GitPanel.h"

#include "GitTooling.h"
#include "ProcessRunner.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qsciscintilla.h>
#include <QVBoxLayout>

namespace {

constexpr int kMaxDiffBytes = 2 * 1024 * 1024;

// Dark/light-friendly diff colors (0 = dark, 1 = light).
struct DiffColors {
    QColor removed, added, header, position, command, fg, paper;
};

DiffColors diffColors(int themeMode)
{
    if (themeMode == 0) {
        return { QColor(0xf1, 0x4c, 0x4c), QColor(0x81, 0xc7, 0x8a),
                 QColor(0xc5, 0x86, 0xc0), QColor(0xd7, 0xba, 0x7d),
                 QColor(0xc5, 0x86, 0xc0), QColor(0xd4, 0xd4, 0xd4),
                 QColor(0x1e, 0x1e, 0x1e) };
    }
    return { QColor(0xa3, 0x15, 0x15), QColor(0x22, 0x86, 0x3a),
             QColor(0x6f, 0x42, 0xc1), QColor(0x99, 0x6f, 0x00),
             QColor(0x6f, 0x42, 0xc1), QColor(0x1f, 0x1f, 0x1f),
             QColor(0xff, 0xff, 0xff) };
}

} // namespace

GitPanel::GitPanel(QWidget *parent)
    : QWidget(parent)
{
    m_probe = new ProcessRunner(this);
    m_diff = new ProcessRunner(this);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Working tree"), 0);
    m_modeCombo->addItem(tr("Staged (HEAD)"), 1);
    m_modeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    m_fileCombo = new QComboBox(this);
    m_fileCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    m_refreshButton = new QPushButton(tr("&Refresh"), this);
    m_stopButton = new QPushButton(tr("St&op"), this);
    m_stopButton->setEnabled(false);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_diffView = new QsciScintilla(this);
    m_diffView->setReadOnly(true);
    m_diffView->setMarginType(0, QsciScintilla::NumberMargin);
    m_diffView->setMarginWidth(0, QStringLiteral("00000"));
    m_diffView->setWrapMode(QsciScintilla::WrapNone);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    m_diffView->setFont(font);
    m_diffLexer = new QsciLexerDiff(m_diffView);
    m_diffLexer->setFont(font);
    m_diffView->setLexer(m_diffLexer);
    setTheme(0);

    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Mode:"), this));
    row->addWidget(m_modeCombo);
    row->addWidget(new QLabel(tr("File:"), this));
    row->addWidget(m_fileCombo, 1);
    row->addWidget(m_refreshButton);
    row->addWidget(m_stopButton);
    row->addWidget(m_statusLabel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addLayout(row);
    layout->addWidget(m_diffView, 1);
    setLayout(layout);

    connect(m_refreshButton, &QPushButton::clicked,
            this, &GitPanel::refreshTargets);
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (m_diff->isRunning())
            m_diff->stop();
    });
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this,
            [this](int) {
        if (m_enabled && !m_probe->isRunning())
            refreshTargets();
    });
    connect(m_fileCombo, &QComboBox::currentIndexChanged, this,
            [this](int) {
        if (m_enabled && !m_probe->isRunning() && !m_diff->isRunning())
            runDiff();
    });

    connect(m_probe, &ProcessRunner::outputLine, this,
            [this](const QString &line) { m_probeLines.append(line); });
    connect(m_probe, &ProcessRunner::finished,
            this, &GitPanel::onProbeFinished);

    // Batched streaming (120 ms) keeps bursts smooth.
    m_flushTimer.setInterval(120);
    m_flushTimer.setSingleShot(true);
    connect(&m_flushTimer, &QTimer::timeout, this, &GitPanel::flushDiff);
    connect(m_diff, &ProcessRunner::outputLine, this,
            [this](const QString &line) {
        if (m_truncated)
            return;
        m_logBuffer.append(line);
        qint64 bytes = 0;
        for (const QString &buffered : m_logBuffer)
            bytes += buffered.size() + 1;
        if (bytes > kMaxDiffBytes) {
            m_truncated = true;
            m_diff->stop();
            return;
        }
        if (!m_flushTimer.isActive())
            m_flushTimer.start();
    });
    connect(m_diff, &ProcessRunner::finished,
            this, &GitPanel::onDiffFinished);

    updateActions();
}

void GitPanel::setProject(const QString &rootPath, bool enabled)
{
    const bool rootChanged = rootPath != m_root;
    if (rootChanged) {
        if (m_diff->isRunning())
            m_diff->stop();
        if (m_probe->isRunning())
            m_probe->stop();
    }

    m_root = rootPath;
    m_git = GitTooling::findGit(qEnvironmentVariable("PATH"));
    m_gitFound = !m_git.isEmpty();
    m_enabled = enabled && !rootPath.isEmpty() && m_gitFound;

    if (!m_enabled) {
        if (!m_git.isEmpty() && !m_outputNotAvailableMessageSent) {
            m_outputNotAvailableMessageSent = true;
            m_diffView->setText(tr("git executable not found in PATH."));
        }

        m_modeCombo->setEnabled(false);
        m_fileCombo->setEnabled(false);
        m_refreshButton->setEnabled(false);
        m_stopButton->setEnabled(false);
        m_diffView->clear();
        m_statusLabel->setText(QString());
        return;
    }

    refreshTargets();
}

void GitPanel::diffWorkingTree()
{
    if (m_modeCombo->currentData().toInt() != 0) {
        m_modeCombo->setCurrentIndex(0); // signal re-runs the probes
        return;
    }
    refreshTargets();
}

void GitPanel::diffStaged()
{
    if (m_modeCombo->currentData().toInt() != 1) {
        m_modeCombo->setCurrentIndex(1); // signal re-runs the probes
        return;
    }
    refreshTargets();
}

void GitPanel::refreshTargets()
{
    if (!m_enabled) {
        updateActions();
        return;
    }

    m_probeLines.clear();
    m_probeKind = ProbeKind::NameStatus;
    QStringList args = { QStringLiteral("diff"),
                         QStringLiteral("--name-status") };
    if (m_modeCombo->currentData().toInt() == 1)
        args << QStringLiteral("--cached");
    m_probe->start(m_git, args, m_root);
    updateActions();
}

void GitPanel::onProbeFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(crashed);
    Q_UNUSED(elapsedMs);

    m_changedFiles = GitTooling::parseNameStatus(
        m_probeLines.join(QLatin1Char('\n')));
    m_probeKind = ProbeKind::None;

    m_fileCombo->clear();
    m_fileCombo->addItem(tr("(all changed files)"), QString());
    for (const GitTooling::ChangedFile &file : m_changedFiles) {
        m_fileCombo->addItem(
            QStringLiteral("%1  %2").arg(file.status, file.path), file.path);
    }
    if (m_changedFiles.isEmpty())
        m_fileCombo->addItem(tr("(no changes)"), QString());
    m_statusLabel->setText(tr("%1 changed file(s)").arg(m_changedFiles.size()));

    runDiff();
}

void GitPanel::runDiff()
{
    m_truncated = false;
    m_logBuffer.clear();
    m_diffView->clear();

    if (m_changedFiles.isEmpty()) {
        m_diffView->setText(tr("No changes."));
        updateActions();
        return;
    }

    m_diffRunning = true;
    m_stopButton->setEnabled(true);
    m_statusLabel->setText(tr("running diff…"));
    emit busyChanged(true, QStringLiteral("git diff"));

    QStringList args = { QStringLiteral("diff") };
    if (m_modeCombo->currentData().toInt() == 1)
        args << QStringLiteral("--cached");
    const QString filter = currentFileFilter();
    if (!filter.isEmpty())
        args << QStringLiteral("--") << filter;

    m_diff->start(m_git, args, m_root);
    updateActions();
}

void GitPanel::onDiffFinished(int exitCode, bool crashed, qint64 elapsedMs)
{
    Q_UNUSED(crashed);
    m_flushTimer.stop();
    flushDiff();
    m_diffRunning = false;
    m_stopButton->setEnabled(false);
    m_statusLabel->setText(tr("done, exit %1, %2 ms")
        .arg(exitCode).arg(elapsedMs));
    emit busyChanged(false, QStringLiteral("git diff"));
    updateActions();
}

void GitPanel::flushDiff()
{
    if (m_logBuffer.isEmpty())
        return;
    const QStringList batch = m_logBuffer;
    m_logBuffer.clear();
    if (m_truncated) {
        m_diffView->append(QStringLiteral("… output truncated …"));
        return;
    }
    m_diffView->append(batch.join(QLatin1Char('\n')));
}

void GitPanel::setTheme(int themeMode)
{
    const DiffColors colors = diffColors(themeMode);
    m_diffView->setColor(colors.fg);
    m_diffView->setPaper(colors.paper);
    m_diffLexer->setDefaultColor(colors.fg);
    m_diffLexer->setDefaultPaper(colors.paper);
    m_diffLexer->setColor(colors.fg);
    m_diffLexer->setPaper(colors.paper);
    m_diffLexer->setColor(colors.removed,
                          QsciLexerDiff::LineRemoved);
    m_diffLexer->setColor(colors.added, QsciLexerDiff::LineAdded);
    m_diffLexer->setColor(colors.header, QsciLexerDiff::Header);
    m_diffLexer->setColor(colors.position, QsciLexerDiff::Position);
    m_diffLexer->setColor(colors.command, QsciLexerDiff::Command);
    m_diffLexer->setColor(colors.header, QsciLexerDiff::Comment);
}

void GitPanel::updateActions()
{
    const bool ready = m_enabled && m_gitFound;
    m_refreshButton->setEnabled(ready && !m_probe->isRunning());
    m_stopButton->setEnabled(m_diff->isRunning());
}

QString GitPanel::diffText() const
{
    return m_diffView ? m_diffView->text() : QString();
}

QString GitPanel::currentFileFilter() const
{
    return m_fileCombo ? m_fileCombo->currentData().toString() : QString();
}
