#include "ProcessUtil.h"

#include <QRegularExpression>

namespace ProcessUtil {

QString stripAnsi(const QString &text)
{
    if (!text.contains(QChar(0x1B)))
        return text;

    static const QRegularExpression csi(
        QStringLiteral("\\x1B\\[[0-9;?]*[ -/]*[@-~]"));
    static const QRegularExpression osc(
        QStringLiteral("\\x1B\\][^\\x07]*(?:\\x07|\\x1B\\\\)"));

    QString result = text;
    result.remove(osc);
    result.remove(csi);
    // Drop any malformed leftover escape characters.
    result.remove(QChar(0x1B));
    return result;
}

QString normalizeNewlines(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return normalized;
}

} // namespace ProcessUtil
