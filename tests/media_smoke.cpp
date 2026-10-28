#include "video_proxy.h"
#include <QAudioOutput>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMediaPlayer>
#include <QTextStream>
#include <QTimer>
#include <QVideoSink>
#include <QVideoFrame>

// Explicit online/manual check: metadata is a private yt-dlp JSON file, never a package asset.
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (app.arguments().size() != 2) return 2;
    QFile metadata(app.arguments()[1]);
    if (!metadata.open(QIODevice::ReadOnly)) return 2;
    youtube::VideoProxy proxy;
    const auto lease = proxy.video(QJsonDocument::fromJson(metadata.readAll()).object(), "");
    if (!lease) { QTextStream(stdout) << "Lease failed: " << lease.error().message << '\n'; return 1; }
    QMediaPlayer player;
    QVideoSink video;
    QAudioOutput audio;
    audio.setMuted(true);
    player.setAudioOutput(&audio);
    player.setVideoSink(&video);
    int frames = 0;
    bool sought = false;
    qint64 seekTarget = 0;
    QObject::connect(&video, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame &frame) {
        if (frame.isValid()) ++frames;
        if (frames > 20 && player.position() > 1000 && player.hasAudio()) {
            if (!sought && player.isSeekable() && player.duration() > 10'000) {
                sought = true; seekTarget = qMin<qint64>(10'000, player.duration() / 2); player.setPosition(seekTarget);
            } else if (sought && frame.startTime() >= seekTarget * 1000) app.exit(0);
        }
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &app, [&](QMediaPlayer::Error code, const QString &) {
        QTextStream(stdout) << "Media error code: " << int(code) << '\n'; app.exit(1);
    });
    QTimer::singleShot(45'000, &app, [&] { app.exit(3); });
    player.setSource(QUrl(lease.value()));
    player.play();
    const int result = app.exec();
    QTextStream(stdout) << "result=" << result << " frames=" << frames << " position=" << player.position()
        << " audio=" << player.hasAudio() << " video=" << player.hasVideo() << " seek=" << sought << '\n';
    player.stop();
    proxy.close();
    return result;
}
