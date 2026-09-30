import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

/*
 * Export every account to an encrypted file in ~/Documents.
 *
 * The passphrase is the only key — there is no recovery, which the page
 * says before the Export button becomes reachable. Errors stay on this
 * page as a toast; success emits done() so the Settings page (what
 * survives the pop) reports the confirmation for its full length.
 */
Page {
    id: exportPage

    signal done(string message)

    allowedOrientations: Orientation.Portrait

    // Fixed when the page opens: the export target must not shift while
    // the user is typing the passphrase.
    property string targetPath: backup.defaultBackupPath()

    function exportNow() {
        var error = backup.saveBackup(targetPath, passphrase.text)
        if (error.length > 0) {
            toast.show(error)
        } else {
            var name = targetPath.substring(targetPath.lastIndexOf('/') + 1)
            exportPage.done("Backup saved to Documents/" + name)
            pageStack.pop()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            x: Theme.horizontalPageMargin
            width: parent.width - Theme.horizontalPageMargin * 2
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Export backup"
                leftMargin: 0
            }

            Label {
                width: parent.width
                text: "Write an encrypted copy of all accounts to this " +
                      "device's Documents folder, where the Files app can " +
                      "reach it."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Label {
                width: parent.width
                text: "File: Documents/" +
                      exportPage.targetPath.substring(
                          exportPage.targetPath.lastIndexOf('/') + 1)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                truncationMode: TruncationMode.Fade
            }

            SectionHeader {
                text: "Passphrase"
            }

            TextField {
                id: passphrase
                width: parent.width
                label: "Passphrase"
                placeholderText: "Encrypts the file"
                echoMode: TextInput.Password
                EnterKey.enabled: text.length > 0
                EnterKey.onClicked: confirm.forceActiveFocus()
            }

            TextField {
                id: confirm
                width: parent.width
                label: "Repeat passphrase"
                placeholderText: "Must match"
                echoMode: TextInput.Password
                EnterKey.enabled: text.length > 0 && text === passphrase.text
                EnterKey.onClicked: exportPage.exportNow()
            }

            Label {
                width: parent.width
                visible: confirm.text.length > 0
                         && confirm.text !== passphrase.text
                text: "Passphrases do not match"
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Label {
                width: parent.width
                text: "The backup opens only with this passphrase — there " +
                      "is no recovery if it is lost. Keep it somewhere safe."
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Export"
                enabled: passphrase.text.length > 0
                         && passphrase.text === confirm.text
                onClicked: exportPage.exportNow()
            }
        }
    }

    Toast { id: toast }
}
