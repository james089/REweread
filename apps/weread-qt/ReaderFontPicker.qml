import QtQuick

Rectangle {
    id: picker
    property var choices: []
    property string selectedName: ""
    property color inkColor: "#111111"
    property color accentColor: "#0f5f42"
    signal chosen(string name)
    signal dismissed()
    color: "white"
    // Consume all blank-space interactions before the underlying reader.
    MouseArea { anchors.fill: parent }
    function choiceCenter(name) {
        for (var i = 0; i < fontCards.count; ++i) {
            var card = fontCards.itemAt(i)
            if (card.modelData.name === name && card.modelData.ready)
                return card.mapToItem(picker, card.width / 2, card.height / 2)
        }
        return null
    }
    function tap(x, y) {
        if (x >= width - 100 && y <= 88) { dismissed(); return true }
        for (var i = 0; i < fontCards.count; ++i) {
            var card = fontCards.itemAt(i)
            var p = card.mapFromItem(picker, x, y)
            if (p.x >= 0 && p.y >= 0 && p.x < card.width && p.y < card.height) {
                if (card.modelData.ready) chosen(card.modelData.name)
                return true
            }
        }
        return true
    }
    Text { x: 28; y: 28; text: "选择字体"; font.pixelSize: 30; font.bold: true; color: picker.inkColor }
    Text {
        x: parent.width - 94; y: 28; width: 70; height: 44
        text: "完成"; font.pixelSize: 24; color: picker.accentColor
        horizontalAlignment: Text.AlignHCenter
        MouseArea { anchors.fill: parent; onClicked: picker.dismissed() }
    }
    Text {
        x: 28; y: 82; width: parent.width - 56
        text: "当前：" + picker.selectedName + " · 点选已安装字体预览"
        font.pixelSize: 21; color: picker.inkColor; wrapMode: Text.Wrap
    }
    Grid {
        x: 28; y: 132; width: parent.width - 56
        columns: 3; spacing: 14
        Repeater {
            id: fontCards
            model: picker.choices
            Rectangle {
                required property var modelData
                width: (picker.width - 84) / 3
                height: Math.min(110, Math.max(76, (picker.height - 160) / 5 - 14))
                radius: 7; color: "white"
                border.width: picker.selectedName === modelData.name ? 3 : 1
                border.color: picker.selectedName === modelData.name ? picker.accentColor : picker.inkColor
                Text {
                    x: 8; y: 12; width: parent.width - 16; height: 36
                    text: modelData.name; font.family: modelData.family || "sans-serif"
                    font.pixelSize: 25; fontSizeMode: Text.Fit; minimumPixelSize: 18
                    color: picker.inkColor; horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    x: 8; y: parent.height - 30; width: parent.width - 16
                    text: modelData.ready ? (picker.selectedName === modelData.name ? "使用中" : "已安装") : modelData.statusText
                    font.pixelSize: 17; color: picker.inkColor; horizontalAlignment: Text.AlignHCenter
                }
                MouseArea { anchors.fill: parent; enabled: modelData.ready; onClicked: picker.chosen(modelData.name) }
            }
        }
    }
}
