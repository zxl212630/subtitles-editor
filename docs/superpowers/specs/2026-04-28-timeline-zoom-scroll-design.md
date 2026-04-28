# Timeline Zoom & Scroll Design

**Date:** 2026-04-28
**Status:** Approved

## Overview

Enable the `TimelinePanel` to dynamically adapt to imported video length by supporting horizontal zoom (Ctrl + wheel) and horizontal scroll (wheel), along with a visible horizontal scrollbar.

## Requirements

- Replace fixed `PIXELS_PER_SECOND = 100` with a runtime zoom factor (`pixelsPerSecond_`).
- Mouse wheel scrolls horizontally.
- Ctrl + mouse wheel zooms in/out around the cursor.
- Display a horizontal scrollbar at the bottom of the timeline.
- Ruler tick density must adapt to zoom level to prevent label overlap.
- Subtitle bars and video clip length must scale automatically with the zoom factor.
- All existing public interfaces remain unchanged.

## State Variables

| Variable | Type | Initial Value | Range | Description |
|----------|------|---------------|-------|-------------|
| `pixelsPerSecond_` | `double` | `100.0` | `[10.0, 1000.0]` | Zoom scale in pixels per second |
| `scrollOffsetX_` | `int` | `0` | `[0, contentWidth - viewportWidth]` | Horizontal scroll offset in pixels |
| `hScrollBar_` | `QScrollBar*` | `nullptr` | — | Bottom horizontal scrollbar widget |

`contentWidth = totalDurationMs_ * pixelsPerSecond_ / 1000`.

## Coordinate System

All rendering uses these helpers:

```cpp
int timeToX(qint64 ms) const {
    return TRACK_HEAD_WIDTH + static_cast<int>(ms * pixelsPerSecond_ / 1000.0) - scrollOffsetX_;
}

qint64 xToTime(int x) const {
    if (x < TRACK_HEAD_WIDTH) return 0;
    int relX = x - TRACK_HEAD_WIDTH + scrollOffsetX_;
    return static_cast<qint64>(relX * 1000.0 / pixelsPerSecond_);
}
```

## Ruler Tick Adaptation

To keep labels readable and avoid overlap, the major tick interval adapts to `pixelsPerSecond_`:

| `pixelsPerSecond_` | Major Tick Interval | Minor Tick Interval |
|--------------------|---------------------|---------------------|
| `≥ 200`            | 1 s                 | 0.5 s               |
| `≥ 100`            | 1 s                 | 0.5 s               |
| `≥ 50`             | 2 s                 | 1 s                 |
| `≥ 20`             | 5 s                 | 1 s                 |
| `< 20`             | 10 s                | 5 s                 |

Label format: `HH:MM:SS` when total duration ≥ 1 hour, otherwise `MM:SS`.

## Scrollbar Layout

`TimelinePanel` uses a vertical `QVBoxLayout`:
- Top: `QWidget` (paint area, stretch 1)
- Bottom: `QScrollBar(Qt::Horizontal)` (fixed height, styled to match dark theme)

The scrollbar range and page step are updated on:
- `setTotalDuration`
- `wheelEvent` (zoom)
- `resizeEvent`

## Wheel Event Handling

Override `wheelEvent(QWheelEvent* event)`:

1. **Zoom (Ctrl pressed)**
   - Compute time under cursor before zoom: `t = xToTime(event->pos().x())`.
   - Adjust `pixelsPerSecond_` by factor `1.25` (zoom in) or `0.8` (zoom out).
   - Clamp to `[10.0, 1000.0]`.
   - Recompute `scrollOffsetX_` so that `timeToX(t)` remains at the same screen X.
   - Update scrollbar range and value.
   - Call `update()`.

2. **Scroll (no Ctrl)**
   - `scrollOffsetX_ -= event->angleDelta().y()` (or `.x()` for horizontal wheels).
   - Clamp to `[0, maxContentOffset]`.
   - Sync scrollbar value.
   - Call `update()`.

## Mouse Click

`mousePressEvent` uses `xToTime()` unchanged. The rest of the logic (seek, emit `timeClicked`) stays identical.

## Boundary & Resize Handling

- On `setTotalDuration(ms)`: recompute scrollbar max; clamp `scrollOffsetX_`.
- On `resizeEvent`: recompute scrollbar max and page step; clamp `scrollOffsetX_`.
- Ensure `scrollOffsetX_` never exceeds `max(0, contentWidth - viewportWidth)`.

## Public Interface Compatibility

No changes to existing public API:

```cpp
void setTrack(SubtitleTrack *track);
void setCurrentTime(qint64 ms);
void setTotalDuration(qint64 ms);
```

Signals (`timeClicked`, `itemSelected`, `asrFailed`, `asrSucceeded`, `mediaFileDropped`) remain unchanged. `AppWindow` and other panels require zero modifications.

## UI Styling

Scrollbar style must match the existing dark theme:

```qss
QScrollBar:horizontal {
    background: #1e1e1e;
    height: 14px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal {
    background: #4a4a4a;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::handle:horizontal:hover {
    background: #5a5a5a;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0px;
    border: none;
}
```

## Files to Modify

- `include/TimelinePanel.h`
- `src/TimelinePanel.cpp`

No other files require changes.
