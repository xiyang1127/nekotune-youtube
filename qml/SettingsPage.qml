import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import NekoTune 1.0

Rectangle {
    id: page
    color: api.theme.bgCanvas
    property string message: ""
    property bool busy: false
    ExtensionApi { id: api }
    function refresh() {
        api.invoke("status").then(result => {
            binary.text = result.config.ytDlpPath || "yt-dlp"
            proxy.text = result.config.proxy || ""
            cookies.text = result.config.cookiesFile || ""
            message = result.available ? "已找到 yt-dlp · JavaScript 运行时：" + (result.runtime || "未找到，请安装 Deno") : "尚未找到 yt-dlp，请填写路径。"
        }).catch(error => message = String(error))
    }
    ScrollView {
        anchors.fill: parent; anchors.margins: 28
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 18
            Label { text: "YouTube 设置"; color: api.theme.textPrimary; font.pixelSize: 26; font.bold: true }
            Label { text: "设置解析工具和网络后，即可搜索歌曲、进入作者频道及下载音频。"; color: api.theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "yt-dlp 可执行文件"; color: api.theme.textPrimary }
            TextField { id: binary; objectName: "binaryPath"; Layout.fillWidth: true; placeholderText: "yt-dlp 或可执行文件的完整路径"; selectByMouse: true }
            Label { text: "网络代理（可选）"; color: api.theme.textPrimary }
            TextField { id: proxy; Layout.fillWidth: true; placeholderText: "http://127.0.0.1:7890 或 socks5://127.0.0.1:1080"; selectByMouse: true }
            Label { text: "Cookie 文件（可选）"; color: api.theme.textPrimary }
            TextField { id: cookies; Layout.fillWidth: true; placeholderText: "自己的 Netscape 格式 cookies.txt 文件路径"; selectByMouse: true }
            Label { text: "需要登录验证时填写 Cookie 文件。文件留在本机，由 yt-dlp 读取。"; color: api.theme.textMuted; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Button {
                    objectName: "saveSettings"; text: page.busy ? "保存中…" : "保存设置"; highlighted: true; enabled: !page.busy
                    onClicked: {
                        page.busy = true
                        api.invoke("configure", {ytDlpPath: binary.text, proxy: proxy.text, cookiesFile: cookies.text})
                            .then(() => { page.busy = false; page.message = "设置已保存，返回 YouTube 页面后重试即可。" })
                            .catch(error => { page.busy = false; page.message = String(error) })
                    }
                }
                Button { text: "检测配置"; onClicked: page.refresh() }
            }
            Label { text: page.message; textFormat: Text.PlainText; color: api.theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "视频最高 720p，支持音画分离流同步播放；实际清晰度由视频提供的格式决定。下载优先保留原始 M4A 音频，无需转码，完成后加入本地曲库。"; color: api.theme.textMuted; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Button { text: "yt-dlp 安装说明"; onClicked: Qt.openUrlExternally("https://github.com/yt-dlp/yt-dlp#installation") }
                Button { text: "JavaScript 运行时说明"; onClicked: Qt.openUrlExternally("https://github.com/yt-dlp/yt-dlp/wiki/EJS") }
            }
        }
    }
    Component.onCompleted: refresh()
}
