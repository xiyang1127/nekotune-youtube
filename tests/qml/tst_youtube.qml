import QtQuick
import QtTest
import "../../qml" as YouTube

TestCase {
    id: tests
    name: "YouTubeInterface"
    when: windowShown
    visible: true
    width: 1815; height: 868
    QtObject {
        id: service
        property var requests: []
        property var pending: []
        property var calls: []
        property var releases: []
        signal event(var message)
        function invoke(name, params) {
            if (name === "release-video") { releases = releases.concat([params.url]); return Promise.resolve({}) }
            requests = requests.concat([{name: name, params: params}])
            return new Promise(function(resolve, reject) { service.pending = service.pending.concat([{resolve: resolve, reject: reject}]) })
        }
        function call(name, params) { calls = calls.concat([{name: name, params: params}]); return Promise.resolve({task: "task-1"}) }
    }
    Component { id: logic; YouTube.Controller { serviceApi: service } }
    Component { id: ui; YouTube.Page {} }
    Component { id: settings; YouTube.SettingsPage {} }
    Component { id: video; YouTube.VideoPlayer {} }
    function init() { service.requests = []; service.pending = []; service.calls = []; service.releases = [] }
    function test_staleSearchAndStablePaging() {
        const object = createTemporaryObject(logic, tests)
        object.search("first", "video")
        object.search("second", "channel")
        service.pending[1].resolve({items: [{kind: "channel", id: "second"}], cursor: "2"})
        tryCompare(object, "busy", false)
        service.pending[0].resolve({items: [{kind: "video", id: "first"}]})
        wait(10)
        compare(object.items[0].id, "second")
        object.load(true)
        compare(service.requests[2].params.query, "second")
        compare(service.requests[2].params.kind, "channel")
        compare(service.requests[2].params.cursor, "2")
        service.pending[2].resolve({items: [{kind: "channel", id: "second"}, {kind: "channel", id: "third"}]})
        tryCompare(object, "busy", false)
        compare(object.items.length, 2)
    }
    function test_channelTabsFollowAndBack() {
        const object = createTemporaryObject(logic, tests)
        object.openChannel("https://youtube.com/@artist", "Artist")
        compare(service.requests[0].params.tab, "videos")
        service.pending[0].resolve({items: [], title: "Artist"})
        tryCompare(object, "busy", false)
        object.changeTab("shorts")
        compare(service.requests[1].params.tab, "shorts")
        service.pending[1].resolve({items: []})
        tryCompare(object, "busy", false)
        object.toggleFollow()
        compare(service.requests[2].name, "follow")
        service.pending[2].resolve({channels: [{url: "https://youtube.com/@artist", title: "Artist"}]})
        tryCompare(object, "isFollowing", true)
        object.back()
        compare(object.channelMode, false)
    }
    function test_lateVideoIsReleasedAndHostPaused() {
        const object = createTemporaryObject(logic, tests)
        object.watch({id: "abcdefghijk", title: "Song"})
        object.closeVideo()
        service.pending[0].resolve({url: "http://127.0.0.1/late"})
        tryCompare(service, "releases", ["http://127.0.0.1/late"])
        compare(object.videoUrl, "")
        object.watch({id: "abcdefghijk", title: "Song"})
        service.pending[1].resolve({url: "http://127.0.0.1/current", height: 360})
        tryCompare(object, "videoBusy", false)
        compare(service.calls[0].name, "player.pause")
        compare(object.videoUrl, "http://127.0.0.1/current")
        object.closeVideo()
        compare(service.releases.length, 2)
    }
    function test_downloadTracksOnlyOwnTasks() {
        const object = createTemporaryObject(logic, tests)
        object.download({id: "abcdefghijk", title: "Song"})
        service.event({event: "music.download", task: "task-1", state: "finished", song_id: 42})
        tryCompare(object, "startingDownloads", 0)
        compare(object.downloads[0].state, "finished")
        service.event({event: "music.download", task: "another-task", state: "failed"})
        compare(object.downloads[0].state, "finished")
    }
    function test_pagesLoadAndFit() {
        const object = createTemporaryObject(ui, tests, {width: 1100, height: 820})
        verify(object !== null)
        const field = findChild(object, "searchField")
        verify(field !== null)
        const grid = findChild(object, "videoGrid")
        tryCompare(grid, "count", 3)
        object.focusSearch(); verify(field.activeFocus)
        wait(100)
        if (screenshotPath !== "") grabImage(object).save(screenshotPath)
        object.width = 620; object.height = 640; wait(20)
        verify(grid.width <= 620)
        verify(createTemporaryObject(settings, tests, {width: 700, height: 700}) !== null)
        verify(createTemporaryObject(video, tests, {width: 700, height: 400}) !== null)
    }
    function test_watchLayoutWideAndNarrow() {
        const object = createTemporaryObject(ui, tests, {width: 1815, height: 868})
        const grid = findChild(object, "videoGrid")
        tryCompare(grid, "count", 3)
        object.viewModel.params = {mode: "channel", channel: "https://www.youtube.com/@artist", tab: "videos"}
        object.viewModel.title = "MIMI"
        object.viewModel.description = "作者频道 · 视频与 Shorts"
        object.viewModel.videoTrack = object.viewModel.items[0]
        object.viewModel.videoBusy = true
        const frame = findChild(object, "videoFrame")
        const watch = findChild(object, "watchPanel")
        const browser = findChild(object, "browserPanel")
        const scroll = findChild(object, "browseScroll")
        tryVerify(function() { return frame.width > 800 })
        compare(object.sideBySide, true)
        verify(Math.abs(frame.height - frame.width * 9 / 16) < 2)
        verify(browser.x >= watch.x + watch.width)
        verify(grid.height >= 250)
        verify(browser.mapToItem(object, 0, 0).y >= 0)
        const download = findChild(object, "watchDownload")
        verify(download.mapToItem(object, 0, download.height).y <= object.height - 24)
        wait(40)
        if (screenshotPath !== "") grabImage(object).save(screenshotPath.replace(".png", "-watch.png"))
        object.width = 620; object.height = 640
        tryCompare(object, "sideBySide", false)
        tryVerify(function() { return browser.y >= watch.y + watch.height })
        verify(Math.abs(frame.height - frame.width * 9 / 16) < 2)
        verify(scroll.contentHeight > scroll.height)
        verify(grid.height >= 200)
        scroll.contentY = scroll.contentHeight - scroll.height
        wait(20)
        verify(grid.mapToItem(object, 0, 0).y < object.height)
        object.viewModel.download(object.viewModel.items[0])
        const downloads = findChild(object, "downloadList")
        tryCompare(downloads, "count", 1)
        verify(downloads.height <= 108)
        if (screenshotPath !== "") grabImage(object).save(screenshotPath.replace(".png", "-narrow.png"))
        object.viewModel.closeVideo()
    }
}
