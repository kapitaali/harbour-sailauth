/*
 * On-device probe for the QR scanner work. Two questions, one binary:
 *
 *  A) Which pixelFormat codes does the system's barcode service accept?
 *     org.amberapi.zxing (zxing-daemon, shipped with qr-filter-qml-plugin)
 *     takes a file descriptor holding raw pixels plus width/height/pixelFormat
 *     and returns the decoded text. The daemon is closed and stripped, so the
 *     accepted formats are found out by trying them with a known QR image.
 *
 *  B) Why does qml/pages/ScanPage.qml fail to load on the phone?
 *     The component is compiled (and created) in a headless QQmlEngine, whose
 *     warnings are printed — Silica's PageStack keeps the errorString to
 *     itself, so this is the only way to see it without a release build.
 *
 * Run on the phone (aarch64 build, needs the session bus and a display):
 *
 *   XDG_RUNTIME_DIR=/run/user/100000 WAYLAND_DISPLAY=../../display/wayland-0 \
 *   QT_QPA_PLATFORM=wayland \
 *   DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket \
 *   ./probe_zxing testqr.png /usr/share/harbour-sailauth/qml/pages/ScanPage.qml
 *
 * Build in the engine (target ABI of the phone):
 *
 *   sfdk -c target=SailfishOS-5.1.0.11-aarch64 build-shell sh -c \
 *     'g++ -std=gnu++11 -fPIC \
 *          $(pkg-config --cflags Qt5Core Qt5Gui Qt5Qml Qt5Quick Qt5DBus Qt5Multimedia) \
 *          tests/probe_zxing.cpp -o tests/probe_zxing \
 *          $(pkg-config --libs Qt5Core Qt5Gui Qt5Qml Qt5Quick Qt5DBus Qt5Multimedia)'
 */

#include <QByteArray>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QVariant>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusUnixFileDescriptor>
#include <QtMultimedia/QVideoFrame>

#include <sys/syscall.h>
#include <unistd.h>

#include <cstdio>

static int memFd(const char *name)
{
#ifdef __NR_memfd_create
    return int(syscall(__NR_memfd_create, name, 0u));
#else
    Q_UNUSED(name)
    return -1;
#endif
}

// One decodeFromDescriptor() round trip; every failure mode comes back as text
// so the output stays greppable.
static QString decode(const QByteArray &payload, int width, int height, int pixelFormat)
{
    const int fd = memFd("probe");
    if (fd < 0)
        return QLatin1String("memfd_create failed");

    if (write(fd, payload.constData(), size_t(payload.size())) != ssize_t(payload.size())) {
        close(fd);
        return QLatin1String("write failed");
    }
    lseek(fd, 0, SEEK_SET);

    QDBusInterface iface(QStringLiteral("org.amberapi.zxing"),
                         QStringLiteral("/org/amberapi/zxing"),
                         QStringLiteral("org.amberapi.zxing"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        close(fd);
        return QLatin1String("no service: ") + iface.lastError().message();
    }

    const QDBusReply<QString> reply =
            iface.call(QStringLiteral("decodeFromDescriptor"),
                       QVariant::fromValue(QDBusUnixFileDescriptor(fd)),
                       QVariant::fromValue(quint32(payload.size())),
                       QVariant::fromValue(width),
                       QVariant::fromValue(height),
                       QVariant::fromValue(pixelFormat));
    close(fd);

    if (!reply.isValid())
        return QLatin1String("call error: ") + reply.error().message();
    return reply.value();
}

static void probeFormats(const QString &imagePath)
{
    QImage image(imagePath);
    if (image.isNull()) {
        std::printf("A) cannot load image %s\n", qPrintable(imagePath));
        return;
    }

    struct Candidate {
        const char *name;
        QImage::Format imageFormat;
        QVideoFrame::PixelFormat frameFormat;
        bool luminance;     // Qt 5.6 QImage has no Format_Gray8, so this one is filled by hand
    };

    // The formats a camera pipeline is most likely to deliver.
    const Candidate candidates[] = {
        { "RGB32",           QImage::Format_RGB32,               QVideoFrame::Format_RGB32,      false },
        { "ARGB32",          QImage::Format_ARGB32,              QVideoFrame::Format_ARGB32,     false },
        { "ARGB32_Premul",   QImage::Format_ARGB32_Premultiplied, QVideoFrame::Format_ARGB32_Premultiplied, false },
        { "BGR32",           QImage::Format_RGB32,               QVideoFrame::Format_BGR32,      false },
        { "RGB24",           QImage::Format_RGB888,              QVideoFrame::Format_RGB24,      false },
        { "RGB565",          QImage::Format_RGB16,               QVideoFrame::Format_RGB565,     false },
        { "Gray8/Y8",        QImage::Format_Indexed8,            QVideoFrame::Format_Y8,         true },
    };

    std::printf("A) %d x %d image, trying %d pixel formats\n",
                image.width(), image.height(), int(sizeof(candidates) / sizeof(candidates[0])));

    for (const Candidate &c : candidates) {
        QImage converted;
        if (c.luminance) {
            // Byte-per-byte luminance, exactly what a Y8 frame would carry.
            const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
            converted = QImage(rgb.width(), rgb.height(), QImage::Format_Indexed8);
            for (int y = 0; y < rgb.height(); ++y) {
                const QRgb *src = reinterpret_cast<const QRgb *>(rgb.constScanLine(y));
                uchar *dst = converted.scanLine(y);
                for (int x = 0; x < rgb.width(); ++x)
                    dst[x] = uchar(qGray(src[x]));
            }
        } else {
            converted = image.convertToFormat(c.imageFormat);
        }
        const QByteArray payload(reinterpret_cast<const char *>(converted.bits()),
                                 converted.bytesPerLine() * converted.height());
        const QString result = decode(payload, converted.width(), converted.height(),
                                      int(c.frameFormat));
        std::printf("A) %-14s qvf=%-3d bytes=%-8d -> %s\n",
                    c.name, int(c.frameFormat), payload.size(), qPrintable(result));
    }
}

static void probeQml(const QString &qmlPath)
{
    QQmlEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &e : warnings)
                             std::printf("B) warning: %s\n", qPrintable(e.toString()));
                     });

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlPath));
    if (component.status() == QQmlComponent::Error) {
        std::printf("B) COMPILE FAILED for %s\n", qPrintable(qmlPath));
        for (const QQmlError &e : component.errors())
            std::printf("B)   %s\n", qPrintable(e.toString()));
        return;
    }

    std::printf("B) compile OK: %s\n", qPrintable(qmlPath));
    QObject *object = component.create();
    if (!object) {
        std::printf("B) CREATE FAILED\n");
        for (const QQmlError &e : component.errors())
            std::printf("B)   %s\n", qPrintable(e.toString()));
        return;
    }

    std::printf("B) create OK (%s)\n", object->metaObject()->className());
    delete object;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    if (argc > 1)
        probeFormats(QString::fromLocal8Bit(argv[1]));
    if (argc > 2)
        probeQml(QString::fromLocal8Bit(argv[2]));
    if (argc < 2)
        std::printf("usage: probe_zxing <qr-image> [page.qml]\n");

    return 0;
}
