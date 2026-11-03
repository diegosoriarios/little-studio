#ifndef LITE_STUDIO_LINE_BUFFER_H
#define LITE_STUDIO_LINE_BUFFER_H

#include <QStringList>

// Bounded line buffer with case-insensitive filtering. Keeps memory bounded
// regardless of output volume (build logs, logcat streams).
class LineBuffer
{
public:
    explicit LineBuffer(int maxLines = 5000);

    void append(const QString &line);
    void append(const QStringList &lines);
    void clear();

    QStringList lines() const { return m_lines; }
    int size() const { return m_lines.size(); }
    int maxLines() const { return m_maxLines; }

    // Lines containing filter (case-insensitive); all lines when empty.
    QStringList filtered(const QString &filter) const;

private:
    void trim();

    QStringList m_lines;
    int m_maxLines;
};

#endif
