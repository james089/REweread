import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.reweread.Downloader 1.0

Rectangle {
    id: page
    color: "white"
    property var selectedIds: []
    property bool multiSelect: false
    onMultiSelectChanged: { if (!multiSelect) selectedIds = [] }
    property int pageIndex: 0
    readonly property int pageSize: Math.max(1, Math.floor(shelf.height / 180))
    readonly property int pageCount: Math.max(1, Math.ceil(Downloader.books.length / pageSize))
    onPageCountChanged: pageIndex = Math.min(pageIndex, pageCount - 1)
    function turnPage(direction) { pageIndex = Math.max(0, Math.min(pageCount - 1, pageIndex + direction)) }
    function toggle(id) {
        var next = selectedIds.slice()
        var at = next.indexOf(id)
        if (at >= 0) next.splice(at, 1)
        else next.push(id)
        selectedIds = next
    }
    function enterMulti(id, state) {
        page.multiSelect = true
        if (canImport(state) && selectedIds.indexOf(id) < 0) toggle(id)
    }
    function canImport(state) { return state === "remote" || state === "failed" || state === "removed" }
    function stateText(book) {
        switch (book.importState) {
        case "queued": return "排队中"
        case "imported": return "已导入"
        case "pending": return "待系统确认"
        case "conflict": return "已有重复记录，请核实"
        case "preparing": return "准备中"
        case "downloading": return "下载中 " + (book.progress || 0) + "%"
        case "validating": return "校验中"
        case "importing": return "导入中"
        case "failed": return "失败，可重试"
        case "removed": return "系统中已移除"
        default: return "未导入"
        }
    }
    // Consume the overlay's empty areas as well as its controls.
    MouseArea { anchors.fill: parent }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 20
        RowLayout {
            Layout.fillWidth: true
            Label { text: "微信读书下载"; font.pixelSize: 32; color: "black"; Layout.fillWidth: true }
            RoundedButton {
                id: accountButton
                objectName: "accountButton"
                visible: Downloader.loggedIn; enabled: !Downloader.busy
                text: Downloader.accountName || "已登录"; font.pixelSize: 22
                Layout.maximumWidth: page.width * 0.4; Layout.preferredHeight: 72
                onClicked: logoutDialog.open()
            }
            RoundedButton { objectName: "returnButton"; text: "返回"; Layout.preferredHeight: 72; font.pixelSize: 24; onClicked: { if (page.multiSelect) page.multiSelect = false; else Downloader.closePage() } }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 20
            Label {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: Downloader.status; wrapMode: Text.Wrap; textFormat: Text.PlainText
                color: "black"; font.pixelSize: 22
            }
            Label {
                objectName: "pageInfo"
                visible: Downloader.loggedIn
                text: (page.pageIndex + 1) + " / " + page.pageCount
                color: "black"; font.pixelSize: 24
                horizontalAlignment: Text.AlignRight
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: Downloader.loggedIn
            RoundedButton { visible: Downloader.loggedIn; objectName: "refreshButton"; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.preferredHeight: 88; font.pixelSize: 26; text: "刷新"; enabled: !Downloader.busy; onClicked: { page.selectedIds = []; Downloader.refresh() } }
            RoundedButton {
                visible: Downloader.loggedIn && page.multiSelect
                objectName: "importButton"; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.preferredHeight: 88; font.pixelSize: 26
                text: "导入（" + page.selectedIds.length + "）"
                enabled: Downloader.canQueue && page.selectedIds.length > 0
                onClicked: { Downloader.importBooks(page.selectedIds); page.multiSelect = false; page.selectedIds = [] }
            }
            RoundedButton { visible: enabled; objectName: "cancelButton"; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.preferredHeight: 88; font.pixelSize: 26; text: "取消"; enabled: Downloader.busy; onClicked: Downloader.cancel() }
        }
        Image {
            Layout.preferredWidth: Math.min(450, page.width - 80)
            Layout.preferredHeight: Layout.preferredWidth
            Layout.alignment: Qt.AlignHCenter
            visible: !Downloader.loggedIn && Downloader.qrImage.length > 0
            source: Downloader.qrImage; fillMode: Image.PreserveAspectFit
            smooth: false; cache: false
        }
        Label {
            visible: !Downloader.loggedIn
            text: "使用插件独立账号登录。登录成功后显示微信读书书架。"
            Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 22; color: "black"
        }
        RoundedButton {
            visible: !Downloader.loggedIn && !Downloader.busy
            Layout.fillWidth: true; Layout.preferredHeight: 88
            text: "刷新登录二维码"; font.pixelSize: 26
            onClicked: Downloader.login()
        }
        ListView {
            id: shelf
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: Downloader.loggedIn
            clip: true; spacing: 4
            objectName: "pagedShelf"
            interactive: false
            model: Downloader.books.slice(page.pageIndex * page.pageSize, (page.pageIndex + 1) * page.pageSize)
            DragHandler {
                target: null
                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus | PointerDevice.Mouse
                minimumPointCount: 1; maximumPointCount: 1
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                xAxis.enabled: true; yAxis.enabled: false
                property real distance: 0
                onTranslationChanged: { if (active) distance = centroid.position.x - centroid.pressPosition.x }
                onActiveChanged: {
                    if (active) distance = centroid.position.x - centroid.pressPosition.x
                    else if (Math.abs(distance) >= 80) page.turnPage(distance < 0 ? 1 : -1)
                }
            }
            delegate: Item {
                id: bookRow
                property var book: modelData
                Connections {
                    target: Downloader
                    function onBookUpdated(bookId, updatedBook) {
                        if (bookRow.book.bookId === bookId) bookRow.book = updatedBook
                    }
                }
                TapHandler {
                    objectName: "bookLongPress"
                    gesturePolicy: TapHandler.DragThreshold
                    longPressThreshold: 0.6
                    onLongPressed: page.enterMulti(bookRow.book.bookId, bookRow.book.importState)
                }
                required property var modelData
                width: shelf.width; height: 176
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#cccccc" }
                RowLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 16
                    CheckBox {
                        objectName: "bookSelection"
                        visible: page.multiSelect
                        Layout.preferredWidth: 88; Layout.preferredHeight: 112
                        indicator: Rectangle {
                            x: (parent.width - width) / 2; y: (parent.height - height) / 2
                            width: 52; height: 52; radius: 5
                            color: parent.checked ? "black" : "white"
                            border.width: 3; border.color: parent.enabled ? "black" : "#999999"
                            Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 40; visible: parent.parent.checked }
                        }
                        enabled: page.canImport(bookRow.book.importState) && Downloader.canQueue
                        checked: page.selectedIds.indexOf(bookRow.book.bookId) >= 0
                        onClicked: page.toggle(bookRow.book.bookId)
                    }
                    Rectangle {
                        Layout.preferredWidth: 92; Layout.preferredHeight: 136
                        color: "#eeeeee"; border.color: "#999999"
                        Image {
                            objectName: "bookCoverImage"
                            anchors.fill: parent; anchors.margins: 1
                            source: {
                                var revision = Downloader.coverRevision
                                return Downloader.coverSource(bookRow.book.bookId, bookRow.book.cover || "")
                            }
                            sourceSize.width: 184; sourceSize.height: 272
                            fillMode: Image.PreserveAspectFit; asynchronous: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 12
                        Label {
                            text: bookRow.book.title; textFormat: Text.PlainText
                            font.pixelSize: 24; color: "black"
                            Layout.fillWidth: true; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                        }
                        Label {
                            text: bookRow.book.author; textFormat: Text.PlainText
                            font.pixelSize: 20; color: "#444444"
                            Layout.fillWidth: true; elide: Text.ElideRight
                        }
                        RowLayout {
                            spacing: 10
                            Label {
                                objectName: "bookImportStatus"
                                text: page.stateText(bookRow.book); font.pixelSize: 20; color: "black"
                                opacity: bookRow.book.importState === "remote" ? 0.5 : 1
                            }
                            Rectangle {
                                objectName: "bookImportedCheck"
                                width: 44; height: 44
                                Layout.preferredWidth: 44; Layout.preferredHeight: 44
                                radius: width / 2; color: "#239B56"
                                visible: bookRow.book.importState === "imported"
                                Canvas {
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, width, height)
                                        ctx.strokeStyle = "white"
                                        ctx.lineWidth = 4
                                        ctx.lineCap = "round"
                                        ctx.lineJoin = "round"
                                        ctx.beginPath()
                                        ctx.moveTo(width * 0.27, height * 0.51)
                                        ctx.lineTo(width * 0.43, height * 0.67)
                                        ctx.lineTo(width * 0.74, height * 0.34)
                                        ctx.stroke()
                                    }
                                }
                            }
                        }
                    }
                    RoundedButton {
                        objectName: "bookImportButton"
                        visible: !page.multiSelect && enabled
                        Layout.preferredWidth: 164; Layout.preferredHeight: 104
                        font.pixelSize: 24
                        text: bookRow.book.importState === "queued" ? "排队中" : "导入"
                        enabled: (page.canImport(bookRow.book.importState) || bookRow.book.importState === "queued") && Downloader.canQueue
                        onClicked: { if (bookRow.book.importState === "queued") Downloader.cancelQueued(bookRow.book.bookId); else Downloader.importBooks([bookRow.book.bookId]) }
                    }
                }
            }
        }
    }
    Dialog {
        id: logoutDialog
        objectName: "logoutDialog"
        parent: Overlay.overlay
        title: "退出登录？"; modal: true; dim: true
        padding: 40
        implicitHeight: 420; height: 420
        width: Math.min(640, page.width - 64)
        x: ((parent ? parent.width : page.width) - width) / 2
        y: ((parent ? parent.height : page.height) - height) / 2
        Overlay.modal: Rectangle { color: "#80000000" }
        background: Rectangle { color: "white"; radius: 18; border.width: 1; border.color: "black" }
        header: Label {
            text: logoutDialog.title; font.pixelSize: 28; color: "black"
            leftPadding: 40; rightPadding: 40; topPadding: 32; bottomPadding: 20
        }
        contentItem: Label { text: "退出当前微信读书账号并返回扫码登录页。已下载书籍和封面缓存会保留。"; wrapMode: Text.Wrap; font.pixelSize: 24; color: "black" }
        footer: Item {
            implicitHeight: 128; height: 128
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 40; anchors.rightMargin: 40; anchors.bottomMargin: 40
                spacing: 20
                RoundedButton { objectName: "cancelLogoutButton"; text: "取消"; font.pixelSize: 26; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.preferredHeight: 88; onClicked: logoutDialog.reject() }
                RoundedButton { objectName: "confirmLogoutButton"; text: "退出登录"; font.pixelSize: 26; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.preferredHeight: 88; onClicked: logoutDialog.accept() }
            }
        }
        onAccepted: Downloader.logout()
    }
}
