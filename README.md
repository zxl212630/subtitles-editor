# Subtitles Editor

一个基于 C++17 和 Qt6 开发的 macOS 视频字幕编辑器，提供直观的时间轴编辑界面和实时视频预览功能。

> 💡 **Vibe Coding 项目**：本项目完全通过 AI 辅助编程（Vibe Coding）方式开发完成，展示了人机协作的开发模式。

## ✨ 功能特性

### 🎬 视频播放
- 支持多种视频格式：MP4、MKV、AVI、MOV
- 实时视频预览与字幕叠加显示
- 流畅的播放控制（播放、暂停、跳转、倍速）

### 📝 字幕编辑
- 可视化时间轴编辑界面
- 支持字幕的添加、编辑、删除和排序
- 精确的时间码调整（毫秒级）
- 字幕搜索和批量操作
- 多选模式支持批量修改

### 📤 导出支持
- **SRT** - 最通用的字幕格式
- **ASS** - 支持高级样式
- **Premiere Pro XML** - 与 Adobe Premiere Pro 无缝集成
- **Final Cut Pro XML** - 与 Final Cut Pro 无缝集成

### 🎨 界面设计
- 深色/浅色主题切换
- 自定义主题色
- 响应式布局，支持面板拖拽调整

### 🔧 高级功能
- ASR 语音识别（腾讯云）
- 对象存储（腾讯云、阿里云）
- 多语言国际化支持
- 项目管理与自动保存

## 💻 平台支持

| 平台 | 状态 | 备注 |
|------|------|------|
| macOS (Apple Silicon) | ✅ 已验证 | M1/M2/M3 芯片 |
| macOS (Intel) | ⚠️ 未验证 | 欢迎测试反馈 |
| Windows | ❌ 暂不支持 | 计划中 |

## 📥 下载

前往 [GitHub Releases](https://github.com/zxl212630/subtitles-editor/releases) 页面下载最新版本。

> ⚠️ **注意**：macOS 版本为未签名版本，首次打开可能会遇到安全提示。

## ❓ 常见问题

### macOS 提示"无法打开，因为无法验证开发者"

由于应用未签名，macOS 可能会阻止打开。有三种解决方法：

1. **方法一**：前往 系统偏好设置 > 安全性与隐私，点击"仍要打开"
2. **方法二**：右键点击应用，选择"打开"，在弹出窗口中点击"打开"
3. **方法三**：在终端执行以下命令：
   ```bash
   xattr -cr /Applications/SubtitlesEditor.app
   ```

## 📦 依赖项

| 依赖 | 版本 | 用途 |
|------|------|------|
| Qt6 | 6.5.7+ | UI 框架 |
| FFmpeg | 8.0 | 视频/音频解码 |
| QWindowKit | - | 自定义标题栏 |

## 🚀 构建指南

### 环境准备

确保已安装以下依赖：
- CMake 3.16+
- Qt6 6.5.7+
- FFmpeg 8.0
- C++17 兼容的编译器

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/zxl212630/subtitles-editor.git
cd subtitles-editor

# 配置（默认 SDK 路径，可通过 -D 参数覆盖）
cmake -B cmake-build-debug -S .

# 编译
cmake --build cmake-build-debug

# 运行
./cmake-build-debug/subtitles-editor
```

### SDK 路径配置

如果 SDK 安装在非默认路径，可通过 CMake 参数指定：

```bash
cmake -B cmake-build-debug -S . \
  -DQt6_ROOT=/path/to/qt6 \
  -DQWindowKit_ROOT=/path/to/qwindowkit \
  -DFFmpeg_ROOT=/path/to/ffmpeg
```

## 📁 项目结构

```
subtitles-editor/
├── include/          # 头文件
├── src/              # 源文件
├── resources/        # 资源文件（图标、翻译等）
├── CMakeLists.txt    # CMake 配置
└── README.md
```

### 核心模块

| 模块 | 文件 | 功能 |
|------|------|------|
| 主窗口 | `AppWindow` | 应用主窗口，管理整体布局 |
| 字幕轨道 | `SubtitleTrack` | 字幕数据模型 |
| 字幕列表 | `SubtitleListPanel` | 左侧面板，搜索与列表 |
| 视频预览 | `VideoPreviewPanel` | 右侧面板，视频播放与显示 |
| 时间轴 | `TimelinePanel` | 底部面板，时间轴编辑 |
| 导出器 | `SubtitleExporter` | 多格式字幕导出 |
| ASR 服务 | `AsrServiceBase` | 语音识别抽象接口 |

## 🎯 使用说明

1. **导入视频**：通过菜单或拖拽方式导入视频文件
2. **编辑字幕**：在时间轴上添加或选择字幕，输入文本内容
3. **调整时间**：拖动时间轴上的字幕块或手动输入时间码
4. **预览效果**：实时查看字幕在视频中的显示效果
5. **导出字幕**：选择所需格式导出字幕文件

## 🛠️ 开发工具

```bash
# 代码格式化（提交前必须执行）
clang-format -i src/*.cpp include/*.h

# 静态分析
clang-tidy src/*.cpp -- -std=c++17
```

## 📄 许可证

本项目采用 GPLv3 许可证 - 详见 [LICENSE](LICENSE) 文件

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'feat: Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

## 📧 联系方式

- GitHub: [@zxl212630](https://github.com/zxl212630)
- Gitee: [@zxl212630](https://gitee.com/zxl212630)
