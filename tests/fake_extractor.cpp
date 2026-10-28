#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    const auto target = args.last();
    if (target.contains("slow")) { QTimer::singleShot(30'000, &app, &QCoreApplication::quit); return app.exec(); }
    if (target.contains("error")) { QTextStream(stderr) << "secret-token https://private.example/?password=hidden\n"; return 1; }
    if (target.contains("malformed")) { QTextStream(stdout) << "not-json"; return 0; }
    if (target.contains("oversized")) { QTextStream(stdout) << QString(9 * 1024 * 1024, 'x'); return 0; }
    const QJsonObject video{{"id", "abcdefghijk"}, {"title", "Fixture song"}, {"channel", "Fixture artist"},
        {"channel_url", "https://www.youtube.com/@fixture"}, {"duration", 180.5},
        {"url", "https://rr1.googlevideo.com/videoplayback?expire=9999999999"}, {"ext", "m4a"},
        {"http_headers", QJsonObject{{"User-Agent", "fixture"}}}};
    QJsonObject result = video;
    if (args.contains("--flat-playlist")) {
        QJsonArray entries;
        for (int i = 0; i < 20; ++i) entries.append(video);
        result = {{"title", "Fixture artist"}, {"entries", entries}};
    }
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact);
    return 0;
}
