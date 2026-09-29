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
#include <QDebug>

#include "totp.h"
#include "database.h"
#include "accountmodel.h"
#include "clipboardhelper.h"
#include "importer.h"

int main(int argc, char *argv[])
{
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
    QQmlContext *context = view->rootContext();
    context->setContextProperty("totp", &totp);
    context->setContextProperty("accountModel", &model);
    context->setContextProperty("database", &db);
    context->setContextProperty("clipboardHelper", &clipboard);
    context->setContextProperty("importer", &importer);

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
