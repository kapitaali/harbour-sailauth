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
                onClicked: {
                    // Silica would only show its generic error page if the
                    // scanner failed to compile, so surface the real reason.
                    var scanner = Qt.createComponent("ScanPage.qml")
                    if (scanner.status === Component.Error) {
                        var why = scanner.errorString()
                        console.error("ScanPage failed to load:", why)
                        toast.show("Scan failed: "
                                   + (why.length > 90 ? why.slice(0, 90) + "…" : why))
                    } else {
                        pageStack.push(scanner)
                    }
                }
            }

            TextField {
                id: issuerField
                label: "Issuer"
                placeholderText: "e.g. GitHub"
                width: parent.width
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: nameField.focus = true
            }

            TextField {
                id: nameField
                label: "Account name"
                placeholderText: "e.g. user@example.com"
                width: parent.width
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: secretField.focus = true
            }

            TextField {
                id: secretField
                label: "Secret key"
                placeholderText: "Base32 encoded secret"
                width: parent.width
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

    /*
     * A code ScanPage decoded belongs to this form: fill it in. Nothing pops
     * here — the scanner closes itself after showing "QR code scanned", and
     * a toast fired below surfaces once it is gone.
     */
    function applyScannedCode(text) {
        if (text.indexOf("otpauth://") !== 0)
            return

        var top = pageStack.currentPage
        if (!top || top.scannerPage !== true)
            return

        var parsed = importer.parseText(text)
        if (parsed.length === 0) {
            toast.show("Could not read that QR code")
            return
        }

        var account = parsed[0]
        issuerField.text = account.issuer
        nameField.text = account.name
        secretField.text = account.secret
        digitsField.text = account.digits
        periodField.text = account.period
    }

    Connections {
        target: qrFilter
        onDecoded: addPage.applyScannedCode(text)
    }

    Toast { id: toast }
}
