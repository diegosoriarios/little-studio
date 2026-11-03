#include <QApplication>

#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Lite Studio");
    QApplication::setOrganizationName("LiteStudio");
    QApplication::setApplicationVersion("0.1.0");

    QApplication::setStyle("Fusion");
    Theme::applyApplication(Theme::Mode::Dark);

    MainWindow window;
    window.resize(1100, 720);
    window.show();

    // Optional argument: a folder to browse, or a file to open directly.
    if (argc > 1)
        window.openPath(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}
