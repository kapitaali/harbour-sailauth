/*
 * Encrypted backup check for src/backup.cpp.
 *
 * Drives the two static entry points directly (writeBackup/readBackup), so
 * no model, no database and no SQLite driver are involved: backup.cpp only
 * reaches AccountModel through virtual dispatch, which never becomes a link
 * symbol, and database.h needs Qt5Sql headers but not the library — the
 * same arrangement as tst_importer.cpp.
 *
 * Covers the round trip, wrong passphrase, tampered ciphertext, tampered
 * KDF iteration count (the envelope header is GCM additional authenticated
 * data — this is the check that proves a KDF downgrade attempt fails
 * authentication), garbage envelopes, empty input, clamping of hostile
 * digit/period values and the owner-only file mode.
 *
 * Run inside the Sailfish build engine (Qt 5.6 is what the app links
 * against, so a host build would not prove anything):
 *
 *   sfdk -c target=SailfishOS-5.1.0.11-i486 build-shell sh -c \
 *     'moc src/backup.h -o /tmp/moc_backup.cpp &&
 *      g++ -std=gnu++11 -fPIC $(pkg-config --cflags Qt5Core Qt5Sql) \
 *          src/backup.cpp /tmp/moc_backup.cpp tests/tst_backup.cpp \
 *          -o /tmp/tst_backup $(pkg-config --libs Qt5Core) -lcrypto &&
 *      /tmp/tst_backup'
 */

#include "../src/backup.h"

#include <QFile>
#include <QFileDevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdio>

static int failures = 0;

static void expectInt(const char *what, int got, int want)
{
    if (got == want) {
        std::printf("ok    %-32s %d\n", what, got);
    } else {
        std::printf("FAIL  %-32s got %d, want %d\n", what, got, want);
        ++failures;
    }
}

static void expectStr(const char *what, const QString &got, const QString &want)
{
    if (got == want) {
        std::printf("ok    %-32s %s\n", what, qPrintable(got));
    } else {
        std::printf("FAIL  %-32s got \"%s\", want \"%s\"\n", what,
                    qPrintable(got), qPrintable(want));
        ++failures;
    }
}

static void expectBool(const char *what, bool got, bool want)
{
    if (got == want) {
        std::printf("ok    %-32s %s\n", what, got ? "true" : "false");
    } else {
        std::printf("FAIL  %-32s got %s, want %s\n", what,
                    got ? "true" : "false", want ? "true" : "false");
        ++failures;
    }
}

static QVariantMap account(const QString &issuer, const QString &name,
                           const QString &secret, int digits, int period)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("issuer"), issuer);
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("secret"), secret);
    entry.insert(QStringLiteral("digits"), digits);
    entry.insert(QStringLiteral("period"), period);
    return entry;
}

// Rebuild an envelope file with a mutated object (for tamper tests).
static bool writeEnvelope(const QString &path, const QJsonObject &envelope)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    return true;
}

static QJsonObject readEnvelope(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QJsonObject();
    return QJsonDocument::fromJson(file.readAll()).object();
}

int main()
{
    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::printf("FAIL  temp dir\n");
        return 1;
    }
    const QString path = dir.path() + "/backup.enc";

    QVariantList accounts;
    accounts.append(account("GitHub", "alice", "JBSWY3DPEHPK3PXP", 6, 30));
    accounts.append(account("Example", "bob@example.org", "MFRGGZDFMZTWQ2LK", 8, 60));

    // --- round trip ---
    expectStr("write succeeds", Backup::writeBackup(path, "hunter2", accounts), QString());
    const QVariantMap first = Backup::readBackup(path, "hunter2");
    expectBool("round trip ok", first.value("ok").toBool(), true);
    const QVariantList got = first.value("accounts").toList();
    expectInt("round trip count", got.count(), 2);
    expectStr("issuer", got.value(0).toMap().value("issuer").toString(), "GitHub");
    expectStr("name", got.value(0).toMap().value("name").toString(), "alice");
    expectStr("secret", got.value(0).toMap().value("secret").toString(),
              "JBSWY3DPEHPK3PXP");
    expectInt("digits", got.value(0).toMap().value("digits").toInt(), 6);
    expectInt("second digits", got.value(1).toMap().value("digits").toInt(), 8);
    expectInt("second period", got.value(1).toMap().value("period").toInt(), 60);

    // --- wrong passphrase is indistinguishable from damage, by design ---
    const QVariantMap wrong = Backup::readBackup(path, "nope");
    expectBool("wrong passphrase rejected", !wrong.value("ok").toBool(), true);
    expectStr("wrong passphrase message", wrong.value("error").toString(),
              "Wrong passphrase or damaged file");

    // --- input validation on write ---
    expectStr("empty passphrase refused", Backup::writeBackup(path, "", accounts),
              "Enter a passphrase");
    expectStr("no accounts refused", Backup::writeBackup(path, "x", QVariantList()),
              "There are no accounts to back up");
    QVariantList secretless;
    secretless.append(account("A", "B", "", 6, 30));
    expectStr("secretless entries skipped", Backup::writeBackup(path, "x", secretless),
              "There are no accounts to back up");

    // --- the file must not be group/world readable ---
    // POSIX has one owner bit, Qt exposes Owner and User as separate flags
    // and reports both for it, so 0600 comes back as Owner|User read+write.
    const QFileDevice::Permissions perms = QFile::permissions(path);
    expectBool("owner-only file mode",
               perms == (QFileDevice::ReadOwner | QFileDevice::WriteOwner
                         | QFileDevice::ReadUser | QFileDevice::WriteUser), true);

    // --- a flipped ciphertext byte fails authentication ---
    QJsonObject envelope = readEnvelope(path);
    expectBool("envelope is an object", !envelope.isEmpty(), true);
    {
        QByteArray payload =
                QByteArray::fromBase64(envelope.value("payload").toString().toLatin1());
        payload[0] = char(payload.at(0) ^ 0xFF);
        envelope.insert("payload", QString::fromLatin1(payload.toBase64()));
        const QString tamperedPath = dir.path() + "/tampered-payload.enc";
        expectBool("tampered file written", writeEnvelope(tamperedPath, envelope), true);
        const QVariantMap result = Backup::readBackup(tamperedPath, "hunter2");
        expectBool("tampered payload rejected", !result.value("ok").toBool(), true);
        expectStr("tampered payload message", result.value("error").toString(),
                  "Wrong passphrase or damaged file");
    }

    // --- a tampered KDF iteration count fails too: the header is AAD ---
    envelope = readEnvelope(path);
    {
        QJsonObject kdf = envelope.value("kdf").toObject();
        kdf.insert("iterations", 1000);
        envelope.insert("kdf", kdf);
        const QString downgradedPath = dir.path() + "/downgraded.enc";
        expectBool("downgrade file written", writeEnvelope(downgradedPath, envelope), true);
        const QVariantMap result = Backup::readBackup(downgradedPath, "hunter2");
        expectBool("iteration downgrade rejected", !result.value("ok").toBool(), true);
        expectStr("downgrade message", result.value("error").toString(),
                  "Wrong passphrase or damaged file");
    }

    // --- non-backup files produce a clear message ---
    const QString junkPath = dir.path() + "/junk.txt";
    {
        QFile junk(junkPath);
        junk.open(QIODevice::WriteOnly);
        junk.write("just some notes\n");
        junk.close();
    }
    expectStr("plain text rejected", Backup::readBackup(junkPath, "x")
              .value("error").toString(), "Not a SailOTP backup file");
    expectBool("missing file message",
               Backup::readBackup(path + ".nope", "x").value("error")
                       .toString().startsWith("Cannot read "), true);

    // --- hostile digits/period are clamped rather than stored ---
    {
        QVariantList odd;
        odd.append(account("Odd", "o", "JBSWY3DPEHPK3PXP", 99, 1));
        const QString oddPath = dir.path() + "/odd.enc";
        expectStr("odd values written", Backup::writeBackup(oddPath, "x", odd), QString());
        const QVariantMap result = Backup::readBackup(oddPath, "x");
        expectBool("odd values readable", result.value("ok").toBool(), true);
        const QVariantMap entry = result.value("accounts").toList().value(0).toMap();
        expectInt("digits clamped", entry.value("digits").toInt(), 6);
        expectInt("period clamped", entry.value("period").toInt(), 30);
    }

    if (failures == 0)
        std::printf("\nAll backup checks passed\n");
    else
        std::printf("\nFAILED: %d check(s)\n", failures);

    return failures == 0 ? 0 : 1;
}
