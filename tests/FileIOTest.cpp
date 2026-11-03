#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

#include "FileIO.h"

using Encoding = FileIO::Encoding;
using LineEnding = FileIO::LineEnding;

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

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;
    const QString base = dir.path();
    QString err;

    // 1. UTF-8 + LF round trip.
    {
        const QString path = base + "/a.txt";
        CHECK(FileIO::writeFile(path, "hello\nworld\n", Encoding::Utf8,
                                LineEnding::LF, &err));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && !r.binary);
        CHECK(r.encoding == Encoding::Utf8);
        CHECK(r.eol == LineEnding::LF);
        CHECK(r.text == QStringLiteral("hello\nworld\n"));
    }

    // 2. CRLF written and detected.
    {
        const QString path = base + "/b.txt";
        CHECK(FileIO::writeFile(path, "a\nb\n", Encoding::Utf8,
                                LineEnding::CRLF, &err));
        CHECK(readBytes(path) == QByteArray("a\r\nb\r\n"));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.eol == LineEnding::CRLF);
    }

    // 3. UTF-8 BOM written and preserved.
    {
        const QString path = base + "/c.txt";
        CHECK(FileIO::writeFile(path, "bom", Encoding::Utf8Bom,
                                LineEnding::LF, &err));
        CHECK(readBytes(path).startsWith(QByteArrayLiteral("\xEF\xBB\xBF")));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.encoding == Encoding::Utf8Bom
              && r.text == QStringLiteral("bom"));
    }

    // 4. UTF-16 LE written and read back.
    {
        const QString path = base + "/d.txt";
        CHECK(FileIO::writeFile(path, QStringLiteral("héllo"),
                                Encoding::Utf16LE, LineEnding::LF, &err));
        CHECK(readBytes(path).startsWith(QByteArrayLiteral("\xFF\xFE")));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.encoding == Encoding::Utf16LE
              && r.text == QStringLiteral("héllo"));
    }

    // 5. Binary (NUL byte) is refused.
    {
        const QString path = base + "/e.bin";
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(QByteArray("ab\0cd", 5));
        file.close();
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(!r.ok && r.binary);
    }

    // 6. Invalid UTF-8 falls back to Latin-1 and round-trips byte-exact.
    {
        const QString path = base + "/f.txt";
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(QByteArray("caf\xE9"));
        file.close();
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.encoding == Encoding::Latin1
              && r.text == QString::fromLatin1("caf\xE9"));
        CHECK(FileIO::writeFile(path, r.text, r.encoding, r.eol, &err));
        CHECK(readBytes(path) == QByteArray("caf\xE9"));
    }

    // 7. CR-only line endings.
    {
        const QString path = base + "/g.txt";
        CHECK(FileIO::writeFile(path, "a\nb\n", Encoding::Utf8,
                                LineEnding::CR, &err));
        CHECK(readBytes(path) == QByteArray("a\rb\r"));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.eol == LineEnding::CR && r.text == "a\rb\r");
    }

    // 8. Mixed line endings: dominant style wins (CRLF here).
    {
        const QString path = base + "/h.txt";
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(QByteArray("a\r\nb\nc\r\n"));
        file.close();
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.eol == LineEnding::CRLF);
    }

    // 9. No trailing newline is preserved.
    {
        const QString path = base + "/i.txt";
        CHECK(FileIO::writeFile(path, "a\nb", Encoding::Utf8,
                                LineEnding::LF, &err));
        CHECK(readBytes(path) == QByteArray("a\nb"));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.text == "a\nb");
    }

    // 10. Empty file round trip.
    {
        const QString path = base + "/j.txt";
        CHECK(FileIO::writeFile(path, QString(), Encoding::Utf8,
                                LineEnding::LF, &err));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.text.isEmpty() && r.eol == LineEnding::LF);
    }

    // 11. Multibyte UTF-8 (CJK + emoji) round trip.
    {
        const QString path = base + "/k.txt";
        const QString text = QStringLiteral("héllo 世界 \U0001F600\n");
        CHECK(FileIO::writeFile(path, text, Encoding::Utf8,
                                LineEnding::LF, &err));
        const FileIO::LoadResult r = FileIO::readFile(path);
        CHECK(r.ok && r.encoding == Encoding::Utf8 && r.text == text);
    }

    if (failures == 0) {
        std::printf("ALL FILEIO TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FILEIO TEST(S) FAILED\n", failures);
    return 1;
}
