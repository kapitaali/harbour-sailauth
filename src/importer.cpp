#include "importer.h"
#include "database.h"
#include <QFile>
#include <QTextStream>
#include <QUrlQuery>
#include <QDebug>

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

        if (!trimmed.startsWith("otpauth://")) continue;

        ImportAccount acct = parseOtpAuthUri(trimmed);
        if (!acct.secret.isEmpty()) {
            QVariantMap map;
            map["issuer"] = acct.issuer;
            map["name"] = acct.name;
            map["secret"] = acct.secret;
            map["digits"] = acct.digits;
            map["period"] = acct.period;
            accounts.append(map);
        }
    }

    return accounts;
}

ImportAccount Importer::parseOtpAuthUri(const QString &uri)
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
    QString secret = urlQuery.queryItemValue("secret").toUpper();
    secret.remove(' ');
    secret.remove('-');
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
