/*
 * Application entry point.
 *
 * Uses the explicit SailfishApp pattern (not SailfishApp::main()) so the C++
 * back ends — TOTP, SQLite database, account model, clipboard and importer —
 * can be handed to QML as context properties before the view is shown.
 */

#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickView>
#include <QScopedPointer>
#include <QTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstdio>

#include "totp.h"
#include "database.h"
#include "accountmodel.h"
#include "clipboardhelper.h"
#include "importer.h"
#include "qrfilter.h"

/*
 * Sailfish's Qt build routes qWarning/qDebug to the system journal, which an
 * unprivileged process cannot read back — and QML runtime errors (bad
 * handler names, type mismatches...) land there too. Mirror everything to
 * stderr with a timestamp instead: the debug launch recipe captures stderr,
 * so failures stay diagnosable.
 *
 * A launched (Sailjail-sandboxed) app has no capturable stderr either —
 * its fd 2 is a socket owned by the booster — so also append to a log file
 * in the app's whitelisted data directory.  That directory
 * (~/.local/share/<OrganizationName>/<ApplicationName>/) is created and
 * made writable by the launch profile on every start, which makes
 * sandboxed runs diagnosable after the fact.
 */
namespace {

QFile g_logFile;

void openLogFile()
{
    const QString dir = QDir::homePath()
            + QStringLiteral("/.local/share/harbour.sailotp/harbour-sailotp");
    QDir().mkpath(dir);

    const QString path = dir + QStringLiteral("/sailotp.log");
    if (QFileInfo(path).size() > 512 * 1024) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }

    g_logFile.setFileName(path);
    g_logFile.open(QIODevice::Append);
}

} // namespace

static void mirrorMessages(QtMsgType type, const QMessageLogContext &context,
                           const QString &message)
{
    const char *kind = type == QtFatalMsg    ? "FATAL"
            : type == QtCriticalMsg         ? "CRIT"
            : type == QtWarningMsg          ? "WARN"
            : type == QtInfoMsg             ? "INFO"
                                            : "DBG";
    const QByteArray line = QByteArray("[")
            + QTime::currentTime().toString(QLatin1String("hh:mm:ss.zzz")).toLocal8Bit()
            + ' ' + kind + "] " + (context.file ? context.file : "-")
            + ':' + QByteArray::number(context.line) + ' '
            + message.toLocal8Bit() + '\n';

    std::fwrite(line.constData(), 1, line.size(), stderr);
    std::fflush(stderr);
    if (g_logFile.isOpen()) {
        g_logFile.write(line);
        g_logFile.flush();
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(mirrorMessages);
    openLogFile();

    // Startup marker: attributes everything that follows in a captured log
    // to this launch (and proves log capture is working at all).
    qInfo("sailotp starting");

    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());

    Database db;
    if (!db.initialize()) {
        qWarning() << "Failed to initialize database";
    }

    AccountModel model(&db);
    ClipboardHelper clipboard;
    Importer importer(&db);

    Totp totp;
    QrFilter qrFilter;
    QQmlContext *context = view->rootContext();
    context->setContextProperty("totp", &totp);
    context->setContextProperty("accountModel", &model);
    context->setContextProperty("database", &db);
    context->setContextProperty("clipboardHelper", &clipboard);
    context->setContextProperty("importer", &importer);
    context->setContextProperty("qrFilter", &qrFilter);
    // Build version for the About page (APP_VERSION comes from the .pro,
    // which gets it from the RPM build environment).
    context->setContextProperty("appVersion", QStringLiteral(APP_VERSION));

    view->setSource(SailfishApp::pathTo("qml/harbour-sailotp.qml"));

    // QQuickView shows an empty (white) window if the root component fails to
    // load and, unlike QQmlApplicationEngine, does not report the errors
    // itself — so print them here or the failure is invisible in the logs.
    if (view->status() == QQuickView::Error) {
        const QList<QQmlError> errors = view->errors();
        for (int i = 0; i < errors.count(); ++i)
            qWarning() << errors.at(i).toString();
        qWarning() << "Failed to load" << view->source();
    }

    view->showFullScreen();

    return app->exec();
}
