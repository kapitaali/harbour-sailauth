/*
 * RFC 6238 conformance check for src/totp.cpp.
 *
 * Run inside the Sailfish build engine (Qt 5.6 is what the app links
 * against, so a host build would not prove anything):
 *
 *   sfdk -c target=SailfishOS-5.1.0.11-i486 build-shell sh -c \
 *     'moc src/totp.h -o /tmp/moc_totp.cpp &&
 *      g++ -std=gnu++11 -fPIC $(pkg-config --cflags Qt5Core) \
 *          src/totp.cpp /tmp/moc_totp.cpp tests/tst_totp.cpp \
 *          -o /tmp/tst_totp $(pkg-config --libs Qt5Core) && /tmp/tst_totp'
 */

#include "../src/totp.h"

#include <QByteArray>
#include <QString>
#include <cstdio>

static int failures = 0;

static void expect(const char *what, const QString &got, const QString &want)
{
    if (got == want) {
        std::printf("ok    %-28s %s\n", what, qPrintable(got));
    } else {
        std::printf("FAIL  %-28s got %s, want %s\n", what,
                    qPrintable(got), qPrintable(want));
        ++failures;
    }
}

static void expectBool(const char *what, bool ok)
{
    std::printf("%s  %-28s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++failures;
}

int main()
{
    // RFC 6238 Appendix B: the SHA1 test seed is the ASCII string
    // "12345678901234567890" (the RFC's SHA256/SHA512 seeds are longer, and
    // those algorithms are out of scope here).
    const QByteArray key("12345678901234567890");

    // 8-digit SHA1 vectors, T (seconds) -> counter = floor(T / 30).
    expect("T=59",        Totp::generateTotp(key, 59ULL / 30, 8),          "94287082");
    expect("T=1111111109", Totp::generateTotp(key, 1111111109ULL / 30, 8), "07081804");
    expect("T=1111111111", Totp::generateTotp(key, 1111111111ULL / 30, 8), "14050471");
    expect("T=1234567890", Totp::generateTotp(key, 1234567890ULL / 30, 8), "89005924");
    expect("T=2000000000", Totp::generateTotp(key, 2000000000ULL / 30, 8), "69279037");
    expect("T=20000000000", Totp::generateTotp(key, 20000000000ULL / 30, 8), "65353130");

    // Same counter, 6 digits (also the RFC 4226 HOTP vector for counter 1).
    expect("6 digits", Totp::generateTotp(key, 1, 6), "287082");

    // Base32 of the seed, canonical form and the forms real exports use.
    const QString canonical = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    expect("base32 decode",
           QString::fromLatin1(Totp::base32Decode(canonical)), "12345678901234567890");
    expect("base32 lowercase/padded",
           QString::fromLatin1(Totp::base32Decode("gezdgnbvgy3tqojqgezdgnbvgy3tqojq=")),
           "12345678901234567890");
    expect("base32 with spaces",
           QString::fromLatin1(Totp::base32Decode("GEZD GNBV GY3T QOJQ GEZD GNBV GY3T QOJQ")),
           "12345678901234567890");
    expect("base32 round trip",
           QString::fromLatin1(Totp::base32Decode(canonical)),
           QString::fromLatin1(key));

    // Validation is strict where decoding is lenient.
    Totp totp;
    expectBool("valid secret accepted", totp.validateSecret(canonical));
    expectBool("lowercase accepted", totp.validateSecret("gezdgnbvgy3tqojq"));
    expectBool("padded accepted", totp.validateSecret(canonical + "="));
    expectBool("empty rejected", !totp.validateSecret(""));
    expectBool("digits 0/1/8/9 rejected", !totp.validateSecret("0189018901890189"));
    expectBool("too short rejected", !totp.validateSecret("ABCD"));

    // Live generation: right length, right window, monotonic countdown.
    const QString live = totp.generateCode(canonical, 6, 30);
    expectBool("live code is 6 digits", live.length() == 6);
    bool digitsOnly = true;
    for (const QChar &c : live)
        if (!c.isDigit()) digitsOnly = false;
    expectBool("live code is numeric", digitsOnly);

    const int remaining = totp.remainingSeconds(30);
    expectBool("remaining in 1..30", remaining >= 1 && remaining <= 30);

    expectBool("bogus period tolerated",
               totp.generateCode(canonical, 6, 0).length() == 6);

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
