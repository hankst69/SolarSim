#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SolarSim"));
    QApplication::setOrganizationName(QStringLiteral("SolarSim"));

    MainWindow window;
    window.show();

    return app.exec();
}
