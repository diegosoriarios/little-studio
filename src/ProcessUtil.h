#ifndef LITE_STUDIO_PROCESS_UTIL_H
#define LITE_STUDIO_PROCESS_UTIL_H

#include <QString>

namespace ProcessUtil {

// Removes ANSI/VT escape sequences (CSI colors, OSC titles) from output.
QString stripAnsi(const QString &text);

// Converts CRLF and lone CR to LF so output can be split into lines.
QString normalizeNewlines(const QString &text);

} // namespace ProcessUtil

#endif
