#include "FileCollector.h"

#include "FileTreeView.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>

namespace {
constexpr int kBatchMs = 20;
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
