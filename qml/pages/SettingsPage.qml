import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: settingsPage

    allowedOrientations: Orientation.Portrait

    // Refresh the device-lock state so the toggle below can refuse to
    // enable itself when no PIN/pattern is set.
    Component.onCompleted: appLock.refresh()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: settingsColumn.height + Theme.paddingLarge

        Column {
            id: settingsColumn
            width: parent.width - Theme.horizontalPageMargin * 2
            x: Theme.horizontalPageMargin
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Settings"
                leftMargin: 0
            }

            SectionHeader {
                text: "General"
            }

            TextSwitch {
                id: soundSwitch
                text: "Sound feedback"
                description: "Play a sound when a code is copied"
                checked: true
            }

            SectionHeader {
                text: "Security"
            }

            TextSwitch {
                id: lockSwitch
                text: "Require device lock"
                description: "Require the device PIN or pattern to open SailOTP"
                checked: appLock.requireLock
                onCheckedChanged: {
                    // Guard so the initial binding write is a no-op and an
                    // aborted enable cannot loop.
                    if (checked === appLock.requireLock)
                        return
                    if (checked && appLock.lockQueryFailed) {
                        // The daemon is unreachable (sandboxed launch
                        // without a devicelock profile) — do not claim no
                        // PIN is set when we simply could not check.
                        toast.show("Can't reach the device lock service")
                        checked = false
                    } else if (checked && !appLock.securityCodeSet) {
                        toast.show("No device lock is set — add one in Settings first")
                        checked = false
                    } else {
                        appLock.requireLock = checked
                    }
                }
            }

            SectionHeader {
                text: "Data"
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Export backup"
                onClicked: toast.show("Encrypted backup is not implemented yet")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Import backup"
                onClicked: toast.show("Encrypted backup is not implemented yet")
            }

            Label {
                width: parent.width
                text: "Accounts are stored only in this device's app data " +
                      "directory. Nothing is uploaded anywhere."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: "About"
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "About SailOTP"
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }
    }

    Toast { id: toast }
}
