#include "catalog.h"
#include "extension.h"
#include "extractor.h"
#include "fake_context.h"
#include <QJsonArray>
#include <QNetworkReply>
#include <QPluginLoader>
#include <QTcpSocket>
#include <QtTest>
#include <nekotune/sdk/extension_plugin.h>

using namespace youtube;
class YouTubeTest final : public QObject {
    Q_OBJECT
    QString executable() const {
        return QCoreApplication::applicationDirPath() + "/fake_extractor"
#ifdef Q_OS_WIN
            ".exe"
#endif
            ;
    }
    sdk::Params config() const { return {{"ytDlpPath", executable()}}; }
  private slots:
    void validatesLinksAndBounds() {
        QVERIFY(videoId("abcdefghijk"));
        QCOMPARE(videoId("https://youtu.be/abcdefghijk?t=12").value(), "abcdefghijk");
        QCOMPARE(videoId("https://www.youtube.com/shorts/abcdefghijk").value(), "abcdefghijk");
        QVERIFY(!videoId("https://evil.example/watch?v=abcdefghijk"));
        QVERIFY(!videoId("https://youtube.com.evil/watch?v=abcdefghijk"));
        QVERIFY(!videoId("https://user:password@youtube.com/watch?v=abcdefghijk"));
        QVERIFY(!videoId("--exec rm"));
        QCOMPARE(channelUrl("@歌手").value(), "https://www.youtube.com/@歌手");
        QCOMPARE(channelUrl("https://www.youtube.com/@artist/shorts?x=1").value(), "https://www.youtube.com/@artist");
        QVERIFY(!channelUrl("https://evil.example/@artist"));
        QVERIFY(!channelUrl("https://youtube.com/watch?v=abcdefghijk"));
        QVERIFY(!browseRequest({{"query", "hello"}, {"cursor", "-1"}}));
        QVERIFY(!browseRequest({{"query", "hello"}, {"cursor", "51"}}));
        QVERIFY(!browseRequest({{"query", ""}}));
        auto request = browseRequest({{"query", "song"}, {"cursor", "2"}});
        QVERIFY(request); QCOMPARE(request.value().start, 21); QCOMPARE(request.value().end, 40);
        QCOMPARE(request.value().target, "ytsearch40:song");
        auto author = browseRequest({{"mode", "channel"}, {"channel", "@artist"}, {"tab", "shorts"}});
        QVERIFY(author); QCOMPARE(author.value().target, "https://www.youtube.com/@artist/shorts");
    }
    void metadataAndMediaValidation() {
        auto value = item({{"id", "abcdefghijk"}, {"title", "Title"}, {"duration", 1.5}, {"channel_url", "https://youtube.com/@artist"}});
        QCOMPARE(value.value("duration_ms").toLongLong(), 1500);
        QCOMPARE(value.value("channel_url").toString(), "https://www.youtube.com/@artist");
        QVERIFY(item({{"id", "invalid"}}).isEmpty());
        auto channel = item({{"id", "UCvpredjG93ifbCP1Y77JyFA"}, {"ie_key", "YoutubeTab"}, {"url", "https://youtube.com/@artist"}});
        QCOMPARE(channel.value("kind").toString(), "channel");
        QVERIFY(!mediaUrlAllowed(QUrl("https://googlevideo.com.evil.example/video")));
        QVERIFY(!mediaUrlAllowed(QUrl("http://rr1.googlevideo.com/video")));
        QVERIFY(!mediaUrlAllowed(QUrl("https://127.0.0.1/video")));
        QJsonObject media{{"url", "https://rr1.googlevideo.com/video?expire=2000000000"}, {"ext", "m4a"}};
        auto result = audioFromJson(media); QVERIFY(result); QCOMPARE(result.value().expiresAt, 2000000000000LL);
        media.insert("http_headers", QJsonObject{{"Cookie", "unsafe\r\nInjected: x"}});
        QVERIFY(!audioFromJson(media));
    }
    void extractorFailuresAndCancellation() {
        Extractor extractor;
        for (const auto &target : {"ok", "error", "malformed", "oversized"}) {
            int count = 0;
            bool success = false;
            QString error;
            extractor.run(config(), {"--", target}, [&](Result<QJsonObject> result) {
                ++count; success = bool(result); if (!result) error = result.error().message;
            });
            QTRY_COMPARE_WITH_TIMEOUT(count, 1, 5000);
            QCOMPARE(success, QString(target) == "ok");
            QVERIFY(!error.contains("secret-token")); QVERIFY(!error.contains("password="));
        }
        int cancelled = 0, unavailable = 0;
        for (int i = 0; i < 5; ++i) extractor.run(config(), {"--", "slow"}, [&](Result<QJsonObject> result) {
            QVERIFY(!result);
            if (result.error().code == ErrorCode::Cancelled) ++cancelled;
            else ++unavailable;
        });
        QCOMPARE(unavailable, 1);
        extractor.close();
        QTRY_COMPARE(cancelled, 4);
        QTest::qWait(50); QCOMPARE(cancelled, 4);
    }
    void servicesProviderAndStop() {
        FakeContext context;
        context.configuration = config();
        Extension extension(context);
        bool activated = false;
        extension.activate([&](Result<void> result) { activated = bool(result); });
        QVERIFY(activated);
        for (const auto &name : context.serviceHandlers.keys())
            QVERIFY2(QRegularExpression("^[a-z][a-z0-9._-]{1,100}$").match(name).hasMatch(), qPrintable(name));
        QVERIFY(context.musicProviders.contains("music"));
        QCOMPARE(context.descriptors.value("music/music").value("page").toString(), "browse");
        int calls = 0;
        context.serviceHandlers.value("browse")({{"query", "song"}}, [&](Result<sdk::Params> result) {
            QVERIFY(result); QCOMPARE(result.value().value("items").toList().size(), 20); ++calls;
        });
        QTRY_COMPARE(calls, 1);
        context.musicProviders.value("music")->resolve("abcdefghijk", sdk::ResolvePurpose::Download, [&](Result<sdk::ResolvedAudio> result) {
            QVERIFY(result); QCOMPARE(result.value().extension, "m4a"); ++calls;
        });
        QTRY_COMPARE(calls, 2);
        auto followed = context.call("follow", {{"url", "@artist"}, {"title", "Artist"}});
        QVERIFY(followed); QCOMPARE(followed.value().value("channels").toList().size(), 1);
        QVERIFY(context.call("follow", {{"url", "@artist"}, {"title", "Artist"}}));
        QCOMPARE(context.call("following").value().value("channels").toList().size(), 1);
        QVERIFY(context.call("follow", {{"url", "@artist"}, {"follow", false}}));
        QVERIFY(context.call("following").value().value("channels").toList().isEmpty());
        QVERIFY(!context.call("configure", {{"proxy", "file:///etc/passwd"}}));
        context.serviceHandlers.value("browse")({{"query", "slow"}}, [&](Result<sdk::Params> result) {
            QVERIFY(!result); QCOMPARE(result.error().code, ErrorCode::Cancelled); ++calls;
        });
        extension.close();
        QTRY_COMPARE(calls, 3);
        QVERIFY(context.serviceHandlers.isEmpty()); QVERIFY(context.musicProviders.isEmpty());
    }
    void proxyRejectsUnknownAndRevokedTokens() {
        VideoProxy proxy;
        sdk::ResolvedAudio audio;
        audio.url = QUrl("https://rr1.googlevideo.com/video");
        audio.expiresAt = QDateTime::currentMSecsSinceEpoch() + 60'000;
        auto lease = proxy.lease(audio, ""); QVERIFY(lease);
        QNetworkAccessManager network;
        auto request = [&](QString url) {
            auto *reply = network.get(QNetworkRequest(QUrl(url)));
            QSignalSpy finished(reply, &QNetworkReply::finished);
            QVERIFY(finished.wait(3000));
            QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 404);
            reply->deleteLater();
        };
        const auto url = QUrl(lease.value());
        request("http://127.0.0.1:" + QString::number(url.port()) + "/invalid");
        proxy.release(lease.value());
        request(lease.value());
        proxy.close();
    }
    void adaptiveManifestHidesUrlsAndRevokesChildren() {
        VideoProxy proxy;
        QJsonObject video{{"url", "https://rr1.googlevideo.com/video?secret=signed"}, {"ext", "mp4"}, {"vcodec", "avc1.4D4020"}};
        QJsonObject audio{{"url", "https://rr1.googlevideo.com/audio?secret=signed"}, {"ext", "m4a"}, {"vcodec", "none"}};
        const auto url = proxy.video({{"duration", 180.5}, {"requested_formats", QJsonArray{video, audio}}}, "");
        QVERIFY(url);
        QNetworkAccessManager network;
        auto *reply = network.get(QNetworkRequest(QUrl(url.value())));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(3000));
        const auto manifest = reply->readAll();
        QVERIFY(manifest.contains("application/dash+xml") == false);
        QVERIFY(manifest.contains("contentType=\"video\""));
        QVERIFY(manifest.contains("contentType=\"audio\""));
        QVERIFY(!manifest.contains("googlevideo")); QVERIFY(!manifest.contains("signed"));
        QVERIFY(manifest.contains("PT180.500S"));
        reply->deleteLater();
        QRegularExpression urls("<BaseURL>([^<]+)</BaseURL>");
        auto matches = urls.globalMatch(QString::fromUtf8(manifest));
        QStringList children;
        while (matches.hasNext()) children.append(matches.next().captured(1));
        QCOMPARE(children.size(), 2);
        proxy.release(url.value());
        for (const auto &child : children) {
            auto *request = network.get(QNetworkRequest(QUrl(child)));
            QSignalSpy done(request, &QNetworkReply::finished);
            QVERIFY(done.wait(3000));
            QCOMPARE(request->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 404);
            request->deleteLater();
        }
    }
    void nativePluginLoadsAndUnloads() {
        QPluginLoader loader(QStringLiteral(YOUTUBE_PLUGIN_PATH));
        auto *plugin = qobject_cast<sdk::IExtensionPlugin *>(loader.instance());
        QVERIFY2(plugin, qPrintable(loader.errorString()));
        FakeContext context;
        bool active = false;
        plugin->activate(context, [&](Result<void> result) { active = bool(result); });
        QVERIFY(active); QVERIFY(context.musicProviders.contains("music"));
        plugin->deactivate();
        QVERIFY(context.serviceHandlers.isEmpty()); QVERIFY(context.musicProviders.isEmpty());
        QVERIFY(loader.unload());
    }
};
QTEST_GUILESS_MAIN(YouTubeTest)
#include "youtube_test.moc"
