#include "backup.h"
#include "accountmodel.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

namespace {
const char *kFormat = "harbour-sailotp-backup";
const int kVersion = 1;
const int kIterations = 200000; // PBKDF2-HMAC-SHA256 rounds (~0.1 s on the phone)
const int kKeyBytes = 32;       // AES-256
const int kSaltBytes = 16;
const int kIvBytes = 12;        // GCM's standard 96-bit nonce
const int kTagBytes = 16;
} // namespace

Backup::Backup(QObject *parent)
    : QObject(parent)
    , m_model(nullptr)
{
}

void Backup::setAccountModel(AccountModel *model)
{
    m_model = model;
}

QByteArray Backup::deriveKey(const QString &passphrase, const QByteArray &salt,
                             int iterations)
{
    const QByteArray pass = passphrase.toUtf8();
    QByteArray key(kKeyBytes, '\0');
    PKCS5_PBKDF2_HMAC(pass.constData(), pass.size(),
                      reinterpret_cast<const unsigned char *>(salt.constData()),
                      salt.size(), iterations, EVP_sha256(), key.size(),
                      reinterpret_cast<unsigned char *>(key.data()));
    return key;
}

QByteArray Backup::randomBytes(int count)
{
    QByteArray bytes(count, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char *>(bytes.data()), count) != 1)
        return QByteArray();
    return bytes;
}

/*
 * Additional authenticated data for the envelope header: a tampered
 * iteration count, salt or IV must break the GCM tag rather than just
 * steer the key derivation. NUL separators keep the fields unambiguous
 * (salt and IV are raw bytes and may contain anything).
 */
QByteArray Backup::headerAad(int iterations, const QByteArray &salt,
                             const QByteArray &iv)
{
    QByteArray aad(kFormat, int(qstrlen(kFormat)));
    aad += '\0' + QByteArray::number(kVersion);
    aad += '\0' + QByteArray::number(iterations);
    aad += '\0' + salt;
    aad += '\0' + iv;
    return aad;
}

QString Backup::defaultBackupPath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + QStringLiteral("/Documents");
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    return dir + QStringLiteral("/SailOTP-backup-") + stamp + QStringLiteral(".enc");
}

QString Backup::saveBackup(const QString &filePath, const QString &passphrase)
{
    if (!m_model)
        return QStringLiteral("No accounts loaded");

    QVariantList accounts;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QModelIndex idx = m_model->index(row, 0);
        QVariantMap entry;
        entry.insert(QStringLiteral("issuer"),
                     m_model->data(idx, AccountModel::IssuerRole).toString());
        entry.insert(QStringLiteral("name"),
                     m_model->data(idx, AccountModel::NameRole).toString());
        entry.insert(QStringLiteral("secret"),
                     m_model->data(idx, AccountModel::SecretRole).toString());
        entry.insert(QStringLiteral("digits"),
                     m_model->data(idx, AccountModel::DigitsRole).toInt());
        entry.insert(QStringLiteral("period"),
                     m_model->data(idx, AccountModel::PeriodRole).toInt());
        accounts.append(entry);
    }
    return writeBackup(filePath, passphrase, accounts);
}

QVariantMap Backup::openBackup(const QString &filePath, const QString &passphrase)
{
    return readBackup(filePath, passphrase);
}

QString Backup::writeBackup(const QString &filePath, const QString &passphrase,
                            const QVariantList &accounts)
{
    if (passphrase.isEmpty())
        return QStringLiteral("Enter a passphrase");

    // Payload: only the fields a restore needs.
    QJsonArray accountArray;
    for (const QVariant &var : accounts) {
        const QVariantMap entry = var.toMap();
        if (entry.value(QStringLiteral("secret")).toString().isEmpty())
            continue;
        QJsonObject object;
        object.insert(QStringLiteral("issuer"),
                      entry.value(QStringLiteral("issuer")).toString());
        object.insert(QStringLiteral("name"),
                      entry.value(QStringLiteral("name")).toString());
        object.insert(QStringLiteral("secret"),
                      entry.value(QStringLiteral("secret")).toString());
        object.insert(QStringLiteral("digits"),
                      entry.value(QStringLiteral("digits")).toInt());
        object.insert(QStringLiteral("period"),
                      entry.value(QStringLiteral("period")).toInt());
        accountArray.append(object);
    }
    if (accountArray.isEmpty())
        return QStringLiteral("There are no accounts to back up");

    QJsonObject payloadObject;
    payloadObject.insert(QStringLiteral("accounts"), accountArray);
    const QByteArray plaintext =
            QJsonDocument(payloadObject).toJson(QJsonDocument::Compact);

    const QByteArray salt = randomBytes(kSaltBytes);
    const QByteArray iv = randomBytes(kIvBytes);
    if (salt.isEmpty() || iv.isEmpty())
        return QStringLiteral("Could not read random data");
    const QByteArray key = deriveKey(passphrase, salt, kIterations);
    const QByteArray aad = headerAad(kIterations, salt, iv);

    QByteArray ciphertext(plaintext.size(), '\0');
    QByteArray tag(kTagBytes, '\0');

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return QStringLiteral("Out of memory");
    int len = 0;
    int total = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
            && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) == 1
            && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                                  reinterpret_cast<const unsigned char *>(key.constData()),
                                  reinterpret_cast<const unsigned char *>(iv.constData())) == 1
            && EVP_EncryptUpdate(ctx, nullptr, &len,
                                 reinterpret_cast<const unsigned char *>(aad.constData()),
                                 aad.size()) == 1
            && EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(ciphertext.data()),
                                 &len,
                                 reinterpret_cast<const unsigned char *>(plaintext.constData()),
                                 plaintext.size()) == 1;
    total = len;
    if (ok)
        ok = EVP_EncryptFinal_ex(ctx,
                                 reinterpret_cast<unsigned char *>(ciphertext.data() + total),
                                 &len) == 1;
    total += len; // GCM without padding adds nothing here
    if (ok)
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagBytes,
                                 reinterpret_cast<unsigned char *>(tag.data())) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return QStringLiteral("Encryption failed");
    ciphertext.resize(total);

    QJsonObject kdfObject;
    kdfObject.insert(QStringLiteral("name"), QStringLiteral("pbkdf2-hmac-sha256"));
    kdfObject.insert(QStringLiteral("iterations"), kIterations);
    kdfObject.insert(QStringLiteral("salt"), QString::fromLatin1(salt.toBase64()));

    QJsonObject cipherObject;
    cipherObject.insert(QStringLiteral("name"), QStringLiteral("aes-256-gcm"));
    cipherObject.insert(QStringLiteral("iv"), QString::fromLatin1(iv.toBase64()));
    cipherObject.insert(QStringLiteral("tag"), QString::fromLatin1(tag.toBase64()));

    QJsonObject envelope;
    envelope.insert(QStringLiteral("format"), QLatin1String(kFormat));
    envelope.insert(QStringLiteral("version"), kVersion);
    envelope.insert(QStringLiteral("kdf"), kdfObject);
    envelope.insert(QStringLiteral("cipher"), cipherObject);
    envelope.insert(QStringLiteral("payload"), QString::fromLatin1(ciphertext.toBase64()));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QStringLiteral("Cannot write ") + filePath + QStringLiteral(": ")
                + file.errorString();
    const QByteArray document = QJsonDocument(envelope).toJson(QJsonDocument::Indented);
    const qint64 written = file.write(document);
    file.close();
    if (written != document.size())
        return QStringLiteral("Short write to ") + filePath + QStringLiteral(": ")
                + file.errorString();
    // The file holds secrets: owner-only.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return QString();
}

QVariantMap Backup::readBackup(const QString &filePath, const QString &passphrase)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), QString());
    result.insert(QStringLiteral("accounts"), QVariantList());

    auto fail = [&result](const QString &message) {
        result.insert(QStringLiteral("error"), message);
        return result;
    };

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Cannot read ") + filePath + QStringLiteral(": ")
                    + file.errorString());

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("Not a SailOTP backup file"));

    const QJsonObject envelope = document.object();
    if (envelope.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)
            || envelope.value(QStringLiteral("version")).toInt() != kVersion)
        return fail(QStringLiteral("Not a SailOTP backup file"));

    const QJsonObject kdf = envelope.value(QStringLiteral("kdf")).toObject();
    const QByteArray salt =
            QByteArray::fromBase64(kdf.value(QStringLiteral("salt")).toString().toLatin1());
    const QByteArray iv = QByteArray::fromBase64(
            envelope.value(QStringLiteral("cipher")).toObject()
                    .value(QStringLiteral("iv")).toString().toLatin1());
    QByteArray tag = QByteArray::fromBase64(
            envelope.value(QStringLiteral("cipher")).toObject()
                    .value(QStringLiteral("tag")).toString().toLatin1());
    const QByteArray ciphertext = QByteArray::fromBase64(
            envelope.value(QStringLiteral("payload")).toString().toLatin1());
    const int iterations = kdf.value(QStringLiteral("iterations")).toInt();

    // Sanity bounds — in particular no multi-hour KDF from a hostile file.
    if (salt.size() != kSaltBytes || iv.size() != kIvBytes || tag.size() != kTagBytes
            || ciphertext.isEmpty() || iterations < 1000 || iterations > 5000000)
        return fail(QStringLiteral("This does not look like a SailOTP backup"));

    const QByteArray key = deriveKey(passphrase, salt, iterations);
    const QByteArray aad = headerAad(iterations, salt, iv);

    QByteArray plaintext(ciphertext.size(), '\0');
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return fail(QStringLiteral("Out of memory"));
    int len = 0;
    int total = 0;
    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
            && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) == 1
            && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                                  reinterpret_cast<const unsigned char *>(key.constData()),
                                  reinterpret_cast<const unsigned char *>(iv.constData())) == 1
            && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagBytes,
                                   reinterpret_cast<unsigned char *>(tag.data())) == 1
            && EVP_DecryptUpdate(ctx, nullptr, &len,
                                 reinterpret_cast<const unsigned char *>(aad.constData()),
                                 aad.size()) == 1
            && EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(plaintext.data()),
                                 &len,
                                 reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                                 ciphertext.size()) == 1;
    total = len;
    if (ok)
        ok = EVP_DecryptFinal_ex(ctx,
                                 reinterpret_cast<unsigned char *>(plaintext.data() + total),
                                 &len) == 1;
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return fail(QStringLiteral("Wrong passphrase or damaged file"));
    plaintext.resize(total);

    QJsonParseError payloadError;
    const QJsonDocument payload =
            QJsonDocument::fromJson(plaintext, &payloadError);
    if (payloadError.error != QJsonParseError::NoError || !payload.isObject())
        return fail(QStringLiteral("Backup contents are damaged"));

    QVariantList accounts;
    const QJsonArray accountArray =
            payload.object().value(QStringLiteral("accounts")).toArray();
    for (const QJsonValue &value : accountArray) {
        const QJsonObject object = value.toObject();
        const QString secret = object.value(QStringLiteral("secret")).toString();
        if (secret.isEmpty())
            continue;

        int digits = object.value(QStringLiteral("digits")).toInt();
        if (digits < 4 || digits > 10)
            digits = 6;
        int period = object.value(QStringLiteral("period")).toInt();
        if (period < 5 || period > 300)
            period = 30;

        QVariantMap entry;
        entry.insert(QStringLiteral("issuer"),
                     object.value(QStringLiteral("issuer")).toString());
        entry.insert(QStringLiteral("name"),
                     object.value(QStringLiteral("name")).toString());
        entry.insert(QStringLiteral("secret"), secret);
        entry.insert(QStringLiteral("digits"), digits);
        entry.insert(QStringLiteral("period"), period);
        accounts.append(entry);
    }
    if (accounts.isEmpty())
        return fail(QStringLiteral("The backup contains no accounts"));

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("accounts"), accounts);
    return result;
}
