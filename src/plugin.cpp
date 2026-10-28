#include "extension.h"
#include <nekotune/sdk/extension_plugin.h>

class YouTubePlugin final : public QObject, public nekotune::sdk::IExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID NEKOTUNE_EXTENSION_IID)
    Q_INTERFACES(nekotune::sdk::IExtensionPlugin)
  public:
    void activate(nekotune::sdk::ExtensionContext &context, nekotune::sdk::Done done) override {
        m_extension = std::make_unique<youtube::Extension>(context);
        m_extension->activate(std::move(done));
    }
    void deactivate() override { m_extension.reset(); }
  private:
    std::unique_ptr<youtube::Extension> m_extension;
};
#include "plugin.moc"
