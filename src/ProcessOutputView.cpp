#include "ProcessOutputView.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

ProcessOutputView::ProcessOutputView(QWidget *parent)
    : QWidget(parent)
{
    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(kMaxLines);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setPlaceholderText(tr("Output"));
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    m_view->setFont(font);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter output"));
    m_filter->setClearButtonEnabled(true);

    m_status = new QLabel(this);
    m_status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_stopButton = new QPushButton(tr("&Stop"), this);
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked,
            this, &ProcessOutputView::stopRequested);

    auto *top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(m_filter, 1);
    top->addWidget(m_status, 1);
    top->addWidget(m_stopButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addLayout(top);
    layout->addWidget(m_view, 1);
    setLayout(layout);

    connect(m_filter, &QLineEdit::textChanged, this,
            [this](const QString &) { rebuildView(); });
}

void ProcessOutputView::appendLine(const QString &line)
{
    appendLines({ line });
}

void ProcessOutputView::appendLines(const QStringList &lines)
{
    if (lines.isEmpty())
        return;

    m_lines.append(lines);

    const QString filter = m_filter->text().trimmed();
    QStringList visible;
    if (filter.isEmpty()) {
        visible = lines;
    } else {
        for (const QString &line : lines) {
            if (line.contains(filter, Qt::CaseInsensitive))
                visible.append(line);
        }
    }
    if (visible.isEmpty())
        return;

    const bool atBottom = isAtBottom();
    m_view->appendPlainText(visible.join(QLatin1Char('\n')));
    if (atBottom) {
        QScrollBar *bar = m_view->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}

void ProcessOutputView::clear()
{
    m_lines.clear();
    m_view->clear();
}

void ProcessOutputView::setBusy(bool busy, const QString &status)
{
    m_stopButton->setEnabled(busy);
    if (!status.isNull())
        setStatusText(status);
    else if (!busy)
        setStatusText(QString());
}

void ProcessOutputView::setStatusText(const QString &text)
{
    m_status->setText(text);
}

void ProcessOutputView::rebuildView()
{
    m_view->clear();
    const QStringList visible = m_lines.filtered(m_filter->text().trimmed());
    if (!visible.isEmpty())
        m_view->appendPlainText(visible.join(QLatin1Char('\n')));
    QScrollBar *bar = m_view->verticalScrollBar();
    bar->setValue(bar->maximum());
}

bool ProcessOutputView::isAtBottom() const
{
    const QScrollBar *bar = m_view->verticalScrollBar();
    return bar->value() >= bar->maximum() - 2;
}
