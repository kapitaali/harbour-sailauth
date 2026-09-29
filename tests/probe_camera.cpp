/*
 * On-device probe for the capture-based scanner path, wired exactly like
 * ScanPage: Camera.imageCapture.imageSaved -> QrFilter::submitImageFile()
 * -> org.amberapi.zxing, with the capture Timer gated on imageCapture.ready.
 *
 * What it verifies:
 *   - the full chain end to end: at t=1.2 s it feeds ~/testqr_master.png
 *     straight to submitImageFile(), which must produce "QR decoded:
 *     otpauth://..." (proof: file -> QImage -> packed ARGB32 -> memfd ->
 *     D-Bus -> zxing -> decoded()); further feeds come once warm,
 *   - that real camera captures arrive as private cache files and reach
 *     zxing (ceiling in view => "QR decode returned no code", which still
 *     proves the chain), at the readiness-edge-driven cadence,
 *   - that the continuous focus mode stuck (focus.focusMode = 16) and the
 *     gallery stays untouched (captures go to the app cache dir),
 *   - the environment facts the design rests on: which VideoOutput controls
 *     the camera service offers (all missing => overlay backend, no filter
 *     chain — the reason decoding uses stills at all), whether the nemo
 *     texture backend plugin is loaded, and a grabWindow() screenshot.
 *
 * Qt's own qWarning/QML errors are redirected to stderr here (Sailfish's Qt
 * build sends them to the system journal, which this process cannot read).
 *
 * Kill any running SailfishOTP first — the camera is exclusive.
 *
 * Build in the engine (target ABI of the phone):
 *
 *   sfdk -c target=SailfishOS-5.1.0.11-aarch64 build-shell sh -c \
 *     'moc src/qrfilter.h -o tests/moc_qrfilter.cpp &&
 *      g++ -std=gnu++11 -fPIC \
 *          $(pkg-config --cflags Qt5Core Qt5Gui Qt5Qml Qt5Quick Qt5DBus Qt5Multimedia) \
 *          src/qrfilter.cpp tests/moc_qrfilter.cpp tests/probe_camera.cpp \
 *          -o tests/probe_camera \
 *          $(pkg-config --libs Qt5Core Qt5Gui Qt5Qml Qt5Quick Qt5DBus Qt5Multimedia)'
 */

#include "../src/qrfilter.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QObject>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QCameraImageCapture>
#include <QtMultimedia/QMediaControl>
#include <QtMultimedia/QMediaObject>
#include <QtMultimedia/QMediaService>

#include <cstdio>

namespace {

// Sailfish's QtBuild sends qWarning/qDebug to the system journal, which an
// unprivileged process cannot read back — and that is where QML runtime
// errors land too. Capture everything to stderr instead, timestamped so the
// capture cycle stays measurable (stdout and stderr share one log file).
void captureMessages(QtMsgType type, const QMessageLogContext &context,
                     const QString &message)
{
    const char *kind = type == QtFatalMsg    ? "FATAL"
            : type == QtCriticalMsg         ? "CRIT"
            : type == QtWarningMsg          ? "WARN"
            : type == QtInfoMsg             ? "INFO"
                                            : "DBG";
    std::fprintf(stderr, "[%s %s] %s:%d %s\n",
                 qPrintable(QTime::currentTime().toString(QLatin1String("hh:mm:ss.zzz"))),
                 kind, context.file ? context.file : "-", context.line,
                 qPrintable(message));
    std::fflush(stderr);
}

// Mirrors ScanPage's wiring: continuous focus, captures into the scanner's
// private cache file driven by readiness edges + a fallback beat, all via
// qrFilter.requestCapture()/submitImageFile().
const char kViewfinder[] =
        "import QtQuick 2.0\n"
        "import QtMultimedia 5.6\n"
        "Item {\n"
        "    width: 480\n"
        "    height: 800\n"
        "    readonly property real sourceWidth: viewfinder.sourceRect.width\n"
        "    readonly property real sourceHeight: viewfinder.sourceRect.height\n"
        "    Camera {\n"
        "        id: camera\n"
        "        objectName: \"camera\"\n"
        "        position: Camera.BackFace\n"
        "        focus {\n"
        "            focusMode: Camera.FocusContinuous\n"
        "        }\n"
        "        imageCapture {\n"
        "            onImageSaved: function (id, fileName) { qrFilter.submitImageFile(fileName) }\n"
        "            onReadyForCaptureChanged: {\n"
        "                if (camera.imageCapture.ready) qrFilter.requestCapture()\n"
        "            }\n"
        "        }\n"
        "        Component.onCompleted: {\n"
        "            camera.start()\n"
        "            qrFilter.attachCamera(camera)\n"
        "        }\n"
        "    }\n"
        "    VideoOutput {\n"
        "        id: viewfinder\n"
        "        anchors.fill: parent\n"
        "        source: camera\n"
        "    }\n"
        "    Rectangle { x: 20; y: 20; width: 60; height: 60; color: \"#ff0000\" }\n"
        "    Timer {\n"
        "        interval: 700\n"
        "        repeat: true\n"
        "        running: true\n"
        "        onTriggered: qrFilter.requestCapture()\n"
        "    }\n"
        "}\n";

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    qInstallMessageHandler(captureMessages);

    std::printf("PROBE sanity stdout\n");
    std::fflush(stdout);
    qWarning().noquote() << "PROBE sanity qWarning";
    std::fflush(stderr);

    QrFilter filter;
    QObject::connect(&filter, &QrFilter::decoded, [](const QString &text) {
        std::printf("PROBE decoded signal: %s\n", qPrintable(text));
        std::fflush(stdout);
    });

    const QString qmlPath = QDir::tempPath() + QLatin1String("/probe_camera.qml");
    QFile file(qmlPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::printf("PROBE cannot write %s\n", qPrintable(qmlPath));
        return 1;
    }
    file.write(kViewfinder);
    file.close();

    QQuickView view;
    view.resize(480, 800);
    view.rootContext()->setContextProperty("qrFilter", &filter);
    view.setSource(QUrl::fromLocalFile(qmlPath));
    if (view.status() == QQuickView::Error) {
        for (const QQmlError &e : view.errors())
            std::printf("PROBE view error: %s\n", qPrintable(e.toString()));
        return 1;
    }

    std::printf("PROBE running 14 s: imageSaved -> submitImageFile -> zxing\n");
    std::fflush(stdout);
    view.show();

    // Feed the demo QR to the decoder three times: early (cold D-Bus service
    // — proves the failure is just startup), then twice once warm. Each feed
    // is a fresh copy because submitImageFile() deletes what it reads. A
    // successful decode prints "PROBE decoded signal: otpauth://..." — that
    // is the whole chain: file -> QImage -> ARGB32 -> memfd -> D-Bus -> zxing.
    const auto feed = [&filter](const char *when, int delayMs) {
        QTimer::singleShot(delayMs, [&filter, when]() {
            const QString master = QDir::homePath() + QLatin1String("/testqr_master.png");
            const QString copy = QDir::tempPath() + QLatin1String("/feedqr.png");
            QFile::remove(copy);
            if (!QFile::copy(master, copy)) {
                std::printf("PROBE[%s] feed copy failed (%s)\n", when,
                            qPrintable(master));
                std::fflush(stdout);
                return;
            }
            std::printf("PROBE[%s] feeding demo QR to submitImageFile\n", when);
            std::fflush(stdout);
            filter.submitImageFile(copy);
        });
    };
    feed("feed-cold", 1200);
    feed("feed-warm-1", 8000);
    feed("feed-warm-2", 11500);

    // Which controls does the camera's media service offer? All three missing
    // => Qt picked the window/overlay backend (no filters, no rotation).
    QTimer::singleShot(1500, [&view]() {
        QQuickItem *root = view.rootObject();
        QObject *cam = root ? root->findChild<QObject *>(QStringLiteral("camera")) : nullptr;
        QMediaObject *media = cam ? qobject_cast<QMediaObject *>(
                                        cam->property("mediaObject").value<QObject *>())
                                  : nullptr;
        if (!media || !media->service()) {
            std::printf("PROBE: no camera media service (%p)\n", (void *)media);
            std::fflush(stdout);
            return;
        }

        QMediaService *service = media->service();
        static const struct {
            const char *iid;
            const char *name;
        } kControls[] = {
            { "org.qt-project.qt.videorenderercontrol/5.0", "QVideoRendererControl" },
            { "org.qt-project.qt.videowindowcontrol/5.0", "QVideoWindowControl" },
            { "org.qt-project.qt.gstreamervideosinkcontrol/5.2", "QGStreamerVideoSinkControl" },
        };
        for (const auto &entry : kControls) {
            QMediaControl *control = service->requestControl(entry.iid);
            std::printf("PROBE control %-25s %s\n", entry.name,
                        control ? "available" : "MISSING");
            if (control)
                service->releaseControl(control);
        }
        std::fflush(stdout);
    });

    const auto findCamera = [&view]() -> QObject * {
        QQuickItem *root = view.rootObject();
        return root ? root->findChild<QObject *>(QStringLiteral("camera")) : nullptr;
    };

    // Wrapper properties + camera state + an owned capture control for direct
    // readiness observation, dumped once the camera has settled.
    QCameraImageCapture *ownCapture = nullptr;
    QTimer::singleShot(2500, [&findCamera, &app, &ownCapture]() {
        QObject *cam = findCamera();
        if (!cam)
            return;
        QObject *media = cam->property("mediaObject").value<QObject *>();
        QCamera *qcam = qobject_cast<QCamera *>(media);
        QObject *capture = cam->property("imageCapture").value<QObject *>();

        if (capture) {
            const QMetaObject *meta = capture->metaObject();
            std::printf("PROBE wrapper class=%s\n", meta->className());
            for (int i = 0; i < meta->propertyCount(); ++i)
                std::printf("PROBE   property %s\n", meta->property(i).name());
        }

        // Focus group: verifies FocusContinuous was accepted (FocusContinuous
        // = 0x10 = 16 if the value stuck; warnings about unsupported modes
        // would have gone to stderr through the message handler).
        QObject *focus = cam->property("focus").value<QObject *>();
        if (focus) {
            const QMetaObject *fmeta = focus->metaObject();
            for (int i = 0; i < fmeta->propertyCount(); ++i) {
                const QMetaProperty prop = fmeta->property(i);
                const QVariant value = prop.read(focus);
                QString text = value.toString();
                if (text.isEmpty())
                    text = QString::number(value.toInt());
                std::printf("PROBE   focus.%s = %s\n", prop.name(), qPrintable(text));
            }
        }

        if (qcam) {
            static const char *kStatusName[] = {
                "Unavailable", "Unloaded", "Loading", "Unloading",
                "Loaded", "Standby", "Starting", "Stopping", "Active"
            };
            const int status = int(qcam->status());
            std::printf("PROBE qcam state=%d status=%d(%s) captureMode=%d\n",
                        int(qcam->state()), status,
                        status >= 0 && status <= 8 ? kStatusName[status] : "?",
                        int(qcam->captureMode()));

            QObject::connect(qcam, &QCamera::statusChanged, [](QCamera::Status s) {
                std::printf("PROBE event: qcam statusChanged -> %d\n", int(s));
                std::fflush(stdout);
            });

            ownCapture = new QCameraImageCapture(qcam, &app);
            QObject::connect(ownCapture, &QCameraImageCapture::readyForCaptureChanged,
                             [](bool ready) {
                std::printf("PROBE event: own readyForCaptureChanged -> %d\n",
                            ready ? 1 : 0);
                std::fflush(stdout);
            });
        }
        std::fflush(stdout);
    });

    // Readiness poll: with the timer gated on `ready` this shows the capture
    // cadence (ready dips to 0 only while a capture runs).
    int pollCount = 0;
    QTimer *readyPoll = new QTimer(&app);
    QObject::connect(readyPoll, &QTimer::timeout,
                     [&findCamera, &ownCapture, &pollCount, readyPoll]() {
        if (++pollCount > 26) {
            readyPoll->stop();
            return;
        }
        QObject *cam = findCamera();
        QObject *capture = cam ? cam->property("imageCapture").value<QObject *>() : nullptr;
        const bool wrapperReady = capture && capture->property("ready").toBool();
        std::printf("PROBE poll #%d wrapperReady=%d ownReady=%d\n", pollCount,
                    wrapperReady ? 1 : 0,
                    ownCapture && ownCapture->isReadyForCapture() ? 1 : 0);
        std::fflush(stdout);
    });
    readyPoll->start(500);

    const auto report = [&findCamera, &view](const char *when) {
        QQuickItem *root = view.rootObject();
        QObject *cam = findCamera();
        QObject *media = cam ? cam->property("mediaObject").value<QObject *>() : nullptr;
        QCamera *qcam = qobject_cast<QCamera *>(media);
        std::printf("PROBE[%s] source=%gx%g qcamStatus=%d\n", when,
                    root ? root->property("sourceWidth").toDouble() : -1.0,
                    root ? root->property("sourceHeight").toDouble() : -1.0,
                    qcam ? int(qcam->status()) : -1);
        std::fflush(stdout);
    };
    QTimer::singleShot(4500, [report]() { report("t=4.5s"); });
    QTimer::singleShot(9000, [report]() { report("t=9s"); });
    QTimer::singleShot(13000, [report]() { report("t=13s"); });

    // grabWindow() captures only the QtQuick scene: camera pixels in the image
    // mean in-scene rendering, black + red marker means an overlay window.
    QTimer::singleShot(12500, [&view]() {
        const QImage img = view.grabWindow();
        const QString path = QDir::homePath() + QLatin1String("/probe_grab.png");
        std::printf("PROBE grab %dx%d saved=%d -> %s\n", img.width(), img.height(),
                    img.save(path) ? 1 : 0, qPrintable(path));
        std::fflush(stdout);
    });

    QTimer::singleShot(14000, &app, &QCoreApplication::quit);
    const int result = app.exec();

    std::printf("PROBE finished\n");
    return result;
}
