#ifndef LITE_STUDIO_PROJECT_DETECTOR_H
#define LITE_STUDIO_PROJECT_DETECTOR_H

#include <QString>
#include <QStringList>

// Result of a cheap, bounded project scan (root + one level, small reads).
// hasAndroid/hasApple are capability flags: the native run panels are enabled
// from them, independent of the primary label.
struct ProjectInfo
{
    enum class Kind {
        Unknown,
        Android,
        Apple,
        Flutter,
        ReactNative,
        Expo,
    };

    Kind kind = Kind::Unknown;
    bool hasAndroid = false;
    bool hasApple = false;
    QString androidRoot;   // dir containing gradlew/settings.gradle
    QString appleRoot;     // dir containing xcodeproj/xcworkspace/Podfile
    QStringList evidence;  // human-readable detection reasons

    QString kindName() const;
    QString summary() const;
};

namespace ProjectDetector {

// Pure detection; reads only a handful of small files. No recursion beyond
// one level (android/, ios/) and no indexing.
ProjectInfo detect(const QString &rootPath);

} // namespace ProjectDetector

#endif
