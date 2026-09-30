import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: settingsPage

    allowedOrientations: Orientation.Portrait

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
                checked: settings.soundEnabled
                onCheckedChanged: settings.soundEnabled = checked
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
