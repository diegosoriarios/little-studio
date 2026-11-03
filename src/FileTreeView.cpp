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
        auto *child = new QTreeWidgetItem(dirItem);
        child->setText(0, entry.fileName());
        child->setIcon(0, style()->standardIcon(
            entry.isDir() ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
        child->setData(0, PathRole, entry.absoluteFilePath());
        child->setData(0, IsDirRole, entry.isDir());
        child->setToolTip(0, entry.absoluteFilePath());

        if (entry.isDir()) {
            child->setData(0, LoadedRole, false);
            addPlaceholder(child);
            if (isIgnoredName(entry.fileName()))
                child->setHidden(!m_showIgnored);
        } else {
            child->setData(0, LoadedRole, true);
        }
    }

    dirItem->setData(0, LoadedRole, true);
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
    const QStringList parts =
        canonical.mid(root.size() + 1).split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (!item->data(0, LoadedRole).toBool()) {
            populate(item);
            if (!item->data(0, LoadedRole).toBool())
                return; // hit a non-directory along the way
        }
        item = findChildItem(item, part);
        if (!item)
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

QTreeWidgetItem *FileTreeView::findChildItem(QTreeWidgetItem *parent,
                                              const QString &name) const
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(0) == name)
            return child;
    }
    return nullptr;
}
