import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import "../components"

/*
 * Import accounts from otpauth:// URIs — either from a file another
 * authenticator exported, or pasted straight into the text area.
 *
 * The file picker is pushed as a page (FilePickerPage is a Page, not a
 * Dialog: there is no open()). It pops itself once a selection is made and
 * reports it through selectedContentProperties.
 */
Page {
    id: importPage

    property var foundAccounts: []

    allowedOrientations: Orientation.Portrait

    function applyAccounts(accounts, source) {
        if (accounts.length === 0) {
            toast.show(source + ": no valid accounts found")
            return
        }
        foundAccounts = accounts
        importResults.visible = true
    }

    function doImport() {
        var added = importer.importAccounts(foundAccounts)
        if (added > 0) {
            accountModel.refresh()
            toast.show(added + " account" + (added === 1 ? "" : "s") + " imported")
            pageStack.pop()
        } else {
            toast.show("Nothing imported — duplicates or errors")
        }
    }

    Component {
        id: filePickerComponent

        FilePickerPage {
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties) {
                    importPage.applyAccounts(importer.parseFile(
                                                 selectedContentProperties.filePath), "File")
                }
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: importColumn.height + Theme.paddingLarge

        Column {
            id: importColumn
            width: parent.width - Theme.horizontalPageMargin * 2
            x: Theme.horizontalPageMargin
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Import Accounts"
                leftMargin: 0
            }

            Label {
                width: parent.width
                text: "Import accounts from a backup file exported by another " +
                      "authenticator app (e.g. GNOME Authenticator, Aegis, FreeOTP+)."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: "From file"
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Choose backup file"
                onClicked: pageStack.push(filePickerComponent)
            }

            Label {
                width: parent.width
                text: "Select a .txt file of otpauth:// URIs or a .json backup " +
                      "from Aegis, andOTP or GNOME Authenticator."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: "From text"
            }

            TextArea {
                id: textArea
                width: parent.width
                label: "Paste otpauth:// URIs"
                placeholderText: "otpauth://totp/..."
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Parse"
                enabled: textArea.text.length > 0
                onClicked: importPage.applyAccounts(importer.parseText(textArea.text), "Text")
            }

            SectionHeader {
                text: "Found accounts"
                visible: importResults.visible
            }

            Column {
                id: importResults
                width: parent.width
                visible: false
                spacing: Theme.paddingSmall

                Repeater {
                    id: importResultsList
                    model: importPage.foundAccounts

                    delegate: ListItem {
                        width: importResults.width
                        contentHeight: Theme.itemSizeSmall

                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.horizontalPageMargin
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
                id: importButton
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Import " + importPage.foundAccounts.length +
                      " account" + (importPage.foundAccounts.length === 1 ? "" : "s")
                visible: importResults.visible
                onClicked: importPage.doImport()
            }
        }
    }

    Toast { id: toast }
}
