import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    id: appWindow

    // True from launch until the start gate resolves.  The setting is read
    // synchronously (QSettings), so with the setting off nothing below ever
    // becomes visible or runs.
    property bool locked: appLock.requireLock

    initialPage: Component { MainPage {} }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    Component.onCompleted: appLock.startGate()

    Connections {
        target: appLock
        onAuthenticated: appWindow.locked = false
        onGateFinished: {
            if (passed)
                appWindow.locked = false
        }
    }

    // Start gate: an opaque cover for the (rare) moments when no system
    // security-code prompt is on screen — before the prompt appears and if
    // the user cancels it (then Unlock retries, Close exits).
    Item {
        id: lockGate
        anchors.fill: parent
        z: 1000
        visible: appWindow.locked

        Rectangle {
            anchors.fill: parent
            color: Theme.primaryColor
        }

        // Swallow gestures so the pages underneath stay unreachable.
        MouseArea {
            anchors.fill: parent
            onPressed: {}
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - Theme.horizontalPageMargin * 2
            spacing: Theme.paddingLarge

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: "SailOTP is locked"
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "Enter the device security code to open SailOTP."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Unlock"
                onClicked: appLock.authenticate()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Close SailOTP"
                onClicked: Qt.quit()
            }
        }
    }
}
