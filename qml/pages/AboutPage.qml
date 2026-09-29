import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: aboutPage

    property string sourceLink: "https://github.com/kapitaali/harbour-sailotp"
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
                // The real build version, handed over by the RPM build from
                // the git tag; hand-built binaries report "dev".
                text: "Version " + appVersion
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                width: parent.width
                text: "A native TOTP 2FA authenticator for Sailfish OS.\n\n" +
                      "• RFC 6238 compliant TOTP codes\n" +
                      "• Scan QR codes to add accounts\n" +
                      "• Import accounts from other authenticators\n" +
                      "• Local storage — no cloud, no tracking\n\n" +
                      "All codes and accounts stay on your device."
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: "Support"
            }

            Label {
                width: parent.width
                text: "We love Open Source software and the Jolla ecosystem. " +
                      "If you want to support me or my work, please leave " +
                      "some tip here:"
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Leave a tip"
                onClicked: Qt.openUrlExternally(aboutPage.tipLink)
            }

            SectionHeader {
                text: "Source code"
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "View on GitHub"
                onClicked: Qt.openUrlExternally(aboutPage.sourceLink)
            }

            Label {
                width: parent.width
                text: "SailOTP is free and open source software."
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
