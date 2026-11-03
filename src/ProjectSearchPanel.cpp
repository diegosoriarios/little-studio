#include "ProjectSearchPanel.h"

#include "FileCollector.h"
#include "FileIO.h"
#include "CodeTools.h"
#include "FileTreeView.h"

#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ProjectSearchPanel::ProjectSearchPanel(QWidget *parent)
    : QWidget(parent)
{
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Search in project"));
    connect(m_input, &QLineEdit::returnPressed, this, &ProjectSearchPanel::startSearch);

    m_caseBox = new QCheckBox(tr("Match &case"), this);
    m_wordBox = new QCheckBox(tr("Whole &words"), this);

    m_button = new QPushButton(tr("&Search"), this);
    connect(m_button, &QPushButton::clicked, this, [this]() {
        if (m_searching)
            stopSearch();
        else
            startSearch();
    });

    m_replaceInput = new QLineEdit(this);
    m_replaceInput->setPlaceholderText(tr("Replace with"));
    m_replaceButton = new QPushButton(tr("Replace All in &Results"), this);
    m_replaceButton->setEnabled(false);
    m_replaceButton->setToolTip(tr(
        "Replaces the search term in every file listed in the results"));
    connect(m_replaceButton, &QPushButton::clicked,
            this, &ProjectSearchPanel::replaceInResults);
    connect(m_replaceInput, &QLineEdit::textChanged, this,
            [this](const QString &) { updateActions(); });
    connect(m_results, &QListWidget::itemSelectionChanged, this,
            [this]() { updateActions(); });

    m_status = new QLabel(this);

    m_results = new QListWidget(this);
    m_results->setUniformItemSizes(true);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_results, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) {
        emit fileActivated(item->data(Qt::UserRole).toString(),
                           item->data(Qt::UserRole + 1).toInt(),
                           item->data(Qt::UserRole + 2).toInt());
    });

    auto *top = new QHBoxLayout;
    top->addWidget(m_input, 1);
    top->addWidget(m_caseBox);
    top->addWidget(m_wordBox);
    top->addWidget(m_button);
    top->addWidget(m_status);

    auto *replaceRow = new QHBoxLayout;
    replaceRow->addWidget(m_replaceInput, 1);
    replaceRow->addWidget(m_replaceButton);

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(4, 2, 4, 2);
    main->setSpacing(2);
    main->addLayout(top);
    main->addLayout(replaceRow);
    main->addWidget(m_results, 1);
    setLayout(main);

    connect(&m_timer, &QTimer::timeout, this, &ProjectSearchPanel::searchStep);

    m_collector = new FileCollector(this);
    connect(m_collector, &FileCollector::batch, this,
            &ProjectSearchPanel::updateStatus);
    connect(m_collector, &FileCollector::finished,
            this, &ProjectSearchPanel::onIndexed);
}

void ProjectSearchPanel::openForRoot(const QString &rootPath)
{
    m_rootPath = rootPath;
    m_input->setFocus();
    m_input->selectAll();
}

void ProjectSearchPanel::stopSearch()
{
    m_searching = false;
    m_collector->stop();
    m_timer.stop();
    m_button->setText(tr("&Search"));
    updateStatus();
}

void ProjectSearchPanel::startSearch()
{
    stopSearch();
    m_results->clear();
    m_matchCount = 0;
    m_fileIndex = 0;
    m_files.clear();

    const QString term = m_input->text();
    if (m_rootPath.isEmpty() || term.isEmpty()) {
        updateStatus();
        return;
    }

    // Phase 1: chunked indexing; phase 2 (scanning) starts in onIndexed().
    m_searching = true;
    m_button->setText(tr("&Stop"));
    m_collector->start(m_rootPath, kMaxFiles);
    updateStatus();
}

void ProjectSearchPanel::onIndexed()
{
    m_files = m_collector->files();
    m_fileIndex = 0;
    updateStatus();
    if (m_searching && !m_files.isEmpty())
        m_timer.start(0);
    else
        stopSearch();
}

void ProjectSearchPanel::searchStep()
{
    if (!m_searching) {
        m_timer.stop();
        return;
    }

    QElapsedTimer clock;
    clock.start();
    while (m_fileIndex < m_files.size() && clock.elapsed() < 20
           && m_matchCount < kMaxMatches) {
        searchFile(m_files.at(m_fileIndex));
        ++m_fileIndex;
    }
    updateStatus();

    if (m_fileIndex >= m_files.size() || m_matchCount >= kMaxMatches) {
        const bool capped = m_matchCount >= kMaxMatches;
        stopSearch();
        if (capped)
            m_status->setText(tr("Stopped at %1 matches").arg(kMaxMatches));
    }
}

void ProjectSearchPanel::searchFile(const QString &path)
{
    const QFileInfo info(path);
    if (info.size() > kMaxSearchBytes)
        return;

    const FileIO::LoadResult result = FileIO::readFile(path);
    if (!result.ok || result.binary)
        return;

    const QString term = m_input->text();
    if (term.isEmpty())
        return;
    const Qt::CaseSensitivity cs = m_caseBox->isChecked()
        ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const bool wholeWord = m_wordBox->isChecked();
    const auto isWordChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_');
    };

    const QString relPath = QDir(m_rootPath).relativeFilePath(path);
    const int textSize = result.text.size();
    int pos = 0;
    while (m_matchCount < kMaxMatches) {
        pos = result.text.indexOf(term, pos, cs);
        if (pos < 0)
            break;

        bool match = true;
        if (wholeWord) {
            if (pos > 0 && isWordChar(result.text.at(pos - 1)))
                match = false;
            const int end = pos + term.size();
            if (end < textSize && isWordChar(result.text.at(end)))
                match = false;
        }
        if (match) {
            // Line number = newlines strictly before the match start.
            int line = 0;
            for (int i = 0; i < pos; ++i) {
                if (result.text.at(i) == QLatin1Char('\n'))
                    ++line;
            }
            const int lineStart = static_cast<int>(
                result.text.lastIndexOf(QLatin1Char('\n'), pos - 1)) + 1;
            int lineEnd = static_cast<int>(
                result.text.indexOf(QLatin1Char('\n'), pos));
            if (lineEnd < 0)
                lineEnd = textSize;
            QString snippet = result.text.mid(lineStart,
                qMin(lineEnd, lineStart + 200) - lineStart);
            if (lineEnd - lineStart > 200)
                snippet += QStringLiteral("…");
            addResult(relPath, line, pos - lineStart, snippet);
        }
        pos += term.size();
    }
}

void ProjectSearchPanel::addResult(const QString &path, int line, int index,
                                   const QString &lineText)
{
    auto *item = new QListWidgetItem(m_results);
    item->setText(QStringLiteral("%1:%2  %3").arg(path).arg(line + 1).arg(lineText));
    item->setData(Qt::UserRole, path);
    item->setData(Qt::UserRole + 1, line);
    item->setData(Qt::UserRole + 2, index);
    ++m_matchCount;
}

void ProjectSearchPanel::updateActions()
{
    m_replaceButton->setEnabled(!m_searching
        && m_results->count() > 0
        && !m_replaceInput->text().isEmpty());
}

void ProjectSearchPanel::replaceInResults()
{
    const QString term = m_input->text().trimmed();
    const QString replacement = m_replaceInput->text();
    if (term.isEmpty() || m_searching)
        return;

    // Distinct files in the current results.
    QStringList files;
    int resultCount = 0;
    for (int i = 0; i < m_results->count(); ++i) {
        const QString path = m_results->item(i)->data(Qt::UserRole).toString();
        if (!files.contains(path))
            files.append(path);
        ++resultCount;
    }
    if (files.isEmpty())
        return;

    QMessageBox confirm(QMessageBox::Question, tr("Replace in Project"),
        tr("Replace %1 occurrence(s) of \"%2\" in %3 file(s)?\n\n"
           "Files are saved in place with their original encoding.")
            .arg(resultCount).arg(term).arg(files.size()),
        QMessageBox::Yes | QMessageBox::No, this);
    confirm.setDefaultButton(QMessageBox::No);
    if (confirm.exec() != QMessageBox::Yes)
        return;

    const bool caseSensitive = m_caseBox->isChecked();
    const bool wholeWord = m_wordBox->isChecked();
    int replaced = 0;
    int fileCount = 0;
    int failed = 0;
    for (const QString &path : files) {
        const FileIO::LoadResult loaded = FileIO::readFile(path);
        if (!loaded.ok || loaded.binary)
            continue;
        const CodeTools::ReplaceResult result = CodeTools::replaceAll(
            loaded.text, term, replacement, caseSensitive, wholeWord);
        if (result.count == 0)
            continue;
        if (!FileIO::writeFile(path, result.text, loaded.encoding,
                               loaded.eol, nullptr)) {
            ++failed;
            continue;
        }
        replaced += result.count;
        ++fileCount;
    }

    m_status->setText(tr("replaced %1 in %2 file(s)%3")
        .arg(replaced).arg(fileCount)
        .arg(failed > 0 ? tr(", %1 failed").arg(failed) : QString()));
    startSearch();
}

void ProjectSearchPanel::updateStatus()
{
    if (m_collector->isRunning()) {
        m_status->setText(tr("indexing… %1 files").arg(
            m_collector->files().size()));
    } else if (m_searching) {
        m_status->setText(tr("%1 — %2/%3 files")
            .arg(m_matchCount).arg(m_fileIndex).arg(m_files.size()));
    } else {
        m_status->setText(tr("%1 matches").arg(m_matchCount));
    }
}
