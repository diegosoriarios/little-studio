#include "QuickOpen.h"

#include "FileCollector.h"
#include "FuzzyMatch.h"

#include <algorithm>

#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStyle>
#include <QVBoxLayout>

namespace {
constexpr int kMaxResults = 50;
constexpr int kMaxFiles = 50000;
}

QuickOpen::QuickOpen(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Quick Open"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(560, 360);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Go to file..."));
    m_input->installEventFilter(this);

    m_list = new QListWidget(this);
    m_list->setUniformItemSizes(true);

    m_status = new QLabel(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(m_input);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_status);

    m_collector = new FileCollector(this);
    connect(m_collector, &FileCollector::batch, this,
            &QuickOpen::refilter);
    connect(m_collector, &FileCollector::finished, this,
            &QuickOpen::refilter);

    connect(m_input, &QLineEdit::textChanged, this,
            [this](const QString &) { refilter(); });
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) {
        emit fileActivated(item->data(Qt::UserRole).toString());
        hide();
    });
}

void QuickOpen::openForRoot(const QString &rootPath)
{
    m_rootPath = rootPath;
    m_files.clear();
    m_input->clear();
    m_list->clear();
    m_collector->start(m_rootPath, kMaxFiles);

    // Center over the main window.
    if (parentWidget()) {
        const QRect geo = parentWidget()->geometry();
        move(geo.center().x() - width() / 2, geo.top() + geo.height() / 6);
    }
    show();
    raise();
    activateWindow();
    m_input->setFocus();
    refilter();
}

void QuickOpen::refilter()
{
    m_files = m_collector->files();
    showFileList();
    const int indexed = m_files.size();
    if (m_collector->isRunning())
        m_status->setText(tr("indexing… %1 files").arg(indexed));
    else
        m_status->setText(tr("%1 files").arg(indexed));
}

void QuickOpen::showFileList()
{
    const QString term = m_input->text().trimmed();
    const QString currentRoot = m_rootPath;

    struct Candidate {
        QString path;
        int score;
    };
    QVector<Candidate> candidates;
    candidates.reserve(m_files.size());

    for (const QString &file : m_files) {
        const QString rel = QDir(currentRoot).relativeFilePath(file);
        const int score = fuzzyScore(rel, term);
        if (score >= 0)
            candidates.append({ rel, score });
    }

    std::stable_sort(candidates.begin(), candidates.end(),
        [](const Candidate &a, const Candidate &b) {
            if (a.score != b.score)
                return a.score > b.score;
            if (a.path.size() != b.path.size())
                return a.path.size() < b.path.size();
            return a.path < b.path;
        });

    m_list->clear();
    const int count = qMin(kMaxResults, candidates.size());
    for (int i = 0; i < count; ++i) {
        auto *item = new QListWidgetItem(m_list);
        item->setText(candidates[i].path);
        item->setData(Qt::UserRole, currentRoot + QLatin1Char('/')
            + candidates[i].path);
        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

bool QuickOpen::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Down:
            if (m_list->count() > 0)
                m_list->setCurrentRow(
                    qMin(m_list->currentRow() + 1, m_list->count() - 1));
            return true;
        case Qt::Key_Up:
            m_list->setCurrentRow(qMax(m_list->currentRow() - 1, 0));
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_list->currentItem()) {
                emit fileActivated(
                    m_list->currentItem()->data(Qt::UserRole).toString());
                hide();
            }
            return true;
        default:
            break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void QuickOpen::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QDialog::keyPressEvent(event);
}
