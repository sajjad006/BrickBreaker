#include <QApplication>
#include <QCoreApplication>

#include "gamewidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Needed for QSettings and for the leaderboard database path
    QCoreApplication::setOrganizationName("BrickBreakerQt");
    QCoreApplication::setApplicationName("BrickBreaker");

    GameWidget game;
    game.show();

    return app.exec();
}
