#ifndef IMPORTER_H
#define IMPORTER_H

#include <QObject>
#include <QVariantList>
#include <QUrl>

class Database;

struct ImportAccount {
    QString issuer;
    QString name;
    QString secret;
    int digits;
    int period;
};

class Importer : public QObject
{
    Q_OBJECT
public:
    explicit Importer(Database *db, QObject *parent = nullptr);

    Q_INVOKABLE QVariantList parseFile(const QString &filePath);
    Q_INVOKABLE QVariantList parseText(const QString &text);
    Q_INVOKABLE int importAccounts(const QVariantList &accounts);

private:
    ImportAccount parseOtpAuthUri(const QString &uri);

    Database *m_db;
};

#endif // IMPORTER_H
