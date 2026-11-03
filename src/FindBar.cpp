#include "FindBar.h"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QStyle>
#include <QToolButton>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);

    m_searchField = new QLineEdit(this);
    m_searchField->setPlaceholderText(tr("Find"));
    m_searchField->setClearButtonEnabled(true);
    m_searchField->installEventFilter(this);

    m_replaceField = new QLineEdit(this);
    m_replaceField->setPlaceholderText(tr("Replace with"));
    m_replaceField->setFixedWidth(200);
    m_replaceField->installEventFilter(this);

    m_caseBox = new QCheckBox(tr("Match &case"), this);
    m_wordBox = new QCheckBox(tr("Whole &words"), this);

    m_status = new QLabel(this);
    m_status->setMinimumWidth(80);

    auto *closeButton = new QToolButton(this);
    closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeButton->setAutoRaise(true);
    closeButton->setToolTip(tr("Close (Escape)"));
    connect(closeButton, &QToolButton::clicked, this, &FindBar::closed);

    auto *prevButton = new QPushButton(tr("&Previous"), this);
    connect(prevButton, &QPushButton::clicked,
            this, &FindBar::findPreviousRequested);

    auto *nextButton = new QPushButton(tr("&Next"), this);
    connect(nextButton, &QPushButton::clicked,
            this, &FindBar::findNextRequested);

    auto *replaceButton = new QPushButton(tr("&Replace"), this);
    connect(replaceButton, &QPushButton::clicked,
            this, &FindBar::replaceRequested);

    auto *replaceAllButton = new QPushButton(tr("Replace &All"), this);
    connect(replaceAllButton, &QPushButton::clicked,
            this, &FindBar::replaceAllRequested);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->addWidget(closeButton);
    layout->addWidget(m_searchField, 1);
    layout->addWidget(prevButton);
    layout->addWidget(nextButton);
    layout->addWidget(m_caseBox);
    layout->addWidget(m_wordBox);
    layout->addWidget(m_status);
    layout->addWidget(m_replaceField, 0);
    layout->addWidget(replaceButton);
    layout->addWidget(replaceAllButton);
    setLayout(layout);

    connect(m_searchField, &QLineEdit::textChanged,
            this, &FindBar::optionsChanged);
    connect(m_caseBox, &QCheckBox::toggled, this, &FindBar::optionsChanged);
    connect(m_wordBox, &QCheckBox::toggled, this, &FindBar::optionsChanged);

    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &FindBar::closed);
}

QString FindBar::searchText() const
{
    return m_searchField->text();
}

QString FindBar::replaceText() const
{
    return m_replaceField->text();
}

bool FindBar::matchCase() const
{
    return m_caseBox->isChecked();
}

bool FindBar::wholeWord() const
{
    return m_wordBox->isChecked();
}

void FindBar::setSearchText(const QString &text)
{
    m_searchField->setText(text);
}

void FindBar::setMatchCase(bool on)
{
    m_caseBox->setChecked(on);
}

void FindBar::setWholeWord(bool on)
{
    m_wordBox->setChecked(on);
}

void FindBar::showMessage(const QString &text)
{
    m_status->setText(text);
}

void FindBar::focusSearchField()
{
    m_searchField->setFocus();
    m_searchField->selectAll();
}

bool FindBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            const bool shift = keyEvent->modifiers().testFlag(Qt::ShiftModifier);
            if (watched == m_searchField) {
                if (shift)
                    emit findPreviousRequested();
                else
                    emit findNextRequested();
            } else if (watched == m_replaceField) {
                emit replaceRequested();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
