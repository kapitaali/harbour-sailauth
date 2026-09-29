import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

/*
 * Add an account by typing its details.
 *
 * No tab bar: Sailfish's TabBar is a private Silica type (Harbour disallows
 * Sailfish.Silica.private), so scanning lives on its own page instead, one
 * push away from this form.
 */
Page {
    id: addPage

    property string scannedSecret: ""
    property string scannedIssuer: ""
    property string scannedName: ""

    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: formColumn.height + Theme.paddingLarge

        Column {
            id: formColumn
            width: parent.width - Theme.horizontalPageMargin * 2
            x: Theme.horizontalPageMargin
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Add Account"
                leftMargin: 0
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Scan QR code instead"
                onClicked: pageStack.push(Qt.resolvedUrl("ScanPage.qml"))
            }

            TextField {
                id: issuerField
                label: "Issuer"
                placeholderText: "e.g. GitHub"
                width: parent.width
                text: addPage.scannedIssuer
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: nameField.focus = true
            }

            TextField {
                id: nameField
                label: "Account name"
                placeholderText: "e.g. user@example.com"
                width: parent.width
                text: addPage.scannedName
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: secretField.focus = true
            }

            TextField {
                id: secretField
                label: "Secret key"
                placeholderText: "Base32 encoded secret"
                width: parent.width
                text: addPage.scannedSecret
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: digitsField.focus = true
            }

            Label {
                width: parent.width
                visible: secretField.text.length > 0 && !totp.validateSecret(secretField.text)
                text: "Not a valid Base32 secret (A–Z and 2–7 only)"
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            TextField {
                id: digitsField
                label: "Digits"
                placeholderText: "6"
                width: parent.width
                inputMethodHints: Qt.ImhDigitsOnly
                text: "6"
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: periodField.focus = true
            }

            TextField {
                id: periodField
                label: "Period (seconds)"
                placeholderText: "30"
                width: parent.width
                inputMethodHints: Qt.ImhDigitsOnly
                text: "30"
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: saveButton.focus = true
            }

            Button {
                id: saveButton
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Add account"
                enabled: totp.validateSecret(secretField.text)
                onClicked: {
                    var digits = parseInt(digitsField.text) || 6
                    var period = parseInt(periodField.text) || 30
                    accountModel.addAccount(issuerField.text.trim(),
                                            nameField.text.trim(),
                                            secretField.text.trim(),
                                            digits, period)
                    toast.show("Account added")
                    pageStack.pop()
                }
            }
        }
    }

    Toast { id: toast }
}
