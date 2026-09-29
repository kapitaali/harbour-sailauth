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
 */
static void mirrorMessages(QtMsgType type, const QMessageLogContext &context,
                           const QString &message)
{
    const char *kind = type == QtFatalMsg    ? "FATAL"
            : type == QtCriticalMsg         ? "CRIT"
            : type == QtWarningMsg          ? "WARN"
            : type == QtInfoMsg             ? "INFO"
                                            : "DBG";
    std::fprintf(stderr, "[%s %s] %s:%d %s\n",
                 qPrintable(QTime::currentTime().toString(QLatin1String("hh:mm:ss.zzz"))),
                 kind, context.file ? context.file : "-",
                 context.line, qPrintable(message));
    std::fflush(stderr);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(mirrorMessages);

    // Startup marker: attributes everything that follows in a captured log
    // to this launch (and proves stderr capture is working at all).
    std::fprintf(stderr, "[%s INFO] sailotp starting\n",
                 qPrintable(QTime::currentTime().toString(QLatin1String("hh:mm:ss.zzz"))));
    std::fflush(stderr);

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
