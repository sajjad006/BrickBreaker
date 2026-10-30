#include "leaderboard.h"

#include <QDir>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

Leaderboard::Leaderboard()
    : connectionName("leaderboard")
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    filePath = dir + "/leaderboard.db";

    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        qWarning() << "Leaderboard: cannot open" << filePath << ":" << db.lastError().text();
        return;
    }

    QSqlQuery q(db);
    ok = q.exec("CREATE TABLE IF NOT EXISTS scores ("
                "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  name  TEXT    NOT NULL,"
                "  score INTEGER NOT NULL,"
                "  level INTEGER NOT NULL,"
                "  date  TEXT    NOT NULL DEFAULT '')");
    if (!ok)
        qWarning() << "Leaderboard: cannot create table:" << q.lastError().text();
}

Leaderboard::~Leaderboard()
{
    db.close();
    db = QSqlDatabase();                  // drop our handle before removing the connection
    QSqlDatabase::removeDatabase(connectionName);
}

int Leaderboard::load(Row out[], int max) const
{
    if (!ok) return 0;

    QSqlQuery q(db);
    q.prepare("SELECT name, score, level, date FROM scores "
              "ORDER BY score DESC, id ASC LIMIT :n");
    q.bindValue(":n", max);
    if (!q.exec()) {
        qWarning() << "Leaderboard: load failed:" << q.lastError().text();
        return 0;
    }
    int n = 0;
    while (n < max && q.next()) {
        out[n].name  = q.value(0).toString();
        out[n].score = q.value(1).toInt();
        out[n].level = q.value(2).toInt();
        out[n].date  = q.value(3).toString();
        ++n;
    }
    return n;
}

bool Leaderboard::insert(const QString &name, int score, int level, const QString &date)
{
    if (!ok) return false;

    QSqlQuery q(db);
    q.prepare("INSERT INTO scores (name, score, level, date) "
              "VALUES (:name, :score, :level, :date)");
    q.bindValue(":name", name);
    q.bindValue(":score", score);
    q.bindValue(":level", level);
    q.bindValue(":date", date.isNull() ? QString("") : date);   // a null QString binds as SQL NULL
    if (!q.exec()) {
        qWarning() << "Leaderboard: insert failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Leaderboard::clear()
{
    if (!ok) return false;
    QSqlQuery q(db);
    if (!q.exec("DELETE FROM scores")) {
        qWarning() << "Leaderboard: clear failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Leaderboard::isEmpty() const
{
    if (!ok) return true;
    QSqlQuery q(db);
    return !(q.exec("SELECT 1 FROM scores LIMIT 1") && q.next());
}
