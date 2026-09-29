#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantList>
#include <QStandardPaths>
#include <QDir>

struct Account {
    int id;
    QString issuer;
    QString name;
    QString secret;
    int digits;
    int period;
};

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    Q_INVOKABLE bool initialize();
    Q_INVOKABLE QVariantList getAccounts();
    Q_INVOKABLE bool addAccount(const QString &issuer, const QString &name,
                                 const QString &secret, int digits, int period);
    Q_INVOKABLE bool updateAccount(int id, const QString &issuer, const QString &name,
                                    const QString &secret, int digits, int period);
    Q_INVOKABLE bool deleteAccount(int id);
    Q_INVOKABLE bool accountExists(const QString &secret);

private:
    QSqlDatabase m_db;
    bool createTables();
};

#endif // DATABASE_H
