/*
 * QR code decoding — see qrfilter.h for the capture-based flow and why the
 * platform service is used.
 *
 * submitImageFile() loads one captured still (scaled while decoding) and
 * converts it to tightly packed ARGB32 -> submitPending() writes the pixels
 * into a memfd and calls the service asynchronously -> callFinished() emits
 * decoded() and lets the next image through. One decode in flight at a time
 * keeps things cheap.
 *
 * Diagnostics go to stderr with fprintf: Sailfish's QtBuild routes qWarning
 * to the system journal (libQt5Core links sd_journal_send), which an
 * unprivileged app process cannot read back — stderr is what the debug
 * launch recipe captures. Every line is timestamped because the interesting
 * question is always "how long did the capture take".
 */

#include "qrfilter.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusUnixFileDescriptor>

#include <QtMultimedia/QCamera>
#include <QtMultimedia/QCameraViewfinderSettings>

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QVariant>

#include <sys/syscall.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace {

const char kService[] = "org.amberapi.zxing";
const char kObjectPath[] = "/org/amberapi/zxing";
const char kInterface[] = "org.amberapi.zxing";

// The pixel format the daemon decodes — see tests/probe_zxing.cpp, which
// found that QVideoFrame::Format_ARGB32 works while RGB32/Y8/RGB24/RGB565
// return nothing.
const int kPixelFormat = 1;     // QVideoFrame::Format_ARGB32

// How long to leave the camera alone after handing over an image.
const int kSampleIntervalMs = 150;

// Longest edge sent to the daemon. With the capture negotiated down to
// kCaptureMaxEdge this never triggers — it is the safety net should the
// camera ever hand over a larger still (native is 8192x6144, ~200 MB as
// ARGB32, which no D-Bus message would carry). 19.7 MB payloads
// (2560x1920) were accepted by the daemon on-device.
const int kDecodeMaxEdge = 2560;

// Largest capture edge requested from the camera. Native stills are
// 8192x6144 and take ~3 s each (measured on-device); at 1920x1440 the
// capture cycle drops to ~1.5 s and the payload fits D-Bus comfortably.
// The supported list contains 2560x1440, but selecting it makes camerabin
// fail with "failed to negotiate caps" and drop the pipeline to Unloaded —
// the camera then never comes back, so 1920x1440 is the ceiling.
const int kCaptureMaxEdge = 1920;

int createFrameFd()
{
#ifdef __NR_memfd_create
    return int(syscall(__NR_memfd_create, "sailotp-frame", 0u));
#else
    return -1;
#endif
}

const char *timestamp()
{
    static char buffer[16];
    const QByteArray now = QTime::currentTime().toString(QLatin1String("hh:mm:ss.zzz")).toLatin1();
    std::memcpy(buffer, now.constData(), size_t(now.size()));
    buffer[now.size()] = '\0';
    return buffer;
}

} // namespace

QrFilter::QrFilter(QObject *parent)
    : QObject(parent)
    , m_resolutionDone(false)
    , m_busy(0)
    , m_frames(0)
    , m_width(0)
    , m_height(0)
    , m_fd(-1)
{
}

QrFilter::~QrFilter()
{
    if (m_fd >= 0)
        close(m_fd);
}

void QrFilter::attachCamera(QObject *qmlCamera)
{
    if (!qmlCamera) {
        std::fprintf(stderr, "[%s] QR attach: no camera object\n", timestamp());
        std::fflush(stderr);
        return;
    }

    QObject *capture = qmlCamera->property("imageCapture").value<QObject *>();
    if (!capture) {
        std::fprintf(stderr, "[%s] QR attach: camera has no imageCapture group\n", timestamp());
        std::fflush(stderr);
        return;
    }

    m_camera = qmlCamera;
    m_captureGroup = capture;
    m_resolutionDone = false;           // this camera still wants configuring

    // Captures go to a private cache file, never to the gallery: the path is
    // handed to every capture via captureToLocation(), read back through
    // imageSaved and deleted by submitImageFile(). Sweeping the old file
    // here also cleans up after a crash mid-scan.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath() + QLatin1String("/harbour-sailotp");
    QDir().mkpath(dir);
    m_scanFilePath = dir + QLatin1String("/scan.jpg");
    QFile::remove(m_scanFilePath);

    std::fprintf(stderr,
                 "[%s] QR attach: capture=%s scan file=%s\n", timestamp(),
                 capture->metaObject()->className(), qPrintable(m_scanFilePath));
    std::fflush(stderr);

    // Log every readiness flip: the capture cadence is measured from these
    // lines (a ready=false stretch with no imageSaved means a stalled
    // capture, ready=true without requests means requestCapture refused).
    connect(capture, SIGNAL(readyForCaptureChanged(bool)),
            this, SLOT(logReady(bool)), Qt::UniqueConnection);

    attemptResolution(0);
}

void QrFilter::logReady(bool ready)
{
    std::fprintf(stderr, "[%s] QR ready=%d\n", timestamp(), ready ? 1 : 0);
    std::fflush(stderr);
}

bool QrFilter::requestCapture()
{
    QObject *capture = m_captureGroup.data();
    if (!capture || m_scanFilePath.isEmpty())
        return false;
    if (!m_resolutionDone)
        return false;                   // a resolution change may be reloading
                                        // the pipeline — capturing now would
                                        // be swallowed by the reload
    if (!capture->property("ready").toBool())
        return false;                   // a capture is already in flight

    const bool invoked = QMetaObject::invokeMethod(capture, "captureToLocation",
                                                   Q_ARG(QString, m_scanFilePath));
    std::fprintf(stderr, "[%s] QR capture requested (invoke=%d)\n",
                 timestamp(), invoked ? 1 : 0);
    std::fflush(stderr);
    return invoked;
}

void QrFilter::attemptResolution(int attempt)
{
    QObject *capture = m_captureGroup.data();
    if (!capture)
        return;
    if (configureResolution(capture)) {
        m_resolutionDone = true;
        return;
    }
    if (attempt < 5) {
        // The camera reports no resolutions while it is still starting up
        // (the first attempt right after Camera.onCompleted sees an empty
        // list), so keep asking until it settles.
        QTimer::singleShot(1500, this, [this, attempt]() {
            attemptResolution(attempt + 1);
        });
    } else {
        m_resolutionDone = true;        // give up — the default size works
        std::fprintf(stderr, "[%s] QR attach: no resolutions ever reported — "
                     "keeping the camera's default capture size\n", timestamp());
        std::fflush(stderr);
    }
}

bool QrFilter::configureResolution(QObject *capture)
{
    // Default stills are the sensor's full 8192x6144 — a ~3 s capture cycle
    // and a 200 MB ARGB32 buffer. Ask for the largest supported size that
    // fits kCaptureMaxEdge; if the camera reports nothing (it may still be
    // starting up), leave the default alone rather than risk a broken size.
    const QCamera *qcam = qobject_cast<QCamera *>(
            m_camera ? m_camera->property("mediaObject").value<QObject *>()
                     : nullptr);
    if (!qcam) {
        std::fprintf(stderr, "[%s] QR configure: no QCamera yet\n", timestamp());
        std::fflush(stderr);
        return false;                   // camera not constructed yet — retry
    }

    QSize chosen;
    QString supported;
    const QList<QSize> sizes = qcam->supportedViewfinderResolutions();
    for (const QSize &s : sizes) {
        if (!supported.isEmpty())
            supported += QLatin1Char(' ');
        supported += QString::fromLatin1("%1x%2").arg(s.width()).arg(s.height());
        if (s.isValid() && s.width() <= kCaptureMaxEdge
                && s.height() <= kCaptureMaxEdge
                && s.width() * s.height() > chosen.width() * chosen.height()) {
            chosen = s;
        }
    }

    if (supported.isEmpty())
        return false;                   // not answering yet — retry

    int set = 0;
    QSize readBack;
    if (chosen.isValid()) {
        set = capture->setProperty("resolution", QSize(chosen)) ? 1 : 0;
        readBack = capture->property("resolution").toSize();
    }

    std::fprintf(stderr,
                 "[%s] QR attach: capture=%s supported=[%s] chosen=%dx%d set=%d readBack=%dx%d\n",
                 timestamp(), capture->metaObject()->className(),
                 qPrintable(supported), chosen.width(), chosen.height(), set,
                 readBack.isValid() ? readBack.width() : -1,
                 readBack.isValid() ? readBack.height() : -1);
    std::fflush(stderr);
    return true;
}

void QrFilter::submitImageFile(const QString &fileName)
{
    // Every capture is logged: scans produce one file per second at most,
    // and the log shows how long each cycle takes.
    ++m_frames;
    std::fprintf(stderr, "[%s] QR file #%d %s\n", timestamp(), m_frames,
                 qPrintable(fileName));
    std::fflush(stderr);

    QImageReader reader(fileName);
    reader.setAutoTransform(true);      // honour EXIF orientation

    // Scale during decoding: only the small buffer is allocated, and Qt's
    // JPEG reader can downscale cheaply.
    const QSize size = reader.size();
    if (size.isValid()
            && qMax(size.width(), size.height()) > kDecodeMaxEdge) {
        reader.setScaledSize(size.scaled(kDecodeMaxEdge, kDecodeMaxEdge,
                                         Qt::KeepAspectRatio));
    }

    const QImage image = reader.read();
    QFile::remove(fileName);            // scans must not litter the gallery

    if (image.isNull()) {
        std::fprintf(stderr, "[%s] QR read failed: %s\n", timestamp(),
                     qPrintable(reader.errorString()));
        std::fflush(stderr);
        return;
    }

    std::fprintf(stderr, "[%s] QR pixels %dx%d -> %dx%d\n", timestamp(),
                 size.width(), size.height(), image.width(), image.height());
    std::fflush(stderr);

    submitFrame(image);
}

void QrFilter::submitFrame(const QImage &image)
{
    if (image.isNull())
        return;

    if (!m_busy.testAndSetOrdered(0, 1))
        return;                         // a decode is still in flight

    if (m_lastSubmit.isValid() && m_lastSubmit.elapsed() < kSampleIntervalMs) {
        m_busy.storeRelease(0);
        return;
    }

    const QImage argb = image.format() == QImage::Format_ARGB32
            ? image
            : image.convertToFormat(QImage::Format_ARGB32);
    if (argb.isNull()) {
        m_busy.storeRelease(0);
        return;
    }

    // The daemon reads width*height*4 bytes with no stride, so rows are
    // repacked if the capture's lines happen to be padded.
    const int stride = argb.width() * 4;
    m_pixels.resize(stride * argb.height());
    if (argb.bytesPerLine() == stride) {
        std::memcpy(m_pixels.data(), argb.constBits(), size_t(m_pixels.size()));
    } else {
        for (int row = 0; row < argb.height(); ++row)
            std::memcpy(m_pixels.data() + row * stride, argb.constScanLine(row),
                        size_t(stride));
    }

    m_width = argb.width();
    m_height = argb.height();
    m_lastSubmit.start();

    QMetaObject::invokeMethod(this, "submitPending", Qt::QueuedConnection);
}

void QrFilter::submitPending()
{
    const int fd = createFrameFd();
    if (fd < 0) {
        m_busy.storeRelease(0);
        return;
    }

    if (write(fd, m_pixels.constData(), size_t(m_pixels.size())) != ssize_t(m_pixels.size())) {
        close(fd);
        m_busy.storeRelease(0);
        return;
    }
    lseek(fd, 0, SEEK_SET);

    QDBusInterface iface(QLatin1String(kService), QLatin1String(kObjectPath),
                         QLatin1String(kInterface), QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        static int logged = 0;
        if (logged++ < 3)
            std::fprintf(stderr, "[%s] QR service unavailable: %s (%s)\n", timestamp(),
                         qPrintable(iface.lastError().message()),
                         qPrintable(iface.lastError().name()));
        close(fd);
        m_busy.storeRelease(0);
        return;
    }

    QList<QVariant> arguments;
    arguments << QVariant::fromValue(QDBusUnixFileDescriptor(fd))
              << QVariant::fromValue(quint32(m_pixels.size()))
              << QVariant::fromValue(m_width)
              << QVariant::fromValue(m_height)
              << QVariant::fromValue(kPixelFormat);

    m_fd = fd;      // closed once the service has read the buffer
    QDBusPendingCallWatcher *watcher =
            new QDBusPendingCallWatcher(iface.asyncCallWithArgumentList(QLatin1String("decodeFromDescriptor"),
                                                                        arguments),
                                        this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &QrFilter::callFinished);
}

void QrFilter::callFinished(QDBusPendingCallWatcher *watcher)
{
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
    m_busy.storeRelease(0);

    if (watcher->isError()) {
        // Most often the service still starting up on the first frame; the
        // next sample retries.
        static int logged = 0;
        if (logged++ < 3)
            std::fprintf(stderr, "[%s] QR decode failed: %s\n", timestamp(),
                         qPrintable(watcher->error().message()));
    } else {
        const QString text = QDBusReply<QString>(watcher->reply()).value();
        if (text.isEmpty()) {
            static int empty = 0;
            if (empty++ < 3)
                std::fprintf(stderr, "[%s] QR decode returned no code\n", timestamp());
        } else {
            std::fprintf(stderr, "[%s] QR decoded: %s\n", timestamp(),
                         qPrintable(text));
            emit decoded(text);
        }
    }
    std::fflush(stderr);

    watcher->deleteLater();
}
