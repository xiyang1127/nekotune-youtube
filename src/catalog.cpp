#include "catalog.h"
#include <QDateTime>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>
#include <cmath>

namespace youtube {
Result<QString> videoId(const QString &input) {
    static const QRegularExpression id("^[A-Za-z0-9_-]{11}$");
    QString value = input.trimmed();
    if (!id.match(value).hasMatch()) {
        const QUrl url(value, QUrl::StrictMode);
        if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.port(443) != 443)
            return failure("请输入有效的 YouTube 视频链接或视频 ID");
        if (url.host() == "youtu.be")
            value = url.path().mid(1);
        else if (QStringList{"youtube.com", "www.youtube.com", "m.youtube.com", "music.youtube.com"}.contains(url.host())) {
            if (url.path() == "/watch") value = QUrlQuery(url).queryItemValue("v");
            else if (url.path().startsWith("/shorts/") || url.path().startsWith("/embed/")) value = url.path().section('/', 2, 2);
        }
    }
    if (!id.match(value).hasMatch()) return failure("无效的 YouTube 视频 ID");
    return value;
}

Result<QString> channelUrl(const QString &input) {
    QString value = input.trimmed();
    if (value.startsWith('@')) value.prepend("https://www.youtube.com/");
    if (QRegularExpression("^UC[A-Za-z0-9_-]{22}$").match(value).hasMatch())
        value.prepend("https://www.youtube.com/channel/");
    QUrl url(value, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != "https" || !url.userInfo().isEmpty() || url.port(443) != 443 ||
        !QStringList{"youtube.com", "www.youtube.com", "m.youtube.com"}.contains(url.host()))
        return failure("请输入 YouTube 作者频道链接、@用户名或频道 ID");
    const auto parts = url.path().split('/', Qt::SkipEmptyParts);
    QString path;
    if (!parts.isEmpty() && parts[0].startsWith('@') && parts[0].size() > 1)
        path = "/" + parts[0];
    else if (parts.size() >= 2 && QStringList{"channel", "c", "user"}.contains(parts[0]) && !parts[1].isEmpty())
        path = "/" + parts[0] + "/" + parts[1];
    if (path.isEmpty() || path.contains("..") || path.contains('\\') || path.contains(QChar::Null))
        return failure("无效的 YouTube 作者频道地址");
    QUrl clean("https://www.youtube.com");
    clean.setPath(path);
    return clean.toString();
}

bool mediaUrlAllowed(const QUrl &url) {
    return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty() &&
        url.port(443) == 443 && url.host().endsWith(".googlevideo.com");
}

sdk::Params item(const QJsonObject &object) {
    const auto id = object.value("id").toString();
    const bool channel = object.value("ie_key").toString() == "YoutubeTab" && id.startsWith("UC");
    if (!channel && !videoId(id)) return {};
    QString authorUrl = object.value("channel_url").toString();
    if (authorUrl.isEmpty()) authorUrl = object.value("uploader_url").toString();
    if (channel) authorUrl = object.value("url").toString();
    const auto canonical = channelUrl(authorUrl);
    QString thumbnail;
    const auto thumbs = object.value("thumbnails").toArray();
    if (!thumbs.isEmpty()) thumbnail = thumbs.last().toObject().value("url").toString();
    if (!channel && thumbnail.isEmpty()) thumbnail = "https://i.ytimg.com/vi/" + id + "/hqdefault.jpg";
    if (QUrl(thumbnail).scheme() != "https") thumbnail.clear();
    const double duration = object.value("duration").toDouble();
    return {{"id", id}, {"kind", channel ? "channel" : "video"}, {"title", object.value("title").toString("未命名视频")},
        {"artist", object.value("channel").toString(object.value("uploader").toString())},
        {"channel_url", canonical ? canonical.value() : QString()}, {"cover_url", thumbnail},
        {"duration_ms", std::isfinite(duration) && duration > 0 ? qint64(qMin(duration, 31'536'000.0) * 1000) : 0},
        {"view_count", object.value("view_count").toVariant()},
        {"webpage_url", channel && canonical ? canonical.value() : "https://www.youtube.com/watch?v=" + id},
        {"description", object.value("description").toString()}};
}

sdk::Track trackFromItem(const sdk::Params &value) {
    return {value.value("id").toString(), value.value("title").toString(), value.value("artist").toString(), {},
        value.value("cover_url").toString(), value.value("duration_ms").toLongLong()};
}

Result<sdk::ResolvedAudio> audioFromJson(const QJsonObject &object) {
    sdk::ResolvedAudio audio;
    audio.url = QUrl(object.value("url").toString());
    if (!mediaUrlAllowed(audio.url)) return failure("未取得可播放的 YouTube 媒体地址，请更新 yt-dlp 后重试", ErrorCode::Unavailable);
    const auto headers = object.value("http_headers").toObject();
    static const QRegularExpression token("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$");
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        const auto key = it.key().toLower();
        const auto value = it.value().toString();
        if (QStringList{"host", "connection", "content-length", "transfer-encoding"}.contains(key)) continue;
        if (!token.match(it.key()).hasMatch() || value.contains('\r') || value.contains('\n'))
            return failure("无效的媒体请求头");
        audio.headers.insert(it.key(), value);
    }
    audio.extension = object.value("ext").toString();
    if (!QStringList{"m4a", "webm", "mp4", "opus", "mp3", "ogg", "aac"}.contains(audio.extension))
        return failure("此媒体格式暂不支持");
    audio.expiresAt = QUrlQuery(audio.url).queryItemValue("expire").toLongLong() * 1000;
    if (!audio.expiresAt) audio.expiresAt = QDateTime::currentMSecsSinceEpoch() + 10 * 60 * 1000;
    audio.allowedHosts = {"*.googlevideo.com"};
    audio.maxBytes = 512LL * 1024 * 1024;
    return audio;
}

Result<BrowseRequest> browseRequest(const sdk::Params &params) {
    const QString cursor = params.value("cursor").toString();
    bool ok = true;
    const int page = cursor.isEmpty() ? 1 : cursor.toInt(&ok);
    if (!ok || page < 1 || page > 50) return failure("无效的分页游标");
    BrowseRequest request;
    request.start = (page - 1) * 20 + 1;
    request.end = page * 20;
    const auto mode = params.value("mode", "search").toString();
    if (mode == "channel") {
        const auto url = channelUrl(params.value("channel").toString());
        if (!url) return url.error();
        const auto tab = params.value("tab", "videos").toString();
        if (!QStringList{"videos", "shorts", "streams"}.contains(tab)) return failure("不支持的频道分类");
        request.target = url.value() + "/" + tab;
    } else if (mode == "search" || mode == "discover") {
        const auto query = mode == "discover" ? QString("音乐 官方 MV") : params.value("query").toString().trimmed();
        if (query.isEmpty() || query.size() > 300) return failure("请输入 1–300 个字的歌曲、歌手或视频名称");
        if (query.startsWith("https://")) {
            const auto id = videoId(query);
            if (!id) return id.error();
            request.target = "https://www.youtube.com/watch?v=" + id.value();
        } else if (params.value("kind").toString() == "channel") {
            QUrl url("https://www.youtube.com/results");
            QUrlQuery q;
            q.addQueryItem("search_query", query);
            q.addQueryItem("sp", "EgIQAg==");
            url.setQuery(q);
            request.target = url.toString(QUrl::FullyEncoded);
        } else request.target = "ytsearch" + QString::number(request.end) + ":" + query;
    } else return failure("不支持的浏览模式");
    return request;
}

sdk::Params browseResult(const QJsonObject &object, const BrowseRequest &request) {
    QVariantList items;
    auto entries = object.value("entries").toArray();
    const bool playlist = object.contains("entries");
    if (!playlist) entries.append(object);
    for (const auto &entry : entries) {
        const auto value = item(entry.toObject());
        if (!value.isEmpty()) items.append(value);
    }
    return {{"items", items}, {"cursor", playlist && entries.size() >= 20 && request.end < 1000 ? QString::number(request.end / 20 + 1) : QString()},
        {"title", object.value("channel").toString(object.value("title").toString())},
        {"description", object.value("description").toString()}};
}
}
