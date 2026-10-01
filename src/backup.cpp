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
const char *kFormat = "harbour-sailauth-backup";
const int kVersion = 1;
const int kIterations = 200000; // PBKDF2-HMAC-SHA256 rounds (~0.1 s on the phone)
const int kKeyBytes = 32;       // AES-256
const int kSaltBytes = 16;
const int kIvBytes = 12;        // GCM's standard 96-bit nonce
const int kTagBytes = 16;

// ~/Documents/<prefix><timestamp><suffix>, the folder the Files app shows.
QString documentPath(const QString &prefix, const QString &suffix)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + QStringLiteral("/Documents");
    const QString stamp =
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    return dir + QLatin1Char('/') + prefix + stamp + suffix;
}

/*
 * Percent-encode every byte outside A–Z a–z 0–9, uppercase hex — the
 * exact set GNOME Authenticator applies to label and issuer when it
 * writes this format (percent-encoding's NON_ALPHANUMERIC). QUrl's
 * default would leave the RFC 3986 unreserved "-._~" raw; both decode
 * identically, but encoding everything makes our lines byte-for-byte
 * like GNOME's own exports, and a fully encoded component contains
 * nothing any parser could split on.
 */
QByteArray encodeComponent(const QString &text)
{
    static const char kHex[] = "0123456789ABCDEF";
    const QByteArray utf8 = text.toUtf8();
    QByteArray encoded;
    encoded.reserve(utf8.size() * 3);
    const char *bytes = utf8.constData();
    for (int i = 0; i < utf8.size(); ++i) {
        const unsigned char byte = static_cast<unsigned char>(bytes[i]);
        if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
                || (byte >= '0' && byte <= '9')) {
            encoded += char(byte);
        } else {
            encoded += '%';
            encoded += kHex[byte >> 4];
            encoded += kHex[byte & 0x0F];
        }
    }
    return encoded;
}

/*
 * One account as a single otpauth:// URI — the same shape and the same
 * encoding GNOME Authenticator's own writer produces (account name and
 * issuer percent-encoded with NON_ALPHANUMERIC, secret raw, parameter
 * order secret, issuer, algorithm, digits, period), which its
 * line-based restore parses leniently and every other authenticator
 * understands. The files diff cleanly against one another.
 */
QString otpauthLine(const QVariantMap &entry)
{
    // Normalise the way importers expect: base32 upper-case without
    // padding, spaces or dashes — the same pass Importer applies on read,
    // so a padded or lower-cased secret round-trips identically.
    QString secret = entry.value(QStringLiteral("secret")).toString().toUpper();
    secret.remove(QLatin1Char('='));
    secret.remove(QLatin1Char(' '));
    secret.remove(QLatin1Char('-'));

    QString issuer = entry.value(QStringLiteral("issuer")).toString().trimmed();
    QString name = entry.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
        name = issuer;
    if (name.isEmpty())
        name = QStringLiteral("account");

    int digits = entry.value(QStringLiteral("digits")).toInt();
    if (digits < 4 || digits > 10)
        digits = 6;
    int period = entry.value(QStringLiteral("period")).toInt();
    if (period < 5 || period > 300)
        period = 30;

    QString uri = QStringLiteral("otpauth://totp/")
            + QString::fromLatin1(encodeComponent(name))
            + QStringLiteral("?secret=") + secret;
    if (!issuer.isEmpty())
        uri += QStringLiteral("&issuer=")
                + QString::fromLatin1(encodeComponent(issuer));
    uri += QStringLiteral("&algorithm=SHA1")
            + QStringLiteral("&digits=") + QString::number(digits)
            + QStringLiteral("&period=") + QString::number(period);
    return uri;
}
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
    return documentPath(QStringLiteral("SailAuth-backup-"), QStringLiteral(".enc"));
}

QString Backup::defaultTextExportPath() const
{
    return documentPath(QStringLiteral("SailAuth-export-"), QStringLiteral(".txt"));
}

QVariantList Backup::modelAccounts() const
{
    QVariantList accounts;
    if (!m_model)
        return accounts;
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
    return accounts;
}

QString Backup::saveBackup(const QString &filePath, const QString &passphrase)
{
    if (!m_model)
        return QStringLiteral("No accounts loaded");
    return writeBackup(filePath, passphrase, modelAccounts());
}

QString Backup::saveTextExport(const QString &filePath)
{
    if (!m_model)
        return QStringLiteral("No accounts loaded");
    return writeTextExport(filePath, modelAccounts());
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

    return writeDocument(filePath,
                         QJsonDocument(envelope).toJson(QJsonDocument::Indented));
}

QString Backup::writeTextExport(const QString &filePath,
                                const QVariantList &accounts)
{
    QByteArray document;
    int exported = 0;
    for (const QVariant &var : accounts) {
        const QVariantMap entry = var.toMap();
        if (entry.value(QStringLiteral("secret")).toString().isEmpty())
            continue;
        document += otpauthLine(entry).toUtf8();
        document += '\n';
        ++exported;
    }
    if (exported == 0)
        return QStringLiteral("There are no accounts to export");

    return writeDocument(filePath, document);
}

QString Backup::writeDocument(const QString &filePath, const QByteArray &content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QStringLiteral("Cannot write ") + filePath + QStringLiteral(": ")
                + file.errorString();
    const qint64 written = file.write(content);
    file.close();
    if (written != content.size())
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
        return fail(QStringLiteral("Not a SailAuth backup file"));

    const QJsonObject envelope = document.object();
    if (envelope.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)
            || envelope.value(QStringLiteral("version")).toInt() != kVersion)
        return fail(QStringLiteral("Not a SailAuth backup file"));

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
        return fail(QStringLiteral("This does not look like a SailAuth backup"));

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
