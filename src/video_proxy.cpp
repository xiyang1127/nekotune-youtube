#include "video_proxy.h"
#include <QDateTime>
#include <QJsonArray>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUuid>
#include <QXmlStreamWriter>
#include <algorithm>
#include <limits>

namespace youtube {
namespace {
constexpr qint64 bufferLimit = 512 * 1024;
void reject(QTcpSocket *socket, int code) {
    socket->write("HTTP/1.1 " + QByteArray::number(code) + " Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    socket->disconnectFromHost();
}
}
VideoProxy::VideoProxy(QObject *parent) : QObject(parent) {
    connect(&m_server, &QTcpServer::newConnection, this, &VideoProxy::accept);
    m_cleanup.setInterval(60'000);
    connect(&m_cleanup, &QTimer::timeout, this, [this] {
        for (const auto &key : m_streams.keys()) {
            const auto stream = m_streams.value(key);
            if (!stream) continue;
            stream->sockets.removeIf([](const auto &socket) { return socket.isNull(); });
            if (stream->owner.isEmpty() && !active(stream) && QDateTime::currentMSecsSinceEpoch() - stream->touched > 30 * 60 * 1000)
                release("http://127.0.0.1/" + key);
        }
    });
}
VideoProxy::~VideoProxy() { close(); }
bool VideoProxy::active(const std::shared_ptr<Stream> &stream) const {
    if (std::any_of(stream->sockets.cbegin(), stream->sockets.cend(), [](const auto &socket) { return !socket.isNull(); })) return true;
    for (const auto &child : stream->children) {
        const auto nested = m_streams.value(child);
        if (nested && active(nested)) return true;
    }
    return false;
}
void VideoProxy::close() {
    m_cleanup.stop();
    for (const auto &key : m_streams.keys()) release("http://127.0.0.1/" + key);
    m_server.close();
    const auto sockets = findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *socket : sockets) { socket->abort(); delete socket; }
}
Result<QString> VideoProxy::lease(const sdk::ResolvedAudio &audio, const QString &proxy) {
    if (!mediaUrlAllowed(audio.url)) return failure("无效的视频流地址");
    if (m_streams.size() >= 64) {
        QString oldest;
        qint64 touched = std::numeric_limits<qint64>::max();
        for (auto it = m_streams.begin(); it != m_streams.end(); ++it) {
            if (it.value()->owner.isEmpty() && !active(it.value()) && it.value()->touched < touched) { oldest = it.key(); touched = it.value()->touched; }
        }
        if (oldest.isEmpty()) return failure("打开的媒体流过多，请先关闭播放器");
        release("http://127.0.0.1/" + oldest);
    }
    if (!m_server.isListening() && !m_server.listen(QHostAddress::LocalHost, 0))
        return failure("无法启动本地视频转发", ErrorCode::Io);
    QNetworkProxy networkProxy(QNetworkProxy::DefaultProxy);
    if (!proxy.isEmpty()) {
        const QUrl url(proxy);
        if (!QStringList{"http", "socks5", "socks5h"}.contains(url.scheme()))
            return failure("内嵌视频代理支持 http 或 socks5，请修改代理设置");
        networkProxy = QNetworkProxy(url.scheme() == "http" ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy,
            url.host(), url.port(url.scheme() == "http" ? 8080 : 1080), url.userName(), url.password());
    }
    m_network.setProxy(networkProxy);
    const auto token = QUuid::createUuid().toString(QUuid::Id128);
    auto stream = std::make_shared<Stream>();
    stream->audio = audio;
    stream->touched = QDateTime::currentMSecsSinceEpoch();
    m_streams.insert(token, stream);
    m_cleanup.start();
    return "http://127.0.0.1:" + QString::number(m_server.serverPort()) + "/" + token;
}
Result<QString> VideoProxy::video(const QJsonObject &metadata, const QString &proxy) {
    const auto formats = metadata.value("requested_formats").toArray();
    if (formats.isEmpty()) {
        const auto media = audioFromJson(metadata);
        if (!media) return media.error();
        return lease(media.value(), proxy);
    }
    if (formats.size() != 2 || metadata.value("duration").toDouble() <= 0)
        return failure("此视频暂不支持内嵌播放");
    auto manifest = std::make_shared<Stream>();
    const auto token = QUuid::createUuid().toString(QUuid::Id128) + ".mpd";
    manifest->touched = QDateTime::currentMSecsSinceEpoch();
    manifest->audio.expiresAt = std::numeric_limits<qint64>::max();
    QXmlStreamWriter xml(&manifest->manifest);
    xml.writeStartDocument();
    xml.writeStartElement("MPD");
    xml.writeDefaultNamespace("urn:mpeg:dash:schema:mpd:2011");
    xml.writeAttribute("type", "static");
    xml.writeAttribute("profiles", "urn:mpeg:dash:profile:isoff-on-demand:2011");
    xml.writeAttribute("minBufferTime", "PT2S");
    xml.writeAttribute("mediaPresentationDuration", "PT" + QString::number(metadata.value("duration").toDouble(), 'f', 3) + "S");
    xml.writeStartElement("Period");
    int index = 0;
    for (const auto &value : formats) {
        const auto format = value.toObject();
        const auto audio = audioFromJson(format);
        if (!audio) { for (const auto &child : manifest->children) release("http://127.0.0.1/" + child); return audio.error(); }
        const auto url = lease(audio.value(), proxy);
        if (!url) { for (const auto &child : manifest->children) release("http://127.0.0.1/" + child); return url.error(); }
        const auto child = QUrl(url.value()).path().mid(1);
        manifest->children.append(child);
        m_streams.value(child)->owner = token;
        manifest->audio.expiresAt = qMin(manifest->audio.expiresAt, audio.value().expiresAt);
        const auto kind = format.value("vcodec").toString() == "none" ? QString("audio") : QString("video");
        xml.writeStartElement("AdaptationSet");
        xml.writeAttribute("mimeType", kind + "/mp4");
        xml.writeAttribute("contentType", kind);
        xml.writeStartElement("Representation");
        xml.writeAttribute("id", QString::number(++index));
        xml.writeAttribute("bandwidth", QString::number(qMax(1, int(format.value("tbr").toDouble(1000) * 1000))));
        xml.writeTextElement("BaseURL", url.value());
        xml.writeEndElement(); xml.writeEndElement();
    }
    xml.writeEndElement(); xml.writeEndElement(); xml.writeEndDocument();
    m_streams.insert(token, manifest);
    return "http://127.0.0.1:" + QString::number(m_server.serverPort()) + "/" + token;
}
void VideoProxy::release(const QString &url) {
    const auto stream = m_streams.take(QUrl(url).path().mid(1));
    if (!stream) return;
    for (const auto &child : stream->children) release("http://127.0.0.1/" + child);
    for (const auto &socket : stream->sockets) if (socket) socket->abort();
}
void VideoProxy::accept() {
    while (auto *socket = m_server.nextPendingConnection()) {
        socket->setParent(this);
        if (findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly).size() > 12) {
            socket->abort(); socket->deleteLater(); continue;
        }
        socket->setReadBufferSize(16 * 1024 + 1);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        auto *timeout = new QTimer(socket);
        timeout->setSingleShot(true);
        timeout->start(10'000);
        connect(timeout, &QTimer::timeout, socket, &QTcpSocket::abort);
        auto buffer = std::make_shared<QByteArray>();
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer, connection, timeout] {
            *buffer += socket->readAll();
            if (buffer->size() > 16 * 1024) { reject(socket, 431); return; }
            if (!buffer->contains("\r\n\r\n")) return;
            disconnect(*connection);
            timeout->stop();
            const auto lines = buffer->split('\n');
            const auto request = lines.value(0).trimmed().split(' ');
            if (request.size() != 3 || (request[0] != "GET" && request[0] != "HEAD")) { reject(socket, 405); return; }
            const auto stream = m_streams.value(QString::fromLatin1(request[1]).mid(1));
            if (!stream) { reject(socket, 404); return; }
            if (stream->audio.expiresAt < QDateTime::currentMSecsSinceEpoch()) { reject(socket, 410); return; }
            QByteArray range;
            for (const auto &line : lines) if (line.toLower().startsWith("range:")) range = line.mid(6).trimmed();
            if (!range.isEmpty() && !QRegularExpression("^bytes=([0-9]+-[0-9]*|-[0-9]+)$").match(QString::fromLatin1(range)).hasMatch()) {
                reject(socket, 416); return;
            }
            stream->touched = QDateTime::currentMSecsSinceEpoch();
            if (!stream->owner.isEmpty() && m_streams.contains(stream->owner)) m_streams.value(stream->owner)->touched = stream->touched;
            stream->sockets.append(socket);
            if (!stream->manifest.isEmpty()) {
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/dash+xml\r\nContent-Length: " + QByteArray::number(stream->manifest.size()) + "\r\nConnection: close\r\n\r\n");
                if (request[0] != "HEAD") socket->write(stream->manifest);
                socket->disconnectFromHost();
                return;
            }
            upstream(socket, stream->audio, request[0], range);
        });
    }
}
void VideoProxy::upstream(QTcpSocket *socket, sdk::ResolvedAudio audio, const QByteArray &method,
                          const QByteArray &range, int redirects) {
    if (redirects > 4 || !mediaUrlAllowed(audio.url)) { reject(socket, 502); return; }
    QNetworkRequest request(audio.url);
    request.setTransferTimeout(30'000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    for (auto it = audio.headers.begin(); it != audio.headers.end(); ++it) request.setRawHeader(it.key().toLatin1(), it.value().toUtf8());
    request.setRawHeader("Accept-Encoding", "identity");
    if (!range.isEmpty()) request.setRawHeader("Range", range);
    auto *reply = m_network.sendCustomRequest(request, method);
    reply->setParent(socket);
    reply->setReadBufferSize(bufferLimit);
    connect(socket, &QTcpSocket::disconnected, reply, &QNetworkReply::abort);
    auto sent = std::make_shared<bool>(false);
    auto redirected = std::make_shared<bool>(false);
    auto pump = [socket, reply, sent, redirected, method] {
        if (!*sent || *redirected || socket->state() != QAbstractSocket::ConnectedState) return;
        if (method != "HEAD") {
            const auto room = bufferLimit - socket->bytesToWrite();
            if (room > 0) socket->write(reply->read(room));
        }
        if (reply->isFinished() && (method == "HEAD" || reply->bytesAvailable() == 0)) socket->disconnectFromHost();
    };
    auto headers = [this, socket, reply, audio, method, range, redirects, sent, redirected, pump]() mutable {
        if (*sent || *redirected) return;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (!status.isValid()) { if (reply->isFinished()) { *sent = true; reject(socket, 502); } return; }
        const int code = status.toInt();
        if (QList<int>{301, 302, 303, 307, 308}.contains(code)) {
            *redirected = true;
            auto next = audio;
            next.url = audio.url.resolved(reply->header(QNetworkRequest::LocationHeader).toUrl());
            if (next.url.host() != audio.url.host()) {
                for (const auto &key : next.headers.keys())
                    if (key.compare("cookie", Qt::CaseInsensitive) == 0 || key.compare("authorization", Qt::CaseInsensitive) == 0)
                        next.headers.remove(key);
            }
            reply->abort();
            reply->deleteLater();
            upstream(socket, next, method, range, redirects + 1);
            return;
        }
        *sent = true;
        if (code != 200 && code != 206 && code != 416) { reject(socket, code == 403 ? 403 : 502); reply->abort(); return; }
        QByteArray header = "HTTP/1.1 " + QByteArray::number(code) + " " + (code == 206 ? "Partial Content" : "OK") + "\r\nConnection: close\r\n";
        for (const auto &name : {QByteArray("Content-Type"), QByteArray("Content-Length"), QByteArray("Content-Range"), QByteArray("Accept-Ranges")}) {
            auto value = reply->rawHeader(name);
            if (!value.isEmpty() && !value.contains('\r') && !value.contains('\n')) header += name + ": " + value + "\r\n";
        }
        socket->write(header + "\r\n");
        pump();
    };
    connect(reply, &QNetworkReply::metaDataChanged, socket, headers);
    connect(reply, &QNetworkReply::finished, socket, headers);
    connect(reply, &QIODevice::readyRead, socket, pump);
    connect(reply, &QNetworkReply::finished, socket, pump);
    connect(socket, &QTcpSocket::bytesWritten, reply, pump);
}
}
