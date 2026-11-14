#ifndef LITE_STUDIO_FILE_TREE_VIEW_H
#define LITE_STUDIO_FILE_TREE_VIEW_H

#include <QSet>
#include <QTreeWidget>

class QFileInfo;

// Lazy-loading project file tree.
// Directory contents are read when the user expands a folder, probing one
// level ahead to merge single-child folder chains.
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

    // Follows a single-child chain of directories and returns the merged label
    // (e.g. "src/main/java") plus the final absolute and canonical paths.
    QString compactDirectoryName(QTreeWidgetItem *parentItem,
                                 const QFileInfo &first,
                                 QString *absolutePath,
                                 QString *canonicalPath) const;

    // Finds the child whose path contains targetPath (merged rows may span
    // several path segments).
    QTreeWidgetItem *findChildForPath(QTreeWidgetItem *parent,
                                      const QString &targetPath) const;

    // True when canonicalPath matches this item or any of its ancestors.
    static bool chainContainsCanonical(QTreeWidgetItem *item,
                                       const QString &canonicalPath);

    static constexpr int PathRole = Qt::UserRole;
    static constexpr int IsDirRole = Qt::UserRole + 1;
    static constexpr int LoadedRole = Qt::UserRole + 2;
    static constexpr int CanonicalRole = Qt::UserRole + 3;
    static constexpr int MaxCompactDepth = 32;

    QString m_rootPath;
    bool m_showIgnored = false;
};

#endif
