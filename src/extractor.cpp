#include "extractor.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimer>

namespace youtube {
Extractor::~Extractor() { close(); }
void Extractor::close() {
    const auto processes = m_processes;
    for (auto *process : processes) {
        // Kill only our own metadata workers; no shell or downloader subprocesses are used.
        process->setProperty("cancelled", true);
        process->kill();
        process->waitForFinished(1000);
    }
}
void Extractor::run(const sdk::Params &config, const QStringList &arguments, Reply done) {
    if (m_processes.size() >= 4) { done(failure("正在处理其他请求，请稍后重试", ErrorCode::Unavailable)); return; }
    auto binary = config.value("ytDlpPath", "yt-dlp").toString().trimmed();
    binary = QStandardPaths::findExecutable(binary);
    if (binary.isEmpty()) { done(failure("找不到 yt-dlp，请在 YouTube 设置中填写可执行文件路径", ErrorCode::Unavailable)); return; }
    QStringList args{"--ignore-config", "--no-plugin-dirs", "--no-cache-dir", "--no-warnings", "--no-progress", "--no-colors",
        "--socket-timeout", "20", "--retries", "1", "--extractor-retries", "1", "--skip-download", "--dump-single-json"};
    const auto proxy = config.value("proxy").toString().trimmed();
    if (!proxy.isEmpty()) {
        const QUrl url(proxy);
        if (!url.isValid() || url.host().isEmpty() || !QStringList{"http", "https", "socks5", "socks5h"}.contains(url.scheme())) {
            done(failure("代理地址必须使用 http、https 或 socks5 协议")); return;
        }
        args << "--proxy" << proxy;
    }
    const auto cookies = config.value("cookiesFile").toString().trimmed();
    if (!cookies.isEmpty()) {
        if (!QFileInfo(cookies).isFile()) { done(failure("Cookie 文件不存在")); return; }
        args << "--cookies" << cookies;
    }
    if (QStandardPaths::findExecutable("deno").isEmpty() && !QStandardPaths::findExecutable("node").isEmpty())
        args << "--js-runtimes" << "node";
    args << arguments;
    auto *process = new QProcess(this);
    auto output = std::make_shared<QByteArray>();
    auto error = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    auto complete = [this, process, done = std::move(done), finished](Result<QJsonObject> value) mutable {
        if (std::exchange(*finished, true)) return;
        m_processes.remove(process);
        process->deleteLater();
        done(std::move(value));
    };
    connect(process, &QProcess::readyReadStandardOutput, this, [process, output] {
        *output += process->readAllStandardOutput();
        if (output->size() > 8 * 1024 * 1024) { process->setProperty("oversize", true); process->kill(); }
    });
    connect(process, &QProcess::readyReadStandardError, this, [process, error] {
        *error = (*error + process->readAllStandardError()).right(32 * 1024);
    });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError code) mutable {
        if (code == QProcess::FailedToStart) complete(failure("无法启动 yt-dlp，请检查路径和执行权限", ErrorCode::Io));
    });
    connect(process, &QProcess::finished, this, [process, output, error, complete](int code, QProcess::ExitStatus status) mutable {
        *output += process->readAllStandardOutput();
        *error = (*error + process->readAllStandardError()).right(32 * 1024);
        if (process->property("cancelled").toBool()) { complete(failure("请求已取消", ErrorCode::Cancelled)); return; }
        if (process->property("timeout").toBool()) { complete(failure("YouTube 请求超时，请检查网络或代理", ErrorCode::Unavailable)); return; }
        if (process->property("oversize").toBool()) { complete(failure("YouTube 返回的数据过大")); return; }
        const auto document = QJsonDocument::fromJson(*output);
        if (status == QProcess::NormalExit && document.isObject() && (code == 0 || !document.object().value("entries").toArray().isEmpty())) {
            auto object = document.object();
            if (code != 0) object.insert("partial", true);
            complete(object);
        } else {
            // Do not forward stderr: it can include cookie paths, proxy credentials and signed URLs.
            QString message = "YouTube 解析失败，请检查网络、更新 yt-dlp 或配置 Cookie";
            if (error->contains("Sign in") || error->contains("bot")) message = "YouTube 要求登录验证，请在设置中指定自己的 Cookie 文件";
            else if (error->contains("Requested format")) message = "此视频暂无可用的媒体格式，请更新 yt-dlp 或打开原网页";
            else if (error->contains("Private video")) message = "此视频为私密视频";
            else if (error->contains("not available") || error->contains("unavailable")) message = "此视频不可用或存在地区限制";
            complete(failure(message, ErrorCode::Unavailable));
        }
    });
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, [process] { process->setProperty("timeout", true); process->kill(); });
    timer->start(90'000);
    m_processes.insert(process);
    process->start(binary, args);
}
}
