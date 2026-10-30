#ifndef LEADERBOARD_H
#define LEADERBOARD_H

#include <QString>
#include <QSqlDatabase>

// Local SQLite storage for the high-score table.
// The file lives in the per-user app data folder, e.g. on Linux:
//   ~/.local/share/BrickBreakerQt/BrickBreaker/leaderboard.db
// Every finished run that makes the table is one row in the "scores" table;
// the game only ever reads back the best few.
class Leaderboard
{
public:
    struct Row { QString name; int score; int level; QString date; };

    Leaderboard();
    ~Leaderboard();

    bool isOpen() const { return ok; }
    QString path() const { return filePath; }

    // best rows first (ties: the older row first); returns how many were read
    int  load(Row out[], int max) const;
    bool insert(const QString &name, int score, int level, const QString &date);
    bool clear();
    bool isEmpty() const;

private:
    QSqlDatabase db;
    QString connectionName;
    QString filePath;
    bool ok = false;
};

#endif // LEADERBOARD_H
