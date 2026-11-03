#include "LexerFactory.h"

#include <QFileInfo>

#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerxml.h>

#include "SimpleLexer.h"

namespace {

enum class Language {
    Plain,
    Kotlin,
    Swift,
    Java,
    Gradle,
    Xml,
    Json,
    Markdown,
};

Language languageForFile(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    if (name.endsWith(QStringLiteral(".gradle.kts"), Qt::CaseInsensitive))
        return Language::Gradle;

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("kt") || suffix == QLatin1String("kts"))
        return Language::Kotlin;
    if (suffix == QLatin1String("swift"))
        return Language::Swift;
    if (suffix == QLatin1String("java"))
        return Language::Java;
    if (suffix == QLatin1String("gradle"))
        return Language::Gradle;
    if (suffix == QLatin1String("xml") || suffix == QLatin1String("fxml")
        || suffix == QLatin1String("plist") || suffix == QLatin1String("storyboard")
        || suffix == QLatin1String("xib") || suffix == QLatin1String("svg"))
        return Language::Xml;
    if (suffix == QLatin1String("json") || suffix == QLatin1String("jsonc")
        || suffix == QLatin1String("geojson"))
        return Language::Json;
    if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown")
        || suffix == QLatin1String("mdown"))
        return Language::Markdown;
    return Language::Plain;
}

} // namespace

QString LexerFactory::languageName(const QString &path)
{
    switch (languageForFile(path)) {
    case Language::Kotlin:
        return QStringLiteral("Kotlin");
    case Language::Swift:
        return QStringLiteral("Swift");
    case Language::Java:
        return QStringLiteral("Java");
    case Language::Gradle:
        return QStringLiteral("Gradle");
    case Language::Xml:
        return QStringLiteral("XML");
    case Language::Json:
        return QStringLiteral("JSON");
    case Language::Markdown:
        return QStringLiteral("Markdown");
    case Language::Plain:
        break;
    }
    return QStringLiteral("Plain Text");
}

QsciLexer *LexerFactory::create(const QString &path, QObject *parent)
{
    switch (languageForFile(path)) {
    case Language::Kotlin:
        return new SimpleLexer(SimpleLexer::Kotlin, parent);
    case Language::Swift:
        return new SimpleLexer(SimpleLexer::Swift, parent);
    case Language::Java:
    case Language::Gradle:
        // Groovy and Kotlin DSL gradle files are close enough to Java
        // for strings/comments/numbers highlighting.
        return new QsciLexerJava(parent);
    case Language::Xml:
        return new QsciLexerXML(parent);
    case Language::Json:
        return new QsciLexerJSON(parent);
    case Language::Markdown:
        return new QsciLexerMarkdown(parent);
    case Language::Plain:
        break;
    }
    return nullptr;
}
