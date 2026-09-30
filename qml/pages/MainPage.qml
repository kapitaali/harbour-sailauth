import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

/*
 * Main page: one row per account — issuer/account name on the left, the live
 * code on the right, and a bar along the bottom showing how much of the
 * current period is left. Tap copies the code; long press edits or deletes.
 * The field under the header filters the list by issuer or account name —
 * pushed explicitly with the keyboard's Search (Enter) key rather than live:
 * refreshing the rows while typing closed the virtual keyboard every time
 * (focus was cleared to nothing on each update; root cause never found, so
 * updates simply never happen mid-typing). Clearing the field restores the
 * full list.
 *
 * The search field lives in the list's header (the standard Silica pattern):
 * with a fixed field above the list, the PullDownMenu — which rests at
 * `flickable.originY - height`, i.e. ABOVE the list's top edge — dangled
 * right over it and swallowed taps. Inside the header the menu rests off the
 * top of the page, as it does in stock apps.
 *
 * Note: ids declared inside the header's object tree are not resolvable from
 * page-level code (the ViewPlaceholder binding hit "ReferenceError:
 * searchField is not defined") — reference them only from within the header
 * itself.
 */
Page {
    id: mainPage

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: filteredAccountModel

        header: Column {
            width: listView.width

            PageHeader {
                title: "SailOTP"
            }

            SearchField {
                id: searchField
                x: Theme.horizontalPageMargin
                width: parent.width - Theme.horizontalPageMargin * 2
                placeholderText: "Search accounts"
                // Bound to the *source* count: the field must never blink
                // out from under the user while a filter matches nothing.
                visible: accountModel.count > 0

                // Search is pushed explicitly, not filtered live. Every
                // refresh of the results list while typing closed the
                // virtual keyboard — the log showed focus being cleared to
                // nothing each time — so results now update only when the
                // keyboard's Enter/Search key is pressed, or when the field
                // is cleared with its × button.
                // Note: the enter-key API is the EnterKey ATTACHED property
                // (attached to the field); inline enterKeyText /
                // onEnterKeyClicked do not exist on this Silica and break
                // the whole page ("Type MainPage unavailable").
                EnterKey.enabled: text.length > 0
                EnterKey.text: "Search"
                EnterKey.onClicked: filteredAccountModel.filterText = text
                onTextChanged: {
                    // The × button (or deleting everything): no filter left.
                    if (text.length === 0)
                        filteredAccountModel.filterText = ""
                    // Belt and braces: if focus blips away mid-type, take it
                    // straight back. Typing alone causes no model update, so
                    // this is a no-op in the normal case.
                    forceActiveFocus()
                }
            }
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

        /*
         * Deliberately derived from the counts only — referencing
         * searchField from here used to raise "ReferenceError: searchField
         * is not defined": the placeholder's bindings evaluate before the
         * header object exists.
         */
        ViewPlaceholder {
            enabled: listView.count === 0
            text: accountModel.count > 0 ? "No matches" : "No accounts yet"
            hintText: accountModel.count > 0 ? "Try a different search"
                                            : "Pull down to add an account"
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
            MenuItem {
                text: "About"
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
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
