import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: aboutPage

    property string supportLink: "https://example.com/tip"

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
                title: "About"
                leftMargin: 0
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                // Shipped inside qml/img/ so the path resolves wherever the
                // package installs (a theme lookup would only find icons the
                // system itself ships).
                source: Qt.resolvedUrl("../img/harbour-sailotp.png")
                width: 86
                height: 86
            }

            Label {
                width: parent.width
                text: "SailOTP"
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeExtraLarge
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                width: parent.width
                text: "Version 0.1.0"
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                width: parent.width
                text: "A native TOTP authenticator for Sailfish OS.\n\n" +
                      "• RFC 6238 compliant TOTP codes\n" +
                      "• Local storage (no cloud, no tracking)\n" +
                      "• Import accounts from other authenticators\n" +
                      "• Clean Sailfish Silica UI\n\n" +
                      "All data stays on your device."
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: "Support"
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Tip the developer"
                onClicked: Qt.openUrlExternally(aboutPage.supportLink)
            }

            Label {
                width: parent.width
                text: "If you find this app useful, consider sending a small tip to support development."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: "License"
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
