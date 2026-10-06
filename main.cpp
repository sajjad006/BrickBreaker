#include <QApplication>
#include <QCoreApplication>

#include "gamewidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Needed so QSettings can store the high score
    QCoreApplication::setOrganizationName("BrickBreakerQt");
    QCoreApplication::setApplicationName("BrickBreaker");

    GameWidget game;
    game.show();

    return app.exec();
}
