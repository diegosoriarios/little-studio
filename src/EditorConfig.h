#ifndef LITE_STUDIO_EDITOR_CONFIG_H
#define LITE_STUDIO_EDITOR_CONFIG_H

#include <QString>

// Minimal .editorconfig support: indentation properties only.
// Sections match the file's base name via glob (case-insensitive, * does not
// cross into directory paths); later sections override earlier ones.
namespace EditorConfig {

struct Settings {
    bool indentStyleSet = false;
    bool useTabs = false;
    bool indentSizeSet = false;
    int indentSize = 4;
    bool tabWidthSet = false;
    int tabWidth = 4;
};

Settings loadFor(const QString &rootPath, const QString &fileName);

} // namespace EditorConfig

#endif
