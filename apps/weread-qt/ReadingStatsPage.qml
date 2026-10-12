import QtQuick

Rectangle {
    id: page
    property var stats: ({})
    property bool accountReady: false
    property color inkColor: "#111111"
    property color accentColor: "#0f5f42"
    property string fontFamily: ""
    signal closed()
    color: "white"

    function duration(value) {
        var ms = Math.max(0, Number(value) || 0)
        if (ms > 0 && ms < 60000) return "不足 1 分钟"
        var minutes = Math.floor(ms / 60000)
        return minutes >= 60 ? Math.floor(minutes / 60) + " 小时 " + (minutes % 60) + " 分" : minutes + " 分钟"
    }

    Rectangle {
        id: backButton
        x: 36; y: 30; width: 132; height: 64
        color: "white"; border.color: page.inkColor; radius: 8
        Text { anchors.centerIn: parent; text: "‹ 返回"; font.pixelSize: 26; color: page.inkColor }
        MouseArea { anchors.fill: parent; onClicked: page.closed() }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter; y: 42
        text: "阅读统计"; font.pixelSize: 34; font.bold: true
        font.family: page.fontFamily; color: page.inkColor
    }
    Flickable {
        x: 44; y: 124; width: parent.width - 88; height: parent.height - 150
        clip: true; boundsBehavior: Flickable.StopAtBounds
        contentHeight: column.height + 30
        Column {
            id: column
            width: parent.width; spacing: 24
            Text {
                width: parent.width
                text: page.accountReady ? String(page.stats.monthLabel || "本月") + " · 本机阅读" : "登录后开始记录阅读时长"
                color: page.inkColor; font.pixelSize: 26; font.family: page.fontFamily
                wrapMode: Text.Wrap
            }
            Rectangle {
                width: parent.width; height: 170
                color: "white"; border.color: page.inkColor; radius: 10
                Column {
                    x: 24; y: 22; spacing: 14; width: parent.width - 48
                    Text { text: "本月时长"; color: page.inkColor; font.pixelSize: 26 }
                    Text {
                        width: parent.width; text: page.duration(page.stats.monthMs)
                        color: page.accentColor; font.pixelSize: 48; font.bold: true
                        fontSizeMode: Text.Fit; minimumPixelSize: 28
                    }
                }
            }
            Row {
                width: parent.width; spacing: 20
                Repeater {
                    model: [ { label: "今日", ms: page.stats.todayMs }, { label: "累计", ms: page.stats.totalMs } ]
                    Column {
                        required property var modelData
                        width: (column.width - 20) / 2; spacing: 12
                        Text { text: modelData.label; font.pixelSize: 24; color: page.inkColor }
                        Text {
                            width: parent.width; text: page.duration(modelData.ms)
                            font.pixelSize: 30; font.bold: true; color: page.inkColor
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
            Rectangle { width: parent.width; height: 1; color: page.inkColor }
            Text {
                width: parent.width
                text: "本月阅读 " + Number(page.stats.activeDays || 0) + " 天 · " + Number(page.stats.booksRead || 0) + " 本"
                font.pixelSize: 24; color: page.inkColor; wrapMode: Text.Wrap
            }
            Text { text: "近 7 天"; font.pixelSize: 28; font.bold: true; color: page.inkColor }
            Repeater {
                model: page.stats.days || []
                Row {
                    required property var modelData
                    width: column.width; height: 38; spacing: 14
                    Text { width: parent.width * 0.44; text: modelData.date; color: page.inkColor; font.pixelSize: 24 }
                    Text {
                        width: parent.width * 0.56 - 14; text: page.duration(modelData.ms)
                        horizontalAlignment: Text.AlignRight; color: page.inkColor; font.pixelSize: 24
                    }
                }
            }
            Rectangle { width: parent.width; height: 1; color: page.inkColor }
            Text {
                width: parent.width
                text: "按北京时间统计，仅记录本机前台正文阅读。休眠、后台和自动测试不计入。\n"
                      + (page.stats.firstDate ? "自 " + page.stats.firstDate + " 开始记录。" : "从本次更新后的实际阅读开始记录。")
                      + "云端历史月份尚不可用；同步成功不会清空这里的时长。"
                wrapMode: Text.Wrap; color: page.inkColor; font.pixelSize: 21
                lineHeight: 1.25
            }
        }
    }
}
