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

# Split window border color in manual dwindle layout
splitcolor=0xEB441EFF

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

### Overview Jump Mode
| Setting | Default | Description |
| :--- | :--- | :--- |
| `jump_label_decorate_fg_color` | `0xc4939dff` | text color. |
| `jump_label_decorate_bg_color` | `0x201b14ff` | background color.|
| `jump_label_decorate_focus_fg_color` | `0x201b14ff` |  text color for focus. |
| `jump_label_decorate_focus_bg_color` | `0xc4939dff` | background color for focus.|
| `jump_label_decorate_border_color` | `0x8BAA9Bff` | border color.|
| `jump_label_decorate_border_width` | `4` | border width.|
| `jump_label_decorate_corner_radius` | `5` | corner radius.|
| `jump_label_decorate_padding_x` | `10` | horizontal padding.|
| `jump_label_decorate_padding_y` | `10` | vertical padding.|
| `jump_label_decorate_font_desc` | `monospace Bold 16` | font set.|

### Tab Bar For Monocle Layout
| Setting | Default | Description |
| :--- | :--- | :--- |
| `tab_bar_height` | `50` | Height of the tab bar for monocle layout. |
| `tab_bar_decorate_fg_color` | `0xc4939dff` | text color.
| `tab_bar_decorate_bg_color` | `0x201b14ff` | background color.|
| `tab_bar_decorate_focus_fg_color` | `0x201b14ff` | text color for focus. |
| `tab_bar_decorate_focus_bg_color` | `0xc4939dff` | background color for focus.|
| `tab_bar_decorate_border_color` | `0x8BAA9Bff` | border color.|
| `tab_bar_decorate_border_width` | `4` | border width.|
| `tab_bar_decorate_corner_radius` | `5` | corner radius.|
| `tab_bar_decorate_padding_x` | `0` | horizontal padding.|
| `tab_bar_decorate_padding_y` | `0` | vertical padding.|
| `tab_bar_decorate_font_desc` | `monospace Bold 16` | font set.|

## Borders

Control the appearance of window borders.

## Cursor Theme

Set the size and theme of your mouse cursor.

```ini
cursor_size=24
cursor_theme=Adwaita
```
