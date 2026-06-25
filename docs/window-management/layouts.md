---
title: Layouts
description: Configure and switch between different window layouts.
---

## Supported Layouts

mangowm supports a variety of layouts that can be assigned per tag.

- `tile`
- `scroller`
- `monocle`
- `grid`
- `deck`
- `center_tile`
- `vertical_tile`
- `right_tile`
- `vertical_scroller`
- `vertical_grid`
- `vertical_deck`
- `dwindle`
- `fair`
- `vertical_fair`

---

## Scroller Layout

The Scroller layout positions windows in a scrollable strip, similar to PaperWM.

### Configuration

| Setting | Default | Description |
| :--- | :--- | :--- |
| `scroller_structs` | `20` | Width reserved on sides when window ratio is 1. |
| `scroller_default_proportion` | `0.9` | Default width proportion for new windows. |
| `scroller_focus_center` | `0` | Always center the focused window (1 = enable). |
| `scroller_prefer_center` | `0` | Center focused window only if it was outside the view. |
| `scroller_prefer_overspread` | `1` | Allow windows to overspread when there's extra space. |
| `edge_scroller_pointer_focus` | `1` | Focus windows even if partially off-screen. |
| `edge_scroller_focus_allow_speed` | `0.0` | Allow pointer focus to happen if the pointer moves at a speed greater than this value. |
| `scroller_proportion_preset` | `0.5,0.8,1.0` | Presets for cycling window widths. |
| `scroller_ignore_proportion_single` | `1` | Ignore proportion adjustments for single windows. |
| `scroller_default_proportion_single` | `1.0` | Default proportion for single windows in scroller. **Requires `scroller_ignore_proportion_single=0` to take effect.** |

> **Warning:** `scroller_prefer_overspread`, `scroller_focus_center`, and `scroller_prefer_center` interact with each other. Their priority order is:
>
> **scroller_prefer_overspread > scroller_focus_center > scroller_prefer_center**
>
> To ensure a lower-priority setting takes effect, you must set all higher-priority options to `0`.

```ini
# Example scroller configuration
scroller_structs=20
scroller_default_proportion=0.9
scroller_focus_center=0
scroller_prefer_center=0
scroller_prefer_overspread=1
edge_scroller_pointer_focus=1
edge_scroller_focus_allow_speed=0.0
scroller_default_proportion_single=1.0
scroller_proportion_preset=0.5,0.8,1.0
```

---

## Master-Stack Layouts

These settings apply to layouts like `tile` and `center_tile`.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `new_is_master` | `1` | New windows become the master window. |
| `default_mfact` | `0.55` | The split ratio between master and stack areas. |
| `default_nmaster` | `1` | Number of allowed master windows. |
| `smartgaps` | `0` | Disable gaps when only one window is present. |
| `center_master_overspread` | `0` | (Center Tile) Master spreads across screen if no stack exists. |
| `center_when_single_stack` | `1` | (Center Tile) Center master when only one stack window exists. |

```ini
# Example master-stack configuration
new_is_master=1
smartgaps=0
default_mfact=0.55
default_nmaster=1
```

---

## Dwindle Layout

The Dwindle layout arranges windows as a binary tree of recursive splits. Each new window splits the focused window's container, producing a spiral-like tiling.

### Configuration

| Setting | Default | Description |
| :--- | :--- | :--- |
| `dwindle_split_ratio` | `0.5` | Ratio used for new splits (`0.05`–`0.95`). |
| `dwindle_smart_split` | `0` | Pick the split axis from the cursor's position inside the focused window. The new window appears on the cursor's side. |
| `dwindle_hsplit` | `1` | Side-by-side splits: where the new window goes. `0` = follow cursor, `1` = right, `2` = left. |
| `dwindle_vsplit` | `1` | Top/bottom splits: where the new window goes. `0` = follow cursor, `1` = below, `2` = above. |
| `dwindle_preserve_split` | `0` | Keep the sibling's split orientation when a window is closed. |
| `dwindle_smart_resize` | `0` | When dragging to resize, move the split toward the cursor regardless of which side was grabbed. |
| `dwindle_drop_simple_split` | `1` | Drag-to-tile drop preview. `1` = 2-zone preview matching `dwindle_split_ratio`, `0` = 4-quadrant preview. |
| `dwindle_manual_split` | `0` | Manually split windows mode. |

```ini
# Example dwindle configuration
dwindle_split_ratio=0.5
dwindle_smart_split=0
dwindle_hsplit=0
dwindle_vsplit=0
dwindle_preserve_split=0
dwindle_smart_resize=0
dwindle_drop_simple_split=1
```

---

## Monocle Layout

The Monocle layout is similar to the Center Tile layout, but it places windows in a single column instead of a row.

### Configuration

| Setting | Default | Description |
| :--- | :--- | :--- |
| `monocle_new_in_group` | `0` | New windows are auto grouped in the focused window. |

---

## Switching Layouts
| Setting | Default | Description |
| :--- | :--- | :--- |
| `circle_layout` | - | A comma-separated list of layouts `switch_layout` cycles through,the value sample:`tile,scroller`. |

You can switch layouts dynamically or set a default for specific tags using [Tag Rules](/docs/window-management/rules#tag-rules).

**Keybinding Examples:**

```ini
# Cycle through layouts
circle_layout=grid,scroller,tile
bind=SUPER,n,switch_layout

# Set specific layout
bind=SUPER,t,setlayout,tile
bind=SUPER,s,setlayout,scroller
```
