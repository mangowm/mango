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
- `tgmix`

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

## Switching Layouts

You can switch layouts dynamically or set a default for specific tags using [Tag Rules](/docs/window-management/rules#tag-rules).

**Keybinding Examples:**

```ini
# Cycle through layouts
bind=SUPER,n,switch_layout

# Set specific layout
bind=SUPER,t,setlayout,tile
bind=SUPER,s,setlayout,scroller
```
