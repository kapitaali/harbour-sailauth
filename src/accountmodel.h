#ifndef ACCOUNTMODEL_H
#define ACCOUNTMODEL_H

#include <QAbstractListModel>
#include <QVariantList>
#include "database.h"

class AccountModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        IssuerRole,
        NameRole,
        SecretRole,
        DigitsRole,
        PeriodRole,
        CodeRole,
        RemainingRole
    };

    explicit AccountModel(Database *db, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void tick();
    Q_INVOKABLE void addAccount(const QString &issuer, const QString &name,
                                const QString &secret, int digits, int period);
    Q_INVOKABLE void updateAccount(int id, const QString &issuer, const QString &name,
                                   const QString &secret, int digits, int period);
    Q_INVOKABLE void deleteAccount(int id);
    Q_INVOKABLE QString generateCode(int index);
    Q_INVOKABLE int remainingSeconds(int index);
    Q_INVOKABLE QString getAccountIssuer(int index);

signals:
    void countChanged();

private:
    Database *m_db;
    QVariantList m_accounts;
};

#endif // ACCOUNTMODEL_H
