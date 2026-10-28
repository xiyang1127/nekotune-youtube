import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var controller
    required property var theme
    spacing: 14
    Rectangle {
        objectName: "videoFrame"
        Layout.fillWidth: true
        Layout.preferredHeight: width * 9 / 16
        Layout.minimumHeight: width * 9 / 16
        Layout.maximumHeight: width * 9 / 16
        color: "#08090b"; radius: 12; clip: true
        Image {
            anchors.fill: parent
            visible: panel.controller.videoUrl === ""
            source: panel.controller.videoTrack.cover_url || ""
            asynchronous: true; fillMode: Image.PreserveAspectFit; opacity: 0.35
        }
        Loader {
            id: playerLoader
            anchors.fill: parent
            active: panel.controller.videoUrl !== ""
            source: "VideoPlayer.qml"
            onLoaded: { item.controller = panel.controller; item.theme = panel.theme }
            onStatusChanged: if (status === Loader.Error)
                panel.controller.videoMessage = "视频组件不可用，请安装与宿主匹配的 Qt Multimedia。"
        }
        ColumnLayout {
            anchors.centerIn: parent
            visible: panel.controller.videoUrl === ""
            BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: panel.controller.videoBusy; visible: running }
            Label { text: panel.controller.videoBusy ? "正在加载视频…" : "视频暂不可用"; color: "#eeeeef" }
        }
        Label {
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 14
            visible: panel.controller.videoHeight > 0
            text: panel.controller.videoHeight + "p"; color: "white"; font.pixelSize: 12
        }
    }
    Label {
        Layout.fillWidth: true
        text: panel.controller.videoTrack.title || ""; textFormat: Text.PlainText
        color: panel.theme.textPrimary; font.pixelSize: 20; font.bold: true
        wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 8
        ActionButton {
            objectName: "watchAuthor"
            Layout.fillWidth: true; Layout.maximumWidth: 260
            paletteTokens: panel.theme; quiet: true; textAlignment: Text.AlignLeft
            text: (panel.controller.videoTrack.artist || "作者主页") + "  ↗"
            enabled: !!panel.controller.videoTrack.channel_url
            onClicked: panel.controller.openChannel(panel.controller.videoTrack.channel_url, panel.controller.videoTrack.artist)
        }
        Item { Layout.fillWidth: true }
        ActionButton { objectName: "watchDownload"; paletteTokens: panel.theme; text: "下载歌曲"; highlighted: true; onClicked: panel.controller.download(panel.controller.videoTrack) }
        ActionButton { paletteTokens: panel.theme; text: "关闭"; onClicked: panel.controller.closeVideo() }
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 8
        ActionButton { paletteTokens: panel.theme; text: "上一个"; onClicked: panel.controller.nextVideo(-1) }
        ActionButton { paletteTokens: panel.theme; text: "下一个"; onClicked: panel.controller.nextVideo(1) }
        ActionButton { paletteTokens: panel.theme; text: "重试"; visible: panel.controller.videoMessage !== ""; onClicked: panel.controller.watch(panel.controller.videoTrack) }
        Item { Layout.fillWidth: true }
        ActionButton { paletteTokens: panel.theme; quiet: true; text: "在 YouTube 打开 ↗"; onClicked: Qt.openUrlExternally(panel.controller.videoTrack.webpage_url) }
    }
    Label {
        Layout.fillWidth: true
        text: panel.controller.videoMessage; textFormat: Text.PlainText
        visible: text !== ""; wrapMode: Text.Wrap
        color: panel.theme.statusError || "#f48a96"
    }
}
