import QtQuick 2.0
import Sailfish.Silica 1.0
import QtMultimedia 5.6

/*
 * QR viewfinder. Pushed from AddAccountPage; popping returns to the manual
 * form, so there is deliberately no tab bar here — TabBar lives in
 * Sailfish.Silica.private, which Harbour does not allow.
 *
 * Decoding is Phase 2: this page only frames the code and reports what the
 * camera is doing. When the decoder lands it will read frames from this same
 * `camera`/`videoOutput` pair.
 */
Page {
    id: scanPage

    allowedOrientations: Orientation.Portrait

    Camera {
        id: camera
        active: true
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        source: camera
        autoOrientation: true
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
            text: "Point the camera at a QR code"
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - Theme.horizontalPageMargin * 2
        spacing: Theme.paddingLarge

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: "QR decoding is not implemented yet"
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Enter the key manually"
            onClicked: pageStack.pop()
        }
    }
}
