#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTreeWidgetItem>

#include <cstdio>

#include "FileTreeView.h"

// Headless (offscreen) verification of the lazy file tree, focused on symlink
// handling: recursive links are shown but not expandable, and links to
// non-ancestor directories still expand normally.

namespace {

int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n",                      \
                         __FILE__, __LINE__, #cond);                      \
            ++failures;                                                   \
        }                                                                 \
    } while (false)

bool writeFile(const QString &path, const QByteArray &content)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath()))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(content) == content.size();
}

QTreeWidgetItem *findChild(QTreeWidgetItem *parent, const QString &name)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(0) == name)
            return child;
    }
    return nullptr;
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString base = dir.path();

    const QString root = base + "/project";
    const QString outside = base + "/outside";
    CHECK(writeFile(root + "/top.txt", "t\n"));
    CHECK(writeFile(root + "/sub/real.txt", "r\n"));
    CHECK(writeFile(outside + "/out.txt", "o\n"));

#if defined(Q_OS_UNIX)
    CHECK(QFile::link(root, root + "/sub/up"));       // ancestor link
    CHECK(QFile::link(root, root + "/self"));         // self link
    CHECK(QFile::link(outside, root + "/ext"));       // non-ancestor symlink
#endif

    FileTreeView tree;
    tree.setRootPath(root);

    // 1. Root auto-expands and lists its first level.
    QTreeWidgetItem *rootItem = tree.topLevelItem(0);
    CHECK(rootItem != nullptr);
    CHECK(rootItem->isExpanded());
    CHECK(findChild(rootItem, "top.txt") != nullptr);

    QTreeWidgetItem *sub = findChild(rootItem, "sub");
    CHECK(sub != nullptr);

    // 2. A normal directory expands and shows its contents.
    sub->setExpanded(true);
    CHECK(findChild(sub, "real.txt") != nullptr);

    // 3. A link to an ancestor is present but disabled and not expandable.
    QTreeWidgetItem *up = findChild(sub, "up");
    CHECK(up != nullptr);
    CHECK(up->isDisabled());
    CHECK(up->childCount() == 0);
    up->setExpanded(true);
    CHECK(up->childCount() == 0);

    QTreeWidgetItem *self = findChild(rootItem, "self");
    CHECK(self != nullptr);
    CHECK(self->isDisabled());
    CHECK(self->childCount() == 0);

    // 4. A symlink to a non-ancestor directory remains usable.
    QTreeWidgetItem *ext = findChild(rootItem, "ext");
    CHECK(ext != nullptr);
    CHECK(!ext->isDisabled());
    ext->setExpanded(true);
    CHECK(findChild(ext, "out.txt") != nullptr);

    // 5. revealFile still selects a regular file through the tree.
    tree.revealFile(root + "/sub/real.txt");
    CHECK(tree.currentItem() != nullptr);
    CHECK(tree.currentItem()->text(0) == "real.txt");

    if (failures == 0) {
        std::printf("ALL FILETREE TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FILETREE TEST(S) FAILED\n", failures);
    return 1;
}
