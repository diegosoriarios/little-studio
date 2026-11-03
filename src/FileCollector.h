#ifndef LITE_STUDIO_FILE_COLLECTOR_H
#define LITE_STUDIO_FILE_COLLECTOR_H

#include <QObject>
#include <QStringList>
#include <QTimer>

// Chunked, single-threaded project file walk. Emits batch() after each
// time-boxed step so the UI stays responsive; finished() when the walk
// is complete. No background threads.
class FileCollector : public QObject
{
    Q_OBJECT

public:
    explicit FileCollector(QObject *parent = nullptr);

    void start(const QString &rootPath, int maxFiles);
    void stop();

    bool isRunning() const { return m_running; }
    QStringList files() const { return m_files; }

signals:
    void batch();
    void finished();

private:
    void step();

    QTimer m_timer;
    QStringList m_dirs;      // pending directories (stack)
    QStringList m_files;
    int m_maxFiles = 0;
    bool m_running = false;
};

#endif
