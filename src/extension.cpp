#include "extension.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QStandardPaths>
#include <algorithm>

namespace youtube {
Extension::Extension(sdk::ExtensionContext &context) : m_context(context) {}
Extension::~Extension() { close(); }
void Extension::close() {
    m_registrations.clear();
    m_extractor.close();
    m_proxy.close();
}
void Extension::activate(sdk::Done done) {
    auto add = [this](const QString &name, sdk::Handler handler) {
        m_registrations.push_back(m_context.services().add(name, std::move(handler)));
    };
    add("browse", [this](const auto &params, auto reply) { browse(params, std::move(reply)); });
    add("video", [this](const auto &params, auto reply) {
        extract(params.value("id").toString(), true, [this, reply](Result<QJsonObject> result) {
            if (!result) { reply(result.error()); return; }
            const auto url = m_proxy.video(result.value(), m_context.config().get().value("proxy").toString().trimmed());
            if (!url) { reply(url.error()); return; }
            reply(sdk::Params{{"url", url.value()}, {"track", item(result.value())}, {"height", result.value().value("height").toInt()}});
        });
    });
    add("release-video", [this](const auto &params, auto reply) { m_proxy.release(params.value("url").toString()); reply(sdk::Params{}); });
    add("status", [this](const auto &, auto reply) {
        const auto config = m_context.config().get();
        reply(sdk::Params{{"available", !QStandardPaths::findExecutable(config.value("ytDlpPath", "yt-dlp").toString()).isEmpty()},
            {"runtime", !QStandardPaths::findExecutable("deno").isEmpty() ? "deno" : !QStandardPaths::findExecutable("node").isEmpty() ? "node" : ""},
            {"config", config}});
    });
    add("configure", [this](const auto &params, auto reply) {
        auto config = m_context.config().get();
        for (const auto &key : {"ytDlpPath", "proxy", "cookiesFile"}) if (params.contains(key)) config.insert(key, params.value(key).toString().trimmed());
        if (QStandardPaths::findExecutable(config.value("ytDlpPath").toString()).isEmpty()) {
            reply(failure("找不到 yt-dlp 可执行文件")); return;
        }
        const auto proxy = config.value("proxy").toString();
        const QUrl proxyUrl(proxy);
        if (!proxy.isEmpty() && (!proxyUrl.isValid() || proxyUrl.host().isEmpty() ||
            !QStringList{"http", "socks5", "socks5h"}.contains(proxyUrl.scheme()))) {
            reply(failure("代理地址支持 http:// 或 socks5://")); return;
        }
        const auto cookies = config.value("cookiesFile").toString();
        if (!cookies.isEmpty() && !QFileInfo(cookies).isFile()) { reply(failure("Cookie 文件不存在")); return; }
        m_context.config().update(config, [reply](Result<void> result) { if (result) reply(sdk::Params{{"saved", true}}); else reply(result.error()); });
    });
    add("following", [this](const auto &, auto reply) {
        m_context.storage().get("following", [reply](Result<QVariant> value) {
            if (!value) reply(value.error()); else reply(sdk::Params{{"channels", value.value().toList()}});
        });
    });
    add("follow", [this](const auto &params, auto reply) {
        const auto url = channelUrl(params.value("url").toString());
        if (!url) { reply(url.error()); return; }
        m_context.storage().get("following", [this, params, url = url.value(), reply](Result<QVariant> value) {
            if (!value) { reply(value.error()); return; }
            auto list = value.value().toList();
            list.removeIf([&url](const QVariant &entry) { return entry.toMap().value("url").toString() == url; });
            if (params.value("follow", true).toBool()) {
                if (list.size() >= 200) { reply(failure("最多收藏 200 个作者频道")); return; }
                list.append(sdk::Params{{"url", url}, {"title", params.value("title", url).toString().left(200)}});
            }
            m_context.storage().set("following", list, [reply, list](Result<void> result) {
                if (!result) reply(result.error()); else reply(sdk::Params{{"channels", list}});
            });
        });
    });
    m_registrations.push_back(m_context.music().add("music", this, {.name = "YouTube", .download = true, .page = "browse"}));
    if (std::any_of(m_registrations.begin(), m_registrations.end(), [](const auto &handle) { return !handle; })) {
        close(); done(failure("无法注册 YouTube 扩展服务")); return;
    }
    done({});
}
void Extension::browse(const sdk::Params &params, sdk::Reply done) {
    const auto request = browseRequest(params);
    if (!request) { done(request.error()); return; }
    m_extractor.run(m_context.config().get(), {"--flat-playlist", "--playlist-start", QString::number(request.value().start),
        "--playlist-end", QString::number(request.value().end), "--", request.value().target},
        [request = request.value(), done](Result<QJsonObject> result) {
            if (!result) { done(result.error()); return; }
            auto page = browseResult(result.value(), request);
            if (result.value().value("partial").toBool()) page.insert("warning", "部分结果未能加载，可以重试或继续浏览");
            done(page);
        });
}
void Extension::extract(const QString &id, bool video, Extractor::Reply done) {
    const auto valid = videoId(id);
    if (!valid) { done(valid.error()); return; }
    const QString format = video ? "bv[ext=mp4][vcodec^=avc1][height<=720][protocol=https]+ba[ext=m4a][protocol=https]/bv[ext=mp4][height<=720][protocol=https]+ba[ext=m4a][protocol=https]/b[height<=720][protocol=https]" : "ba[ext=m4a][protocol=https]/ba[protocol=https]";
    m_extractor.run(m_context.config().get(), {"--no-playlist", "--format", format, "--", "https://www.youtube.com/watch?v=" + valid.value()}, std::move(done));
}
void Extension::search(const QString &query, const QString &cursor, std::function<void(Result<sdk::SearchPage>)> done) {
    browse({{"query", query}, {"cursor", cursor}}, [done](Result<sdk::Params> result) {
        if (!result) { done(result.error()); return; }
        sdk::SearchPage page;
        for (const auto &value : result.value().value("items").toList()) {
            const auto entry = value.toMap();
            if (entry.value("kind") == "video") page.tracks.append(trackFromItem(entry));
        }
        page.cursor = result.value().value("cursor").toString();
        done(page);
    });
}
void Extension::track(const QString &id, std::function<void(Result<sdk::Track>)> done) {
    extract(id, false, [done](Result<QJsonObject> result) {
        if (!result) done(result.error()); else done(trackFromItem(item(result.value())));
    });
}
void Extension::resolve(const QString &id, sdk::ResolvePurpose purpose, std::function<void(Result<sdk::ResolvedAudio>)> done) {
    Q_UNUSED(purpose)
    extract(id, false, [this, done](Result<QJsonObject> result) {
        if (!result) { done(result.error()); return; }
        auto audio = audioFromJson(result.value());
        const auto proxy = m_context.config().get().value("proxy").toString().trimmed();
        if (audio && !proxy.isEmpty()) {
            const auto url = m_proxy.lease(audio.value(), proxy);
            if (!url) { done(url.error()); return; }
            audio.value().url = QUrl(url.value());
            audio.value().headers.clear();
            audio.value().allowedHosts.clear();
        }
        done(std::move(audio));
    });
}
}
