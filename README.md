# YouTube 漫游

NekoTune SDK 3 原生扩展。插件 ID 为 `nekotune.youtube`，音源为 `nekotune.youtube/music`。

- 搜索歌曲、歌手、MV，也可以单独搜索作者频道或粘贴视频链接。
- 点击视频中的作者进入主页，浏览视频、Shorts、直播回放，分页加载。
- 收藏作者，之后从「作者收藏」直接回访。收藏保存在本机，不同步 YouTube 订阅。
- 在插件内播放视频，支持暂停、拖动进度、音量、上下集与列表内自动连播。
- 视频优先使用最高 720p 的 H.264 + AAC 分离流，通过本地 DASH 清单同步播放；也兼容音视频合一流。
- 「听歌」接入宿主播放队列；「下载」优先获取原始 M4A，无需转码，下载完成后自动入库。
- 同时最多三首下载，显示进度、完成、错误和取消状态。下载由宿主校验、去重和管理。

## 安装与使用

1. 在 NekoTune「扩展」中安装构建生成的 ZIP，或挂载构建目录 `extensions/nekotune.youtube/`。
2. 确认信任并启用「YouTube 漫游」，打开侧栏的同名页面。
3. 在页面右上角「设置」检测 yt-dlp；不在 PATH 时填写其可执行文件的完整路径。
4. 搜歌时选择「歌曲 / 视频」；找作者时选择「作者 / 频道」。也可直接在搜索框输入 `@用户名` 或频道链接，宽屏下有「直达频道」入口。
5. 点击封面或「看视频」观看；点击作者名访问频道；在频道中点击「收藏作者」。

运行需要匹配宿主的 Qt Multimedia QML 模块及 FFmpeg 后端，以及近期版本的 [yt-dlp](https://github.com/yt-dlp/yt-dlp#installation)。推荐同时安装 Deno 和 yt-dlp 的默认依赖，以满足 [YouTube JavaScript 解析要求](https://github.com/yt-dlp/yt-dlp/wiki/EJS)。本机已有这些依赖时无需重复安装。使用 uv 的环境可执行：

```sh
uv tool install 'yt-dlp[default]'
# 更新已有工具：
uv tool upgrade yt-dlp
```

如果应用从桌面启动后找不到工具，请填完整路径。设置支持 HTTP/SOCKS5 代理，代理同时用于解析、内嵌视频和音频传输；空值跟随相应运行时的默认网络配置。需要登录验证时，可显式指定自己的 Netscape 格式 Cookie 文件；插件不会自动读取浏览器账号。

「发现」是音乐搜索入口；插件不接入账号推荐流。地区限制、登录验证、删除／私密内容等错误会显示在页面中。正在直播且无法取得完整静态媒体信息的内容可能无法内嵌观看，可以打开原网页。视频播放会暂停宿主音乐，宿主开始播放音乐时也会暂停视频。

## 独立构建

使用目标宿主 Release 对应平台的 SDK、Qt 小版本和编译器主版本：

```sh
cmake -S . -B /path/to/workspace/build/youtube -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/nekotune-sdk
cmake --build /path/to/workspace/build/youtube --parallel 2
ctest --test-dir /path/to/workspace/build/youtube --output-on-failure
```

本地 SDK 源码可通过 `-DNEKOTUNE_SDK_SOURCE_DIR=/path/to/NekoTune/sdk` 指定，源码构建由开发者确保工具链兼容。构建输出：

- 开发目录：`build/youtube/extensions/nekotune.youtube/`
- 安装包：`build/youtube/packages/nekotune.youtube-<platform>.nekotune.zip`

工作区入口支持 `-DNEKOTUNE_WORKSPACE_EXTENSIONS=youtube`。源码仓库：[xiyang1127/nekotune-youtube](https://github.com/xiyang1127/nekotune-youtube)。仓库中的 CI 分别构建和测试 Linux、Windows，并上传安装包；运行结果与安装包见 [GitHub Actions](https://github.com/xiyang1127/nekotune-youtube/actions)。不会自动发布版本。

## 验证

CTest 使用假的解析进程，覆盖链接与媒体地址校验、分页、错误脱敏、取消与停用、作者收藏、音源注册、原生插件加载、DASH 清单和媒体租约回收。QML 测试覆盖旧请求返回后的状态保护、频道切换、分页去重、视频释放、下载事件和页面加载。

可选的真实媒体验收：将 yt-dlp `--dump-single-json` 的音画分离解析结果保存到工作区外的临时文件，再运行：

```sh
QT_QPA_PLATFORM=offscreen build/youtube/youtube_media_smoke /path/to/private-metadata.json
```

该程序检查 Qt Multimedia 是否收到视频帧并识别音轨，默认不在 CTest / CI 中联网运行。解析结果包含短期签名地址，不应提交或打包。

后端通过 `QProcess` 传递参数，不启动 shell；限制并发、输出大小和超时。视频在后端的随机回环地址转发，请求头与上游签名地址不传给 QML。转发验证媒体主机、支持 Range 和流式背压；关闭视频或停用扩展会撤销对应租约。扩展不依赖宿主源码符号。
