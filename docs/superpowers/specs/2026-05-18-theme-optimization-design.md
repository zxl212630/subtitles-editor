# 主题优化设计文档 (2026-05-18)

## 1. 背景与目标
当前字幕编辑器提供“深色”和“浅色”两种模式。然而，浅色模式在视频剪辑场景下容易造成视觉干扰，且不符合专业剪辑软件的审美趋势。
本项目标是移除浅色模式，并基于现代剪辑软件（如剪映、Premiere）的视觉风格，提供三套层次分明的暗色主题方案。

## 2. 主题方案定义

我们将提供以下三个主题，每个主题通过 `bgBase` (底层), `bgPanel` (面板), `bgLighter` (交互元素) 三层色值建立视觉深度。

### 2.1 经典暗色 (Dark) - 保持现状
现有的默认主题，灰色调适中。
- `bgBase`: `#151515`
- `bgPanel`: `#1e1e1e`
- `bgLighter`: `#262626`
- `border`: `#3f3f46`
- `textNormal`: `#ffffff`

### 2.2 深度灰黑 (Deep Gray) - 极致沉浸
优化自原 OLED 方案，采用极深灰而非纯黑，确保 UI 边缘可辨识。
- `bgBase`: `#0a0a0a`
- `bgPanel`: `#121212`
- `bgLighter`: `#1e1e1e`
- `border`: `#262626`
- `textNormal`: `#eeeeee`

### 2.3 专业午夜蓝 (Midnight) - 舒适护眼
带有低饱和度的蓝色底调，降低视觉疲劳。
- `bgBase`: `#111218`
- `bgPanel`: `#171821`
- `bgLighter`: `#212330`
- `border`: `#2d2f3f`
- `textNormal`: `#c0caf5`

## 3. 修改范围

### 3.1 核心逻辑 (ThemeManager)
- 移除 `themes_["light"]` 和 `themeNames_["light"]` 定义。
- 新增 `themes_["oled"]` (映射到深度灰黑) 和 `themes_["midnight"]` 定义。
- 在 `availableThemes()` 中返回 `{"dark", "oled", "midnight"}`。
- 确保 `loadQssTemplate` 在请求不存在的主题时回退到 `dark`。

### 3.2 资源文件
- 删除 `resources/themes/light.qss`。
- 修改 `resources/resources.qrc`，移除对 `light.qss` 的引用。

### 3.3 配置界面 (ConfigDialog)
- 更新 `ThemeSelectorWidget` 的初始化代码：
  - 移除 `light` 选项。
  - 添加 `oled` 和 `midnight` 选项，并设置对应的预览色块。
- 更新 `loadConfig` 逻辑，如果当前配置为 `light`，则自动重置为 `dark`。

### 3.4 兼容性处理
- 在 `ConfigManager` 或 `ThemeManager` 初始化时，检测旧配置。如果是 `light`，强制切换为 `dark`。

## 4. 测试建议
- 验证切换三个主题时，主界面、设置对话框、时间轴的颜色是否正确更新。
- 验证原有的“强调色（Primary Color）”在三个暗色主题下是否都能保持良好的可读性。
- 验证删除 `light.qss` 后，程序启动不会报错或崩溃。
