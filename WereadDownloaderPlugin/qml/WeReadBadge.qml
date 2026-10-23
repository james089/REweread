import QtQuick 2.15

Item {
    id: badge
    objectName: "wereadNativeBookBadge"
    property string badgeStatus: ""
    implicitWidth: 48
    implicitHeight: 48
    visible: badgeStatus !== ""
    Image {
        anchors.fill: parent
        source: badge.badgeStatus === "sync" ? "icons/sync.svg" : "icons/weread.png"
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(badge.width, badge.height)
    }
    Rectangle {
        visible: badge.badgeStatus === "error"
        anchors.right: parent.right; anchors.top: parent.top
        width: 20; height: 20; radius: 10; color: "white"; border.color: "black"; border.width: 2
        Text { anchors.centerIn: parent; text: "!"; font.pixelSize: 17; font.bold: true; color: "black" }
    }
}
