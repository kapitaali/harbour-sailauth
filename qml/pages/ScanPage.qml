import QtQuick 2.0
import Sailfish.Silica 1.0
import QtMultimedia 5.6
import "../components"

/*
 * QR viewfinder. Pushed from AddAccountPage; popping returns to the manual
 * form, so there is deliberately no tab bar here — TabBar lives in
 * Sailfish.Silica.private, which Harbour does not allow.
 *
 * Decoding does NOT go through VideoOutput's `filters`: the camera service on
 * this device offers no renderer control, so VideoOutput uses Qt's overlay
 * backend, which runs no filter chain at all (see src/qrfilter.h for the full
 * findings). Instead the page takes still captures — camerabin's image
 * branch, the same path the gallery uses for photos — into a private cache
 * file (qrFilter.requestCapture()), and hands each written file back via
 * imageSaved (the preview signal imageCaptured never fires on this device;
 * only file captures are guaranteed). A decoded otpauth URI fills
 * AddAccountPage's form (it owns the form), but the scanner closes itself
 * only after saying so — an instant, silent pop read as "the scan failed".
 *
 * Focus runs continuously on purpose: stills taken with focus latched at
 * startup came out soft as the phone moved, and soft stills do not decode.
 */
Page {
    id: scanPage

    // Lets AddAccountPage tell a live scanner from one that was already popped.
    readonly property bool scannerPage: true

    allowedOrientations: Orientation.Portrait

    // Set once an otpauth code decoded: stops the camera, flips the reticle
    // green and delays the pop so the confirmation is actually seen.
    property bool succeeded: false

    // Consecutive capture failures, so a broken camera says so once instead
    // of silently never scanning.
    property int captureFailures: 0

    function noteCaptureFailure() {
        if (++captureFailures === 3) {
            toast.show("Camera capture failed")
        }
    }

    function succeed() {
        if (succeeded)
            return
        succeeded = true
        captureTimer.stop()
        camera.stop()
        successTimer.restart()
    }

    Camera {
        id: camera
        position: Camera.BackFace

        focus {
            focusMode: Camera.FocusContinuous
        }

        imageCapture {
            onImageSaved: function (id, fileName) {
                scanPage.captureFailures = 0
                qrFilter.submitImageFile(fileName)
            }
            onCaptureFailed: function (id, message) {
                scanPage.noteCaptureFailure()
            }
            // Fire the next capture the moment the camera is ready again —
            // polling alone missed the short ready windows.
            onReadyForCaptureChanged: {
                if (camera.imageCapture.ready && !scanPage.succeeded) {
                    qrFilter.requestCapture()
                }
            }
        }

        Component.onCompleted: qrFilter.attachCamera(camera)
    }

    VideoOutput {
        id: viewfinder
        anchors.fill: parent
        source: camera
    }

    // Fallback beat: the readiness edge above does most of the work; this
    // covers page activation and any missed edge. requestCapture() itself
    // refuses while a capture is in flight, so the two never queue up.
    // It also watches for a dead camera (camerabin has been seen to drop to
    // Unloaded after a pipeline error) and restarts it after it stays down
    // for a couple of beats.
    property int unloadedTicks: 0
    property int cameraRestarts: 0
    Timer {
        id: captureTimer
        interval: 700
        repeat: true
        running: scanPage.status === PageStatus.Active && !scanPage.succeeded
        onTriggered: {
            if (camera.status === Camera.UnloadedStatus) {
                if (++scanPage.unloadedTicks >= 3 && scanPage.cameraRestarts < 5) {
                    scanPage.unloadedTicks = 0
                    scanPage.cameraRestarts++
                    camera.start()
                }
            } else {
                scanPage.unloadedTicks = 0
            }
            qrFilter.requestCapture()
        }
    }

    // Confirmation linger: banner + green reticle stay up for a beat before
    // returning to the (already filled) form.
    Timer {
        id: successTimer
        interval: 1600
        onTriggered: {
            if (pageStack.currentPage === scanPage) {
                pageStack.pop()
            }
        }
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            captureFailures = 0
            camera.start()
        } else if (status === PageStatus.Inactive && !succeeded) {
            camera.stop()
        }
    }

    Rectangle {
        id: reticle
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.68
        height: width
        radius: Theme.paddingSmall
        color: "transparent"
        border.color: scanPage.succeeded ? "#33cc33" : Theme.highlightColor
        border.width: 3
        opacity: scanPage.succeeded ? 1.0 : 0.55
    }

    /*
     * The confirmation itself, in the middle of the screen: during a scan
     * the eye is on the reticle, so a toast down at the edge was easy to
     * miss even while the frame turned green. Appears only on success and
     * lingers until successTimer pops the page.
     */
    Rectangle {
        id: successBanner
        anchors.centerIn: parent
        width: successLabel.width + Theme.paddingLarge * 2
        height: successLabel.height + Theme.paddingMedium * 2
        radius: Theme.paddingSmall
        color: Theme.highlightBackgroundColor
        opacity: scanPage.succeeded ? 1.0 : 0.0
        visible: opacity > 0.0
        z: 1

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }

        Label {
            id: successLabel
            anchors.centerIn: parent
            text: "QR code scanned"
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeLarge
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.paddingLarge
        anchors.horizontalCenter: parent.horizontalCenter
        width: scanHint.width + Theme.paddingLarge * 2
        height: scanHint.height + Theme.paddingMedium
        radius: Theme.paddingSmall
        color: Theme.highlightBackgroundColor
        opacity: 0.85

        Label {
            id: scanHint
            anchors.centerIn: parent
            text: scanPage.succeeded ? "QR code scanned"
                                     : "Point the camera at a QR code"
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Button {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.paddingLarge * 5
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Enter the key manually"
        onClicked: pageStack.pop()
    }

    Connections {
        target: qrFilter

        onDecoded: {
            if (text.indexOf("otpauth://") !== 0) {
                // Anything else is a code we cannot use; keep scanning.
                toast.show("Not an authenticator QR code")
            } else {
                // AddAccountPage fills its form; the scanner confirms and
                // closes itself via succeed().
                scanPage.succeed()
            }
        }
    }

    Toast { id: toast }
}
