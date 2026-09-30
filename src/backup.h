#ifndef BACKUP_H
#define BACKUP_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class AccountModel;

/*
 * Export of the account list, in two shapes.
 *
 * The encrypted backup stretches a passphrase with PBKDF2-HMAC-SHA256
 * (200k rounds) and seals the accounts JSON with AES-256-GCM, so a wrong
 * passphrase — or any edit to the file, including its header — fails
 * authentication instead of decrypting to garbage. The header fields
 * (format, version, iterations, salt, IV) are fed in as GCM additional
 * authenticated data, which is what makes a tampered KDF iteration count
 * detectable. The file itself is a plain JSON envelope with base64
 * salt/IV/tag/ciphertext, named SailOTP-backup-<stamp>.enc.
 *
 * The plain-text export writes one otpauth:// URI per line, named
 * SailOTP-export-<stamp>.txt — the format GNOME Authenticator restores
 * under "Authenticator" (FreeOTP+ compatible, text/plain) and other
 * authenticator apps read directly. It carries no protection beyond the
 * device, and the page that writes it says so.
 *
 * Both land in ~/Documents with owner-only permissions.
 *
 * All crypto primitives come from OpenSSL's libcrypto (libcrypto.so.3),
 * which the Harbour allowed-library list explicitly permits. The
 * Q_INVOKABLE methods are the QML-facing wrappers around the statics,
 * which do all the work and take a plain account list — so the tests can
 * drive them without a model or a database.
 */
class Backup : public QObject
{
    Q_OBJECT
public:
    explicit Backup(QObject *parent = nullptr);

    void setAccountModel(AccountModel *model);

    // Target path for the next export: ~/Documents/SailOTP-backup-<stamp>.enc
    Q_INVOKABLE QString defaultBackupPath() const;

    // Target path for the next plain-text export:
    // ~/Documents/SailOTP-export-<stamp>.txt
    Q_INVOKABLE QString defaultTextExportPath() const;

    // "" on success, otherwise a message fit for the UI.
    Q_INVOKABLE QString saveBackup(const QString &filePath,
                                   const QString &passphrase);
    Q_INVOKABLE QString saveTextExport(const QString &filePath);

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
    static QString writeTextExport(const QString &filePath,
                                   const QVariantList &accounts);

private:
    static QByteArray deriveKey(const QString &passphrase,
                                const QByteArray &salt, int iterations);
    static QByteArray randomBytes(int count);
    static QByteArray headerAad(int iterations, const QByteArray &salt,
                                const QByteArray &iv);
    static QString writeDocument(const QString &filePath,
                                 const QByteArray &content);
    QVariantList modelAccounts() const;

    AccountModel *m_model;
};

#endif // BACKUP_H
