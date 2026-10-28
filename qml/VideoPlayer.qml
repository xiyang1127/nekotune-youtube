import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root
    property var controller: null
    property var theme: ({})
    function time(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        const minutes = Math.floor(seconds / 60)
        return (minutes >= 60 ? Math.floor(minutes / 60) + ":" + ("0" + minutes % 60).slice(-2) : minutes)
            + ":" + ("0" + seconds % 60).slice(-2)
    }
    Connections {
        target: root.controller
        function onHostPlaybackStarted() { player.pause() }
    }
    MediaPlayer {
        id: player
        source: root.controller ? root.controller.videoUrl : ""
        videoOutput: video
        audioOutput: AudioOutput { volume: volume.value }
        onSourceChanged: if (source.toString() !== "") play()
        onErrorOccurred: if (root.controller) root.controller.videoMessage = "视频播放失败，地址可能已过期，请点击重试。"
        onMediaStatusChanged: if (mediaStatus === MediaPlayer.EndOfMedia && root.controller) root.controller.nextVideo(1)
    }
    VideoOutput { id: video; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit }
    BusyIndicator {
        anchors.centerIn: parent
        visible: player.mediaStatus === MediaPlayer.LoadingMedia || player.mediaStatus === MediaPlayer.BufferingMedia
        running: visible
    }
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 76
        gradient: Gradient {
            GradientStop { position: 0; color: "#00000000" }
            GradientStop { position: 1; color: "#e6000000" }
        }
    }
    RowLayout {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 12; spacing: 10
        ActionButton {
            paletteTokens: root.theme
            text: player.playing ? "暂停" : "播放"
            onClicked: {
                if (player.playing) player.pause()
                else if (root.controller) root.controller.serviceApi.call("player.pause", {}).then(() => player.play()).catch(() => {})
            }
        }
        Slider {
            id: seek; Layout.fillWidth: true; from: 0; to: Math.max(1, player.duration)
            value: player.position; enabled: player.seekable
            onMoved: player.position = value
            Accessible.name: "播放进度"
        }
        Label { text: root.time(player.position) + " / " + root.time(player.duration); color: "white"; font.pixelSize: 12 }
        Slider {
            id: volume; from: 0; to: 1; value: 0.7
            visible: root.width >= 500
            Layout.preferredWidth: 76; Accessible.name: "视频音量"
        }
    }
    onVisibleChanged: if (!visible) player.pause()
    Component.onDestruction: player.stop()
}
