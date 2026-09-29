#ifndef TOTP_H
#define TOTP_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QCryptographicHash>

class Totp : public QObject
{
    Q_OBJECT
public:
    explicit Totp(QObject *parent = nullptr);

    Q_INVOKABLE QString generateCode(const QString &secret, int digits = 6, int period = 30);
    Q_INVOKABLE int remainingSeconds(int period = 30);
    Q_INVOKABLE bool validateSecret(const QString &secret);

    static QByteArray base32Decode(const QString &input);
    static QString generateTotp(const QByteArray &key, quint64 counter, int digits);
    static QByteArray hmac(QCryptographicHash::Algorithm algorithm,
                           const QByteArray &key, const QByteArray &message);
};

#endif // TOTP_H
