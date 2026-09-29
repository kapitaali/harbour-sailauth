#ifndef IMPORTER_H
#define IMPORTER_H

#include <QObject>
#include <QVariantList>
#include <QUrl>

class Database;
class QJsonValue;
class QJsonObject;

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
    ImportAccount parseOtpAuthUri(const QString &uri) const;
    QVariantList parseJson(const QString &json);
    void collect(const QJsonValue &value, QVariantList &accounts) const;
    QVariantMap entryFromObject(const QJsonObject &object) const;
    QVariantMap makeAccount(const QString &issuer, const QString &name,
                            const QString &secret, const QString &type,
                            const QString &algorithm, int digits, int period) const;
    void appendUri(QVariantList &accounts, const QString &uri) const;

    Database *m_db;
};

#endif // IMPORTER_H
