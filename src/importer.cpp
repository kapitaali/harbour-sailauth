#include "importer.h"
#include "database.h"
#include "totp.h"
#include <QFile>
#include <QTextStream>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QVariantMap>
#include <QDebug>

namespace {

// Backups spell the same fields differently: Aegis uses "algo" inside "info",
// andOTP uses "algorithm", FreeOTP uses "algo" at the top level.
int intValue(const QJsonObject &object, const char *key, int fallback)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isDouble()) return value.toInt();
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        if (ok) return parsed;
    }
    return fallback;
}

QString stringValue(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isString() ? value.toString().trimmed() : QString();
}

// Does this object describe an account, as opposed to a container that merely
// nests some? Checked before validation so that an entry we reject (an HOTP, a
// Steam code, a SHA-256 secret) is consumed rather than walked into — walking
// into it would find the secret again and import it with default settings.
bool isEntry(const QJsonObject &object)
{
    const QJsonValue info = object.value(QLatin1String("info"));
    if (info.isObject() && !stringValue(info.toObject(), "secret").isEmpty())
        return true;
    return !stringValue(object, "secret").isEmpty();
}

// Same normalisation for every source so that the same key imported from a
// text file and from a JSON backup compares equal and de-duplicates.
QString normaliseSecret(const QString &secret)
{
    QString normalised = secret.toUpper();
    normalised.remove('=');
    normalised.remove(' ');
    normalised.remove('-');
    return normalised;
}

} // namespace

Importer::Importer(Database *db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

QVariantList Importer::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << filePath;
        return QVariantList();
    }

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    // Exports come in two shapes: one otpauth:// URI per line, or a JSON
    // document (Aegis, andOTP, GNOME Authenticator, FreeOTP). Decide on the
    // first non-whitespace character, ignoring a UTF-8 BOM.
    QString trimmed = content.trimmed();
    if (trimmed.startsWith(QChar(0xFEFF))) trimmed = trimmed.mid(1).trimmed();
    if (trimmed.startsWith('[') || trimmed.startsWith('{')) {
        const QVariantList fromJson = parseJson(trimmed);
        // Fall back to the line parser: a plain-text list may open with "["
        // without being JSON, and a JSON file that yields nothing (for
        // instance an encrypted Aegis backup) loses nothing by trying.
        return fromJson.isEmpty() ? parseText(content) : fromJson;
    }

    return parseText(content);
}

QVariantList Importer::parseText(const QString &text)
{
    QVariantList accounts;
    // QString::SkipEmptyParts, not Qt::SkipEmptyParts: the latter only
    // exists since Qt 5.14 and Sailfish OS still ships Qt 5.6.
    QStringList lines = text.split('\n', QString::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Some exporters wrap the URIs in quotes or list syntax ("- uri").
        if (trimmed.startsWith('[') || trimmed.startsWith('-') || trimmed.startsWith('*'))
            trimmed = trimmed.mid(1).trimmed();
        if (trimmed.startsWith('"') || trimmed.startsWith('\''))
            trimmed = trimmed.mid(1, trimmed.length() - 2).trimmed();
        while (trimmed.endsWith(',') || trimmed.endsWith('"') || trimmed.endsWith('\''))
            trimmed.chop(1);

        appendUri(accounts, trimmed);
    }

    return accounts;
}

void Importer::appendUri(QVariantList &accounts, const QString &uri) const
{
    const QString trimmed = uri.trimmed();
    if (!trimmed.startsWith("otpauth://", Qt::CaseInsensitive)) return;

    const ImportAccount acct = parseOtpAuthUri(trimmed);
    if (acct.secret.isEmpty()) return;

    const QVariantMap entry = makeAccount(acct.issuer, acct.name, acct.secret,
                                          QStringLiteral("totp"), QString(),
                                          acct.digits, acct.period);
    if (!entry.isEmpty()) accounts.append(entry);
}

QVariantList Importer::parseJson(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "Invalid JSON:" << error.errorString();
        return QVariantList();
    }

    QVariantList accounts;
    collect(doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()), accounts);
    return accounts;
}

/*
 * Walks a decoded JSON document. Every shape we support reduces to "find the
 * objects that carry a secret":
 *
 *   andOTP / GNOME Authenticator   [ { "type": "TOTP", "secret": ... } ]
 *   Aegis                          { "db": { "entries": [ { "info": { "secret":
 *                                                              ... } } ] } }
 *   FreeOTP                        [ { "url": "otpauth://totp/...", ... } ]
 *   any other exporter             strings anywhere that are otpauth:// URIs
 *
 * The last rule is deliberately last-resort and applies to any string in the
 * document, so an unfamiliar format still imports if it carries plain URIs.
 */
void Importer::collect(const QJsonValue &value, QVariantList &accounts) const
{
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &element : array)
            collect(element, accounts);
        return;
    }

    if (!value.isObject()) {
        appendUri(accounts, value.toString());
        return;
    }

    const QJsonObject object = value.toObject();

    if (isEntry(object)) {
        const QVariantMap entry = entryFromObject(object);
        // The object is consumed either way: an entry rejected by
        // makeAccount() must not be revisited by the walk below.
        if (!entry.isEmpty()) accounts.append(entry);
        return;
    }

    for (QJsonObject::const_iterator it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().isString())
            appendUri(accounts, it.value().toString());
        else
            collect(it.value(), accounts);
    }
}

QVariantMap Importer::entryFromObject(const QJsonObject &object) const
{
    // Aegis: name/issuer at the top level, everything else under "info".
    const QJsonValue infoValue = object.value(QLatin1String("info"));
    if (infoValue.isObject()) {
        const QJsonObject info = infoValue.toObject();
        const QString secret = stringValue(info, "secret");
        if (!secret.isEmpty()) {
            return makeAccount(stringValue(object, "issuer"),
                               stringValue(object, "name"),
                               secret,
                               stringValue(object, "type"),
                               stringValue(info, "algo"),
                               intValue(info, "digits", 6),
                               intValue(info, "period", 30));
        }
    }

    // andOTP and friends: one flat object per account.
    const QString secret = stringValue(object, "secret");
    if (!secret.isEmpty()) {
        QString algorithm = stringValue(object, "algorithm");
        if (algorithm.isEmpty()) algorithm = stringValue(object, "algo");

        return makeAccount(stringValue(object, "issuer"),
                           stringValue(object, "label"),
                           secret,
                           stringValue(object, "type"),
                           algorithm,
                           intValue(object, "digits", 6),
                           intValue(object, "period", 30));
    }

    return QVariantMap();
}

QVariantMap Importer::makeAccount(const QString &issuer, const QString &name,
                                  const QString &secret, const QString &type,
                                  const QString &algorithm, int digits, int period) const
{
    // Only time-based codes on SHA-1. Importing an HOTP, a Steam code or a
    // SHA-256 secret as if it were SHA-1 TOTP would produce plausible-looking
    // codes that never match the service — worse than importing nothing.
    if (!type.isEmpty() && type.compare(QLatin1String("totp"), Qt::CaseInsensitive) != 0)
        return QVariantMap();
    if (!algorithm.isEmpty() && algorithm.compare(QLatin1String("SHA1"), Qt::CaseInsensitive) != 0)
        return QVariantMap();

    Totp totp;
    const QString normalisedSecret = normaliseSecret(secret);
    if (!totp.validateSecret(normalisedSecret)) return QVariantMap();

    // Match parseOtpAuthUri(): a bare label may be "Issuer:account".
    QString cleanIssuer = issuer;
    QString cleanName = name;
    if (cleanIssuer.isEmpty()) {
        const int colon = cleanName.indexOf(':');
        if (colon >= 0) {
            cleanIssuer = cleanName.left(colon).trimmed();
            cleanName = cleanName.mid(colon + 1).trimmed();
        }
    }

    QVariantMap entry;
    entry["issuer"] = cleanIssuer;
    entry["name"] = cleanName;
    entry["secret"] = normalisedSecret;
    // Totp::generateCode() only ever emits 6 or 8 digits; store what it will
    // actually produce rather than an unsupported value.
    entry["digits"] = (digits == 8) ? 8 : 6;
    entry["period"] = (period > 0) ? period : 30;
    return entry;
}

ImportAccount Importer::parseOtpAuthUri(const QString &uri) const
{
    ImportAccount result;
    result.digits = 6;
    result.period = 30;

    if (!uri.startsWith("otpauth://")) return result;

    QString rest = uri.mid(10); // Remove "otpauth://"

    // Split type from path+query
    int slashIdx = rest.indexOf('/');
    if (slashIdx < 0) return result;

    QString type = rest.left(slashIdx).toLower();
    if (type != "totp") {
        qDebug() << "Unsupported OTP type:" << type;
        return result;
    }

    QString pathAndQuery = rest.mid(slashIdx + 1);

    // Split path from query
    int queryIdx = pathAndQuery.indexOf('?');
    QString path = queryIdx >= 0 ? pathAndQuery.left(queryIdx) : pathAndQuery;
    QString query = queryIdx >= 0 ? pathAndQuery.mid(queryIdx + 1) : "";

    // Parse label (path) — can be "Issuer:AccountName" or just "AccountName"
    QString label = QUrl::fromPercentEncoding(path.toUtf8());
    int colonIdx = label.indexOf(':');
    if (colonIdx >= 0) {
        result.issuer = label.left(colonIdx).trimmed();
        result.name = label.mid(colonIdx + 1).trimmed();
    } else {
        result.name = label.trimmed();
    }

    // Parse query parameters
    QUrlQuery urlQuery(query);

    // Secrets are Base32; normalise so duplicates compare equal.
    QString secret = normaliseSecret(urlQuery.queryItemValue("secret"));
    result.secret = secret;

    QString issuerParam = urlQuery.queryItemValue("issuer").trimmed();
    if (!issuerParam.isEmpty()) {
        result.issuer = issuerParam;
    }

    QString digitsStr = urlQuery.queryItemValue("digits");
    if (!digitsStr.isEmpty()) {
        bool ok;
        int d = digitsStr.toInt(&ok);
        if (ok && (d == 6 || d == 8)) {
            result.digits = d;
        }
    }

    QString periodStr = urlQuery.queryItemValue("period");
    if (!periodStr.isEmpty()) {
        bool ok;
        int p = periodStr.toInt(&ok);
        if (ok && p > 0) {
            result.period = p;
        }
    }

    return result;
}

int Importer::importAccounts(const QVariantList &accounts)
{
    if (!m_db) return 0;

    int imported = 0;
    for (const QVariant &var : accounts) {
        QVariantMap map = var.toMap();
        QString secret = map["secret"].toString();
        if (secret.isEmpty()) continue;
        if (m_db->accountExists(secret)) continue;

        if (m_db->addAccount(map["issuer"].toString(),
                             map["name"].toString(),
                             secret,
                             map["digits"].toInt(),
                             map["period"].toInt())) {
            imported++;
        }
    }

    qDebug() << "Imported" << imported << "of" << accounts.count() << "accounts";
    return imported;
}
