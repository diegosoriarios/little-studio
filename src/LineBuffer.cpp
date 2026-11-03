#include "LineBuffer.h"

LineBuffer::LineBuffer(int maxLines)
    : m_maxLines(qMax(1, maxLines))
{
}

void LineBuffer::append(const QString &line)
{
    m_lines.append(line);
    trim();
}

void LineBuffer::append(const QStringList &lines)
{
    if (lines.isEmpty())
        return;
    if (lines.size() >= m_maxLines) {
        m_lines = lines.mid(lines.size() - m_maxLines);
        return;
    }
    m_lines.append(lines);
    trim();
}

void LineBuffer::clear()
{
    m_lines.clear();
}

QStringList LineBuffer::filtered(const QString &filter) const
{
    if (filter.isEmpty())
        return m_lines;

    QStringList result;
    result.reserve(m_lines.size());
    for (const QString &line : m_lines) {
        if (line.contains(filter, Qt::CaseInsensitive))
            result.append(line);
    }
    return result;
}

void LineBuffer::trim()
{
    const int excess = m_lines.size() - m_maxLines;
    if (excess > 0)
        m_lines.erase(m_lines.begin(), m_lines.begin() + excess);
}
