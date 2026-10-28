#pragma once
#include "catalog.h"
#include <QObject>
#include <QProcess>
#include <QSet>

namespace youtube {
class Extractor final : public QObject {
    Q_OBJECT
  public:
    explicit Extractor(QObject *parent = nullptr) : QObject(parent) {}
    ~Extractor() override;
    using Reply = std::function<void(Result<QJsonObject>)>;
    void run(const sdk::Params &config, const QStringList &arguments, Reply done);
    void close();
  private:
    QSet<QProcess *> m_processes;
};
}
