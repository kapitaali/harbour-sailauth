#include "database.h"
#include <QDebug>

Database::Database(QObject *parent) : QObject(parent)
{
}

Database::~Database()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool Database::initialize()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dataDir + "/sailauth.db");

    if (!m_db.open()) {
        qDebug() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}

bool Database::createTables()
{
    QSqlQuery query(m_db);
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS accounts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "issuer TEXT NOT NULL DEFAULT '', "
        "name TEXT NOT NULL DEFAULT '', "
        "secret TEXT NOT NULL, "
        "digits INTEGER NOT NULL DEFAULT 6, "
        "period INTEGER NOT NULL DEFAULT 30, "
        "sort_order INTEGER NOT NULL DEFAULT 0"
        ")"
    );

    if (!ok) {
        qDebug() << "Failed to create table:" << query.lastError().text();
    }
    return ok;
}

QVariantList Database::getAccounts()
{
    QVariantList result;
    QSqlQuery query(m_db);
    query.exec("SELECT id, issuer, name, secret, digits, period FROM accounts ORDER BY sort_order, id");

    while (query.next()) {
        QVariantMap account;
        account["id"] = query.value(0).toInt();
        account["issuer"] = query.value(1).toString();
        account["name"] = query.value(2).toString();
        account["secret"] = query.value(3).toString();
        account["digits"] = query.value(4).toInt();
        account["period"] = query.value(5).toInt();
        result.append(account);
    }
    return result;
}

bool Database::addAccount(const QString &issuer, const QString &name,
                           const QString &secret, int digits, int period)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO accounts (issuer, name, secret, digits, period, sort_order) "
                  "VALUES (?, ?, ?, ?, ?, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM accounts))");
    query.addBindValue(issuer);
    query.addBindValue(name);
    query.addBindValue(secret);
    query.addBindValue(digits);
    query.addBindValue(period);

    bool ok = query.exec();
    if (!ok) {
        qDebug() << "Failed to add account:" << query.lastError().text();
    }
    return ok;
}

bool Database::updateAccount(int id, const QString &issuer, const QString &name,
                              const QString &secret, int digits, int period)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE accounts SET issuer=?, name=?, secret=?, digits=?, period=? WHERE id=?");
    query.addBindValue(issuer);
    query.addBindValue(name);
    query.addBindValue(secret);
    query.addBindValue(digits);
    query.addBindValue(period);
    query.addBindValue(id);

    bool ok = query.exec();
    if (!ok) {
        qDebug() << "Failed to update account:" << query.lastError().text();
    }
    return ok;
}

bool Database::deleteAccount(int id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM accounts WHERE id=?");
    query.addBindValue(id);

    bool ok = query.exec();
    if (!ok) {
        qDebug() << "Failed to delete account:" << query.lastError().text();
    }
    return ok;
}

bool Database::accountExists(const QString &secret)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM accounts WHERE secret=?");
    query.addBindValue(secret);
    query.exec();
    if (query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}
