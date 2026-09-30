#ifndef BACKUP_H
#define BACKUP_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class AccountModel;

/*
 * Encrypted backup of the account list.
 *
 * A passphrase is stretched with PBKDF2-HMAC-SHA256 (200k rounds) and the
 * accounts JSON is sealed with AES-256-GCM, so a wrong passphrase — or any
 * edit to the file, including its header — fails authentication instead of
 * decrypting to garbage. The header fields (format, version, iterations,
 * salt, IV) are fed in as GCM additional authenticated data, which is what
 * makes a tampered KDF iteration count detectable.
 *
 * The file itself is a plain JSON envelope with base64 salt/IV/tag/
 * ciphertext, named SailOTP-backup-<stamp>.enc and written to ~/Documents
 * with owner-only permissions.
 *
 * All primitives come from OpenSSL's libcrypto (libcrypto.so.3), which the
 * Harbour allowed-library list explicitly permits. The Q_INVOKABLE methods
 * are the QML-facing wrappers around the two statics, which do all the work
 * and take a plain account list — so the tests can drive them without a
 * model or a database.
 */
class Backup : public QObject
{
    Q_OBJECT
public:
    explicit Backup(QObject *parent = nullptr);

    void setAccountModel(AccountModel *model);

    // Target path for the next export: ~/Documents/SailOTP-backup-<stamp>.enc
    Q_INVOKABLE QString defaultBackupPath() const;

    // "" on success, otherwise a message fit for the UI.
    Q_INVOKABLE QString saveBackup(const QString &filePath,
                                   const QString &passphrase);

    // { "ok": bool, "error": string, "accounts": [ {issuer, name, secret,
    // digits, period}, ... ] } — "accounts" is only meaningful when ok.
    Q_INVOKABLE QVariantMap openBackup(const QString &filePath,
                                       const QString &passphrase);

    // Core, model-free entry points (used by the wrappers above and by
    // tests/tst_backup.cpp). accounts entries are maps with issuer, name,
    // secret, digits, period.
    static QString writeBackup(const QString &filePath,
                               const QString &passphrase,
                               const QVariantList &accounts);
    static QVariantMap readBackup(const QString &filePath,
                                  const QString &passphrase);

private:
    static QByteArray deriveKey(const QString &passphrase,
                                const QByteArray &salt, int iterations);
    static QByteArray randomBytes(int count);
    static QByteArray headerAad(int iterations, const QByteArray &salt,
                                const QByteArray &iv);

    AccountModel *m_model;
};

#endif // BACKUP_H
