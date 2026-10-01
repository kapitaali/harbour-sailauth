import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import "../components"

/*
 * Restore accounts from an encrypted SailAuth backup.
 *
 * Flow mirrors ImportPage: pick the file, then — only after decrypting
 * succeeded — show what is inside, and write to the database when the
 * user taps Restore. Restoration goes through Importer::importAccounts,
 * the same duplicate-skipping path the plain-text import uses. Errors
 * toast on this page; success emits done() for the Settings page to
 * report after the pop.
 */
Page {
    id: backupImportPage

    signal done(string message)

    property string filePath: ""
    property var foundAccounts: []

    allowedOrientations: Orientation.Portrait

    Component {
        id: filePickerComponent

        FilePickerPage {
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties) {
                    backupImportPage.fileChosen(
                                selectedContentProperties.filePath)
                }
            }
        }
    }

    function fileChosen(path) {
        filePath = path
        foundAccounts = []
        passphrase.text = ""
    }

    function openBackup() {
        foundAccounts = []
        var result = backup.openBackup(filePath, passphrase.text)
        if (!result.ok) {
            toast.show(result.error)
            return
        }
        foundAccounts = result.accounts
    }

    function doRestore() {
        var added = importer.importAccounts(foundAccounts)
        if (added > 0) {
            accountModel.refresh()
            backupImportPage.done(added + (added === 1 ? " account" : " accounts")
                                  + " restored")
            pageStack.pop()
        } else {
            toast.show("Nothing restored — duplicates or errors")
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
                title: "Import backup"
                leftMargin: 0
            }

            Label {
                width: parent.width
                text: "Restore accounts from a backup file this app " +
                      "exported. The passphrase you chose when exporting " +
                      "is required."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: "File"
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: backupImportPage.filePath.length > 0
                      ? backupImportPage.filePath.substring(
                            backupImportPage.filePath.lastIndexOf('/') + 1)
                      : "Choose backup file"
                onClicked: pageStack.push(filePickerComponent)
            }

            SectionHeader {
                text: "Passphrase"
                visible: backupImportPage.filePath.length > 0
            }

            TextField {
                id: passphrase
                width: parent.width
                visible: backupImportPage.filePath.length > 0
                label: "Passphrase"
                placeholderText: "The one used when exporting"
                echoMode: TextInput.Password
                EnterKey.enabled: text.length > 0
                EnterKey.onClicked: backupImportPage.openBackup()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Read backup"
                visible: backupImportPage.filePath.length > 0
                enabled: passphrase.text.length > 0
                onClicked: backupImportPage.openBackup()
            }

            SectionHeader {
                text: "Accounts in backup"
                visible: backupImportPage.foundAccounts.length > 0
            }

            Column {
                id: results
                width: parent.width
                visible: backupImportPage.foundAccounts.length > 0
                spacing: Theme.paddingSmall

                Repeater {
                    model: backupImportPage.foundAccounts

                    delegate: ListItem {
                        width: results.width
                        contentHeight: Theme.itemSizeSmall

                        Label {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - Theme.horizontalPageMargin * 2
                            text: (modelData.issuer.length > 0
                                   ? modelData.issuer + " — " : "") + modelData.name
                            color: Theme.primaryColor
                            font.pixelSize: Theme.fontSizeSmall
                            truncationMode: TruncationMode.Fade
                        }
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Restore " + backupImportPage.foundAccounts.length +
                      " account" +
                      (backupImportPage.foundAccounts.length === 1 ? "" : "s")
                visible: backupImportPage.foundAccounts.length > 0
                onClicked: backupImportPage.doRestore()
            }
        }
    }

    Toast { id: toast }
}
