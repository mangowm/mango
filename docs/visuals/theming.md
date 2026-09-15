---
title: Theming
description: Customize the visual appearance of borders, colors, and the cursor.
---

## Dimensions

Control the sizing of window borders and gaps.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `deco.border.width` | `4` | Border width in pixels. |
| `deco.gap.inner.horizontal` | `5` | Horizontal inner gap (between windows). |
| `deco.gap.inner.vertical` | `5` | Vertical inner gap. |
| `deco.gap.outer.horizontal` | `10` | Horizontal outer gap (between windows and screen edges). |
| `deco.gap.outer.vertical` | `10` | Vertical outer gap. |

## Colors

Colors are defined in `0xRRGGBBAA` hex format.

```ini
# Background color of the root window
deco.color.root=0x323232ff

# Inactive window border
deco.color.border=0x444444ff

# Drop shadow when dragging windows
deco.color.drop=0x8FBA7C55

# Split window border color in manual dwindle layout
deco.color.split=0xEB441EFF

# Active window border
deco.color.focus=0xc66b25ff

# Urgent window border (alerts)
deco.color.urgent=0xad401fff
```

### State-Specific Colors

You can also color-code windows based on their state:

| State | Config Key | Default Color |
| :--- | :--- | :--- |
| Maximized | `deco.color.maximize` | `0x89aa61ff` |
| Scratchpad | `deco.color.scratchpad` | `0x516c93ff` |
| Global | `deco.color.global` | `0xb153a7ff` |
| Overlay | `deco.color.overlay` | `0x14a57cff` |

> **Tip:** For scratchpad window sizing, see [Scratchpad](/docs/window-management/scratchpad) configuration.

### Overview Jump Mode
| Setting | Default | Description |
| :--- | :--- | :--- |
| `deco.jumplabel.fg.color` | `0xc4939dff` | text color. |
| `deco.jumplabel.bg.color` | `0x201b14ff` | background color.|
| `deco.jumplabel.focus.fg.color` | `0x201b14ff` |  text color for focus. |
| `deco.jumplabel.focus.bg.color` | `0xc4939dff` | background color for focus.|
| `deco.jumplabel.border.color` | `0x8BAA9Bff` | border color.|
| `deco.jumplabel.border.width` | `4` | border width.|
| `deco.jumplabel.corner.radius` | `5` | corner radius.|
| `deco.jumplabel.padding.x` | `10` | horizontal padding.|
| `deco.jumplabel.padding.y` | `10` | vertical padding.|
| `deco.jumplabel.font.desc` | `monospace Bold 16` | font set.|

### Tab Bar For Monocle Layout
| Setting | Default | Description |
| :--- | :--- | :--- |
| `deco.groupbar.height` | `50` | Height of the tab bar for monocle layout. |
| `deco.groupbar.fg.color` | `0xc4939dff` | text color.
| `deco.groupbar.bg.color` | `0x201b14ff` | background color.|
| `deco.groupbar.focus.fg.color` | `0x201b14ff` | text color for focus. |
| `deco.groupbar.focus.bg.color` | `0xc4939dff` | background color for focus.|
| `deco.groupbar.border.color` | `0x8BAA9Bff` | border color.|
| `deco.groupbar.border.width` | `4` | border width.|
| `deco.groupbar.corner.radius` | `5` | corner radius.|
| `deco.groupbar.padding.x` | `0` | horizontal padding.|
| `deco.groupbar.padding.y` | `0` | vertical padding.|
| `deco.groupbar.font.desc` | `monospace Bold 16` | font set.|

## Borders

Control the appearance of window borders.

## Cursor Theme

Set the size and theme of your mouse cursor.

```ini
cursor.size=24
cursor.theme=Adwaita
```