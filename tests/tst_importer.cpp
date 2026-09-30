/*
 * Import format check for src/importer.cpp.
 *
 * Every sample goes through Importer::parseFile() on a real temp file, so the
 * JSON-vs-text detection and the fallback to the line parser are covered as
 * well as the parsers themselves.
 *
 * importer.cpp references two Database symbols, stubbed below: parsing never
 * calls them (Importer only uses m_db in importAccounts(), which the tests do
 * not reach). Qt5Sql headers are still needed for database.h, but the library
 * is deliberately left off the link line — the build engine does not resolve
 * its runtime for a binary in /tmp.
 *
 * Run inside the Sailfish build engine (Qt 5.6 is what the app links against,
 * so a host build would not prove anything):
 *
 *   sfdk -c target=SailfishOS-5.1.0.11-i486 build-shell sh -c \
 *     'moc src/importer.h -o /tmp/moc_importer.cpp &&
 *      moc src/totp.h -o /tmp/moc_totp.cpp &&
 *      g++ -std=gnu++11 -fPIC $(pkg-config --cflags Qt5Core Qt5Sql) \
 *          src/importer.cpp src/totp.cpp \
 *          /tmp/moc_importer.cpp /tmp/moc_totp.cpp tests/tst_importer.cpp \
 *          -o /tmp/tst_importer $(pkg-config --libs Qt5Core) &&
 *      /tmp/tst_importer'
 */

#include "../src/database.h"
#include "../src/importer.h"

#include <QDir>
#include <QFile>
#include <QString>
#include <QVariantMap>
#include <cstdio>

// See the header comment: only importAccounts() would use these.
bool Database::accountExists(const QString &) { return false; }
bool Database::addAccount(const QString &, const QString &, const QString &, int, int)
{
    return false;
}

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
        std::printf("FAIL  %-32s got '%s', want '%s'\n", what,
                    qPrintable(got), qPrintable(want));
        ++failures;
    }
}

// Writes content to a temp file and returns the parsed accounts.
static QVariantList parse(Importer &importer, const char *name, const QString &content)
{
    const QString path = QDir::tempPath() + QLatin1String("/tst_importer_") + name;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::printf("FAIL  %-32s could not write %s\n", "setup", qPrintable(path));
        ++failures;
        return QVariantList();
    }
    file.write(content.toUtf8());
    file.close();

    return importer.parseFile(path);
}

static QString field(const QVariantList &list, int index, const char *key)
{
    if (index < 0 || index >= list.count()) return QString();
    return list.at(index).toMap().value(QLatin1String(key)).toString();
}

static int intField(const QVariantList &list, int index, const char *key)
{
    if (index < 0 || index >= list.count()) return -1;
    return list.at(index).toMap().value(QLatin1String(key)).toInt();
}

int main()
{
    Importer importer(nullptr);

    // --- plain text, one URI per line, with junk around them ---
    const QVariantList text = parse(importer, "text.txt",
        "otpauth://totp/GitHub:alice@example.com?secret=JBSWY3DPEHPK3PXP"
        "&issuer=GitHub&digits=6&period=30\n"
        "not an otpauth line\n"
        "- otpauth://totp/Example:bob@example.com?secret=mfrggzdfmztwq2lk==\n");
    expectInt("text: account count", text.count(), 2);
    expectStr("text: issuer", field(text, 0, "issuer"), "GitHub");
    expectStr("text: name", field(text, 0, "name"), "alice@example.com");
    expectStr("text: secret uppercased", field(text, 1, "secret"), "MFRGGZDFMZTWQ2LK");

    // --- an encoded colon (%3A) inside the issuer half is not the separator,
    //     and a bare account name with the issuer in the query (the shape
    //     GNOME Authenticator writes and reads) parses as-is ---
    const QVariantList encoded = parse(importer, "encoded.txt",
        "otpauth://totp/ACME%3AX:carol?secret=JBSWY3DPEHPK3PXP\n"
        "otpauth://totp/alice%40example.com?secret=JBSWY3DPEHPK3PXP"
        "&issuer=GitHub&algorithm=SHA1&digits=6&period=30\n"
        "otpauth://totp/v1%2E2%5Fbeta%2Dx%7Ey?secret=JBSWY3DPEHPK3PXP"
        "&issuer=Git%20Hub&algorithm=SHA1&digits=6&period=30\n");
    expectInt("encoded colon: count", encoded.count(), 3);
    expectStr("encoded colon: issuer", field(encoded, 0, "issuer"), "ACME:X");
    expectStr("encoded colon: name", field(encoded, 0, "name"), "carol");
    expectStr("bare path: name", field(encoded, 1, "name"), "alice@example.com");
    expectStr("bare path: issuer from query", field(encoded, 1, "issuer"), "GitHub");
    // NON_ALPHANUMERIC-encoded label and issuer, as GNOME Authenticator
    // exports them, decode back to the original characters.
    expectStr("fully encoded: name", field(encoded, 2, "name"), "v1.2_beta-x~y");
    expectStr("fully encoded: issuer", field(encoded, 2, "issuer"), "Git Hub");

    // --- bracketed text that only looks like JSON ---
    const QVariantList bracketed = parse(importer, "bracketed.txt",
        "[\n"
        "otpauth://totp/Example:dave?secret=JBSWY3DPEHPK3PXP\n"
        "]\n");
    expectInt("bracketed text still parses", bracketed.count(), 1);
    expectStr("bracketed text: name", field(bracketed, 0, "name"), "dave");

    // --- andOTP / GNOME Authenticator shape ---
    const QVariantList andotp = parse(importer, "andotp.json",
        "[\n"
        " {\"type\":\"TOTP\",\"label\":\"alice@example.com\",\"issuer\":\"GitHub\","
        "\"secret\":\"JBSWY3DPEHPK3PXP\",\"algorithm\":\"SHA1\",\"digits\":6,\"period\":30},\n"
        " {\"type\":\"HOTP\",\"label\":\"counter\",\"issuer\":\"Old\","
        "\"secret\":\"JBSWY3DPEHPK3PXP\",\"algorithm\":\"SHA1\",\"digits\":6,\"counter\":5},\n"
        " {\"type\":\"TOTP\",\"label\":\"sha256\",\"issuer\":\"Other\","
        "\"secret\":\"JBSWY3DPEHPK3PXP\",\"algorithm\":\"SHA256\",\"digits\":6,\"period\":30},\n"
        " {\"type\":\"TOTP\",\"label\":\"Work:bob@example.com\","
        "\"secret\":\"JBSWY3DPEHPK3PXP\",\"algorithm\":\"SHA1\",\"digits\":8,\"period\":60}\n"
        "]\n");
    expectInt("andotp: HOTP and SHA256 skipped", andotp.count(), 2);
    expectStr("andotp: issuer", field(andotp, 0, "issuer"), "GitHub");
    expectStr("andotp: label with colon split", field(andotp, 1, "issuer"), "Work");
    expectStr("andotp: name after split", field(andotp, 1, "name"), "bob@example.com");
    expectInt("andotp: digits 8 kept", intField(andotp, 1, "digits"), 8);
    expectInt("andotp: period kept", intField(andotp, 1, "period"), 60);

    // --- Aegis shape: secrets nested under info ---
    const QVariantList aegis = parse(importer, "aegis.json",
        "{\"db\":{\"version\":1,\"entries\":["
        "{\"type\":\"totp\",\"name\":\"alice@example.com\",\"issuer\":\"GitHub\","
        "\"uuid\":\"a\",\"info\":{\"secret\":\"JBSWY3DPEHPK3PXP\",\"algo\":\"SHA1\","
        "\"digits\":6,\"period\":30}},"
        "{\"type\":\"steam\",\"name\":\"steam\",\"issuer\":\"Steam\","
        "\"uuid\":\"b\",\"info\":{\"secret\":\"JBSWY3DPEHPK3PXP\",\"algo\":\"SHA1\"}},"
        "{\"type\":\"totp\",\"issuer\":\"\",\"name\":\"carol\","
        "\"uuid\":\"c\",\"info\":{\"secret\":\" jbswy3dpehpk3pxp= \",\"algo\":\"SHA1\","
        "\"digits\":8,\"period\":60}}"
        "]}}");
    expectInt("aegis: steam entry skipped", aegis.count(), 2);
    expectStr("aegis: issuer", field(aegis, 0, "issuer"), "GitHub");
    expectStr("aegis: name", field(aegis, 0, "name"), "alice@example.com");
    expectStr("aegis: secret normalised", field(aegis, 1, "secret"), "JBSWY3DPEHPK3PXP");
    expectInt("aegis: digits", intField(aegis, 1, "digits"), 8);
    expectInt("aegis: period", intField(aegis, 1, "period"), 60);

    // --- FreeOTP shape: the account is behind a "url" key ---
    const QVariantList freeotp = parse(importer, "freeotp.json",
        "[{\"algo\":\"SHA1\",\"digits\":6,\"issuer\":\"Example\","
        "\"key\":\"JBSWY3DPEHPK3PXP\",\"period\":30,\"type\":\"TOTP\","
        "\"url\":\"otpauth://totp/Example:carol@example.com?secret=JBSWY3DPEHPK3PXP"
        "&issuer=Example\"}]\n");
    expectInt("freeotp: found via url", freeotp.count(), 1);
    expectStr("freeotp: issuer", field(freeotp, 0, "issuer"), "Example");
    expectStr("freeotp: name", field(freeotp, 0, "name"), "carol@example.com");

    // --- an export with no usable secrets (encrypted Aegis) ---
    const QVariantList encrypted = parse(importer, "aegis-encrypted.json",
        "{\"version\":3,\"header\":{\"encrypted\":true,"
        "\"params\":{\"nonce\":\"abc\",\"tag\":\"def\"}},"
        "\"db\":\"0123456789abcdef\"}");
    expectInt("encrypted aegis: nothing imported", encrypted.count(), 0);

    // --- malformed JSON falls back to the line parser, which finds nothing ---
    const QVariantList broken = parse(importer, "broken.json", "{\"db\": {\"entries\": [");
    expectInt("malformed json: nothing imported", broken.count(), 0);

    // --- secrets too short to be a real key are rejected everywhere ---
    const QVariantList weak = parse(importer, "weak.txt",
        "otpauth://totp/Example:weak?secret=ABCDEF2\n");
    expectInt("short secret rejected", weak.count(), 0);

    if (failures == 0)
        std::printf("\nAll importer checks passed\n");
    else
        std::printf("\nFAILED: %d check(s)\n", failures);

    return failures == 0 ? 0 : 1;
}
