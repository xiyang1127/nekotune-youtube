import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import NekoTune 1.0

Rectangle {
    id: page
    color: theme.bgCanvas
    readonly property var theme: api.theme
    property alias viewModel: controller
    property bool showChannelInput: false
    readonly property bool watching: controller.videoTrack.id !== undefined
    readonly property bool sideBySide: watching && width >= 1120
    readonly property real playerColumnWidth: Math.min((main.width - 24) * 0.64, Math.max(300, main.height - 172) * 16 / 9)
    ExtensionApi { id: api }
    Controller { id: controller; Component.onCompleted: init() }
    function focusSearch() { query.forceActiveFocus() }
    function duration(ms) {
        const seconds = Math.floor((ms || 0) / 1000)
        return seconds > 0 ? Math.floor(seconds / 60) + ":" + ("0" + seconds % 60).slice(-2) : ""
    }
    function settings() { if (api.window) api.window.navigate("nekotune.youtube/settings") }
    function submitSearch() {
        const value = query.text.trim()
        if (value.startsWith("@") || /^https:\/\/(www\.|m\.)?youtube\.com\/(channel\/|@|c\/|user\/)/.test(value))
            controller.openChannel(value, value)
        else controller.search(value, searchKind.currentIndex === 1 ? "channel" : "video")
    }
    onWatchingChanged: main.contentY = 0
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: page.width < 700 ? 14 : 24
        spacing: 16
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            Rectangle {
                width: 34; height: 26; radius: 8; color: "#e9424f"
                Label { anchors.centerIn: parent; text: "▶"; color: "white"; font.pixelSize: 14 }
            }
            Label { text: "YouTube 漫游"; color: page.theme.textPrimary; font.pixelSize: 21; font.bold: true; Layout.fillWidth: true }
            ActionButton { paletteTokens: page.theme; quiet: true; text: "发现"; onClicked: controller.navigate({mode: "discover"}, "发现音乐") }
            ActionButton { paletteTokens: page.theme; quiet: true; text: "作者收藏"; onClicked: controller.showFollowing() }
            ActionButton { objectName: "settingsButton"; paletteTokens: page.theme; quiet: true; text: "设置"; onClicked: page.settings() }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            TextField {
                id: query; objectName: "searchField"
                Layout.fillWidth: true; Layout.minimumWidth: 80
                implicitHeight: 42; leftPadding: 14; rightPadding: 14; font.pixelSize: 14
                color: page.theme.textPrimary; placeholderTextColor: page.theme.textMuted
                placeholderText: page.width < 700 ? "搜歌、视频、@作者或链接" : "搜索歌曲、歌手、视频，或输入 @作者 / 链接"
                selectByMouse: true
                background: Rectangle { radius: 9; color: page.theme.bgRaised; border.width: query.activeFocus ? 1 : 0; border.color: page.theme.accentPrimary || "#e9424f" }
                onAccepted: page.submitSearch()
            }
            ComboBox {
                id: searchKind; objectName: "searchKind"
                model: ["歌曲 / 视频", "作者 / 频道"]
                Layout.preferredWidth: page.width < 700 ? 116 : 130
                implicitHeight: 42
                background: Rectangle { radius: 9; color: page.theme.bgRaised }
                contentItem: Text { text: searchKind.displayText; color: page.theme.textPrimary; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 24 }
                indicator: Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "⌄"; color: page.theme.textSecondary }
            }
            ActionButton { objectName: "searchButton"; paletteTokens: page.theme; text: "搜索"; enabled: query.text.trim() !== ""; highlighted: true; implicitHeight: 42; onClicked: page.submitSearch() }
            ActionButton { paletteTokens: page.theme; text: "直达频道"; visible: page.width >= 760; onClicked: page.showChannelInput = !page.showChannelInput }
        }
        RowLayout {
            Layout.fillWidth: true; visible: page.showChannelInput
            TextField {
                id: channelInput; objectName: "channelField"
                Layout.fillWidth: true; implicitHeight: 40; leftPadding: 12
                color: page.theme.textPrimary; placeholderTextColor: page.theme.textMuted
                placeholderText: "输入 @用户名或作者频道链接"; selectByMouse: true
                background: Rectangle { radius: 8; color: page.theme.bgRaised }
                onAccepted: { controller.openChannel(text, text); page.showChannelInput = false }
            }
            ActionButton { paletteTokens: page.theme; text: "进入频道"; enabled: channelInput.text.trim() !== ""; onClicked: { controller.openChannel(channelInput.text, channelInput.text); page.showChannelInput = false } }
        }
        Flickable {
            id: main; objectName: "browseScroll"
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: width; contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: body
                width: main.width
                spacing: 16
                GridLayout {
                    Layout.fillWidth: true
                    columns: page.sideBySide ? 2 : 1
                    columnSpacing: 24; rowSpacing: 24
                    WatchPanel {
                        id: watchPanel; objectName: "watchPanel"
                        controller: controller; theme: page.theme
                        visible: page.watching
                        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: page.sideBySide ? page.playerColumnWidth : body.width
                        Layout.maximumWidth: page.sideBySide ? page.playerColumnWidth : body.width
                    }
                    ColumnLayout {
                        id: browser; objectName: "browserPanel"
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: page.sideBySide ? body.width - 24 - page.playerColumnWidth : body.width
                        Layout.preferredHeight: Math.max(420, main.height)
                        Layout.minimumHeight: 420
                        Layout.maximumHeight: Math.max(420, main.height)
                        spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            ActionButton { paletteTokens: page.theme; quiet: true; text: "← 返回"; visible: controller.history.length > 0; onClicked: controller.back() }
                            Label { text: controller.title; textFormat: Text.PlainText; color: page.theme.textPrimary; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                            ActionButton { paletteTokens: page.theme; text: controller.isFollowing ? "已收藏" : "收藏作者"; visible: controller.channelMode; enabled: !controller.followBusy; onClicked: controller.toggleFollow() }
                            ActionButton { paletteTokens: page.theme; quiet: true; text: "主页 ↗"; visible: controller.channelMode; onClicked: Qt.openUrlExternally(controller.params.channel) }
                        }
                        Label {
                            text: controller.description; textFormat: Text.PlainText
                            visible: controller.channelMode && text !== ""
                            maximumLineCount: 2; elide: Text.ElideRight; wrapMode: Text.Wrap
                            Layout.fillWidth: true; Layout.minimumWidth: 0; color: page.theme.textSecondary; font.pixelSize: 12
                        }
                        RowLayout {
                            visible: controller.channelMode
                            Repeater {
                                model: [{title: "视频", value: "videos"}, {title: "Shorts", value: "shorts"}, {title: "直播回放", value: "streams"}]
                                ActionButton { required property var modelData; paletteTokens: page.theme; text: modelData.title; highlighted: controller.params.tab === modelData.value; onClicked: controller.changeTab(modelData.value) }
                            }
                        }
                        Label { text: controller.message; textFormat: Text.PlainText; visible: text !== ""; color: page.theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        RowLayout {
                            visible: controller.message !== "" && !controller.busy
                            ActionButton { paletteTokens: page.theme; text: "重试加载"; onClicked: controller.load(false) }
                            ActionButton { paletteTokens: page.theme; text: "打开设置"; onClicked: page.settings() }
                        }
                        GridView {
                            id: grid; objectName: "videoGrid"
                            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 200
                            clip: true
                            cellWidth: width / Math.max(1, Math.floor(width / (page.sideBySide ? 230 : 260)))
                            cellHeight: (cellWidth - 14) * 0.5625 + 122
                            model: controller.items
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Item {
                                id: card
                                required property var modelData
                                width: grid.cellWidth; height: grid.cellHeight
                                ColumnLayout {
                                    anchors.fill: parent; anchors.rightMargin: 14; anchors.bottomMargin: 18; spacing: 6
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: width * 0.5625
                                        Layout.minimumHeight: width * 0.5625
                                        Layout.maximumHeight: width * 0.5625
                                        radius: 10; color: page.theme.bgRaised; clip: true
                                        Image { anchors.fill: parent; source: card.modelData.cover_url || ""; asynchronous: true; fillMode: Image.PreserveAspectCrop }
                                        Label { anchors.centerIn: parent; visible: !card.modelData.cover_url; text: card.modelData.kind === "channel" ? "作者频道" : "▶"; color: page.theme.textMuted }
                                        Rectangle {
                                            visible: page.duration(card.modelData.duration_ms) !== ""
                                            anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 6
                                            color: "#cc000000"; radius: 4; width: timeLabel.implicitWidth + 10; height: 22
                                            Label { id: timeLabel; anchors.centerIn: parent; text: page.duration(card.modelData.duration_ms); color: "white"; font.pixelSize: 12 }
                                        }
                                        Button {
                                            anchors.fill: parent; flat: true; text: ""; Accessible.name: "打开 " + card.modelData.title
                                            background: Rectangle { color: parent.hovered ? "#20ffffff" : "transparent"; radius: 10 }
                                            onClicked: controller.watch(card.modelData)
                                        }
                                    }
                                    Label {
                                        text: card.modelData.title; textFormat: Text.PlainText
                                        color: page.theme.textPrimary; font.bold: true; font.pixelSize: 13
                                        wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                                        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredHeight: 34
                                    }
                                    ActionButton {
                                        text: card.modelData.kind === "channel" ? "进入作者频道 →" : (card.modelData.artist || "查看作者")
                                        paletteTokens: page.theme; quiet: true; textAlignment: Text.AlignLeft
                                        Layout.fillWidth: true; implicitHeight: 26; horizontalPadding: 0; padding: 0
                                        enabled: !!card.modelData.channel_url
                                        onClicked: controller.openChannel(card.modelData.channel_url, card.modelData.kind === "channel" ? card.modelData.title : card.modelData.artist)
                                    }
                                    RowLayout {
                                        visible: card.modelData.kind !== "channel"; spacing: 6
                                        ActionButton { paletteTokens: page.theme; text: "播放"; implicitHeight: 32; Layout.fillWidth: true; onClicked: controller.watch(card.modelData) }
                                        ActionButton { paletteTokens: page.theme; text: "听歌"; implicitHeight: 32; Layout.fillWidth: true; onClicked: controller.music("enqueue", card.modelData) }
                                        ActionButton { objectName: "download_" + card.modelData.id; paletteTokens: page.theme; text: "下载"; implicitHeight: 32; Layout.fillWidth: true; onClicked: controller.download(card.modelData) }
                                    }
                                }
                            }
                            footer: Item {
                                width: grid.width; height: controller.cursor || controller.busy ? 56 : 0
                                RowLayout {
                                    anchors.centerIn: parent
                                    BusyIndicator { running: controller.busy; visible: running; implicitWidth: 28; implicitHeight: 28 }
                                    ActionButton { paletteTokens: page.theme; text: "加载更多"; visible: controller.cursor !== ""; enabled: !controller.busy; onClicked: controller.load(true) }
                                }
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: controller.ready && !controller.busy && controller.items.length === 0 && controller.message === ""
                                text: controller.params.mode === "following" ? "还没有收藏作者，搜索歌手或打开频道后收藏吧。" : "搜索一首喜欢的歌，开始漫游。"
                                width: parent.width - 24; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: page.theme.textMuted
                            }
                        }
                    }
                }
            }
        }
        Label {
            text: controller.actionMessage; textFormat: Text.PlainText
            visible: text !== ""; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
            color: page.theme.textSecondary; Layout.fillWidth: true
        }
        ListView {
            id: downloadList; objectName: "downloadList"
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(108, contentHeight)
            visible: count > 0; clip: true; model: controller.downloads
            ScrollBar.vertical: ScrollBar {}
            delegate: RowLayout {
                required property var modelData
                width: downloadList.width; height: 36
                Label { text: modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; Layout.fillWidth: true; color: page.theme.textPrimary }
                ProgressBar { visible: modelData.state === "running"; indeterminate: !modelData.total; value: modelData.total ? modelData.received / modelData.total : 0; Layout.preferredWidth: 100 }
                Label { text: modelData.state === "finished" ? "已加入曲库" : modelData.state === "cancelled" ? "已取消" : modelData.state === "failed" ? (modelData.message || "下载失败") : "下载中"; color: page.theme.textSecondary; textFormat: Text.PlainText; elide: Text.ElideRight; Layout.maximumWidth: 240 }
                ActionButton { paletteTokens: page.theme; visible: modelData.state === "running"; text: "取消"; implicitHeight: 30; onClicked: controller.cancel(modelData.task) }
            }
        }
    }
    Component.onDestruction: controller.dispose()
}
