#include "totp.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QtEndian>

Totp::Totp(QObject *parent) : QObject(parent)
{
}

QByteArray Totp::base32Decode(const QString &input)
{
    static const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QByteArray result;
    int buffer = 0;
    int bitsLeft = 0;

    QString cleaned = input.toUpper();
    cleaned.remove('=');
    cleaned.remove(' ');
    cleaned.remove('-');

    for (const QChar &c : cleaned) {
        int idx = alphabet.indexOf(c);
        if (idx < 0) continue;
        buffer = (buffer << 5) | idx;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            bitsLeft -= 8;
            result.append(static_cast<char>((buffer >> bitsLeft) & 0xFF));
        }
    }
    return result;
}

QString Totp::generateTotp(const QByteArray &key, quint64 counter, int digits)
{
    QByteArray counterBytes(8, 0);
    // Qt 5.6's qToBigEndian takes uchar*, not char*.
    qToBigEndian(counter, reinterpret_cast<uchar *>(counterBytes.data()));

    // RFC 4226/6238 require HMAC, not plain SHA1(key || counter): the key is
    // padded with ipad/opad and hashed twice.
    QByteArray hash = hmac(QCryptographicHash::Sha1, key, counterBytes);

    int offset = hash.at(hash.length() - 1) & 0x0F;
    quint32 binary = (static_cast<quint32>(hash.at(offset) & 0x7F) << 24) |
                     (static_cast<quint32>(hash.at(offset + 1) & 0xFF) << 16) |
                     (static_cast<quint32>(hash.at(offset + 2) & 0xFF) << 8) |
                     static_cast<quint32>(hash.at(offset + 3) & 0xFF);

    quint32 modulus = 1;
    for (int i = 0; i < digits; ++i) modulus *= 10;

    quint32 otp = binary % modulus;
    return QString("%1").arg(otp, digits, 10, QChar('0'));
}

/*
 * HMAC (RFC 2104) on top of QCryptographicHash — Qt has no HMAC of its own
 * until Qt 5.12's QMessageAuthenticationCode. Generic over the digest so
 * SHA256/SHA512 TOTP support is a matter of passing a different algorithm.
 */
QByteArray Totp::hmac(QCryptographicHash::Algorithm algorithm,
                      const QByteArray &key, const QByteArray &message)
{
    const int blockSize = (algorithm == QCryptographicHash::Sha512) ? 128 : 64;

    QByteArray normalisedKey = key;
    if (normalisedKey.size() > blockSize) {
        normalisedKey = QCryptographicHash::hash(normalisedKey, algorithm);
    }

    // QByteArray::resize does not guarantee zeroed bytes, so pad explicitly.
    QByteArray paddedKey(blockSize, '\0');
    paddedKey.replace(0, qMin(normalisedKey.size(), blockSize), normalisedKey);

    QByteArray innerPad(blockSize, '\0');
    QByteArray outerPad(blockSize, '\0');
    for (int i = 0; i < blockSize; ++i) {
        const uchar byte = static_cast<uchar>(paddedKey.at(i));
        innerPad[i] = static_cast<char>(byte ^ 0x36);
        outerPad[i] = static_cast<char>(byte ^ 0x5c);
    }

    const QByteArray innerHash = QCryptographicHash::hash(innerPad + message, algorithm);
    return QCryptographicHash::hash(outerPad + innerHash, algorithm);
}

QString Totp::generateCode(const QString &secret, int digits, int period)
{
    QByteArray key = base32Decode(secret);
    if (key.isEmpty()) return QString();
    if (period <= 0) period = 30;
    if (digits != 6 && digits != 8) digits = 6;

    // QDateTime::toSecsSinceEpoch() only exists since Qt 5.8; Sailfish OS
    // still ships Qt 5.6.
    quint64 counter = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch() / 1000)
                      / static_cast<quint64>(period);
    return generateTotp(key, counter, digits);
}

int Totp::remainingSeconds(int period)
{
    if (period <= 0) period = 30;
    qint64 seconds = QDateTime::currentMSecsSinceEpoch() / 1000;
    return period - static_cast<int>(seconds % period);
}

bool Totp::validateSecret(const QString &secret)
{
    static const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    QString cleaned = secret.toUpper();
    cleaned.remove('=');
    cleaned.remove(' ');
    cleaned.remove('-');
    cleaned.remove('\t');

    if (cleaned.isEmpty()) return false;

    // Strict where base32Decode() is lenient: a secret that is shorter than
    // one Base32 block, or that contains anything outside the alphabet,
    // means the user typed something that is not a key at all.
    for (const QChar &c : cleaned) {
        if (alphabet.indexOf(c) < 0) return false;
    }

    return cleaned.length() >= 8;
}
