---
title: Theming
description: Customize the visual appearance of borders, colors, and the cursor.
---

## Dimensions

Control the sizing of window borders and gaps.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `borderpx` | `4` | Border width in pixels. |
| `gappih` | `5` | Horizontal inner gap (between windows). |
| `gappiv` | `5` | Vertical inner gap. |
| `gappoh` | `10` | Horizontal outer gap (between windows and screen edges). |
| `gappov` | `10` | Vertical outer gap. |

## Colors

Colors are defined in `0xRRGGBBAA` hex format.

```ini
# Background color of the root window
rootcolor=0x323232ff

# Inactive window border
bordercolor=0x444444ff

# Drop shadow when dragging windows
dropcolor=0x8FBA7C55

# Active window border
focuscolor=0xc66b25ff

# Urgent window border (alerts)
urgentcolor=0xad401fff
```

### State-Specific Colors

You can also color-code windows based on their state:

| State | Config Key | Default Color |
| :--- | :--- | :--- |
| Maximized | `maximizescreencolor` | `0x89aa61ff` |
| Scratchpad | `scratchpadcolor` | `0x516c93ff` |
| Global | `globalcolor` | `0xb153a7ff` |
| Overlay | `overlaycolor` | `0x14a57cff` |

> **Tip:** For scratchpad window sizing, see [Scratchpad](/docs/window-management/scratchpad) configuration.

## Cursor Theme

Set the size and theme of your mouse cursor.

```ini
cursor_size=24
cursor_theme=Adwaita
```
