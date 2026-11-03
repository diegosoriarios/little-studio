#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <QStringEncoder>

#include <Qsci/qscilexercpp.h>

#include <algorithm>

#include <cstdio>

#include "EditorTab.h"
#include "FileCollector.h"
#include "FileIO.h"
#include "FuzzyMatch.h"
#include "SimpleLexer.h"

// Headless (offscreen) verification of EditorTab's load/save behavior:
// byte-exact round trips, read-only handling, and save failure paths.

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

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

bool writeRaw(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(bytes) == bytes.size();
}

void makeReadOnly(const QString &path)
{
    QFile::setPermissions(path, QFileDevice::ReadOwner
        | QFileDevice::ReadGroup | QFileDevice::ReadOther);
}

void makeWritable(const QString &path)
{
    QFile::setPermissions(path, QFileDevice::ReadOwner
        | QFileDevice::WriteOwner | QFileDevice::ReadGroup
        | QFileDevice::ReadOther);
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
    QString err;

    // 1. Unmodified save must not rewrite the file (byte- and mtime-exact).
    {
        const QString path = base + "/crlf.txt";
        CHECK(writeRaw(path, "first\r\nsecond\r\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.lineEndingName() == QStringLiteral("CRLF"));
        CHECK(tab.encodingName() == QStringLiteral("UTF-8"));
        const QByteArray before = readBytes(path);
        const QDateTime mtime = QFileInfo(path).lastModified();
        CHECK(tab.save(&err));
        CHECK(readBytes(path) == before);
        CHECK(QFileInfo(path).lastModified() == mtime);
    }

    // 2. UTF-8 BOM: modify and save keeps BOM, order and EOL intact.
    {
        const QString path = base + "/bom.txt";
        CHECK(writeRaw(path, QByteArrayLiteral("\xEF\xBB\xBFhello\n")));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.encodingName() == QStringLiteral("UTF-8 BOM"));
        tab.insert(QStringLiteral("X"));
        CHECK(tab.isModified());
        CHECK(tab.save(&err));
        CHECK(readBytes(path) == QByteArrayLiteral("\xEF\xBB\xBFXhello\n"));
        CHECK(!tab.isModified());
    }

    // 3. UTF-16 LE: decode, modify, save re-encodes with BOM.
    {
        const QString path = base + "/utf16.txt";
        QStringEncoder encoder(QStringConverter::Utf16LE);
        const QByteArray body = encoder.encode(QStringLiteral("héllo"));
        CHECK(writeRaw(path, QByteArrayLiteral("\xFF\xFE") + body));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.encodingName() == QStringLiteral("UTF-16 LE"));
        CHECK(tab.text() == QStringLiteral("héllo"));
        tab.insert(QStringLiteral("X"));
        CHECK(tab.save(&err));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.encoding == FileIO::Encoding::Utf16LE
              && r.text == QStringLiteral("Xhéllo"));
    }

    // 4. Binary files are refused.
    {
        const QString path = base + "/blob.bin";
        CHECK(writeRaw(path, QByteArray("ab\0cd", 5)));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Binary);
    }

    // 5. Large file (> 2 MiB) opens as plain text, no highlighting.
    {
        const QString path = base + "/large.txt";
        CHECK(writeRaw(path, QByteArray(3 * 1024 * 1024, 'x')));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.isLargeFile());
        CHECK(tab.languageName() == QStringLiteral("Plain Text (large file)"));
    }

    // 6. Indentation style detection.
    {
        const QString path = base + "/tabs.txt";
        CHECK(writeRaw(path, "class A {\n\tvoid f();\n}\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.indentationName() == QStringLiteral("Tabs"));
    }
    {
        const QString path = base + "/spaces.txt";
        CHECK(writeRaw(path, "def f():\n    return 1\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.indentationName() == QStringLiteral("Spaces: 4"));
    }

    // 7. Read-only files open read-only.
    {
        const QString path = base + "/locked.txt";
        CHECK(writeRaw(path, "hi\n"));
        makeReadOnly(path);
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.isReadOnlyFile());
        CHECK(tab.isReadOnly());
        makeWritable(path);
    }

    // 8. Save failure: write-protected after open keeps changes and reports.
    {
        const QString path = base + "/fail.txt";
        CHECK(writeRaw(path, "data\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        tab.insert(QStringLiteral("X"));
        CHECK(tab.isModified());
        makeReadOnly(path);
        const bool saved = tab.save(&err);
        CHECK(!saved);
        CHECK(!err.isEmpty());
        CHECK(tab.isModified());
        CHECK(readBytes(path) == QByteArray("data\n"));
        makeWritable(path);
    }

    // 9. Missing trailing newline survives modification.
    {
        const QString path = base + "/tail.txt";
        CHECK(writeRaw(path, "a\nb"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        tab.append(QStringLiteral("c"));
        CHECK(tab.save(&err));
        CHECK(readBytes(path) == QByteArray("a\nbc"));
    }

    // 10. Language detection and lexer selection.
    {
        const QString path = base + "/L.kt";
        CHECK(writeRaw(path, "fun main() {}\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.languageName() == QStringLiteral("Kotlin"));
        CHECK(qobject_cast<SimpleLexer *>(tab.lexer()) != nullptr);
    }
    {
        const QString path = base + "/L.java";
        CHECK(writeRaw(path, "class L {}\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.languageName() == QStringLiteral("Java"));
        CHECK(qobject_cast<QsciLexerCPP *>(tab.lexer()) != nullptr);
    }
    {
        const QString path = base + "/build.gradle.kts";
        CHECK(writeRaw(path, "plugins { java }\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.languageName() == QStringLiteral("Gradle"));
    }
    {
        const QString path = base + "/data.foo";
        CHECK(writeRaw(path, "content\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.languageName() == QStringLiteral("Plain Text"));
        CHECK(tab.lexer() == nullptr);
    }

    // 11. Theme switching recolors both the lexer and the editor.
    {
        const QString path = base + "/Theme.java";
        CHECK(writeRaw(path, "class T {}\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        tab.setTheme(Theme::Mode::Dark);
        const QColor darkKeyword = tab.lexer()->color(QsciLexerCPP::Keyword);
        const QColor darkPaper = tab.lexer()->defaultPaper();
        tab.setTheme(Theme::Mode::Light);
        const QColor lightKeyword = tab.lexer()->color(QsciLexerCPP::Keyword);
        const QColor lightPaper = tab.lexer()->defaultPaper();
        CHECK(darkKeyword != lightKeyword);
        CHECK(darkPaper != lightPaper);
    }

    // 12. Word wrap toggle.
    {
        const QString path = base + "/wrap.txt";
        CHECK(writeRaw(path, "text\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        tab.setWordWrapEnabled(true);
        CHECK(tab.wrapMode() == QsciScintilla::WrapWord);
        tab.setWordWrapEnabled(false);
        CHECK(tab.wrapMode() == QsciScintilla::WrapNone);
    }

    // 13. SimpleLexer styles Kotlin tokens; multibyte text must not
    //     shift styling (Scintilla positions are byte offsets).
    {
        const QString path = base + "/Style.kt";
        const QString content = QStringLiteral(
            "// caf\u00e9\n"
            "fun main() {\n"
            "    val name = \"w\u00f6rld\"\n"
            "}\n");
        CHECK(writeRaw(path, content.toUtf8()));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        tab.SendScintilla(QsciScintillaBase::SCI_COLOURISE, 0UL, -1L);

        auto styleAt = [&tab](const char *needle) -> int {
            if (!tab.findFirst(QString::fromLatin1(needle), false, true,
                               false, false, true, 0, 0))
                return -2;
            int line = 0;
            int index = 0;
            tab.getCursorPosition(&line, &index);
            // The cursor sits after the match; step back to its start.
            const int pos = tab.positionFromLineIndex(line, index)
                - static_cast<int>(qstrlen(needle));
            const int style = static_cast<int>(tab.SendScintilla(
                QsciScintillaBase::SCI_GETSTYLEAT,
                static_cast<unsigned long>(pos)));
            return style;
        };

        CHECK(styleAt("//") == SimpleLexer::LineComment);
        CHECK(styleAt("fun") == SimpleLexer::Keyword);
        CHECK(styleAt("main") == SimpleLexer::Function);
        CHECK(styleAt("val") == SimpleLexer::Keyword);
        CHECK(styleAt("\"w") == SimpleLexer::String);
        CHECK(styleAt("{") == SimpleLexer::Bracket);
        CHECK(styleAt("=") == SimpleLexer::Operator);
    }

    // 14. Highlight-all match counting (case + word-boundary options).
    {
        const QString path = base + "/find.txt";
        CHECK(writeRaw(path, "val a\nval b\nVAL\nvalley\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.highlightMatches(QStringLiteral("val"), true, true) == 2);
        CHECK(tab.highlightMatches(QStringLiteral("val"), false, true) == 3);
        CHECK(tab.highlightMatches(QStringLiteral("val"), true, false) == 3);
        CHECK(tab.highlightMatches(QStringLiteral("val"), false, false) == 4);
        tab.clearMatches();
        CHECK(tab.highlightMatches(QStringLiteral("zzz"), false, false) == 0);
    }

    // 15. Replace all, including a replacement containing the search term.
    {
        const QString path = base + "/replace.txt";
        CHECK(writeRaw(path, "one two one\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.replaceAllMatches(QStringLiteral("one"), QStringLiteral("1"),
                                    true, true) == 2);
        CHECK(tab.save(&err));
        CHECK(readBytes(path) == QByteArray("1 two 1\n"));
    }
    {
        const QString path = base + "/replace2.txt";
        CHECK(writeRaw(path, "a b a\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(tab.replaceAllMatches(QStringLiteral("a"), QStringLiteral("aa"),
                                    true, true) == 2);
        CHECK(tab.save(&err));
        CHECK(readBytes(path) == QByteArray("aa b aa\n"));
    }

    // 16. Forward/backward navigation with wrap-around.
    {
        const QString path = base + "/nav.txt";
        CHECK(writeRaw(path, "one two one two one\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        auto selectionStart = [&tab]() {
            return static_cast<int>(tab.SendScintilla(
                QsciScintillaBase::SCI_GETSELECTIONSTART));
        };
        CHECK(tab.findNextMatch(QStringLiteral("one"), true, true, true));
        CHECK(selectionStart() == 0);
        CHECK(tab.findNextMatch(QStringLiteral("one"), true, true, true));
        CHECK(selectionStart() == 8);
        CHECK(tab.findNextMatch(QStringLiteral("one"), true, true, true));
        CHECK(selectionStart() == 16);
        CHECK(tab.findNextMatch(QStringLiteral("one"), true, true, true));
        CHECK(selectionStart() == 0);
        CHECK(tab.findNextMatch(QStringLiteral("one"), false, true, true));
        CHECK(selectionStart() == 16);
        CHECK(tab.findNextMatch(QStringLiteral("two"), true, true, true));
        CHECK(selectionStart() == 4);
    }

    // 17. changedOnDisk tracks external modification; save refreshes it.
    {
        const QString path = base + "/watch.txt";
        CHECK(writeRaw(path, "stable\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        CHECK(!tab.changedOnDisk());

        // Simulate an external edit by forcing a newer mtime.
        QFile file(path);
        CHECK(file.open(QIODevice::ReadOnly));
        CHECK(file.setFileTime(QDateTime::currentDateTime().addSecs(60),
                               QFileDevice::FileModificationTime));
        file.close();
        CHECK(tab.changedOnDisk());

        tab.insert(QStringLiteral("X"));
        CHECK(tab.save(&err));
        CHECK(!tab.changedOnDisk());
        CHECK(readBytes(path) == QByteArray("Xstable\n"));

        // A file deleted on disk also counts as changed.
        QFile::remove(path);
        CHECK(tab.changedOnDisk());
    }

    // 18. Highlight-all must not move the caret (it used to leave it on
    //     the last match and scroll the viewport all over the file).
    {
        const QString path = base + "/caret.txt";
        CHECK(writeRaw(path, "one two one two one\n"));
        EditorTab tab;
        CHECK(tab.load(path, &err) == EditorTab::LoadStatus::Ok);
        tab.setCursorPosition(0, 0);
        const int caretBefore = static_cast<int>(tab.SendScintilla(
            QsciScintillaBase::SCI_GETCURRENTPOS));
        CHECK(tab.highlightMatches(QStringLiteral("one"), true, true) == 3);
        const int caretAfter = static_cast<int>(tab.SendScintilla(
            QsciScintillaBase::SCI_GETCURRENTPOS));
        CHECK(caretAfter == caretBefore);
    }

    // 19. FileCollector: chunked walk, ignored dirs skipped, sorted output.
    {
        const QString root = base + "/collect";
        CHECK(QDir().mkpath(root + "/src/deep"));
        CHECK(QDir().mkpath(root + "/build"));
        CHECK(QDir().mkpath(root + "/.git/objects"));
        CHECK(writeRaw(root + "/b.txt", "b\n"));
        CHECK(writeRaw(root + "/src/a.txt", "a\n"));
        CHECK(writeRaw(root + "/src/deep/c.txt", "c\n"));
        CHECK(writeRaw(root + "/build/skip.txt", "s\n"));
        CHECK(writeRaw(root + "/.git/objects/skip2.txt", "s\n"));

        FileCollector collector;
        QEventLoop loop;
        QObject::connect(&collector, &FileCollector::finished,
                         &loop, &QEventLoop::quit);
        collector.start(root, 10000);
        loop.exec();

        const QStringList files = collector.files();
        CHECK(files.size() == 3);
        const QString joined = files.join(QLatin1Char('\n'));
        CHECK(!joined.contains(QStringLiteral("/build/")));
        CHECK(!joined.contains(QStringLiteral("/.git/")));
        CHECK(std::is_sorted(files.cbegin(), files.cend()));
        CHECK(files.last().endsWith(QStringLiteral("/src/deep/c.txt"))
              || files.first().endsWith(QStringLiteral("/b.txt")));
    }

    // 20. Fuzzy scoring: prefix > basename > path > subsequence; -1 = miss.
    {
        CHECK(fuzzyScore(QStringLiteral("src/Main.kt"),
                         QStringLiteral("main")) == 100);
        CHECK(fuzzyScore(QStringLiteral("app/appmain.java"),
                         QStringLiteral("main")) == 80);
        CHECK(fuzzyScore(QStringLiteral("libs/main/x.cpp"),
                         QStringLiteral("main")) == 60);
        CHECK(fuzzyScore(QStringLiteral("src/kotlin.txt"),
                         QStringLiteral("kln")) == 40);
        CHECK(fuzzyScore(QStringLiteral("src/kotlin.txt"),
                         QStringLiteral("zzz")) == -1);
        CHECK(fuzzyScore(QStringLiteral("anything.kt"), QString()) == 0);
    }

    if (failures == 0) {
        std::printf("ALL EDITORTAB TESTS PASSED\n");
        return 0;
    }
    std::printf("%d EDITORTAB TEST(S) FAILED\n", failures);
    return 1;
}
