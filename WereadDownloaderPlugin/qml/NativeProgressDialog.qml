import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.reweread.Downloader 1.0

Item {
    id: dialog
    objectName: "wereadNativeProgressDialog"
    visible: Downloader.nativePullVisible
    Rectangle { anchors.fill: parent; color: "#80000000" }
    MouseArea { anchors.fill: parent; onClicked: {} }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 720)
        height: Math.min(parent.height - 64, content.implicitHeight + 64)
        radius: 24
        color: "white"
        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 32
            spacing: 24
            Label { text: "拉取微信读书进度"; font.pixelSize: 30; color: "black"; Layout.fillWidth: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: message.implicitHeight
                contentWidth: availableWidth
                Label {
                    id: message
                    width: parent.width
                    text: Downloader.nativePullMessage
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: 26
                    color: "black"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 24
                RoundedButton {
                    objectName: "nativePullCancel"
                    visible: !Downloader.nativePullApplying
                    Layout.fillWidth: true; Layout.preferredWidth: 1
                    Layout.minimumHeight: 88; Layout.maximumHeight: 88
                    text: Downloader.nativePullCanConfirm || Downloader.nativePullBusy ? "取消" : "关闭"
                    font.pixelSize: 26
                    onClicked: Downloader.cancelNativePull()
                }
                RoundedButton {
                    objectName: "nativePullConfirm"
                    visible: Downloader.nativePullCanConfirm
                    Layout.fillWidth: true; Layout.preferredWidth: 1
                    Layout.minimumHeight: 88; Layout.maximumHeight: 88
                    text: "覆盖本地进度"
                    font.pixelSize: 26
                    onClicked: Downloader.confirmNativePull()
                }
            }
        }
    }
}
