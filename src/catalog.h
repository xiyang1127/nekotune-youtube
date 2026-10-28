#pragma once
#include <nekotune/sdk/providers.h>
#include <QJsonObject>

namespace youtube {
using namespace nekotune;
Result<QString> videoId(const QString &input);
Result<QString> channelUrl(const QString &input);
bool mediaUrlAllowed(const QUrl &url);
sdk::Params item(const QJsonObject &object);
sdk::Track trackFromItem(const sdk::Params &item);
Result<sdk::ResolvedAudio> audioFromJson(const QJsonObject &object);
struct BrowseRequest {
    QString target;
    int start = 1;
    int end = 20;
};
Result<BrowseRequest> browseRequest(const sdk::Params &params);
sdk::Params browseResult(const QJsonObject &object, const BrowseRequest &request);
}
