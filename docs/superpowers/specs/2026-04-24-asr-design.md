# 腾讯云 ASR 语音转字幕功能设计

## 目标
实现音频/视频文件导入后，通过腾讯云 ASR 自动识别生成字幕。

## 流程架构（职责链）

```
拖拽文件 → AudioTranscoder → OssUploader → TencentAsrService → SubtitleTrack
```

## 组件设计

### 1. AudioTranscoder
- 职责：FFmpeg 转码为 WAV/16k 格式
- 输入：原始音频/视频文件路径
- 输出：WAV 文件路径
- 依赖：FFmpeg（项目已有 `~/Tools/ffmpeg/8.0`）

### 2. OssUploader
- 职责：上传文件到阿里云 OSS
- 输入：本地文件路径
- 输出：OSS 文件 URL
- 依赖：阿里云 OSS SDK 或 API

### 3. TencentAsrService
- 职责：调用腾讯云 ASR 接口
- 输入：OSS 文件 URL
- 输出：`QList<TranscriptSegment>` (text, startMs, endMs)
- 信号：`transcribeFinished`, `transcribeProgress`

### 4. SubtitleOverlay（未来扩展）
- 职责：字幕叠加显示在视频画面上
- 可配置：位置、大小、字体、字号
- 依赖：VideoPreviewPanel 视频帧渲染

## 配置项（写死配置文件）

### FFmpeg
- 路径：`~/Tools/ffmpeg/8.0`

### 腾讯云 ASR
- SecretId：（待配置）
- SecretKey：（待配置）
- AppId：（待配置）
- 引擎：16k（16k_zh）

### 阿里云 OSS
- AccessKeyId：（待配置）
- AccessKeySecret：（待配置）
- Bucket：（待配置）
- Region：（待配置）

## 未来扩展项

- [ ] 配置页面（FFmpeg路径、ASR凭证、OSS配置）
- [ ] 安装包打包 FFmpeg
- [ ] 视频帧渲染 + 字幕 Overlay 叠加
- [ ] 字幕样式配置（位置、字体、字号、颜色）
- [ ] ASR 进度显示
- [ ] 转码进度显示
- [ ] 上传进度显示
- [ ] 取消/重试功能
- [ ] 支持其他 ASR 提供商（阿里云、讯飞等）

## 文件结构

| 文件 | 职责 |
|------|------|
| `include/AudioTranscoder.h` | FFmpeg 转码接口 |
| `src/AudioTranscoder.cpp` | 转码实现 |
| `include/OssUploader.h` | 阿里云 OSS 上传接口 |
| `src/OssUploader.cpp` | 上传实现 |
| `include/TencentAsrService.h` | 腾讯云 ASR 接口（继承 AsrServiceBase） |
| `src/TencentAsrService.cpp` | ASR 实现 |
| `include/SubtitleOverlay.h` | 字幕叠加层（未来） |
| `src/SubtitleOverlay.cpp` | 叠加层实现（未来） |
| `CMakeLists.txt` | 添加新源文件 |

## 实现顺序

1. AudioTranscoder（FFmpeg 转码）
2. OssUploader（OSS 上传）
3. TencentAsrService（ASR 调用 + 结果解析）
4. SubtitleTrack 写入
5. 集成到 TimelinePanel 拖拽导入
