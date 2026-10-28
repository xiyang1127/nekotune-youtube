#pragma once
#include "catalog.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>

class QTcpSocket;
namespace youtube {
// Qt Multimedia has no QML API for HTTP headers. Only random loopback leases are exposed.
class VideoProxy final : public QObject {
    Q_OBJECT
  public:
    explicit VideoProxy(QObject *parent = nullptr);
    ~VideoProxy() override;
    Result<QString> lease(const sdk::ResolvedAudio &audio, const QString &proxy);
    Result<QString> video(const QJsonObject &metadata, const QString &proxy);
    void release(const QString &url);
    void close();
  private:
    struct Stream {
        sdk::ResolvedAudio audio;
        qint64 touched = 0;
        QList<QPointer<QTcpSocket>> sockets;
        QByteArray manifest;
        QStringList children;
        QString owner;
    };
    void accept();
    bool active(const std::shared_ptr<Stream> &stream) const;
    void upstream(QTcpSocket *socket, sdk::ResolvedAudio audio, const QByteArray &method,
                  const QByteArray &range, int redirects = 0);
    QTcpServer m_server;
    QNetworkAccessManager m_network;
    QHash<QString, std::shared_ptr<Stream>> m_streams;
    QTimer m_cleanup;
};
}
