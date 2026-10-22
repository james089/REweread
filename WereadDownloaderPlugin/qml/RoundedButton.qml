import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    leftPadding: 32; rightPadding: 32
    topPadding: 16; bottomPadding: 16
    implicitWidth: Math.max(100, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 88
    background: Rectangle {
        radius: 12
        color: !control.enabled ? "#eeeeee" : (control.down || control.checked) ? "black" : "white"
        border.width: 1
        border.color: control.enabled ? "black" : "#cccccc"
    }
    contentItem: Text {
        text: control.text; font: control.font; textFormat: Text.PlainText
        color: !control.enabled ? "#888888" : (control.down || control.checked) ? "white" : "black"
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
}
