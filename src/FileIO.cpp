#include "FileIO.h"

#include <QFile>
#include <QSaveFile>
#include <QStringDecoder>
#include <QStringEncoder>

namespace {

constexpr int kBinaryScanBytes = 8192;

FileIO::LineEnding detectLineEnding(const QString &text)
{
    int crlf = 0;
    int lf = 0;
    int cr = 0;
    const int size = text.size();
    for (int i = 0; i < size; ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\r')) {
            if (i + 1 < size && text.at(i + 1) == QLatin1Char('\n')) {
                ++crlf;
                ++i;
            } else {
                ++cr;
            }
        } else if (c == QLatin1Char('\n')) {
            ++lf;
        }
    }

    if (crlf >= lf && crlf >= cr)
        return crlf > 0 ? FileIO::LineEnding::CRLF : FileIO::LineEnding::LF;
    if (lf >= cr)
        return FileIO::LineEnding::LF;
    return FileIO::LineEnding::CR;
}

// Normalizes every line ending to the requested style before encoding.
QString normalizeLineEndings(const QString &text, FileIO::LineEnding eol)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QLatin1String("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    switch (eol) {
    case FileIO::LineEnding::CRLF:
        normalized.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
        break;
    case FileIO::LineEnding::CR:
        normalized.replace(QLatin1Char('\n'), QLatin1Char('\r'));
        break;
    case FileIO::LineEnding::LF:
        break;
    }
    return normalized;
}

QByteArray encodeText(const QString &text, FileIO::Encoding encoding)
{
    switch (encoding) {
    case FileIO::Encoding::Utf8:
        return text.toUtf8();
    case FileIO::Encoding::Utf8Bom:
        return QByteArrayLiteral("\xEF\xBB\xBF") + text.toUtf8();
    case FileIO::Encoding::Utf16LE: {
        QStringEncoder encoder(QStringConverter::Utf16LE);
        return QByteArrayLiteral("\xFF\xFE") + encoder.encode(text);
    }
    case FileIO::Encoding::Utf16BE: {
        QStringEncoder encoder(QStringConverter::Utf16BE);
        return QByteArrayLiteral("\xFE\xFF") + encoder.encode(text);
    }
    case FileIO::Encoding::Latin1:
        return text.toLatin1();
    }
    return text.toUtf8();
}

} // namespace

FileIO::LoadResult FileIO::readFile(const QString &path)
{
    LoadResult result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QObject::tr("Cannot open %1: %2").arg(path, file.errorString());
        return result;
    }
    const QByteArray data = file.readAll();
    file.close();

    if (data.startsWith(QByteArrayLiteral("\xEF\xBB\xBF"))) {
        result.encoding = Encoding::Utf8Bom;
        result.text = QString::fromUtf8(data.constData() + 3, data.size() - 3);
    } else if (data.startsWith(QByteArrayLiteral("\xFF\xFE"))) {
        result.encoding = Encoding::Utf16LE;
        QStringDecoder decoder(QStringConverter::Utf16LE);
        result.text = decoder.decode(data.mid(2));
    } else if (data.startsWith(QByteArrayLiteral("\xFE\xFF"))) {
        result.encoding = Encoding::Utf16BE;
        QStringDecoder decoder(QStringConverter::Utf16BE);
        result.text = decoder.decode(data.mid(2));
    } else if (data.left(kBinaryScanBytes).contains('\0')) {
        result.binary = true;
        return result;
    } else {
        // Stateless: the whole file is one chunk, so a truncated or
        // invalid UTF-8 sequence is reported as an error immediately.
        QStringDecoder decoder(QStringConverter::Utf8,
                               QStringConverter::Flag::Stateless);
        result.text = decoder.decode(data);
        if (decoder.hasError()) {
            result.text = QString::fromLatin1(data.constData(), data.size());
            result.encoding = Encoding::Latin1;
        } else {
            result.encoding = Encoding::Utf8;
        }
    }

    result.eol = detectLineEnding(result.text);
    result.ok = true;
    return result;
}

bool FileIO::writeFile(const QString &path, const QString &text,
                       Encoding encoding, LineEnding eol, QString *error)
{
    const QString normalized = normalizeLineEndings(text, eol);
    const QByteArray bytes = encodeText(normalized, encoding);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QObject::tr("Cannot save %1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (error)
            *error = QObject::tr("Cannot save %1: %2").arg(path, file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = QObject::tr("Cannot save %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

QString FileIO::encodingToString(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Utf8:
        return QStringLiteral("UTF-8");
    case Encoding::Utf8Bom:
        return QStringLiteral("UTF-8 BOM");
    case Encoding::Utf16LE:
        return QStringLiteral("UTF-16 LE");
    case Encoding::Utf16BE:
        return QStringLiteral("UTF-16 BE");
    case Encoding::Latin1:
        return QStringLiteral("Latin-1");
    }
    return QStringLiteral("UTF-8");
}

QString FileIO::eolToString(LineEnding eol)
{
    switch (eol) {
    case LineEnding::LF:
        return QStringLiteral("LF");
    case LineEnding::CRLF:
        return QStringLiteral("CRLF");
    case LineEnding::CR:
        return QStringLiteral("CR");
    }
    return QStringLiteral("LF");
}
