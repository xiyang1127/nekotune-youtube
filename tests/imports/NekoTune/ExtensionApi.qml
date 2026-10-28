import QtQuick
QtObject {
    property var theme: ({bgCanvas: "#141519", bgRaised: "#25262c", textPrimary: "#f2f2f4", textSecondary: "#a9aab4", textMuted: "#747682"})
    property var window: null
    signal event(var message)
    function invoke(service, params) {
        if (service === "status") return Promise.resolve({available: true, runtime: "deno", config: {ytDlpPath: "yt-dlp"}})
        if (service === "following") return Promise.resolve({channels: []})
        if (service === "browse") return Promise.resolve({items: [
            {id: "abcdefghijk", kind: "video", title: "夜に駆ける / Official Music Video", artist: "YOASOBI", duration_ms: 263000, channel_url: "https://www.youtube.com/@YOASOBI_Official"},
            {id: "bcdefghijkl", kind: "video", title: "A little music for your afternoon", artist: "Music Studio", duration_ms: 187000, channel_url: "https://www.youtube.com/@music"},
            {id: "cdefghijklm", kind: "video", title: "Live session · Acoustic", artist: "Acoustic Room", duration_ms: 301000, channel_url: "https://www.youtube.com/@acoustic"}
        ], cursor: "2"})
        return Promise.resolve({})
    }
    function call(method, params) { return Promise.resolve({task: "download-fixture"}) }
}
