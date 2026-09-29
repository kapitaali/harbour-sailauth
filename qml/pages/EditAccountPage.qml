import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: editPage

    property int accountId: 0
    property string issuer: ""
    property string name: ""
    property string secret: ""
    property int digits: 6
    property int period: 30

    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: editColumn.height

        Column {
            id: editColumn
            width: parent.width - Theme.horizontalPageMargin * 2
            x: Theme.horizontalPageMargin
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Edit Account"
                // The column is inset by the page margin, so the header must
                // not add a second one.
                leftMargin: 0
            }

            TextField {
                id: issuerField
                label: "Issuer"
                placeholderText: "e.g. GitHub"
                width: parent.width
                text: editPage.issuer
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: nameField.focus = true
            }

            TextField {
                id: nameField
                label: "Account name"
                placeholderText: "e.g. user@example.com"
                width: parent.width
                text: editPage.name
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: secretField.focus = true
            }

            TextField {
                id: secretField
                label: "Secret key"
                placeholderText: "Base32 encoded secret"
                width: parent.width
                text: editPage.secret
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: digitsField.focus = true
            }

            TextField {
                id: digitsField
                label: "Digits"
                placeholderText: "6"
                width: parent.width
                inputMethodHints: Qt.ImhDigitsOnly
                text: String(editPage.digits)
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: periodField.focus = true
            }

            TextField {
                id: periodField
                label: "Period (seconds)"
                placeholderText: "30"
                width: parent.width
                inputMethodHints: Qt.ImhDigitsOnly
                text: String(editPage.period)
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: saveButton.focus = true
            }

            Button {
                id: saveButton
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Save"
                enabled: totp.validateSecret(secretField.text)
                onClicked: {
                    var digits = parseInt(digitsField.text) || 6
                    var period = parseInt(periodField.text) || 30
                    accountModel.updateAccount(editPage.accountId,
                                               issuerField.text.trim(),
                                               nameField.text.trim(),
                                               secretField.text.trim(),
                                               digits, period)
                    toast.show("Changes saved")
                    pageStack.pop()
                }
            }
        }
    }

    Toast { id: toast }
}
