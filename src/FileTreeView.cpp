#include "FileTreeView.h"

#include <QDir>
#include <QFileInfo>
#include <QStyle>

FileTreeView::FileTreeView(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setUniformRowHeights(true);
    setAnimated(false);
    setIndentation(16);
    setExpandsOnDoubleClick(false);
    setSelectionMode(QAbstractItemView::SingleSelection);

    // Lazy load: a directory's children are read the first time it expands.
    connect(this, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
        if (item && !item->data(0, LoadedRole).toBool())
            populate(item);
    });

    // Double-click and Enter both activate. Directories toggle; files open.
    connect(this, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item, int) {
        if (!item)
            return;
        if (item->data(0, IsDirRole).toBool()) {
            item->setExpanded(!item->isExpanded());
            return;
        }
        const QString path = item->data(0, PathRole).toString();
        if (!path.isEmpty())
            emit fileActivated(path);
    });
}

void FileTreeView::setRootPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isDir())
        return;

    clear();
    m_rootPath = info.canonicalFilePath();

    const QString name = info.fileName().isEmpty() ? m_rootPath : info.fileName();
    auto *root = new QTreeWidgetItem(this, { name });
    root->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    root->setData(0, PathRole, m_rootPath);
    root->setData(0, IsDirRole, true);
    root->setData(0, LoadedRole, false);
    root->setData(0, CanonicalRole, m_rootPath);
    root->setToolTip(0, m_rootPath);
    addPlaceholder(root);

    // Triggers itemExpanded, which populates the first level lazily.
    root->setExpanded(true);
}

void FileTreeView::setShowIgnored(bool show)
{
    if (m_showIgnored == show)
        return;
    m_showIgnored = show;
    applyIgnoreFilter(invisibleRootItem());
}

void FileTreeView::populate(QTreeWidgetItem *dirItem)
{
    const QString dirPath = dirItem->data(0, PathRole).toString();

    // Remove the placeholder that made the expander visible.
    const QList<QTreeWidgetItem *> placeholder = dirItem->takeChildren();
    qDeleteAll(placeholder);

    const QFileInfoList entries = QDir(dirPath).entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &entry : entries) {
        const bool isDir = entry.isDir();
        const QString canonical = entry.canonicalFilePath();
        const bool recursiveLink = isDir && !canonical.isEmpty()
            && chainContainsCanonical(dirItem, canonical);

        auto *child = new QTreeWidgetItem(dirItem);
        child->setText(0, entry.fileName());
        child->setIcon(0, style()->standardIcon(
            isDir ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
        child->setData(0, PathRole, entry.absoluteFilePath());
        child->setData(0, IsDirRole, isDir);
        child->setData(0, CanonicalRole, canonical);
        child->setToolTip(0, entry.absoluteFilePath());

        if (!isDir) {
            child->setData(0, LoadedRole, true);
            continue;
        }

        if (recursiveLink) {
            // A symlink to this folder or one of its ancestors: show it so the
            // link is discoverable, but never allow expansion (it would
            // recurse forever).
            child->setData(0, LoadedRole, true);
            child->setDisabled(true);
            child->setToolTip(0,
                tr("Recursive link (points to an ancestor folder)"));
            if (isIgnoredName(entry.fileName()))
                child->setHidden(!m_showIgnored);
            continue;
        }

        // Merge single-child directory chains into one row (e.g. src/main/java).
        QString absolutePath = entry.absoluteFilePath();
        QString canonicalPath = canonical;
        const QString displayName = compactDirectoryName(
            dirItem, entry, &absolutePath, &canonicalPath);
        child->setText(0, displayName);
        child->setData(0, PathRole, absolutePath);
        child->setData(0, CanonicalRole, canonicalPath);
        child->setToolTip(0, absolutePath);

        child->setData(0, LoadedRole, false);
        addPlaceholder(child);
        if (isIgnoredName(entry.fileName()))
            child->setHidden(!m_showIgnored);
    }

    dirItem->setData(0, LoadedRole, true);
}

QString FileTreeView::compactDirectoryName(QTreeWidgetItem *parentItem,
                                           const QFileInfo &first,
                                           QString *absolutePath,
                                           QString *canonicalPath) const
{
    QString display = first.fileName();
    QFileInfo current = first;
    QString canonical = first.canonicalFilePath();
    QSet<QString> seen;
    if (!canonical.isEmpty())
        seen.insert(canonical);

    // Never merge through an ignored folder: it stays a normal (possibly
    // hidden) row so the ignored-name toggle keeps working.
    if (!isIgnoredName(first.fileName())) {
        for (int depth = 0; depth < MaxCompactDepth; ++depth) {
            const QFileInfoList entries = QDir(current.absoluteFilePath())
                .entryInfoList(
                    QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
            if (entries.size() != 1)
                break;

            const QFileInfo only = entries.first();
            if (!only.isDir() || isIgnoredName(only.fileName()))
                break;

            const QString onlyCanonical = only.canonicalFilePath();
            if (onlyCanonical.isEmpty() || seen.contains(onlyCanonical)
                || chainContainsCanonical(parentItem, onlyCanonical)) {
                break;
            }

            seen.insert(onlyCanonical);
            display += QLatin1Char('/') + only.fileName();
            current = only;
            canonical = onlyCanonical;
        }
    }

    if (absolutePath)
        *absolutePath = current.absoluteFilePath();
    if (canonicalPath)
        *canonicalPath = canonical;
    return display;
}

bool FileTreeView::chainContainsCanonical(QTreeWidgetItem *item,
                                          const QString &canonicalPath)
{
    for (QTreeWidgetItem *ancestor = item; ancestor;
         ancestor = ancestor->parent()) {
        const QString path = ancestor->data(0, CanonicalRole).toString();
        if (!path.isEmpty() && path == canonicalPath)
            return true;
    }
    return false;
}

void FileTreeView::addPlaceholder(QTreeWidgetItem *dirItem)
{
    // An empty child makes the expander arrow appear before loading.
    auto *placeholder = new QTreeWidgetItem(dirItem);
    placeholder->setDisabled(true);
}

void FileTreeView::applyIgnoreFilter(QTreeWidgetItem *item)
{
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        if (child->data(0, IsDirRole).toBool() && isIgnoredName(child->text(0)))
            child->setHidden(!m_showIgnored);
        applyIgnoreFilter(child);
    }
}

bool FileTreeView::isIgnoredName(const QString &name)
{
    static const QSet<QString> names = {
        QStringLiteral("build"),
        QStringLiteral(".gradle"),
        QStringLiteral(".git"),
        QStringLiteral("DerivedData"),
        QStringLiteral("node_modules"),
        QStringLiteral(".idea"),
    };
    for (const QString &ignored : names) {
        if (name.compare(ignored, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void FileTreeView::revealFile(const QString &path)
{
    if (m_rootPath.isEmpty())
        return;
    const QString canonical = QFileInfo(path).canonicalFilePath();
    const QString root = QDir(m_rootPath).canonicalPath();
    if (canonical.isEmpty() || root.isEmpty())
        return;
    if (canonical != root && !canonical.startsWith(root + QLatin1Char('/')))
        return;
    if (canonical == root)
        return;

    QTreeWidgetItem *item = topLevelItem(0);
    if (!item)
        return;

    while (true) {
        if (!item->data(0, LoadedRole).toBool()) {
            populate(item);
            if (!item->data(0, LoadedRole).toBool())
                return; // hit a non-directory along the way
        }

        QTreeWidgetItem *next = findChildForPath(item, canonical);
        if (!next)
            return;
        item = next;

        const QString itemPath =
            item->data(0, CanonicalRole).toString().isEmpty()
                ? item->data(0, PathRole).toString()
                : item->data(0, CanonicalRole).toString();
        if (itemPath == canonical)
            break;
        if (!item->data(0, IsDirRole).toBool())
            return;
    }
    if (!item || item->data(0, IsDirRole).toBool())
        return;

    for (QTreeWidgetItem *parent = item->parent(); parent;
         parent = parent->parent())
        parent->setExpanded(true);
    setCurrentItem(item);
    scrollToItem(item);
}

QTreeWidgetItem *FileTreeView::findChildForPath(QTreeWidgetItem *parent,
                                                 const QString &targetPath) const
{
    QTreeWidgetItem *best = nullptr;
    int bestLength = -1;
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        QString childPath = child->data(0, CanonicalRole).toString();
        if (childPath.isEmpty())
            childPath = child->data(0, PathRole).toString();
        if (childPath.isEmpty())
            continue;
        if (targetPath != childPath
            && !targetPath.startsWith(childPath + QLatin1Char('/'))) {
            continue;
        }
        // Prefer the most specific match: a symlink to an ancestor can also
        // prefix the target, but the deeper real path is the right one.
        if (childPath.size() > bestLength) {
            best = child;
            bestLength = childPath.size();
        }
    }
    return best;
}
