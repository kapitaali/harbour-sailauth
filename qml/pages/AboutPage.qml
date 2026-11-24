import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: aboutPage

    property string sourceLink: "https://github.com/kapitaali/harbour-sailauth"
    property string tipLink: "https://ko-fi.com/kapitaali"

    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: aboutColumn.height + Theme.paddingLarge

        Column {
            id: aboutColumn
            width: parent.width - Theme.horizontalPageMargin * 2
            x: Theme.horizontalPageMargin
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("About")
                leftMargin: 0
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                // Shipped inside qml/img/ so the path resolves wherever the
                // package installs (a theme lookup would only find icons the
                // system itself ships).
                source: Qt.resolvedUrl("../img/harbour-sailauth.png")
                width: 86
                height: 86
            }

            Label {
                width: parent.width
                text: "SailAuth"
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeExtraLarge
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                width: parent.width
                // The real build version, handed over by the RPM build from
                // the git tag; hand-built binaries report "dev".
                text: qsTr("Version %1").arg(appVersion)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                width: parent.width
                text: qsTr("A native TOTP 2FA authenticator for Sailfish OS.\n\n• RFC 6238 compliant TOTP codes\n• Scan QR codes to add accounts\n• Import accounts from other authenticators\n• Local storage — no cloud, no tracking\n\nAll codes and accounts stay on your device.")
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: qsTr("Support")
            }

            Label {
                width: parent.width
                text: qsTr("We love Open Source software and the Jolla ecosystem. If you want to support me or my work, please leave some tip here:")
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Leave a tip")
                onClicked: Qt.openUrlExternally(aboutPage.tipLink)
            }

            SectionHeader {
                text: qsTr("Source code")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("View on GitHub")
                onClicked: Qt.openUrlExternally(aboutPage.sourceLink)
            }

            Label {
                width: parent.width
                text: qsTr("SailAuth is free and open source software.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: qsTr("License")
            }

            Label {
                width: parent.width
                text: "GPL-3.0-only"
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
