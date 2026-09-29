/*
 * QR code decoding.
 *
 * ScanPage's Camera takes still captures (the same camerabin image branch the
 * gallery app uses for photos); Camera.imageCapture emits imageSaved with the
 * file it wrote, and that file is handed to submitImageFile(), converted to
 * tightly packed ARGB32 and passed to the barcode service the system ships —
 * org.amberapi.zxing (zxing-daemon from the qr-filter-qml-plugin package) —
 * which answers with the text a code carries. The temporary file is deleted
 * after reading so scanning never litters the gallery.
 *
 * Talking to that service over Qt5DBus instead of importing Amber.QrFilter is
 * deliberate: Harbour rejects the Amber.* QML modules, while Qt5DBus and
 * QtMultimedia are both on the allowed list, and a type built into the
 * application needs no module at all (it reaches QML as a context property).
 *
 * WHY STILL CAPTURES INSTEAD OF VIEWFINDER FRAMES: the first implementation
 * used a QAbstractVideoFilter on the VideoOutput, but on this device the
 * camera service exposes neither QVideoRendererControl nor the GStreamer sink
 * control, so QML VideoOutput falls back to Qt's window/overlay backend —
 * and that backend contains no filter chain at all (Qt 5.6,
 * qtmultimediaquicktools/qdeclarativevideooutput_window.cpp: the string
 * "filter" does not occur). Frames also cannot be taken from the camera any
 * other way: QVideoProbe's setSource() fails, and a custom
 * QCamera::setViewfinder() surface receives nothing. Still captures do work —
 * proven on the device: capture() writes IMG_*.jpg to the Pictures folder.
 * (The QML-visible preview signal imageCaptured never fires here — camerabin
 * only emits it with a preview buffer message that does not arrive — which is
 * why the flow keys off imageSaved, the signal the written file guarantees.)
 *
 * tests/probe_zxing.cpp established on the phone which pixel formats the
 * daemon accepts: Format_ARGB32 decodes, everything else tested (RGB32,
 * RGB24, RGB565, Y8) comes back empty. That is why conversion targets ARGB32
 * rather than passing the capture's format through.
 */
#ifndef QRFILTER_H
#define QRFILTER_H

#include <QAtomicInt>
#include <QByteArray>
#include <QImage>
#include <QPointer>
#include <QString>
#include <QTime>

class QDBusPendingCallWatcher;

class QrFilter : public QObject
{
    Q_OBJECT
public:
    explicit QrFilter(QObject *parent = Q_NULLPTR);
    ~QrFilter() override;

    /**
     * Point the scanner at the QML Camera. The device encodes 8192x6144
     * stills by default — a ~3 s capture cycle — so the largest supported
     * capture resolution no bigger than kCaptureMaxEdge (1920x1440; 2560
     * kills the camera, see qrfilter.cpp) is requested. Captures are
     * directed at a private file under the app's cache directory (see
     * requestCapture()) so scanning can never leave photos in the gallery.
     * Everything is logged to stderr: Sailfish's QtBuild routes qWarning to
     * the system journal, which an app process cannot read back.
     */
    Q_INVOKABLE void attachCamera(QObject *qmlCamera);

    /**
     * Ask the camera for one still, written to the scanner's private cache
     * file (imageSaved reports that path; submitImageFile() reads and
     * deletes it). Returns false without requesting anything when the
     * camera is not ready — so callers can fire it on every readiness edge
     * and from a fallback timer without queueing captures.
     */
    Q_INVOKABLE bool requestCapture();

    /**
     * Hand over the file Camera.imageCapture.onImageSaved reported. Reads it
     * (EXIF orientation honoured, scaled down only beyond kDecodeMaxEdge),
     * deletes it, and queues the decode.
     */
    Q_INVOKABLE void submitImageFile(const QString &fileName);

signals:
    /** The text carried by the code that was just decoded. */
    void decoded(const QString &text);

private slots:
    /** Queued: writes the pixels out and calls the service. */
    void submitPending();
    void callFinished(QDBusPendingCallWatcher *watcher);
    /** Timestamped readiness log line (cadence diagnostics). */
    void logReady(bool ready);

private:
    /** Converts one still to packed ARGB32 and queues the decode. */
    void submitFrame(const QImage &image);

    /**
     * Asks the camera for a scan-sized capture resolution. Returns true when
     * done (sizes set, or the camera had nothing to offer); false means
     * "camera still starting, try again later".
     */
    bool configureResolution(QObject *capture);
    void attemptResolution(int attempt);

    QPointer<QObject> m_camera;         // the QML Camera
    QPointer<QObject> m_captureGroup;   // the camera's imageCapture group
    QString m_scanFilePath;             // private file captures are written to
    bool m_resolutionDone;              // camera configured (or gave up)
    QAtomicInt m_busy;         // one decode in flight at a time
    QTime m_lastSubmit;        // decodes are sampled a few times per second
    int m_frames;              // stills offered so far
    QByteArray m_pixels;       // tightly packed ARGB32 payload for submitPending()
    int m_width;
    int m_height;
    int m_fd;                  // kept open until the service has read it
};

#endif // QRFILTER_H
