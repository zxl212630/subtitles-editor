# Subtitle Trim-to-Playhead Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two new toolbar buttons (right align `]`, left align `[`) to `TimelinePanel` that trim a subtitle's edge to the current playhead position, with configurable keyboard shortcuts.

**Architecture:** Pure UI addition. New SVG icons, two new `QToolButton` instances inserted in the existing toolbar layout, a single `trimSubtitleEdgeToPlayhead(bool trimStart)` method that locates the target subtitle and calls the existing `SubtitleTrack::updateItem()` (which is already wired into `QUndoStack`). Dynamic enable/disable computed in the existing `updateToolbarStates()`. New shortcut ids registered in `ConfigDialog`'s `kShortcutDefs` so users can rebind them in Settings.

**Tech Stack:** C++17, Qt 6.5.7 (`QToolButton`, `QKeySequence`, `QUndoStack`), SVG icons via existing `QSvgRenderer` pipeline (`renderSvgToIcon` lambda in `TimelinePanel::updateIcons`).

**Spec:** `docs/superpowers/specs/2026-06-15-subtitle-trim-to-playhead-design.md`

---

## File map

| File | Change | Responsibility |
|------|--------|----------------|
| `resources/icons/trim-right.svg` | create | `]` shape, 24×24, `stroke="#d1d5db"`, matches `delete-subtitle.svg` style |
| `resources/icons/trim-left.svg`  | create | `[` shape, 24×24, same style |
| `resources/resources.qrc`       | modify | Register the two new `<file>` entries under `<qresource prefix="/">` |
| `include/TimelinePanel.h`       | modify | Add `QToolButton *trimRightBtn_`, `QToolButton *trimLeftBtn_`; declare `void trimSubtitleEdgeToPlayhead(bool trimStart)` |
| `src/TimelinePanel.cpp`         | modify | Constructor (buttons + click), `updateIcons` (icons), `retranslateUi` (tooltips), `updateShortcuts` (shortcuts), new `trimSubtitleEdgeToPlayhead` method, `updateToolbarStates` (enable logic) |
| `src/ConfigDialog.cpp`          | modify | Add `timeline_trim_right` and `timeline_trim_left` rows to `kShortcutDefs` |

No changes to `SubtitleTrack`, `SubtitleItem`, or other panels. No new data-model methods. No new dependencies.

---

## Task 1: Create icon SVGs and register in resources.qrc

**Files:**
- Create: `resources/icons/trim-right.svg`
- Create: `resources/icons/trim-left.svg`
- Modify: `resources/resources.qrc:47-51` (add two `<file>` lines after the existing `delete-subtitle.svg` / `snap.svg` block)

- [ ] **Step 1: Create `resources/icons/trim-right.svg`**

Write the following file (a `]` bracket shape, 24×24, matching the style of `delete-subtitle.svg`). The polyline traces top-arm-rightward, then a vertical on the **right** at x=15, then the bottom-arm-leftward — the classic `]` shape:

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#d1d5db" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <polyline points="6,4 15,4 15,20 6,20"/>
</svg>
```

- [ ] **Step 2: Create `resources/icons/trim-left.svg`**

The polyline traces top-arm-leftward, then a vertical on the **left** at x=9, then the bottom-arm-rightward — the classic `[` shape. The points are deliberately mirrored from the right-bracket file so the two icons are visually distinct.

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#d1d5db" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <polyline points="18,4 9,4 9,20 18,20"/>
</svg>
```

- [ ] **Step 3: Register both icons in `resources/resources.qrc`**

Open `resources/resources.qrc`. After the existing `<file>icons/delete-subtitle.svg</file>` line (line 47), insert two new lines so the section reads:

```xml
        <file>icons/delete-subtitle.svg</file>
        <file>icons/trim-right.svg</file>
        <file>icons/trim-left.svg</file>
        <file>icons/snap.svg</file>
```

(The order is just for diff cleanliness — placement has no functional effect.)

- [ ] **Step 4: Commit**

```bash
git add resources/icons/trim-right.svg resources/icons/trim-left.svg resources/resources.qrc
git commit -m "feat(timeline): add SVG icons for trim-right and trim-left buttons"
```

---

## Task 2: Add member declarations to `TimelinePanel.h`

**Files:**
- Modify: `include/TimelinePanel.h:165-167` (add two `QToolButton` pointers next to `deleteBtn_`)
- Modify: `include/TimelinePanel.h:76` area (declare the private method) — add next to other private helper declarations

- [ ] **Step 1: Add the two `QToolButton` member pointers**

In `include/TimelinePanel.h`, locate the toolbar widgets block (around line 165-167):

```cpp
  QToolButton *addBtn_ = nullptr;
  QToolButton *splitBtn_ = nullptr;
  QToolButton *deleteBtn_ = nullptr;
```

Add two new lines directly after `deleteBtn_`:

```cpp
  QToolButton *addBtn_ = nullptr;
  QToolButton *splitBtn_ = nullptr;
  QToolButton *deleteBtn_ = nullptr;
  QToolButton *trimRightBtn_ = nullptr;
  QToolButton *trimLeftBtn_ = nullptr;
```

- [ ] **Step 2: Declare the private method**

In `include/TimelinePanel.h`, in the `private:` section near the other helper methods (around lines 76-79), add a declaration after `drawPlayhead`:

```cpp
  void drawRuler(QPainter &painter);
  void drawSubtitleTrack(QPainter &painter, int y);
  void drawVideoTrack(QPainter &painter, int y);
  void drawEmptyState(QPainter &painter);
  void drawPlayhead(QPainter &painter);

  void trimSubtitleEdgeToPlayhead(bool trimStart);
```

- [ ] **Step 3: Commit**

```bash
git add include/TimelinePanel.h
git commit -m "feat(timeline): declare trimRightBtn_, trimLeftBtn_ and trimSubtitleEdgeToPlayhead"
```

---

## Task 3: Create buttons and wire up UI in `TimelinePanel.cpp`

**Files:**
- Modify: `src/TimelinePanel.cpp:110` area (add `createToolBtn` calls for the two new buttons)
- Modify: `src/TimelinePanel.cpp:136` area (install `ToolTipEventFilter` on the two new buttons)
- Modify: `src/TimelinePanel.cpp:285` area (connect `clicked` signals)
- Modify: `src/TimelinePanel.cpp:1854-1867` area (set icons in `updateIcons`)
- Modify: `src/TimelinePanel.cpp:1876-1887` area (set tooltips in `retranslateUi`)
- Modify: `src/TimelinePanel.cpp:1918-1937` area (wire shortcuts in `updateShortcuts`)

- [ ] **Step 1: Create the two buttons in the constructor**

In `src/TimelinePanel.cpp`, locate the toolbar button creation block (around line 108-110):

```cpp
  createToolBtn(addBtn_, "TimelineToolbarBtn");
  createToolBtn(splitBtn_, "TimelineToolbarBtn");
  createToolBtn(deleteBtn_, "TimelineToolbarBtn");
```

Add two new lines directly after `deleteBtn_` (and before the `tbLayout->addSpacing(4);` block — keep the trim buttons grouped with the other edit buttons, before the stretch):

```cpp
  createToolBtn(addBtn_, "TimelineToolbarBtn");
  createToolBtn(splitBtn_, "TimelineToolbarBtn");
  createToolBtn(deleteBtn_, "TimelineToolbarBtn");
  createToolBtn(trimRightBtn_, "TimelineToolbarBtn");
  createToolBtn(trimLeftBtn_, "TimelineToolbarBtn");
```

- [ ] **Step 2: Install `ToolTipEventFilter` on the two new buttons**

In `src/TimelinePanel.cpp`, locate the existing `installEventFilter` block for the toolbar buttons (around line 130-141). Add two new lines after the `deleteBtn_` line:

```cpp
  deleteBtn_->installEventFilter(ToolTipEventFilter::instance());
  trimRightBtn_->installEventFilter(ToolTipEventFilter::instance());
  trimLeftBtn_->installEventFilter(ToolTipEventFilter::instance());
  snapBtn_->installEventFilter(ToolTipEventFilter::instance());
```

- [ ] **Step 3: Connect `clicked` signals**

In `src/TimelinePanel.cpp`, locate the existing `connect(deleteBtn_, &QToolButton::clicked, ...)` block (around line 245-261). Add the two new `connect` calls directly after it, before the `snapBtn_` connections:

```cpp
  connect(deleteBtn_, &QToolButton::clicked, this, [this]() {
    if (!track_)
      return;
    QList<QString> selectedIds;
    for (const auto &item : track_->items()) {
      if (item.selected) {
        selectedIds.append(item.id);
      }
    }
    if (!selectedIds.isEmpty()) {
      track_->executeBatchAction(tr("删除选中字幕"), [this, selectedIds]() {
        for (const auto &id : selectedIds) {
          track_->removeItem(id);
        }
      });
    }
  });

  connect(trimRightBtn_, &QToolButton::clicked, this,
          [this]() { trimSubtitleEdgeToPlayhead(true); });
  connect(trimLeftBtn_, &QToolButton::clicked, this,
          [this]() { trimSubtitleEdgeToPlayhead(false); });

  connect(snapBtn_, &QToolButton::toggled, this, [this](bool checked) {
```

- [ ] **Step 4: Set icons in `updateIcons`**

In `src/TimelinePanel.cpp`, locate the icon-setting block in `updateIcons` (around line 1854-1867). Add two new lines directly after the `deleteBtn_` icon line:

```cpp
  deleteBtn_->setIcon(
      renderSvgToIcon(":/icons/delete-subtitle.svg", textNormal, 16));
  trimRightBtn_->setIcon(
      renderSvgToIcon(":/icons/trim-right.svg", textNormal, 16));
  trimLeftBtn_->setIcon(
      renderSvgToIcon(":/icons/trim-left.svg", textNormal, 16));
  snapBtn_->setIcon(renderSvgToIcon(":/icons/snap.svg", textNormal, 16));
```

- [ ] **Step 5: Set tooltips in `retranslateUi`**

In `src/TimelinePanel.cpp`, locate the `retranslateUi` method (around line 1875-1894). Add two new lines directly after the `deleteBtn_` tooltip line:

```cpp
  deleteBtn_->setToolTip(tr("删除字幕"));
  trimRightBtn_->setToolTip(tr("右对齐"));
  trimLeftBtn_->setToolTip(tr("左对齐"));
  snapBtn_->setToolTip(tr("自动吸附"));
```

(The `updateShortcuts()` call at the end of `retranslateUi` will append the keyboard-shortcut suffix automatically — see next step.)

- [ ] **Step 6: Wire shortcuts in `updateShortcuts`**

In `src/TimelinePanel.cpp`, locate the `updateShortcuts` method (around line 1902-1938). Add two new `applyShortcut` calls directly after the `deleteBtn_` line, before the `snapBtn_` line:

```cpp
  applyShortcut(deleteBtn_, "timeline_delete", QKeySequence(DEFAULT_DELETE_KEY),
                tr("删除字幕"));
  applyShortcut(trimRightBtn_, "timeline_trim_right",
                QKeySequence(Qt::Key_BracketRight), tr("右对齐"));
  applyShortcut(trimLeftBtn_, "timeline_trim_left",
                QKeySequence(Qt::Key_BracketLeft), tr("左对齐"));
  applyShortcut(snapBtn_, "timeline_snap", QKeySequence(Qt::CTRL | Qt::Key_N),
                tr("自动吸附"));
```

- [ ] **Step 7: Commit**

```bash
git add src/TimelinePanel.cpp
git commit -m "feat(timeline): wire up trim-right and trim-left toolbar buttons"
```

---

## Task 4: Implement `trimSubtitleEdgeToPlayhead` method

**Files:**
- Modify: `src/TimelinePanel.cpp` — add the method body. Place it directly after `updateToolbarStates` (the end of the file is around line 2002) so it sits next to the related toolbar logic.

- [ ] **Step 1: Add the method implementation**

Append the following method to `src/TimelinePanel.cpp` (e.g., immediately after the `updateToolbarStates` closing brace, before `updateZoomControls`):

```cpp
void TimelinePanel::trimSubtitleEdgeToPlayhead(bool trimStart) {
  if (!track_)
    return;

  const auto &items = track_->items();
  if (items.isEmpty())
    return;

  // 1. Try to find a subtitle containing the playhead.
  const SubtitleItem *containing = nullptr;
  for (const auto &item : items) {
    if (item.startMs <= currentTimeMs_ && currentTimeMs_ < item.endMs) {
      containing = &item;
      break;
    }
  }

  QString targetId;
  qint64 newStartMs = 0;
  qint64 newEndMs = 0;

  if (containing) {
    targetId = containing->id;
    if (trimStart) {
      newStartMs = currentTimeMs_;
      newEndMs = containing->endMs;
    } else {
      newStartMs = containing->startMs;
      newEndMs = currentTimeMs_;
    }
  } else {
    if (trimStart) {
      // Nearest subtitle to the LEFT (largest endMs <= playhead).
      const SubtitleItem *left = nullptr;
      for (const auto &item : items) {
        if (item.endMs <= currentTimeMs_) {
          if (!left || item.endMs > left->endMs) {
            left = &item;
          }
        }
      }
      if (!left)
        return;
      targetId = left->id;
      newStartMs = left->startMs;
      newEndMs = currentTimeMs_;
    } else {
      // Nearest subtitle to the RIGHT (smallest startMs > playhead).
      const SubtitleItem *right = nullptr;
      for (const auto &item : items) {
        if (item.startMs > currentTimeMs_) {
          if (!right || item.startMs < right->startMs) {
            right = &item;
          }
        }
      }
      if (!right)
        return;
      targetId = right->id;
      newStartMs = currentTimeMs_;
      newEndMs = right->endMs;
    }
  }

  // Validity guard: refuse to create an inverted or zero-duration item.
  if (newStartMs >= newEndMs)
    return;

  const SubtitleItem *orig = track_->findItem(targetId);
  if (!orig)
    return;

  if (orig->startMs == newStartMs && orig->endMs == newEndMs)
    return; // no-op; do not pollute the undo stack

  SubtitleItem updated = *orig;
  updated.startMs = newStartMs;
  updated.endMs = newEndMs;
  track_->updateItem(targetId, updated);
}
```

- [ ] **Step 2: Commit**

```bash
git add src/TimelinePanel.cpp
git commit -m "feat(timeline): implement trimSubtitleEdgeToPlayhead for right/left align"
```

---

## Task 5: Add enable state logic in `updateToolbarStates`

**Files:**
- Modify: `src/TimelinePanel.cpp:1940-2002` — extend the existing `updateToolbarStates` method.

- [ ] **Step 1: Add the `trimRightBtn_`/`trimLeftBtn_` enable logic**

In `src/TimelinePanel.cpp`, locate the `updateToolbarStates` method. Find the section that iterates over items to set the `splitBtn_` state (around line 1994-2001):

```cpp
  bool canSplit = false;
  for (const auto &item : items) {
    if (item.startMs < currentTimeMs_ && currentTimeMs_ < item.endMs) {
      canSplit = true;
      break;
    }
  }
  splitBtn_->setEnabled(canSplit);
}
```

Replace the entire block with the following (keeping the `canSplit` logic intact, and adding the trim-enable logic right after it):

```cpp
  bool canSplit = false;
  for (const auto &item : items) {
    if (item.startMs < currentTimeMs_ && currentTimeMs_ < item.endMs) {
      canSplit = true;
      break;
    }
  }
  splitBtn_->setEnabled(canSplit);

  // Trim-right (]): enabled if a subtitle contains the playhead, or if any
  // subtitle lies to the left of the playhead (endMs <= currentTimeMs_).
  // Trim-left ([): same containing check, plus any subtitle to the right
  // (startMs > currentTimeMs_).
  bool isContaining = false;
  bool hasLeft = false;
  bool hasRight = false;
  for (const auto &item : items) {
    if (item.startMs <= currentTimeMs_ && currentTimeMs_ < item.endMs) {
      isContaining = true;
    } else if (item.endMs <= currentTimeMs_) {
      hasLeft = true;
    } else if (item.startMs > currentTimeMs_) {
      hasRight = true;
    }
  }
  trimRightBtn_->setEnabled(isContaining || hasLeft);
  trimLeftBtn_->setEnabled(isContaining || hasRight);
}
```

The three booleans partition every item: containing, left (ends at or before playhead), or right (starts after playhead). The `else if` chain is correct because an item cannot satisfy more than one branch in a non-overlapping timeline. The `isContaining` short-circuit guarantees the buttons are enabled even when the playhead is inside the very first or very last subtitle (where `hasLeft` / `hasRight` would be false).

- [ ] **Step 2: Commit**

```bash
git add src/TimelinePanel.cpp
git commit -m "feat(timeline): dynamic enable/disable for trim-right and trim-left buttons"
```

---

## Task 6: Register shortcut entries in `ConfigDialog.cpp`

**Files:**
- Modify: `src/ConfigDialog.cpp:39-57` — add two new rows to `kShortcutDefs` so the new shortcuts show up in Settings → Shortcuts.

- [ ] **Step 1: Add the new shortcut definitions**

In `src/ConfigDialog.cpp`, locate the `kShortcutDefs` array (around line 39-57). Add two new entries directly after `{"timeline_delete", DEFAULT_DELETE_KEY, false},` and before `{"timeline_snap", ...}`:

```cpp
    {"timeline_split", Qt::Key_S, false},
    {"timeline_delete", DEFAULT_DELETE_KEY, false},
    {"timeline_trim_right", Qt::Key_BracketRight, false},
    {"timeline_trim_left", Qt::Key_BracketLeft, false},
    {"timeline_snap", Qt::CTRL | Qt::Key_N, false},
```

- [ ] **Step 2: Commit**

```bash
git add src/ConfigDialog.cpp
git commit -m "feat(config): register timeline_trim_right and timeline_trim_left shortcuts"
```

---

## Task 7: Build, format, and verify

**Files:** none (verification only)

- [ ] **Step 1: Format the modified C++ files**

```bash
clang-format -i src/TimelinePanel.cpp src/ConfigDialog.cpp include/TimelinePanel.h
```

Expected: no output, exit code 0.

- [ ] **Step 2: Build the project**

```bash
cmake --build cmake-build-debug
```

Expected: build succeeds with no errors. Warnings are tolerable; new errors are not. (Note: the `.qrc` file is compiled into a static C++ table at build time; if Qt's `rcc` is missing icons, the build will fail with a clear error.)

- [ ] **Step 3: Manual smoke test — load the app**

```bash
nohup ./cmake-build-debug/subtitles-editor.app/Contents/MacOS/subtitles-editor > /tmp/trim_smoke.log 2>&1 &
```

Confirm the app launches without crashing. Tail the log:

```bash
tail -20 /tmp/trim_smoke.log
```

Expected: no `qFatal`, no `QObject::connect: ...` warnings about `trimRightBtn_` / `trimLeftBtn_`. If a `nullptr` connect warning appears, the button was not created — revisit Task 3 Step 1.

- [ ] **Step 4: Visual verification of toolbar**

Open the app, load a project (or use `setupDummyData()` in DEBUG builds if available), and confirm:

1. The two new icons (a `]` and a `[`) appear between the delete (trash) icon and the snap/zoom controls.
2. Tooltips on hover read `右对齐 (])` and `左对齐 ([)`.
3. With a non-empty subtitle list, both buttons are enabled by default. With an empty list, both are disabled.

- [ ] **Step 5: Functional verification (manual)**

From the spec's testing section, run through:

1. Place the playhead inside a subtitle; press `]`. Its start jumps to the playhead. `Ctrl+Z` reverts.
2. Playhead inside a subtitle; press `[`. Its end jumps to the playhead. `Ctrl+Z` reverts.
3. Playhead in a gap; press `]`. The end of the nearest left subtitle jumps to the playhead.
4. Playhead in a gap; press `[`. The start of the nearest right subtitle jumps to the playhead.
5. Playhead at timeline start (0) → `[` enabled, `]` disabled.
6. Playhead at timeline end → `]` enabled, `[` disabled.
7. Empty track → both disabled.
8. Rebind the shortcut in Settings → keyboard still triggers the new button.
9. Reload a project → enable state still correct.

If any step fails, the relevant task is the one to revisit:
- Steps 1-2 fail: revisit Task 4 (the `trimSubtitleEdgeToPlayhead` method).
- Steps 3-4 fail: revisit Task 4 (the fallback to nearest-left/nearest-right).
- Step 5-7 fail: revisit Task 5 (the enable logic).
- Step 8 fails: revisit Task 6 (the `kShortcutDefs` entries).

- [ ] **Step 6: Final commit (if formatting changes touched any tracked file)**

```bash
git status
```

If `clang-format` changed anything tracked:

```bash
git add -u
git commit -m "style: clang-format trim-to-playhead changes"
```

Otherwise, no commit is needed.

---

## Self-review notes (carried out before handoff)

**Spec coverage check:**
- "Behavior" section: Task 4 implements both algorithms (`trimStart=true` for `]`, `trimStart=false` for `[`).
- "UI: toolbar placement after deleteBtn_, order ] then [": Task 3 Step 1.
- "Icons: trim-right.svg, trim-left.svg": Task 1.
- "Tooltips and shortcuts": Task 3 Steps 5-6.
- "Button enable state (dynamic)": Task 5.
- "Files to change" (all 6): covered by Tasks 1, 2, 3, 4, 5, 6.
- "Out of scope" respected: no new `SubtitleTrack` methods, no model changes.
- "Testing" manual checklist: Task 7 Step 5.

**Placeholder scan:** No "TBD"/"TODO"/"implement later" phrases. All code blocks contain actual code.

**Type / name consistency:**
- `trimRightBtn_`, `trimLeftBtn_`: used identically in header (Task 2), constructor (Task 3 Step 1), filter install (Step 2), connect (Step 3), updateIcons (Step 4), retranslateUi (Step 5), updateShortcuts (Step 6), updateToolbarStates (Task 5).
- `trimSubtitleEdgeToPlayhead(bool trimStart)`: declared in Task 2, defined in Task 4, called from Task 3 Step 3 with `true` / `false`.
- Shortcut ids `timeline_trim_right` / `timeline_trim_left`: used identically in `applyShortcut` calls (Task 3 Step 6) and in `kShortcutDefs` (Task 6).
- `Qt::Key_BracketRight` / `Qt::Key_BracketLeft`: Qt 6 enum values, used in both the button shortcut binding (Task 3 Step 6) and the ConfigDialog default (Task 6).

No inconsistencies found.
