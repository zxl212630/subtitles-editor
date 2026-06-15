# Subtitle Trim-to-Playhead (Right/Left Align)

**Date:** 2026-06-15
**Status:** Design approved; pending implementation plan

## Goal

Add two new toolbar buttons to `TimelinePanel` that trim a subtitle's edge to the current playhead position:

- **]** (right align) — moves the subtitle's *start* (or the *end* of the nearest left subtitle) to the playhead.
- **[** (left align) — moves the subtitle's *end* (or the *start* of the nearest right subtitle) to the playhead.

Shortcuts: `]` and `[`. Both are user-rebindable via the existing shortcut configuration system.

## Behavior

### Right align — `]`

1. Find a subtitle containing the playhead: `startMs <= playhead < endMs`.
2. If found: set its `startMs = playhead`. If new `startMs >= endMs`, no-op.
3. If not found: find the nearest subtitle to the **left** (largest `endMs <= playhead`).
4. If found: set its `endMs = playhead`. If new `endMs <= startMs`, no-op.
5. If no target: do nothing.

### Left align — `[`

1. Find a subtitle containing the playhead: `startMs <= playhead < endMs`.
2. If found: set its `endMs = playhead`. If new `endMs <= startMs`, no-op.
3. If not found: find the nearest subtitle to the **right** (smallest `startMs > playhead`).
4. If found: set its `startMs = playhead`. If new `startMs >= endMs`, no-op.
5. If no target: do nothing.

### Properties

- Operation is **undoable**: routed through the existing `SubtitleTrack::updateItem()` (undo command name "修改字幕"), so `Ctrl+Z` reverts.
- **Selection state is not changed.**
- **Multi-select is ignored** — only the subtitle at the playhead (or nearest on the appropriate side) is affected.
- A no-op case never produces an undo entry.

## UI

### Toolbar placement

The two buttons are placed **after** the existing `deleteBtn_` and **before** the right-side stretch, in the order `]` then `[`:

```
[ 全选 ] [ 取消选择 ] [ 撤销 ] [ 恢复 ]   [ + ] [ ✂ ] [ 🗑 ] [ ] 右对齐 [ [ 左对齐 ]   ...   [ 吸附 ] [ 自适应 ] [ - ] [====] [ + ]
```

### Icons (new files)

- `resources/icons/trim-right.svg` — a `]` shape
- `resources/icons/trim-left.svg` — a `[` shape

Both are 24×24, `stroke="#d1d5db"`, `stroke-width="2"`, `stroke-linecap="round"`, `stroke-linejoin="round"`, matching the existing `delete-subtitle.svg` style. Registered in `resources/resources.qrc` alongside the other timeline icons.

### Tooltips and shortcuts

- `trimRightBtn_` → tooltip `"右对齐 (])"`, shortcut id `timeline_trim_right`, default `Qt::Key_BracketRight`.
- `trimLeftBtn_` → tooltip `"左对齐 ([)"`, shortcut id `timeline_trim_left`, default `Qt::Key_BracketLeft`.

### Button enable state (dynamic)

Computed in `updateToolbarStates()`:

- `]` enabled iff: containing subtitle exists **OR** any subtitle has `endMs <= playhead` (i.e., one to the left).
- `[` enabled iff: containing subtitle exists **OR** any subtitle has `startMs > playhead` (i.e., one to the right).
- When `track_` is null or items is empty → both disabled.

## Files to change

1. `resources/icons/trim-right.svg` (new)
2. `resources/icons/trim-left.svg` (new)
3. `resources/resources.qrc` — register the two new icons
4. `include/TimelinePanel.h` — add `QToolButton *trimRightBtn_`, `QToolButton *trimLeftBtn_`; declare private `void trimSubtitleEdgeToPlayhead(bool trimStart)`
5. `src/TimelinePanel.cpp`:
   - Constructor: create the two buttons via the existing `createToolBtn` lambda after `deleteBtn_`; install `ToolTipEventFilter`; connect `clicked` to `trimSubtitleEdgeToPlayhead(true/false)`
   - `updateIcons()`: set icons from the new SVGs via `renderSvgToIcon`
   - `retranslateUi()`: set base tooltips
   - `updateShortcuts()`: wire `timeline_trim_right` (default `]`) and `timeline_trim_left` (default `[`) via the existing `applyShortcut` lambda
   - `updateToolbarStates()`: dynamic enable/disable per the rules above
   - Add `trimSubtitleEdgeToPlayhead(bool)` implementation
6. `src/ConfigDialog.cpp` — add `timeline_trim_right` and `timeline_trim_left` entries to `kShortcutDefs` so they appear in Settings → Shortcuts

## Out of scope (YAGNI)

- No new methods on `SubtitleTrack` — reuse `updateItem()`.
- No changes to `SubtitleItem` or the data model.
- No ripple effects to other panels (list/preview are read-only consumers of `startMs`/`endMs`).
- No visual animation or status-bar feedback — the timeline redraws naturally on `dataChanged()`.

## Testing

Manual verification in the running app:

1. Place the playhead inside a subtitle; press `]`. Its `startMs` jumps to the playhead; `Ctrl+Z` reverts.
2. Playhead inside a subtitle; press `[`. Its `endMs` jumps to the playhead; `Ctrl+Z` reverts.
3. Playhead in a gap (between two subtitles); press `]`. The end of the nearest left subtitle jumps to the playhead.
4. Playhead in a gap; press `[`. The start of the nearest right subtitle jumps to the playhead.
5. Playhead at timeline start (0) with subtitles present → `[` enabled, `]` disabled.
6. Playhead at timeline end → `]` enabled, `[` disabled.
7. Empty track → both buttons disabled.
8. Toolbar button enabled-state matches the playhead position rules above.
9. Rebind the shortcut in Settings → keyboard still triggers the new button.
10. Reload a project → button enable state still correct.
