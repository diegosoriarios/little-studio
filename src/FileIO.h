#ifndef LITE_STUDIO_FILE_IO_H
#define LITE_STUDIO_FILE_IO_H

#include <QString>

// Synchronous file reading/writing helpers.
// No threads, no watchers: reads happen on the UI thread by design.
class FileIO
{
public:
    enum class Encoding {
        Utf8,
        Utf8Bom,
        Utf16LE,
        Utf16BE,
        Latin1, // fallback for bytes that are not valid UTF-8
    };

    enum class LineEnding {
        LF,
        CRLF,
        CR,
    };

    struct LoadResult {
        bool ok = false;
        bool binary = false;
        QString error;
        QString text;
        Encoding encoding = Encoding::Utf8;
        LineEnding eol = LineEnding::LF;
    };

    static LoadResult readFile(const QString &path);
    static bool writeFile(const QString &path, const QString &text,
                          Encoding encoding, LineEnding eol, QString *error);

    static QString encodingToString(Encoding encoding);
    static QString eolToString(LineEnding eol);
};

#endif
