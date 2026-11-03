#ifndef LITE_STUDIO_FILE_TREE_VIEW_H
#define LITE_STUDIO_FILE_TREE_VIEW_H

#include <QSet>
#include <QTreeWidget>

// Lazy-loading project file tree.
// Directory contents are read only when the user expands a folder.
// There is no file watcher and no background indexing.
class FileTreeView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit FileTreeView(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    QString rootPath() const { return m_rootPath; }

    bool showIgnored() const { return m_showIgnored; }
    void setShowIgnored(bool show);

    // Case-insensitive match against the ignored-folder name list.
    static bool isIgnoredName(const QString &name);

    // Loads (as needed) and selects the tree item for a file inside the
    // current project, expanding its ancestors.
    void revealFile(const QString &path);

signals:
    void fileActivated(const QString &path);

private:
    void populate(QTreeWidgetItem *dirItem);
    void addPlaceholder(QTreeWidgetItem *dirItem);
    void applyIgnoreFilter(QTreeWidgetItem *item);
    QTreeWidgetItem *findChildItem(QTreeWidgetItem *parent,
                                   const QString &name) const;

    static constexpr int PathRole = Qt::UserRole;
    static constexpr int IsDirRole = Qt::UserRole + 1;
    static constexpr int LoadedRole = Qt::UserRole + 2;

    QString m_rootPath;
    bool m_showIgnored = false;
};

#endif
