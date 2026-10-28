#pragma once
#include "extractor.h"
#include "video_proxy.h"
#include <nekotune/sdk/extension_context.h>

namespace youtube {
class Extension final : public QObject, public sdk::IMusicProvider {
    Q_OBJECT
  public:
    explicit Extension(sdk::ExtensionContext &context);
    ~Extension() override;
    void activate(sdk::Done done);
    void close();
    void search(const QString &query, const QString &cursor, std::function<void(Result<sdk::SearchPage>)> done) override;
    void track(const QString &id, std::function<void(Result<sdk::Track>)> done) override;
    void resolve(const QString &id, sdk::ResolvePurpose purpose, std::function<void(Result<sdk::ResolvedAudio>)> done) override;
  private:
    void browse(const sdk::Params &params, sdk::Reply done);
    void extract(const QString &id, bool video, Extractor::Reply done);
    sdk::ExtensionContext &m_context;
    Extractor m_extractor;
    VideoProxy m_proxy;
    std::vector<sdk::RegistrationPtr> m_registrations;
};
}
