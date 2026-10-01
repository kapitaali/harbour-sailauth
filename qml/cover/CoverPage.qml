import QtQuick 2.0
import Sailfish.Silica 1.0

/*
 * Cover: the first account's live code plus a countdown bar, and one action
 * (add) reachable straight from the multitasking view.
 *
 * `appWindow` is the id of the ApplicationWindow in harbour-sailauth.qml.
 * Covers are instantiated by that ApplicationWindow, so the id resolves; the
 * push happens only after activate() because a covered app's page stack is
 * not interactive yet.
 */
CoverBackground {
    id: cover

    property string coverCode: ""
    property int coverRemaining: 0
    property int coverPeriod: 30
    property string coverLabel: ""

    function update() {
        if (accountModel.count > 0) {
            cover.coverCode = accountModel.generateCode(0)
            cover.coverRemaining = accountModel.remainingSeconds(0)
            cover.coverPeriod = 30
            cover.coverLabel = accountModel.getAccountIssuer(0)
        } else {
            cover.coverCode = ""
            cover.coverRemaining = 0
            cover.coverLabel = ""
        }
    }

    Label {
        id: coverTitle
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.paddingLarge
        text: "SailAuth"
        color: Theme.primaryColor
        font.pixelSize: Theme.fontSizeMedium
    }

    Label {
        id: coverIssuerLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: coverTitle.bottom
        anchors.topMargin: Theme.paddingSmall
        width: parent.width - Theme.horizontalPageMargin * 2
        horizontalAlignment: Text.AlignHCenter
        truncationMode: TruncationMode.Fade
        text: cover.coverLabel
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeSmall
        visible: cover.coverLabel.length > 0
    }

    Label {
        id: coverCodeLabel
        anchors.centerIn: parent
        text: cover.coverCode
        color: Theme.primaryColor
        font.pixelSize: Theme.fontSizeExtraLarge
        font.bold: true
        font.family: "monospace"
        visible: cover.coverCode.length > 0
    }

    Rectangle {
        id: coverProgress
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.paddingLarge
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.6
        height: 4
        radius: 2
        color: Theme.primaryColor
        opacity: 0.3
        visible: cover.coverCode.length > 0

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.min(1, 1 - (cover.coverRemaining
                                                  / Math.max(1, cover.coverPeriod)))
            radius: 2
            color: Theme.highlightColor
        }
    }

    Label {
        id: emptyLabel
        anchors.centerIn: parent
        text: "No accounts yet"
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeMedium
        visible: cover.coverCode.length === 0
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-new"
            onTriggered: {
                // `appWindow` is the id of the ApplicationWindow in
                // harbour-sailauth.qml; the Silica-internal alias is the same
                // object reached through the context chain, and covers are
                // created inside it, so either way the window is reachable.
                // activate() first: a covered app's page stack is inert.
                var window = (typeof appWindow !== "undefined")
                             ? appWindow : __silica_applicationwindow_instance
                window.activate()
                window.pageStack.push(Qt.resolvedUrl("../pages/AddAccountPage.qml"))
            }
        }
    }

    Connections {
        target: accountModel
        onCountChanged: cover.update()
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: cover.update()
    }

    Component.onCompleted: cover.update()
}
