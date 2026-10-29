import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.reweread.Downloader 1.0

Rectangle {
    id: page
    color: "white"
    property int activeTab: 0
    onActiveTabChanged: if (activeTab === 1) Downloader.refreshCloudStatistics()
    function duration(seconds) {
        if (typeof seconds !== "number" || seconds < 0) return "—"
        var n = Math.floor(seconds)
        if (n >= 3600) return Math.floor(n / 3600) + "小时" + Math.floor(n % 3600 / 60) + "分"
        return Math.floor(n / 60) + "分" + n % 60 + "秒"
    }
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
        anchors.bottomMargin: Downloader.loggedIn ? 108 : 32
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
        ProgressBar {
            objectName: "timeUploadProgressBar"
            Layout.fillWidth: true
            visible: Downloader.timeUploadActive
            from: 0; to: Math.max(1, Downloader.timeUploadTotal)
            value: Downloader.timeUploadDone
            indeterminate: Downloader.timeUploadTotal === 0
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
                visible: Downloader.loggedIn && page.activeTab === 0
                text: (page.pageIndex + 1) + " / " + page.pageCount
                color: "black"; font.pixelSize: 24
                horizontalAlignment: Text.AlignRight
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: Downloader.loggedIn && page.activeTab === 0
            RoundedButton { visible: Downloader.loggedIn; objectName: "refreshButton"; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumHeight: 72; Layout.preferredHeight: 72; Layout.maximumHeight: 72; font.pixelSize: 26; text: "刷新"; enabled: !Downloader.busy; onClicked: { page.selectedIds = []; Downloader.refresh() } }
            RoundedButton {
                visible: Downloader.loggedIn && page.multiSelect && page.activeTab === 0
                objectName: "importButton"; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumHeight: 72; Layout.preferredHeight: 72; Layout.maximumHeight: 72; font.pixelSize: 26
                text: "导入（" + page.selectedIds.length + "）"
                enabled: Downloader.canQueue && page.selectedIds.length > 0
                onClicked: { Downloader.importBooks(page.selectedIds); page.multiSelect = false; page.selectedIds = [] }
            }
            RoundedButton {
                objectName: "exitMultiSelectButton"
                visible: page.multiSelect
                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumHeight: 72; Layout.preferredHeight: 72; Layout.maximumHeight: 72
                font.pixelSize: 26; text: "退出多选"
                onClicked: page.multiSelect = false
            }
            RoundedButton { visible: enabled; objectName: "cancelButton"; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumHeight: 72; Layout.preferredHeight: 72; Layout.maximumHeight: 72; font.pixelSize: 26; text: "取消"; enabled: Downloader.busy; onClicked: Downloader.cancel() }
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
            visible: Downloader.loggedIn && page.activeTab === 0
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
                            Layout.fillWidth: true; spacing: 10
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
                    RowLayout {
                        spacing: 12
                        visible: !page.multiSelect && (bookImportAction.enabled
                            || bookRow.book.importState === "imported"
                            || (bookRow.book.importState === "removed" && bookRow.book.hasDownloadCache !== false && !Downloader.busy))
                        RoundedButton {
                            id: bookImportAction
                            objectName: "bookImportButton"
                            visible: enabled
                            leftPadding: 20; rightPadding: 20
                            Layout.minimumWidth: 144; Layout.preferredWidth: 144; Layout.maximumWidth: 144
                            Layout.minimumHeight: 88; Layout.preferredHeight: 88; Layout.maximumHeight: 88
                            font.pixelSize: 24
                            text: bookRow.book.importState === "queued" ? "排队中" : "导入"
                            enabled: (page.canImport(bookRow.book.importState) || bookRow.book.importState === "queued") && Downloader.canQueue
                            onClicked: { if (bookRow.book.importState === "queued") Downloader.cancelQueued(bookRow.book.bookId); else Downloader.importBooks([bookRow.book.bookId]) }
                        }
                        ColumnLayout {
                            visible: !page.multiSelect && bookRow.book.importState === "imported"
                            Layout.preferredWidth: 300; Layout.maximumWidth: 300
                            spacing: 6
                            Label {
                                objectName: "bookReadingProgress"
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                textFormat: Text.PlainText; elide: Text.ElideRight
                                font.pixelSize: 18; color: "#555555"
                                text: "本地 " + (typeof bookRow.book.localProgress === "number" && bookRow.book.localProgress >= 0 ? bookRow.book.localProgress.toFixed(1) + "%" : "—")
                                    + " · " + (bookRow.book.localChapter || "章节待识别")
                            }
                            Label {
                                objectName: "cloudReadingProgress"
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                textFormat: Text.PlainText; elide: Text.ElideRight
                                font.pixelSize: 18; color: "#555555"
                                text: "云端 " + (typeof bookRow.book.cloudProgress === "number" && bookRow.book.cloudProgress >= 0 ? bookRow.book.cloudProgress.toFixed(1) + "%" : "—")
                                    + " · " + (bookRow.book.cloudChapter || "章节待识别")
                                    + (bookRow.book.cloudProgressStale && bookRow.book.cloudProgress >= 0 ? "（待更新）" : "")
                            }
                            RowLayout {
                                spacing: 12
                                RoundedButton {
                                    id: uploadProgressAction
                                    objectName: "uploadProgressButton"
                                    visible: !page.multiSelect && bookRow.book.importState === "imported"
                                    enabled: !Downloader.busy
                                    leftPadding: 20; rightPadding: 20
                                    Layout.minimumWidth: 144; Layout.preferredWidth: 144; Layout.maximumWidth: 144
                                    Layout.minimumHeight: 56; Layout.preferredHeight: 56; Layout.maximumHeight: 56
                                    font.pixelSize: 24; text: "上传进度"
                                    onClicked: Downloader.uploadProgress(bookRow.book.bookId)
                                }
                                RoundedButton {
                                    objectName: "downloadProgressButton"
                                    visible: !page.multiSelect && bookRow.book.importState === "imported"
                                    enabled: !Downloader.busy
                                    leftPadding: 20; rightPadding: 20
                                    Layout.minimumWidth: 144; Layout.preferredWidth: 144; Layout.maximumWidth: 144
                                    Layout.minimumHeight: 56; Layout.preferredHeight: 56; Layout.maximumHeight: 56
                                    font.pixelSize: 24; text: "下载进度"
                                    onClicked: Downloader.downloadProgress(bookRow.book.bookId)
                                }
                            }
                        }
                        RoundedButton {
                            objectName: "deleteCacheButton"
                            visible: !page.multiSelect && Downloader.loggedIn && !Downloader.busy
                                && bookRow.book.importState === "removed"
                                && bookRow.book.hasDownloadCache !== false
                            leftPadding: 20; rightPadding: 20
                            Layout.minimumWidth: 144; Layout.preferredWidth: 144; Layout.maximumWidth: 144
                            Layout.minimumHeight: 88; Layout.preferredHeight: 88; Layout.maximumHeight: 88
                            font.pixelSize: 24; text: "删除缓存"
                            onClicked: Downloader.clearCache(bookRow.book.bookId)
                        }
                    }
                }
            }
        }
        ColumnLayout {
            visible: Downloader.loggedIn && page.activeTab === 1
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 20; color: "#555555"
                text: Downloader.statisticsStatus || "云端统计；请配置插件独立的统计 API"
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 16
                Label { Layout.fillWidth: true; text: "账号云端累计 " + page.duration(Downloader.cloudReadingTotalSeconds); font.pixelSize: 26; font.bold: true }
                RoundedButton { objectName: "statisticsApiButton"; text: Downloader.statisticsConfigured ? "统计 API 设置" : "配置统计 API"; font.pixelSize: 22; Layout.preferredHeight: 58; onClicked: statisticsApiDialog.open() }
                RoundedButton { text: "刷新统计"; font.pixelSize: 22; Layout.preferredHeight: 58; onClicked: Downloader.refreshCloudStatistics() }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 16
                Label { text: "统计时区"; font.pixelSize: 22; color: "black" }
                ComboBox {
                    objectName: "readingTimeZoneSelector"
                    id: readingTimeZoneSelector
                    Layout.fillWidth: true; Layout.preferredWidth: 760; Layout.maximumWidth: 760
                    Layout.minimumHeight: 58; Layout.preferredHeight: 58; Layout.maximumHeight: 58
                    model: Downloader.readingTimeZoneLabels
                    currentIndex: Math.max(0, Downloader.readingTimeZones.indexOf(Downloader.readingTimeZone))
                    font.pixelSize: 24
                    popup.width: readingTimeZoneSelector.width
                    delegate: ItemDelegate {
                        width: readingTimeZoneSelector.width
                        height: 58
                        contentItem: Text {
                            text: modelData
                            font: readingTimeZoneSelector.font
                            color: "black"
                            leftPadding: 16
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideNone
                            wrapMode: Text.NoWrap
                        }
                    }
                    onActivated: function(index) { Downloader.readingTimeZone = Downloader.readingTimeZones[index] }
                }
                Label { objectName: "readingClockLabel"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; text: Downloader.readingClock; font.pixelSize: 20; color: "#444444" }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 16
                RoundedButton {
                    objectName: "previousReadingWeek"; text: "‹ 上周"; font.pixelSize: 22
                    Layout.minimumHeight: 58; Layout.preferredHeight: 58; Layout.maximumHeight: 58
                    onClicked: Downloader.shiftReadingWeek(-1)
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 24; font.bold: true; text: Downloader.readingWeekLabel }
                    Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 18; color: "#555555"; text: "本周 " + page.duration(Downloader.readingWeekTotalSeconds) + " · 最多 " + Downloader.readingWeekPeakLabel }
                }
                RoundedButton {
                    objectName: "nextReadingWeek"; text: "下周 ›"; font.pixelSize: 22
                    Layout.minimumHeight: 58; Layout.preferredHeight: 58; Layout.maximumHeight: 58
                    onClicked: Downloader.shiftReadingWeek(1)
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 220; radius: 12
                color: "white"; border.color: "#aaaaaa"
                RowLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 6
                    Repeater {
                        model: Downloader.readingWeekStats
                        delegate: Item {
                            required property var modelData
                            Layout.fillWidth: true; Layout.fillHeight: true
                            Rectangle {
                                anchors.fill: parent; anchors.margins: 2; radius: 10
                                color: Downloader.readingDate === modelData.date ? "#eeeeee" : "white"
                                border.width: Downloader.readingDate === modelData.date ? 2 : 1
                                border.color: Downloader.readingDate === modelData.date ? "black" : "#dddddd"
                            }
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 2
                                Label {
                                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                                    font.pixelSize: 16; color: "#444444"
                                    text: modelData.seconds > 0 ? page.duration(modelData.seconds) : "—"
                                }
                                Item {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
                                        width: Math.max(14, parent.width * 0.46)
                                        height: modelData.seconds <= 0 || Downloader.readingWeekMaximumSeconds <= 0 ? 4
                                            : Math.max(8, (parent.height - 8) * modelData.seconds / Downloader.readingWeekMaximumSeconds)
                                        color: Downloader.readingDate === modelData.date ? "black" : "#777777"
                                        radius: 4
                                    }
                                }
                                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 20; font.bold: Downloader.readingDate === modelData.date; text: modelData.day }
                                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 16; color: "#555555"; text: modelData.shortDate }
                            }
                            TapHandler { onTapped: Downloader.readingDate = modelData.date }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                RoundedButton {
                    objectName: "previousReadingDate"; text: "‹ 前一天"; font.pixelSize: 22
                    Layout.minimumHeight: 58; Layout.preferredHeight: 58; Layout.maximumHeight: 58
                    onClicked: Downloader.shiftReadingDate(-1)
                }
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 22; font.bold: true
                    text: Downloader.readingDate === "" ? "全部日期" : (Downloader.readingDate === "undated" ? "无日期历史" : Downloader.readingDate)
                }
                RoundedButton {
                    objectName: "nextReadingDate"; text: "后一天 ›"; font.pixelSize: 22
                    Layout.minimumHeight: 58; Layout.preferredHeight: 58; Layout.maximumHeight: 58
                    onClicked: Downloader.shiftReadingDate(1)
                }
                RoundedButton {
                    objectName: "allReadingDates"; text: "全部"; font.pixelSize: 20
                    Layout.minimumHeight: 58; Layout.preferredHeight: 58; Layout.maximumHeight: 58
                    visible: Downloader.readingDate !== ""
                    onClicked: Downloader.readingDate = ""
                }

            }
            Label {
                Layout.fillWidth: true; font.pixelSize: 22; font.bold: true; color: "black"
                text: (Downloader.readingDate === "" ? "云端累计" : "云端当日") + "阅读 " + page.duration(Downloader.readingDateTotalSeconds)
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap
                text: (Downloader.readingDate === "" ? "本机逐书累计" : "本机当天逐书记录") + " · 仅本机记录，不含其他设备"
                font.pixelSize: 22; color: "#555555"
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 20; color: "#555555"
                text: "补传提交该书全部未传时长；云端按提交日期统计"
            }
            ListView {
                objectName: "readingTimeList"
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
                model: Downloader.readingStats
                delegate: Rectangle {
                    width: ListView.view.width; height: 168; radius: 12
                    border.color: "#aaaaaa"; color: "white"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 20; spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 10
                            Label { text: modelData.title; textFormat: Text.PlainText; font.pixelSize: 24; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: (Downloader.readingDate === "" ? "本机累计 " : "本机当天 ") + page.duration(modelData.localSeconds); font.pixelSize: 22; color: "#444444" }
                            Label {
                                objectName: "pendingReadingTimeLabel"
                                Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 20; color: "#444444"
                                text: (modelData.pendingSeconds < 0 ? "待传时长未知" : "全部待传 " + page.duration(modelData.pendingSeconds))
                                    + (modelData.timeUploadUncertain ? " · 回执待核对" : "")
                            }
                        }
                        RoundedButton {
                            objectName: "uploadReadingTimeButton"
                            text: "补传时长"; font.pixelSize: 22
                            Layout.preferredWidth: 160; Layout.preferredHeight: 58
                            enabled: !Downloader.busy && modelData.canUploadTime
                            onClicked: Downloader.uploadReadingTime(modelData.bookId)
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent; width: parent.width - 40
                    visible: parent.count === 0
                    text: Downloader.readingDate === "" ? "暂无本机阅读记录" : "当天暂无本机阅读记录"
                    wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 24; color: "#555555"
                }
            }
        }
    }
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        visible: Downloader.loggedIn; height: 88; color: "white"; border.color: "black"; border.width: 1
        Row {
            anchors.centerIn: parent; spacing: 32
            RoundedButton { objectName: "shelfTabButton"; width: 240; height: 58; topPadding: 8; bottomPadding: 8; text: "书架"; font.pixelSize: 24; font.bold: page.activeTab === 0; checked: page.activeTab === 0; onClicked: page.activeTab = 0 }
            RoundedButton { objectName: "readingTimeTabButton"; width: 240; height: 58; topPadding: 8; bottomPadding: 8; text: "阅读时间统计"; font.pixelSize: 24; font.bold: page.activeTab === 1; checked: page.activeTab === 1; onClicked: { page.multiSelect = false; page.activeTab = 1 } }
        }
    }
    Item {
        id: statisticsApiDialog
        objectName: "statisticsApiDialog"
        anchors.fill: parent
        z: 100
        visible: false
        readonly property string title: "独立统计 API 配置"
        property bool revealKey: false
        function open() { revealKey = false; statisticsApiInput.text = Downloader.statisticsApiKeyForEditing(); visible = true; statisticsApiInput.forceActiveFocus() }
        function close() { statisticsApiInput.focus = false; Qt.inputMethod.hide(); statisticsApiInput.text = ""; revealKey = false; visible = false }
        function reject() { close() }
        function accept() { Downloader.setStatisticsApiKey(statisticsApiInput.text); close() }
        Rectangle { anchors.fill: parent; color: "#80000000"; MouseArea { anchors.fill: parent } }
        Rectangle {
            id: apiPanel
            objectName: "statisticsApiPanel"
            width: Math.min(760, page.width - 64)
            height: apiContent.implicitHeight + 80
            anchors.horizontalCenter: parent.horizontalCenter
            y: 32
            color: "white"; radius: 18; border.color: "black"
            MouseArea { anchors.fill: parent }
            ColumnLayout {
                id: apiContent
                objectName: "statisticsApiContent"
                x: 40; y: 40; width: parent.width - 80
                spacing: 24
                Label { Layout.fillWidth: true; text: statisticsApiDialog.title; font.pixelSize: 28 }
                Label { Layout.fillWidth: true; text: "在微信读书官方统计 API 页面获取当前账号的 API Key：weread.qq.com/r/weread-skills。仅保存在本插件内。"; font.pixelSize: 22; wrapMode: Text.Wrap }
                RowLayout {
                    Layout.fillWidth: true; spacing: 16
                TextField { id: statisticsApiInput; objectName: "statisticsApiInput"; Layout.fillWidth: true; Layout.preferredHeight: 88; font.pixelSize: 24; echoMode: statisticsApiDialog.revealKey ? TextInput.Normal : TextInput.Password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; placeholderText: "输入 API Key"; background: Rectangle { radius: 12; color: "white"; border.color: "black" } }
                    RoundedButton {
                        objectName: "statisticsApiReveal"
                        text: statisticsApiDialog.revealKey ? "隐藏" : "显示"
                        Layout.preferredWidth: 112; Layout.preferredHeight: 88
                        font.pixelSize: 24
                        focusPolicy: Qt.NoFocus
                        onClicked: { statisticsApiDialog.revealKey = !statisticsApiDialog.revealKey; statisticsApiInput.forceActiveFocus() }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.topMargin: 8; spacing: 20
                    RoundedButton { objectName: "statisticsApiCancel"; text: "取消"; font.pixelSize: 26; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.preferredHeight: 88; onClicked: statisticsApiDialog.reject() }
                    RoundedButton { objectName: "statisticsApiSave"; text: "保存"; font.pixelSize: 26; Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.preferredHeight: 88; enabled: statisticsApiInput.text.trim().length >= 8; onClicked: statisticsApiDialog.accept() }
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
