#include "TerminalPanel.h"

#include <QDir>
#include <QVBoxLayout>

#include <qtermwidget.h>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
}

void TerminalPanel::setProjectRoot(const QString &root)
{
    m_root = root;
}

void TerminalPanel::startShell()
{
    if (!m_terminal) {
        m_terminal = new QTermWidget(0, this);
        m_terminal->setWorkingDirectory(
            m_root.isEmpty() ? QDir::homePath() : m_root);
        m_terminal->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Expanding);
        layout()->addWidget(m_terminal);
        connect(m_terminal, &QTermWidget::finished, this,
                [this]() { m_started = false; });
    }

    if (!m_started) {
        m_terminal->startShellProgram();
        m_started = true;
    }
    m_terminal->setFocus();
}

void TerminalPanel::restartShell()
{
    if (m_terminal) {
        layout()->removeWidget(m_terminal);
        m_terminal->deleteLater();
        m_terminal = nullptr;
        m_started = false;
    }
    startShell();
}

void TerminalPanel::focusTerminal()
{
    if (m_terminal)
        m_terminal->setFocus();
}
