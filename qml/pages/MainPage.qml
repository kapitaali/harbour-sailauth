import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

/*
 * Main page: one row per account — issuer/account name on the left, the live
 * code on the right, and a bar along the bottom showing how much of the
 * current period is left. Tap copies the code; long press edits or deletes.
 */
Page {
    id: mainPage

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: accountModel

        header: PageHeader {
            title: "SailOTP"
        }

        delegate: ListItem {
            id: accountItem
            width: listView.width
            contentHeight: Theme.itemSizeLarge

            Column {
                id: textColumn
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - Theme.horizontalPageMargin * 2 - codeLabel.width - Theme.paddingLarge

                Label {
                    width: parent.width
                    text: model.issuer.length > 0 ? model.issuer : model.name
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeMedium
                    truncationMode: TruncationMode.Fade
                }

                Label {
                    width: parent.width
                    visible: text.length > 0
                    text: model.name
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    truncationMode: TruncationMode.Fade
                }
            }

            Label {
                id: codeLabel
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                text: model.code
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                font.bold: true
                font.family: "monospace"
            }

            // Progress bar: fraction of the current period already elapsed.
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 2
                color: Theme.primaryColor
                opacity: 0.2

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * Math.min(1, 1 - (model.remaining / model.period))
                    color: Theme.highlightColor
                    opacity: 1.0

                    Behavior on width {
                        NumberAnimation { duration: 200 }
                    }
                }
            }

            onClicked: {
                clipboardHelper.setText(model.code)
                toast.show("Code copied")
            }

            menu: ContextMenu {
                MenuItem {
                    text: "Edit"
                    onClicked: pageStack.push(Qt.resolvedUrl("EditAccountPage.qml"), {
                        accountId: model.accountId,
                        issuer: model.issuer,
                        name: model.name,
                        secret: model.secret,
                        digits: model.digits,
                        period: model.period
                    })
                }
                MenuItem {
                    text: "Delete"
                    onClicked: pageStack.push(Qt.resolvedUrl("ConfirmDeletePage.qml"), {
                        accountId: model.accountId,
                        issuer: model.issuer,
                        name: model.name
                    })
                }
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0
            text: "No accounts yet"
            hintText: "Pull down to add an account"
        }

        PullDownMenu {
            MenuItem {
                text: "Add account"
                onClicked: pageStack.push(Qt.resolvedUrl("AddAccountPage.qml"))
            }
            MenuItem {
                text: "Import accounts"
                onClicked: pageStack.push(Qt.resolvedUrl("ImportPage.qml"))
            }
            MenuItem {
                text: "Settings"
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
        }

        PushUpMenu {
            MenuItem {
                text: "About"
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }
    }

    /*
     * One tick per second. This only notifies the delegates that the code and
     * the countdown changed — resetting the whole model every second would
     * tear down each delegate and lose scroll position and open menus.
     * Whenever the time window rolls over the model reports both roles again,
     * so the codes themselves are always recomputed from the clock.
     */
    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: accountModel.tick()
    }

    Toast { id: toast }
}
