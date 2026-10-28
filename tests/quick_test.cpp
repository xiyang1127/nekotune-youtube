#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>
class Setup final : public QObject {
    Q_OBJECT
  public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        engine->rootContext()->setContextProperty("screenshotPath", qEnvironmentVariable("YOUTUBE_SCREENSHOT"));
    }
};
QUICK_TEST_MAIN_WITH_SETUP(youtube, Setup)
#include "quick_test.moc"
