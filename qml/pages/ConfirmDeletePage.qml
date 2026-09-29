import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: confirmPage

    property int accountId: 0
    property string issuer: ""
    property string name: ""

    allowedOrientations: Orientation.Portrait

    Column {
        anchors.centerIn: parent
        width: parent.width - Theme.horizontalPageMargin * 2
        spacing: Theme.paddingLarge

        Label {
            width: parent.width
            text: "Delete account?"
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeLarge
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            width: parent.width
            text: confirmPage.issuer.length > 0
                  ? confirmPage.issuer + (confirmPage.name.length > 0
                                          ? "\n" + confirmPage.name : "")
                  : confirmPage.name
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingLarge

            Button {
                text: "Cancel"
                onClicked: pageStack.pop()
            }

            Button {
                text: "Delete"
                onClicked: {
                    accountModel.deleteAccount(confirmPage.accountId)
                    pageStack.pop()
                }
            }
        }
    }
}
