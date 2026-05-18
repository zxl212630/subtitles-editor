# AGENTS.md

本文件为 OpenCode 及其相关 Agent 提供指导。

> ℹ️ **单一事实来源**：有关本项目详细的**架构设计**、**组件定义**、**构建命令**及**UI 规范**，请严格参考 [CLAUDE.md](./CLAUDE.md)。

## 快速参考

- **构建与运行**：参考 `CLAUDE.md` 中的 `Build Commands`。
- **代码规范**：参考 `CLAUDE.md` 中的 `Formatting & Analysis`（提交前必须运行 `clang-format`）。
- **SDK 路径**：默认路径及覆盖方式见 `CLAUDE.md` 的 `SDK Paths` 部分。

## 特定说明 (Agent Specific)

- **信号流向**：`TimelinePanel` → `MediaPlayer` (seek/play), `MediaPlayer` → `TimelinePanel/VideoPreviewPanel` (time/state sync), `SubtitleTrack` → `VideoPreviewPanel` (subtitle refresh)。
- **Pimpl 模式**：`AppWindow` 必须使用 Pimpl 模式（结构体 `Private`，`std::unique_ptr<Private> d`）。
- **配置**：`config.ini` 从 `QStandardPaths::AppConfigLocation` 读取。
