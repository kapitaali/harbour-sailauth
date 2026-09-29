#include "accountmodel.h"
#include "totp.h"

AccountModel::AccountModel(Database *db, QObject *parent)
    : QAbstractListModel(parent), m_db(db)
{
    refresh();
}

int AccountModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_accounts.count();
}

QVariant AccountModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_accounts.count())
        return QVariant();

    const QVariantMap &account = m_accounts.at(index.row()).toMap();

    switch (role) {
    case IdRole: return account["id"];
    case IssuerRole: return account["issuer"];
    case NameRole: return account["name"];
    case SecretRole: return account["secret"];
    case DigitsRole: return account["digits"];
    case PeriodRole: return account["period"];
    case CodeRole: {
        Totp totp;
        return totp.generateCode(account["secret"].toString(),
                                 account["digits"].toInt(),
                                 account["period"].toInt());
    }
    case RemainingRole: {
        Totp totp;
        return totp.remainingSeconds(account["period"].toInt());
    }
    default: return QVariant();
    }
}

QHash<int, QByteArray> AccountModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "accountId";
    roles[IssuerRole] = "issuer";
    roles[NameRole] = "name";
    roles[SecretRole] = "secret";
    roles[DigitsRole] = "digits";
    roles[PeriodRole] = "period";
    roles[CodeRole] = "code";
    roles[RemainingRole] = "remaining";
    return roles;
}

int AccountModel::count() const
{
    return m_accounts.count();
}

void AccountModel::refresh()
{
    beginResetModel();
    m_accounts = m_db->getAccounts();
    endResetModel();
    emit countChanged();
}

/*
 * Called once a second from QML. Codes change when the time window rolls
 * over, so this only notifies the delegates about the two roles that vary —
 * a full model reset here would destroy and recreate every delegate once per
 * second (losing scroll position, open context menus and flick momentum).
 */
void AccountModel::tick()
{
    if (m_accounts.isEmpty()) return;

    emit dataChanged(index(0), index(m_accounts.count() - 1),
                     QVector<int>() << CodeRole << RemainingRole);
}

void AccountModel::addAccount(const QString &issuer, const QString &name,
                               const QString &secret, int digits, int period)
{
    if (m_db->addAccount(issuer, name, secret, digits, period)) {
        refresh();
    }
}

void AccountModel::updateAccount(int id, const QString &issuer, const QString &name,
                                  const QString &secret, int digits, int period)
{
    if (m_db->updateAccount(id, issuer, name, secret, digits, period)) {
        refresh();
    }
}

void AccountModel::deleteAccount(int id)
{
    if (m_db->deleteAccount(id)) {
        refresh();
    }
}

QString AccountModel::generateCode(int index)
{
    if (index < 0 || index >= m_accounts.count()) return QString();
    const QVariantMap &account = m_accounts.at(index).toMap();
    Totp totp;
    return totp.generateCode(account["secret"].toString(),
                             account["digits"].toInt(),
                             account["period"].toInt());
}

int AccountModel::remainingSeconds(int index)
{
    if (index < 0 || index >= m_accounts.count()) return 0;
    const QVariantMap &account = m_accounts.at(index).toMap();
    Totp totp;
    return totp.remainingSeconds(account["period"].toInt());
}

QString AccountModel::getAccountIssuer(int index)
{
    if (index < 0 || index >= m_accounts.count()) return QString();
    const QVariantMap &account = m_accounts.at(index).toMap();
    return account["issuer"].toString();
}
