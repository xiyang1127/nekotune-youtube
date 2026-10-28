import QtQuick
import NekoTune 1.0

QtObject {
    id: root
    property var serviceApi: api
    property var api: ExtensionApi {}
    property var eventConnection: Connections {
        target: root.serviceApi
        function onEvent(event) { root.downloadEvent(event) }
    }
    property bool ready: false
    property bool available: false
    property bool busy: false
    property bool videoBusy: false
    property bool followBusy: false
    property bool disposed: false
    property int generation: 0
    property int videoGeneration: 0
    property var items: []
    property var following: []
    property var history: []
    property var downloads: []
    property var earlyEvents: []
    property int startingDownloads: 0
    property var params: ({mode: "discover"})
    property string cursor: ""
    property string title: "发现音乐"
    property string description: ""
    property string message: ""
    property string videoMessage: ""
    property string actionMessage: ""
    property string videoUrl: ""
    property var videoTrack: ({})
    property int videoHeight: 0
    readonly property bool channelMode: params.mode === "channel"
    readonly property bool isFollowing: following.some(value => value.url === params.channel)
    signal hostPlaybackStarted()

    function init() {
        serviceApi.invoke("following").then(result => following = result.channels || []).catch(error => actionMessage = String(error))
        serviceApi.invoke("status").then(result => {
            if (disposed) return
            ready = true; available = result.available
            if (available) navigate({mode: "discover"}, "发现音乐", false)
            else message = "尚未找到 yt-dlp，请打开右上角设置完成配置。"
        }).catch(error => { ready = true; message = String(error) })
    }
    function snapshot() { return {params: params, title: title, description: description, items: items, cursor: cursor} }
    function navigate(nextParams, nextTitle, remember) {
        if (remember !== false) history = history.concat([snapshot()]).slice(-30)
        params = Object.assign({}, nextParams)
        title = nextTitle; description = ""; items = []; cursor = ""; message = ""
        load(false)
    }
    function back() {
        if (!history.length) return
        ++generation; busy = false
        const previous = history[history.length - 1]
        history = history.slice(0, -1)
        params = previous.params; title = previous.title; description = previous.description
        items = previous.items; cursor = previous.cursor; message = ""
    }
    function showFollowing() {
        history = history.concat([snapshot()]).slice(-30)
        ++generation; busy = false; cursor = ""; message = ""
        params = {mode: "following"}; title = "收藏的作者"; description = "保存在本机，点击作者进入频道"
        items = following.map(value => ({kind: "channel", id: value.url, title: value.title, channel_url: value.url, cover_url: ""}))
    }
    function load(more) {
        if (params.mode === "following" || (more && (!cursor || busy))) return
        const request = ++generation
        const requestParams = Object.assign({}, params, {cursor: more ? cursor : ""})
        busy = true; message = ""
        serviceApi.invoke("browse", requestParams).then(result => {
            if (disposed || request !== generation) return
            busy = false
            const incoming = result.items || []
            const keys = {}
            const combined = more ? items.concat(incoming) : incoming
            items = combined.filter(value => { const key = value.kind + ":" + value.id; if (keys[key]) return false; keys[key] = true; return true })
            cursor = result.cursor || ""
            if (channelMode) { title = result.title || title; description = result.description || "" }
            message = result.warning || (items.length ? "" : "没有找到结果，换个关键词试试。")
        }).catch(error => { if (request === generation && !disposed) { busy = false; message = String(error) } })
    }
    function search(query, kind) {
        if (!query.trim()) return
        navigate({mode: "search", query: query.trim(), kind: kind || "video"}, "搜索 · " + query.trim())
    }
    function openChannel(url, name, tab) {
        if (url) navigate({mode: "channel", channel: url, tab: tab || "videos"}, name || "作者频道")
    }
    function changeTab(tab) {
        if (!channelMode || params.tab === tab) return
        params = Object.assign({}, params, {tab: tab}); items = []; cursor = ""; load(false)
    }
    function toggleFollow() {
        if (!channelMode || followBusy) return
        followBusy = true
        serviceApi.invoke("follow", {url: params.channel, title: title, follow: !isFollowing}).then(result => {
            following = result.channels || []; followBusy = false
        }).catch(error => { actionMessage = String(error); followBusy = false })
    }
    function release(url) { if (url) serviceApi.invoke("release-video", {url: url}).catch(() => {}) }
    function closeVideo() {
        ++videoGeneration; videoBusy = false
        const url = videoUrl; videoUrl = ""; videoTrack = ({}); videoMessage = ""; release(url)
    }
    function watch(track) {
        if (track.kind === "channel") { openChannel(track.channel_url, track.title); return }
        closeVideo()
        videoTrack = track; videoBusy = true
        const request = ++videoGeneration
        serviceApi.invoke("video", {id: track.id}).then(result => {
            if (disposed || request !== videoGeneration) { release(result.url); return }
            return serviceApi.call("player.pause", {}).then(() => {
                if (disposed || request !== videoGeneration) { release(result.url); return }
                videoUrl = result.url; videoTrack = result.track || track; videoHeight = result.height || 0; videoBusy = false
            }).catch(error => { release(result.url); throw error })
        }).catch(error => { if (!disposed && request === videoGeneration) { videoBusy = false; videoMessage = String(error) } })
    }
    function nextVideo(offset) {
        const index = items.findIndex(value => value.id === videoTrack.id)
        for (let i = index + offset; i >= 0 && i < items.length; i += offset) {
            if (items[i].kind === "video") { watch(items[i]); return }
        }
        videoMessage = cursor ? "本页已看完，请加载更多视频。" : "已经到列表末尾。"
    }
    function music(method, track) {
        if (method === "enqueue") closeVideo()
        actionMessage = ""
        serviceApi.call("music." + method, {source: "nekotune.youtube/music", track: track, play: true})
            .then(() => actionMessage = "已开始播放音频").catch(error => actionMessage = String(error))
    }
    function download(track) {
        if (startingDownloads + downloads.filter(value => value.state === "running").length >= 3) {
            actionMessage = "最多同时下载 3 首歌曲，请等待当前任务完成。"; return
        }
        ++startingDownloads; actionMessage = "正在准备下载…"
        serviceApi.call("music.download", {source: "nekotune.youtube/music", track: track}).then(result => {
            --startingDownloads
            downloads = downloads.concat([{task: result.task, title: track.title, state: "running", message: "解析并下载音频…", received: 0, total: 0}]).slice(-8)
            const events = earlyEvents; earlyEvents = []
            for (const event of events) downloadEvent(event)
            actionMessage = "下载任务已创建，完成后自动加入本地曲库。"
        }).catch(error => { --startingDownloads; actionMessage = String(error) })
    }
    function downloadEvent(event) {
        if (event.event === "player.state_changed" && event.state === "playing") hostPlaybackStarted()
        if (event.event !== "music.download") return
        const index = downloads.findIndex(value => value.task === event.task)
        if (index < 0) { if (startingDownloads > 0) earlyEvents = earlyEvents.concat([event]).slice(-30); return }
        const copy = downloads.slice()
        copy[index] = Object.assign({}, copy[index], event)
        downloads = copy
    }
    function cancel(task) {
        serviceApi.call("music.cancel", {task: task}).catch(error => actionMessage = String(error))
    }
    function dispose() { disposed = true; ++generation; closeVideo() }
    Component.onDestruction: dispose()
}
