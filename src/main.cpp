#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QString>
#include <QTextStream>

#include <cstring>

#include "MainWindow.h"
#include "Theme.h"

#ifndef LITE_STUDIO_VERSION
#define LITE_STUDIO_VERSION "0.0.0-dev"
#endif

namespace {

const char *const kVersion = LITE_STUDIO_VERSION;

bool isVersionFlag(const char *arg)
{
    return std::strcmp(arg, "--version") == 0 || std::strcmp(arg, "-v") == 0;
}

bool isHelpFlag(const char *arg)
{
    return std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0;
}

} // namespace

int main(int argc, char *argv[])
{
    // Handle --version/--help before QApplication so they work without a
    // display server (packaging scripts and CI rely on this).
    for (int i = 1; i < argc; ++i) {
        if (isVersionFlag(argv[i])) {
            QTextStream(stdout) << "Lite Studio " << kVersion << "\n";
            return 0;
        }
        if (isHelpFlag(argv[i])) {
            QTextStream(stdout)
                << "Lite Studio " << kVersion << "\n\n"
                << "Usage: lite-studio [options] [path]\n\n"
                << "Options:\n"
                << "  -h, --help       Show this help and exit.\n"
                << "  -v, --version    Show the version and exit.\n\n"
                << "Arguments:\n"
                << "  path             Folder to browse, or file to open.\n";
            return 0;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName("Lite Studio");
    QApplication::setOrganizationName("LiteStudio");
    QApplication::setApplicationVersion(kVersion);
    QApplication::setDesktopFileName("lite-studio");

    QIcon appIcon;
    for (int size : {16, 32, 48, 64, 128, 256, 512, 1024}) {
        appIcon.addFile(QStringLiteral(":/lite-studio-%1.png").arg(size),
                        QSize(size, size));
    }
    QApplication::setWindowIcon(appIcon);

    QApplication::setStyle("Fusion");
    Theme::applyApplication(Theme::Mode::Dark);

    MainWindow window;
    window.resize(1100, 720);
    window.show();

    // First non-option argument: a folder to browse, or a file to open.
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '\0' || argv[i][0] == '-')
            continue;
        window.openPath(QString::fromLocal8Bit(argv[i]));
        break;
    }

    return app.exec();
}
