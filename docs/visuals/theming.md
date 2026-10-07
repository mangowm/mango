---
title: Theming
description: Customize the visual appearance of borders, colors, and the cursor.
---

## Dimensions

Control the sizing of window borders and gaps.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `border_px` | `4` | Border width in pixels. |
| `gap_inner_horizontal` | `5` | Horizontal inner gap (between windows). |
| `gap_inner_vertical` | `5` | Vertical inner gap. |
| `gap_outer_horizontal` | `10` | Horizontal outer gap (between windows and screen edges). |
| `gap_outer_vertical` | `10` | Vertical outer gap. |

## Colors

Colors are defined in `0xRRGGBBAA` hex format.

```ini
# Background color of the root window
root_color=0x323232ff

# Inactive window border
border_color=0x444444ff

# Drop shadow when dragging windows
drop_color=0x8FBA7C55

# Split window border color in manual dwindle layout
split_color=0xEB441EFF

# Active window border
focus_color=0xc66b25ff

# Urgent window border (alerts)
urgent_color=0xad401fff
```

### State-Specific Colors

You can also color-code windows based on their state:

| State | Config Key | Default Color |
| :--- | :--- | :--- |
| Maximized | `maximized_screen_color` | `0x89aa61ff` |
| Scratchpad | `scratchpad_color` | `0x516c93ff` |
| Global | `global_color` | `0xb153a7ff` |
| Overlay | `overlay_color` | `0x14a57cff` |

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

### Group Bar
| Setting | Default | Description |
| :--- | :--- | :--- |
| `always_show_group_bar` | `0` | Show the group bar (with close button) on every window. |
| `group_bar_height` | `33` | Group bar height. |
| `group_bar_close_button_enable` | `1` | Show the close button. |
| `group_bar_button_size` | `16` | Close button size in px. |
| `group_bar_button_margin` | `4` | Close button inset from the right edge in px. |
| `group_bar_button_color` | `0xad401fff` | Close button colour. |
| `group_bar_decorate_fg_color` | `0xc0caf5ff` | text color.
| `group_bar_decorate_bg_color` | `0x1a1b26ff` | background color.|
| `group_bar_decorate_focus_fg_color` | `0x9ece6aff` | text color for focus. |
| `group_bar_decorate_focus_bg_color` | `0x2f3d33ff` | background color for focus.|
| `group_bar_decorate_border_color` | `0x3b4261ff` | border color.|
| `group_bar_decorate_border_width` | `4` | border width.|
| `group_bar_decorate_corner_radius` | `5` | corner radius.|
| `group_bar_decorate_padding_x` | `0` | horizontal padding.|
| `group_bar_decorate_padding_y` | `0` | vertical padding.|
| `group_bar_decorate_font_desc` | `monospace Bold 13` | font set.|

### Auto Tab Bar
| Setting | Default | Description |
| :--- | :--- | :--- |
| `monocle_tab_mode` | `0` | Auto merge stacked windows into a tab in monocle layout. |
| `deck_tab_mode` | `0` | Auto merge stack area windows into a tab in deck layout. |
| `tab_bar_height` | `33` | Height of the auto tab bar. |
| `tab_bar_decorate_fg_color` | `0xc0caf5ff` | text color.
| `tab_bar_decorate_bg_color` | `0x1a1b26ff` | background color.|
| `tab_bar_decorate_focus_fg_color` | `0x7aa2f7ff` | text color for focus. |
| `tab_bar_decorate_focus_bg_color` | `0x2b3550ff` | background color for focus.|
| `tab_bar_decorate_border_color` | `0x3b4261ff` | border color.|
| `tab_bar_decorate_border_width` | `4` | border width.|
| `tab_bar_decorate_corner_radius` | `5` | corner radius.|
| `tab_bar_decorate_padding_x` | `0` | horizontal padding.|
| `tab_bar_decorate_padding_y` | `0` | vertical padding.|
| `tab_bar_decorate_font_desc` | `monospace Bold 13` | font set.|

## Borders

Control the appearance of window borders.

## Cursor Theme

Set the size and theme of your mouse cursor.

```ini
cursor_size=24
cursor_theme=Adwaita
```
