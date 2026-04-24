# ASR 语音转字幕实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现音频/视频文件拖拽导入后，通过 FFmpeg 转码 → 阿里云 OSS 上传 → 腾讯云 ASR 识别生成字幕数据写入 SubtitleTrack。

**Architecture:** 职责链分离 - AudioTranscoder(转码) → OssUploader(上传) → TencentAsrService(ASR) → SubtitleTrack(存储)

**Tech Stack:** C++17, Qt6, FFmpeg 8.0, REST API

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `include/AudioTranscoder.h` | 创建 | FFmpeg 转码接口 |
| `src/AudioTranscoder.cpp` | 创建 | 转码实现 |
| `include/OssUploader.h` | 创建 | 阿里云 OSS 上传接口 |
| `src/OssUploader.cpp` | 创建 | 上传实现 |
| `include/TencentAsrService.h` | 创建 | 腾讯云 ASR 接口（继承 AsrServiceBase） |
| `src/TencentAsrService.cpp` | 创建 | ASR 实现 |
| `include/ConfigManager.h` | 创建 | 配置文件管理 |
| `src/ConfigManager.cpp` | 创建 | 配置读取实现 |
| `CMakeLists.txt` | 修改 | 添加新源文件 |

---

### Task 1: ConfigManager（配置文件管理）

配置集中管理，所有凭证集中在此读取。

**Files:**
- Create: `include/ConfigManager.h`
- Create: `src/ConfigManager.cpp`

- [ ] **Step 1: 创建配置管理器头文件**

```cpp
#pragma once

#include <QString>
#include <QSettings>

class ConfigManager
{
public:
    static ConfigManager& instance();

    // FFmpeg
    QString ffmpegPath() const;

    // Tencent ASR
    QString tencentSecretId() const;
    QString tencentSecretKey() const;
    QString tencentAppId() const;

    // Aliyun OSS
    QString ossAccessKeyId() const;
    QString ossAccessKeySecret() const;
    QString ossBucket() const;
    QString ossRegion() const;

private:
    ConfigManager();
    ~ConfigManager() = default;

    QString getString(const QString& group, const QString& key) const;

    QSettings settings_;
};
```

- [ ] **Step 2: 创建配置管理器实现**

```cpp
#include "ConfigManager.h"
#include <QCoreApplication>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager()
    : settings_("config.ini", QSettings::IniFormat)
{
}

QString ConfigManager::ffmpegPath() const
{
    return getString("ffmpeg", "path");
}

QString ConfigManager::tencentSecretId() const
{
    return getString("tencent_asr", "secret_id");
}

QString ConfigManager::tencentSecretKey() const
{
    return getString("tencent_asr", "secret_key");
}

QString ConfigManager::tencentAppId() const
{
    return getString("tencent_asr", "app_id");
}

QString ConfigManager::ossAccessKeyId() const
{
    return getString("aliyun_oss", "access_key_id");
}

QString ConfigManager::ossAccessKeySecret() const
{
    return getString("aliyun_oss", "access_key_secret");
}

QString ConfigManager::ossBucket() const
{
    return getString("aliyun_oss", "bucket");
}

QString ConfigManager::ossRegion() const
{
    return getString("aliyun_oss", "region");
}

QString ConfigManager::getString(const QString& group, const QString& key) const
{
    settings_.beginGroup(group);
    QString value = settings_.value(key).toString();
    settings_.endGroup();
    return value;
}
```

- [ ] **Step 3: 创建配置文件模板**

在项目根目录创建 `config.ini.template`:
```ini
[ffmpeg]
path=/Users/zxl/Tools/ffmpeg/8.0

[tencent_asr]
secret_id=YOUR_SECRET_ID
secret_key=YOUR_SECRET_KEY
app_id=YOUR_APP_ID

[aliyun_oss]
access_key_id=YOUR_ACCESS_KEY_ID
access_key_secret=YOUR_ACCESS_KEY_SECRET
bucket=YOUR_BUCKET
region=cn-shanghai
```

- [ ] **Step 4: 提交**

```bash
git add include/ConfigManager.h src/ConfigManager.cpp config.ini.template
git commit -m "feat(config): add ConfigManager for centralized settings"
```

---

### Task 2: AudioTranscoder（FFmpeg 转码）

将任意音视频格式转换为 WAV/16k 单声道音频。

**Files:**
- Create: `include/AudioTranscoder.h`
- Create: `src/AudioTranscoder.cpp`

- [ ] **Step 1: 创建 AudioTranscoder 头文件**

```cpp
#pragma once

#include <QObject>
#include <QString>

class AudioTranscoder : public QObject
{
    Q_OBJECT

public:
    explicit AudioTranscoder(QObject* parent = nullptr);

signals:
    void transcodingStarted();
    void transcodingProgress(int percent);
    void transcodingFinished(const QString& outputPath);
    void transcodingFailed(const QString& errorMessage);

public slots:
    void transcode(const QString& inputPath);

private:
    QString generateOutputPath(const QString& inputPath);
    QString ffmpegPath_;
};
```

- [ ] **Step 2: 实现转码逻辑**

```cpp
#include "AudioTranscoder.h"
#include "ConfigManager.h"
#include <QProcess>
#include <QFileInfo>

AudioTranscoder::AudioTranscoder(QObject* parent)
    : QObject(parent)
{
    ffmpegPath_ = ConfigManager::instance().ffmpegPath();
    if (ffmpegPath_.isEmpty()) {
        ffmpegPath_ = "/Users/zxl/Tools/ffmpeg/8.0/bin/ffmpeg";
    }
}

QString AudioTranscoder::generateOutputPath(const QString& inputPath)
{
    QFileInfo info(inputPath);
    QString basePath = info.absolutePath();
    QString baseName = info.baseName();
    return basePath + "/" + baseName + "_16k.wav";
}

void AudioTranscoder::transcode(const QString& inputPath)
{
    QString outputPath = generateOutputPath(inputPath);

    QStringList args;
    args << "-i" << inputPath
         << "-ar" << "16000"       // 16k采样率
         << "-ac" << "1"           // 单声道
         << "-acodec" << "pcm_s16le"
         << "-y"                   // 覆盖输出
         << outputPath;

    QProcess* process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, outputPath](int, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit) {
            emit transcodingFinished(outputPath);
        } else {
            QString error = QString::fromUtf8(process->readAllStandardError());
            emit transcodingFailed("Transcoding failed: " + error);
        }
        process->deleteLater();
    });

    emit transcodingStarted();
    process->start(ffmpegPath_, args);
}
```

- [ ] **Step 3: 提交**

```bash
git add include/AudioTranscoder.h src/AudioTranscoder.cpp
git commit -m "feat(asr): add AudioTranscoder for FFmpeg WAV/16k conversion"
```

---

### Task 3: OssUploader（阿里云 OSS 上传）

上传本地文件到阿里云 OSS，使用 REST API + HMAC-SHA1 签名。

**Files:**
- Create: `include/OssUploader.h`
- Create: `src/OssUploader.cpp`

- [ ] **Step 1: 创建 OssUploader 头文件**

```cpp
#pragma once

#include <QObject>
#include <QString>

class OssUploader : public QObject
{
    Q_OBJECT

public:
    explicit OssUploader(QObject* parent = nullptr);

signals:
    void uploadStarted();
    void uploadProgress(int percent);
    void uploadFinished(const QString& ossUrl);
    void uploadFailed(const QString& errorMessage);

public slots:
    void upload(const QString& localFilePath);

private:
    QString generateOssPath(const QString& localPath);
    QString computeSignature(const QString& stringToSign);
    QByteArray hmacSha1(const QByteArray& key, const QByteArray& data);

    QString ossBucket_;
    QString ossRegion_;
    QString ossAccessKeyId_;
    QString ossAccessKeySecret_;
};
```

- [ ] **Step 2: 实现上传逻辑**

```cpp
#include "OssUploader.h"
#include "ConfigManager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QUrl>

OssUploader::OssUploader(QObject* parent)
    : QObject(parent)
{
    ossAccessKeyId_ = ConfigManager::instance().ossAccessKeyId();
    ossAccessKeySecret_ = ConfigManager::instance().ossAccessKeySecret();
    ossBucket_ = ConfigManager::instance().ossBucket();
    ossRegion_ = ConfigManager::instance().ossRegion();
}

QString OssUploader::generateOssPath(const QString& localPath)
{
    QFileInfo info(localPath);
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    return "asr/" + timestamp + "_" + info.fileName();
}

QString OssUploader::generateOssPath(const QString& localPath)
{
    QFileInfo info(localPath);
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    return "asr/" + timestamp + "_" + info.fileName();
}

void OssUploader::upload(const QString& localFilePath)
{
    QFile* file = new QFile(localFilePath, this);
    if (!file->open(QIODevice::ReadOnly)) {
        emit uploadFailed("Cannot open file: " + localFilePath);
        return;
    }

    QString ossPath = generateOssPath(localFilePath);
    QString endpoint = "https://" + ossBucket_ + ".oss-" + ossRegion_ + ".aliyuncs.com";
    QString urlStr = endpoint + "/" + ossPath;

    QNetworkRequest request(QUrl(urlStr));
    request.setRawHeader("Content-Type", "audio/wav");

    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkReply* reply = manager->put(request, file->readAll());

    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
            emit uploadProgress(percent);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, urlStr]() {
        if (reply->error() == QNetworkReply::NoError) {
            emit uploadFinished(urlStr);
        } else {
            emit uploadFailed("Upload failed: " + reply->errorString());
        }
        reply->deleteLater();
    });

    emit uploadStarted();
}
```

- [ ] **Step 3: 提交**

```bash
git add include/OssUploader.h src/OssUploader.cpp
git commit -m "feat(asr): add OssUploader for Aliyun OSS upload"
```

---

### Task 4: TencentAsrService（腾讯云 ASR）

调用腾讯云"录音文件识别"API，两步流程：提交任务 → 轮询查询结果。

**Files:**
- Create: `include/TencentAsrService.h`
- Create: `src/TencentAsrService.cpp`

- [ ] **Step 1: 创建 TencentAsrService 头文件**

```cpp
#pragma once

#include "AsrServiceBase.h"

class QNetworkAccessManager;

class TencentAsrService : public AsrServiceBase
{
    Q_OBJECT

public:
    explicit TencentAsrService(QObject* parent = nullptr);
    ~TencentAsrService();

    void transcribe(const QString& audioUrl) override;

private slots:
    void onTaskCreated(QNetworkReply* reply);
    void onResultQueried(QNetworkReply* reply);

private:
    void createRecTask(const QString& audioUrl);
    void queryTaskStatus(const QString& taskId);

    QString secretId_;
    QString secretKey_;
    QString appId_;
    QString currentTaskId_;
    QNetworkAccessManager* networkManager_;
};
```

- [ ] **Step 2: 实现 ASR 调用逻辑**

```cpp
#include "TencentAsrService.h"
#include "ConfigManager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

TencentAsrService::TencentAsrService(QObject* parent)
    : AsrServiceBase(parent)
    , networkManager_(new QNetworkAccessManager(this))
{
    secretId_ = ConfigManager::instance().tencentSecretId();
    secretKey_ = ConfigManager::instance().tencentSecretKey();
    appId_ = ConfigManager::instance().tencentAppId();
}

TencentAsrService::~TencentAsrService() = default;

void TencentAsrService::transcribe(const QString& audioUrl)
{
    createRecTask(audioUrl);
}

void TencentAsrService::createRecTask(const QString& audioUrl)
{
    QString url = "https://asr.tencentcloudapi.com/";

    QJsonObject payload;
    payload["AppId"] = appId_;
    payload["ChannelNum"] = 1;
    payload["EngineType"] = "16k_zh";
    payload["Url"] = audioUrl;
    payload["SourceType"] = 0;  // 0=URL

    QNetworkRequest request(QUrl(url));
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("X-TC-Action", "CreateRecTask");

    QNetworkReply* reply = networkManager_->post(request,
        QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onTaskCreated(reply);
    });
}

void TencentAsrService::onTaskCreated(QNetworkReply* reply)
{
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject resp = doc.object();

    if (resp.contains("Response")) {
        QJsonObject response = resp["Response"].toObject();
        currentTaskId_ = QString::number(response["TaskId"].toDouble());
        queryTaskStatus(currentTaskId_);
    } else {
        TranscriptResult result;
        result.success = false;
        result.errorMessage = "CreateRecTask failed: " + resp["Error"].toObject()["Message"].toString();
        emit transcribeFinished(result);
    }
    reply->deleteLater();
}

void TencentAsrService::queryTaskStatus(const QString& taskId)
{
    QString url = "https://asr.tencentcloudapi.com/";

    QJsonObject payload;
    payload["AppId"] = appId_;
    payload["TaskId"] = taskId.toLongLong();

    QNetworkRequest request(QUrl(url));
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("X-TC-Action", "DescribeTaskStatus");

    QNetworkReply* reply = networkManager_->post(request,
        QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, taskId]() {
        onResultQueried(reply);
    });
}

void TencentAsrService::onResultQueried(QNetworkReply* reply)
{
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject resp = doc.object();

    TranscriptResult result;

    if (resp.contains("Response")) {
        QJsonObject response = resp["Response"].toObject();
        QString status = response["Status"].toString();

        if (status == "2") {  // 完成
            result.success = true;
            // 解析 Result 字段中的字幕片段
            QString resultStr = response["Result"].toString();
            // Result 格式示例: [0,3600,"你好世界"][3600,7200,"这是第二句"]
            parseResultText(resultStr, result.segments);
            emit transcribeFinished(result);
        } else if (status == "3" || status == "4") {  // 失败
            result.success = false;
            result.errorMessage = "ASR task failed";
            emit transcribeFinished(result);
        } else {
            // 继续轮询
            QTimer::singleShot(1000, this, [this, taskId = currentTaskId_]() {
                queryTaskStatus(taskId);
            });
            emit transcribeProgress(50);  // 中间进度
        }
    } else {
        result.success = false;
        result.errorMessage = "Query failed";
        emit transcribeFinished(result);
    }
    reply->deleteLater();
}

void TencentAsrService::parseResultText(const QString& resultStr, QList<TranscriptSegment>& segments)
{
    // 简单解析腾讯云返回的 Result 格式
    // 格式: [start,end,"text"][start,end,"text"]...
    QRegularExpression re(R"(\[(\d+),(\d+),"([^"]+)"\])");
    QRegularExpressionMatchIterator it = re.globalMatch(resultStr);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        TranscriptSegment seg;
        seg.startMs = match.capturedRef(1).toLongLong();
        seg.endMs = match.capturedRef(2).toLongLong();
        seg.text = match.capturedRef(3).toString();
        segments.append(seg);
    }
}
```

- [ ] **Step 3: 提交**

```bash
git add include/TencentAsrService.h src/TencentAsrService.cpp
git commit -m "feat(asr): add TencentAsrService for cloud ASR integration"
```

---

### Task 5: 集成到 TimelinePanel（拖拽导入）

将转码→上传→ASR 的流程串起来。

**Files:**
- Modify: `src/TimelinePanel.cpp`
- Modify: `include/TimelinePanel.h`

- [ ] **Step 1: 添加拖拽事件处理**

在 `TimelinePanel.h` 中添加:
```cpp
void dragEnterEvent(QDragEnterEvent* event) override;
void dropEvent(QDropEvent* event) override;
```

在 `TimelinePanel.cpp` 中添加:
```cpp
void TimelinePanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TimelinePanel::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mime->hasUrls()) return;

    QUrl url = mime->urls().first();
    QString localPath = url.toLocalFile();

    // 触发 ASR 流程
    AudioTranscoder* transcoder = new AudioTranscoder(this);
    OssUploader* uploader = new OssUploader(this);
    TencentAsrService* asrService = new TencentAsrService(this);

    connect(transcoder, &AudioTranscoder::transcodingFinished, uploader, &OssUploader::upload);
    connect(uploader, &OssUploader::uploadFinished, asrService, &TencentAsrService::transcribe);
    connect(asrService, &AsrServiceBase::transcribeFinished, this, [this](const AsrServiceBase::TranscriptResult& result) {
        if (result.success) {
            for (const auto& seg : result.segments) {
                SubtitleItem item;
                item.id = QUuid::createUuid().toString();
                item.text = seg.text;
                item.startMs = seg.startMs;
                item.endMs = seg.endMs;
                track_->addItem(item);
            }
        }
    });

    transcoder->transcode(localPath);
}
```

- [ ] **Step 2: 提交**

```bash
git add src/TimelinePanel.cpp include/TimelinePanel.h
git commit -m "feat(asr): integrate ASR pipeline into TimelinePanel drag-drop"
```

---

### Task 6: 更新 CMakeLists.txt

添加新源文件。

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 添加新文件到 SOURCES 和 HEADERS**

在 SOURCES 中添加:
```cmake
src/ConfigManager.cpp
src/AudioTranscoder.cpp
src/OssUploader.cpp
src/TencentAsrService.cpp
```

在 HEADERS 中添加:
```cmake
include/ConfigManager.h
include/AudioTranscoder.h
include/OssUploader.h
include/TencentAsrService.h
```

- [ ] **Step 2: 提交**

```bash
git add CMakeLists.txt
git commit -m "build: add ASR service source files to CMakeLists"
```

---

## 自检清单

- [ ] Spec coverage: 所有组件都有对应实现
- [ ] Placeholder scan: 无 TBD/TODO
- [ ] Type consistency: AsrServiceBase::TranscriptSegment 使用一致
- [ ] 配置文件 config.ini.template 已创建
- [ ] 所有新文件都已提交

---

**Plan complete and saved to `docs/superpowers/plans/2026-04-24-asr-implementation.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
