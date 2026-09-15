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
| `layout.scroller.structs` | `20` | Width reserved on sides when window ratio is 1. |
| `layout.scroller.default.proportion` | `0.9` | Default width proportion for new windows. |
| `layout.scroller.focus.center` | `0` | Always center the focused window (1 = enable). |
| `layout.scroller.prefer.center` | `0` | Center focused window only if it was outside the view. |
| `layout.scroller.prefer.overspread` | `1` | Allow windows to overspread when there's extra space. |
| `layout.scroller.edge.pointer.focus` | `1` | Focus windows even if partially off-screen. |
| `layout.scroller.edge.focus.allow.speed` | `0.0` | Allow pointer focus to happen if the pointer moves at a speed greater than this value. |
| `layout.scroller.proportion.preset` | `0.5,0.8,1.0` | Presets for cycling window widths. |
| `layout.scroller.ignore.proportion.single` | `1` | Ignore proportion adjustments for single windows. |
| `layout.scroller.default.proportion.single` | `1.0` | Default proportion for single windows in scroller. **Requires `layout.scroller.ignore.proportion.single=0` to take effect.** |

> **Warning:** `layout.scroller.prefer.overspread`, `layout.scroller.focus.center`, and `layout.scroller.prefer.center` interact with each other. Their priority order is:
>
> **layout.scroller.prefer.overspread > layout.scroller.focus.center > layout.scroller.prefer.center**
>
> To ensure a lower-priority setting takes effect, you must set all higher-priority options to `0`.

```ini
# Example scroller configuration
layout.scroller.structs=20
layout.scroller.default.proportion=0.9
layout.scroller.focus.center=0
layout.scroller.prefer.center=0
layout.scroller.prefer.overspread=1
layout.scroller.edge.pointer.focus=1
layout.scroller.edge.focus.allow.speed=0.0
layout.scroller.default.proportion.single=1.0
layout.scroller.proportion.preset=0.5,0.8,1.0
```

---

## Master-Stack Layouts

These settings apply to layouts like `tile` and `center_tile`.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `layout.master.new.is.master` | `1` | New windows become the master window. |
| `layout.master.default.mfact` | `0.55` | The split ratio between master and stack areas. |
| `layout.master.default.nmaster` | `1` | Number of allowed master windows. |
| `layout.master.center.overspread` | `0` | (Center Tile) Master spreads across screen if no stack exists. |
| `layout.master.center.single.stack` | `1` | (Center Tile) Center master when only one stack window exists. |

```ini
# Example master-stack configuration
layout.master.new.is.master=1
deco.gap.smart=0
layout.master.default.mfact=0.55
layout.master.default.nmaster=1
tag.num=9
tag.gather=0
```

---

## Dwindle Layout

The Dwindle layout arranges windows as a binary tree of recursive splits. Each new window splits the focused window's container, producing a spiral-like tiling.

### Configuration

| Setting | Default | Description |
| :--- | :--- | :--- |
| `layout.dwindle.split.ratio` | `0.5` | Ratio used for new splits (`0.05`–`0.95`). |
| `layout.dwindle.smart.split` | `0` | Pick the split axis from the cursor's position inside the focused window. The new window appears on the cursor's side. |
| `layout.dwindle.hsplit` | `1` | Side-by-side splits: where the new window goes. `0` = follow cursor, `1` = right, `2` = left. |
| `layout.dwindle.vsplit` | `1` | Top/bottom splits: where the new window goes. `0` = follow cursor, `1` = below, `2` = above. |
| `layout.dwindle.preserve.split` | `0` | Keep the sibling's split orientation when a window is closed. |
| `layout.dwindle.smart.resize` | `0` | When dragging to resize, move the split toward the cursor regardless of which side was grabbed. |
| `layout.dwindle.drop.simple.split` | `1` | Drag-to-tile drop preview. `1` = 2-zone preview matching `layout.dwindle.split.ratio`, `0` = 4-quadrant preview. |
| `layout.dwindle.manual.split` | `0` | Manually split windows mode. |

```ini
# Example dwindle configuration
layout.dwindle.split.ratio=0.5
layout.dwindle.smart.split=0
layout.dwindle.hsplit=0
layout.dwindle.vsplit=0
layout.dwindle.preserve.split=0
layout.dwindle.smart.resize=0
layout.dwindle.drop.simple.split=1
```

---

## Switching Layouts
| Setting | Default | Description |
| :--- | :--- | :--- |
| `layout.circle.layout` | - | A comma-separated list of layouts `switch_layout` cycles through,the value sample:`tile,scroller`. |

You can switch layouts dynamically or set a default for specific tags using [Tag Rules](/docs/window-management/rules#tag-rules).

**Keybinding Examples:**

```ini
# Cycle through layouts
layout.circle.layout=grid,scroller,tile
bind=SUPER,n,switch_layout

# Set specific layout
bind=SUPER,t,setlayout,tile
bind=SUPER,s,setlayout,scroller
```