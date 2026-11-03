#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

#include "EditorConfig.h"

// Headless tests for minimal .editorconfig parsing (indentation only).

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

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString base = dir.path();

    // 1. Missing file -> defaults unset.
    {
        const QString root = base + "/none";
        CHECK(QDir().mkpath(root));
        const EditorConfig::Settings settings =
            EditorConfig::loadFor(root, QStringLiteral("Main.kt"));
        CHECK(!settings.indentStyleSet);
        CHECK(!settings.indentSizeSet);
        CHECK(!settings.tabWidthSet);
    }

    // 2. Global section + per-extension override (later wins).
    {
        const QString root = base + "/mixed";
        CHECK(writeFile(root + "/.editorconfig",
            "root = true\n"
            "\n"
            "[*]\n"
            "indent_style = space\n"
            "indent_size = 4\n"
            "\n"
            "[*.kt]\n"
            "indent_style = tab\n"
            "indent_size = 2\n"));
        const EditorConfig::Settings kotlin =
            EditorConfig::loadFor(root, QStringLiteral("Main.kt"));
        CHECK(kotlin.indentStyleSet);
        CHECK(kotlin.useTabs);
        CHECK(kotlin.indentSizeSet);
        CHECK(kotlin.indentSize == 2);

        const EditorConfig::Settings cpp =
            EditorConfig::loadFor(root, QStringLiteral("main.cpp"));
        CHECK(cpp.indentStyleSet);
        CHECK(!cpp.useTabs);
        CHECK(cpp.indentSize == 4);
    }

    // 3. Exact-name section (Makefile) + tab_width.
    {
        const QString root = base + "/make";
        CHECK(writeFile(root + "/.editorconfig",
            "[Makefile]\n"
            "indent_style = tab\n"
            "tab_width = 8\n"));
        const EditorConfig::Settings settings =
            EditorConfig::loadFor(root, QStringLiteral("Makefile"));
        CHECK(settings.indentStyleSet);
        CHECK(settings.useTabs);
        CHECK(settings.tabWidthSet);
        CHECK(settings.tabWidth == 8);
        CHECK(!settings.indentSizeSet);
    }

    // 4. Non-matching extension keeps defaults unset.
    {
        const QString root = base + "/onlykt";
        CHECK(writeFile(root + "/.editorconfig",
            "[*.kt]\nindent_style = tab\n"));
        const EditorConfig::Settings settings =
            EditorConfig::loadFor(root, QStringLiteral("Main.java"));
        CHECK(!settings.indentStyleSet);
        CHECK(!settings.indentSizeSet);
    }

    // 5. Comments and blank lines are tolerated.
    {
        const QString root = base + "/comments";
        CHECK(writeFile(root + "/.editorconfig",
            "# comment\n"
            "; another comment\n"
            "\n"
            "[*.py]\n"
            "indent_style = space\n"
            "indent_size = 4 ; inline comment\n"));
        const EditorConfig::Settings settings =
            EditorConfig::loadFor(root, QStringLiteral("app.py"));
        CHECK(settings.indentStyleSet);
        CHECK(!settings.useTabs);
        CHECK(settings.indentSize == 4);
    }

    if (failures == 0) {
        std::printf("ALL EDITORCONFIG TESTS PASSED\n");
        return 0;
    }
    std::printf("%d EDITORCONFIG TEST(S) FAILED\n", failures);
    return 1;
}
