# Organize Installers by Platform Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Organize installation files into a dedicated `dist/` directory with platform-specific subdirectories for GitHub releases and local file management.

**Architecture:** Create a `dist/` directory with `macos/` and `windows/` subdirectories. Move existing DMG file, update build script and CI/CD workflow to output to new directory structure, and update README with download and troubleshooting information.

**Tech Stack:** Bash, GitHub Actions, Markdown

---

## File Structure

- Create: `dist/macos/` (directory)
- Create: `dist/windows/` (directory)
- Modify: `scripts/package-macos.sh:9,248-255`
- Modify: `.github/workflows/release.yml:23`
- Modify: `README.md` (add sections)
- Move: `SubtitlesEditor-1.0.0-macOS-arm64.dmg` → `dist/macos/SubtitlesEditor-1.0.0-macOS-arm64-unsigned.dmg`

---

### Task 1: Create Directory Structure

**Files:**
- Create: `dist/macos/` (directory)
- Create: `dist/windows/` (directory)

- [ ] **Step 1: Create dist/macos directory**

```bash
mkdir -p dist/macos
```

- [ ] **Step 2: Create dist/windows directory**

```bash
mkdir -p dist/windows
```

- [ ] **Step 3: Verify directories exist**

```bash
ls -la dist/
```

Expected: Two directories: `macos/` and `windows/`

- [ ] **Step 4: Commit**

```bash
git add dist/
git commit -m "feat: create dist directory structure for installers"
```

---

### Task 2: Move and Rename DMG File

**Files:**
- Move: `SubtitlesEditor-1.0.0-macOS-arm64.dmg` → `dist/macos/SubtitlesEditor-1.0.0-macOS-arm64-unsigned.dmg`

- [ ] **Step 1: Move and rename DMG file**

```bash
mv SubtitlesEditor-1.0.0-macOS-arm64.dmg dist/macos/SubtitlesEditor-1.0.0-macOS-arm64-unsigned.dmg
```

- [ ] **Step 2: Verify file moved**

```bash
ls -la dist/macos/
```

Expected: `SubtitlesEditor-1.0.0-macOS-arm64-unsigned.dmg` exists

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: move DMG to dist/macos with unsigned suffix"
```

---

### Task 3: Modify Build Script

**Files:**
- Modify: `scripts/package-macos.sh:9,248-255`

- [ ] **Step 1: Update DMG name to include -unsigned suffix**

Change line 9 from:
```bash
DMG_NAME="SubtitlesEditor-1.0.0-macOS-arm64"
```
to:
```bash
DMG_NAME="SubtitlesEditor-1.0.0-macOS-arm64-unsigned"
```

- [ ] **Step 2: Update output path to dist/macos directory**

Change lines 248-255 from:
```bash
STAGING_DIR=$(mktemp -d)
cp -R "$APP_BUNDLE" "$STAGING_DIR/"
ln -sf /Applications "$STAGING_DIR/Applications"
hdiutil create -volname "$DMG_NAME" \
    -srcfolder "$STAGING_DIR" \
    -ov -format UDZO \
    "$PROJECT_DIR/$DMG_NAME.dmg"
rm -rf "$STAGING_DIR"
```
to:
```bash
STAGING_DIR=$(mktemp -d)
cp -R "$APP_BUNDLE" "$STAGING_DIR/"
ln -sf /Applications "$STAGING_DIR/Applications"
mkdir -p "$PROJECT_DIR/dist/macos"
hdiutil create -volname "$DMG_NAME" \
    -srcfolder "$STAGING_DIR" \
    -ov -format UDZO \
    "$PROJECT_DIR/dist/macos/$DMG_NAME.dmg"
rm -rf "$STAGING_DIR"
```

- [ ] **Step 3: Update echo message**

Change line 259 from:
```bash
echo "DMG: $PROJECT_DIR/$DMG_NAME.dmg"
```
to:
```bash
echo "DMG: $PROJECT_DIR/dist/macos/$DMG_NAME.dmg"
```

- [ ] **Step 4: Verify script changes**

```bash
grep -n "DMG_NAME\|dist/macos" scripts/package-macos.sh
```

Expected: Line 9 shows `-unsigned`, lines 248-259 show `dist/macos` path

- [ ] **Step 5: Commit**

```bash
git add scripts/package-macos.sh
git commit -m "feat: update build script to output to dist/macos with unsigned suffix"
```

---

### Task 4: Modify CI/CD Workflow

**Files:**
- Modify: `.github/workflows/release.yml:23`

- [ ] **Step 1: Update file path in release workflow**

Change line 23 from:
```yaml
files: SubtitlesEditor-*-macOS-arm64.dmg
```
to:
```yaml
files: dist/macos/SubtitlesEditor-*-macOS-arm64-unsigned.dmg
```

- [ ] **Step 2: Verify workflow changes**

```bash
grep -n "files:" .github/workflows/release.yml
```

Expected: Line 23 shows `dist/macos/` path

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/release.yml
git commit -m "feat: update CI/CD workflow to use dist/macos path"
```

---

### Task 5: Update README

**Files:**
- Modify: `README.md` (add sections)

- [ ] **Step 1: Add download section after platform support table**

Add after line 44 (after the platform support table):
```markdown

## 📥 下载

前往 [GitHub Releases](https://github.com/zxl212630/subtitles-editor/releases) 页面下载最新版本。

> ⚠️ **注意**：macOS 版本为未签名版本，首次打开可能会遇到安全提示。
```

- [ ] **Step 2: Add troubleshooting section**

Add after the download section:
```markdown

## ❓ 常见问题

### macOS 提示"无法打开，因为无法验证开发者"

由于应用未签名，macOS 可能会阻止打开。有三种解决方法：

1. **方法一**：前往 系统偏好设置 > 安全性与隐私，点击"仍要打开"
2. **方法二**：右键点击应用，选择"打开"，在弹出窗口中点击"打开"
3. **方法三**：在终端执行以下命令：
   ```bash
   xattr -cr /Applications/SubtitlesEditor.app
   ```
```

- [ ] **Step 3: Verify README changes**

```bash
grep -n "下载\|常见问题\|xattr" README.md
```

Expected: Download section and troubleshooting section with xattr command

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: add download section and troubleshooting for unsigned app"
```

---

## Verification

1. Verify `dist/macos/` directory exists and contains DMG file
2. Verify `dist/windows/` directory exists (empty)
3. Verify build script outputs to correct location
4. Verify CI/CD workflow references correct path
5. Verify README contains all required information
