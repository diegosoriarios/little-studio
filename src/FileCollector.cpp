#include "FileCollector.h"

#include "FileTreeView.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>

namespace {
constexpr int kBatchMs = 20;
// Defense-in-depth bound for pathological trees. Cycles are already handled by
// the canonical-path visited set; this caps a walk that keeps finding genuinely
// distinct directories.
constexpr int kMaxDirs = 200000;
}

FileCollector::FileCollector(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &FileCollector::step);
}

void FileCollector::start(const QString &rootPath, int maxFiles)
{
    stop();
    m_files.clear();
    m_dirs.clear();
    m_visited.clear();
    m_dirs.append(rootPath);
    m_maxFiles = maxFiles;
    m_running = true;
    m_timer.start(0);
}

void FileCollector::stop()
{
    m_running = false;
    m_timer.stop();
}

void FileCollector::step()
{
    if (!m_running) {
        m_timer.stop();
        return;
    }

    QElapsedTimer clock;
    clock.start();
    while (!m_dirs.isEmpty() && m_files.size() < m_maxFiles
           && clock.elapsed() < kBatchMs) {
        const QString dir = m_dirs.takeLast();

        // Follow symlinks, but never walk a directory twice. A symlink cycle
        // (a -> b -> a) or a link back to an ancestor would otherwise keep
        // queueing directories forever, because the file cap does not bound
        // an empty directory cycle.
        const QString canonical = QFileInfo(dir).canonicalFilePath();
        if (canonical.isEmpty() || m_visited.contains(canonical))
            continue;
        if (m_visited.size() >= kMaxDirs) {
            m_dirs.clear();
            break;
        }
        m_visited.insert(canonical);

        const QFileInfoList entries = QDir(dir).entryInfoList(
            QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            if (m_files.size() >= m_maxFiles)
                break;
            if (entry.isDir()) {
                if (!FileTreeView::isIgnoredName(entry.fileName()))
                    m_dirs.append(entry.absoluteFilePath());
            } else if (entry.isFile()) {
                m_files.append(entry.absoluteFilePath());
            }
        }
    }

    emit batch();

    if (m_dirs.isEmpty() || m_files.size() >= m_maxFiles) {
        m_files.sort();
        m_running = false;
        m_timer.stop();
        emit finished();
    }
}
