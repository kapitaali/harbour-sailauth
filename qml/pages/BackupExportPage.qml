import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

/*
 * Export every account, in one of two shapes:
 *
 * - Encrypted backup (.enc): AES-256-GCM under a passphrase — only
 *   SailAuth opens it, and only with that passphrase.
 * - Plain text (.txt): one otpauth:// URI per line — what GNOME
 *   Authenticator restores under "Authenticator" (FreeOTP+ compatible)
 *   and other authenticator apps read directly.
 *
 * The passphrase block belongs to the encrypted format only; the plain
 * text file gets its own warning instead. Errors stay on this page as a
 * toast; success emits done() so the Settings page (what survives the
 * pop) reports the confirmation for its full length.
 */
Page {
    id: exportPage

    signal done(string message)

    allowedOrientations: Orientation.Portrait

    property bool encrypted: formatBox.currentIndex === 0

    // Re-evaluated whenever the format changes, so each shape targets its
    // own freshly stamped file name.
    property string targetPath: encrypted ? backup.defaultBackupPath()
                                          : backup.defaultTextExportPath()

    function fileName() {
        return targetPath.substring(targetPath.lastIndexOf('/') + 1)
    }

    function exportNow() {
        var error = encrypted
                ? backup.saveBackup(targetPath, passphrase.text)
                : backup.saveTextExport(targetPath)
        if (error.length > 0) {
            toast.show(error)
        } else {
            exportPage.done(encrypted
                            ? qsTr("Backup saved to Documents/%1").arg(fileName())
                            : qsTr("Export saved to Documents/%1").arg(fileName()))
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
                title: qsTr("Export")
                leftMargin: 0
            }

            SectionHeader { text: qsTr("Format") }

            ComboBox {
                id: formatBox
                width: parent.width
                label: qsTr("Format")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Encrypted backup (.enc)") }
                    MenuItem { text: qsTr("Plain text for other apps (.txt)") }
                }
            }

            Label {
                width: parent.width
                text: exportPage.encrypted
                      ? qsTr("Write an encrypted copy of all accounts to this device's Documents folder, where the Files app can reach it.")
                      : qsTr("Write one otpauth:// URI per line — readable by GNOME Authenticator, FreeOTP+, Aegis and other apps.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Label {
                width: parent.width
                text: qsTr("File: Documents/%1").arg(exportPage.fileName())
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                truncationMode: TruncationMode.Fade
            }

            SectionHeader {
                text: qsTr("Passphrase")
                visible: exportPage.encrypted
            }

            TextField {
                id: passphrase
                visible: exportPage.encrypted
                width: parent.width
                label: qsTr("Passphrase")
                placeholderText: qsTr("Encrypts the file")
                echoMode: TextInput.Password
                EnterKey.enabled: text.length > 0
                EnterKey.onClicked: confirm.forceActiveFocus()
            }

            TextField {
                id: confirm
                visible: exportPage.encrypted
                width: parent.width
                label: qsTr("Repeat passphrase")
                placeholderText: qsTr("Must match")
                echoMode: TextInput.Password
                EnterKey.enabled: text.length > 0 && text === passphrase.text
                EnterKey.onClicked: exportPage.exportNow()
            }

            Label {
                width: parent.width
                visible: exportPage.encrypted && confirm.text.length > 0
                         && confirm.text !== passphrase.text
                text: qsTr("Passphrases do not match")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Label {
                width: parent.width
                visible: exportPage.encrypted
                text: qsTr("The backup opens only with this passphrase — there is no recovery if it is lost. Keep it somewhere safe.")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Label {
                width: parent.width
                visible: !exportPage.encrypted
                text: qsTr("Plain text is not encrypted: anyone who gets this file can generate your codes. Delete it after the other app has imported it.")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Export")
                enabled: !exportPage.encrypted
                         || (passphrase.text.length > 0
                             && passphrase.text === confirm.text)
                onClicked: exportPage.exportNow()
            }
        }
    }

    Toast { id: toast }
}
