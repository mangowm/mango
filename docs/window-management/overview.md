---
title: Overview
description: Configure overview, jump mode, overcircle, and the window switcher.
---

Mango has several related ways to inspect or switch windows: the standard
overview, workspace overview, jump mode, overcircle, and the switcher. They
share some appearance settings, but each mode has its own entry command and
interaction model.

| Mode | Layout | Primary command |
| :--- | :--- | :--- |
| Overview | Packed cards in the normal overview grid | `toggleoverview` |
| Workspace overview | Offscreen workspace previews; draggable windows | `toggleworkspaceoverview` |
| Jump mode | Overview grid with one-key jump hints | `togglejump` |
| Overcircle | Focused card in the center with other windows on both sides | `overcircle` |
| Switcher | Centered thumbnail panel | `switcher` |

There is also a separate workspace overview. It renders each tag to an offscreen
workspace texture and lets windows be dragged between the visible workspace
previews.

The keybindings below are examples; replace the shortcuts with ones that are
free in your configuration. For the complete dispatcher reference, see
[Keybindings](/docs/bindings/keys).

## Overview

The standard overview packs every window on the current monitor into a grid.
By default it includes windows from every tag. Pass `1` to
`toggleoverview` to include only windows on the current tagset.

### Keybindings

```ini
# Toggle the overview for all tags
bind=SUPER+ALT,o,toggleoverview,

# Toggle the overview for the current tagset only
bind=SUPER+ALT+SHIFT,o,toggleoverview,1

# Enter or leave without toggling
bind=SUPER+ALT,Return,enteroverview,
bind=SUPER+ALT+Escape,leaveoverview,
```

| Command | Parameter | Description |
| :--- | :--- | :--- |
| `toggleoverview` | `[1]` | Toggle the standard overview. Use `1` to restrict it to the current tagset. |
| `enteroverview` | - | Enter the standard overview. |
| `leaveoverview` | - | Leave the overview. |

The default configuration also provides four-finger swipe gestures for entering
and leaving the overview:

```ini
gesturebind=none,up,4,enteroverview
gesturebind=none,down,4,leaveoverview
```

### Configuration

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `overview_gap_inner` | integer | `5` | Gap between cards. Range: 0–1000. |
| `overview_gap_outer` | integer | `30` | Gap between the card grid and the monitor edges. Range: 0–1000. |
| `enable_hotarea` | integer | `0` | Enter the standard overview when the pointer enters the configured corner (`0`: disabled, `1`: enabled). |
| `hotarea_size` | integer | `10` | Width and height of the hot-area trigger in pixels. Range: 1–1000. |
| `hotarea_corner` | integer | `2` | Trigger corner: `0` top-left, `1` top-right, `2` bottom-left, `3` bottom-right. |
| `hotarea_disable_on_fullscreen` | integer | `1` | Do not trigger the hot area while a fullscreen window is focused (`0`: disabled, `1`: enabled). |

```ini
overview_gap_inner=5
overview_gap_outer=30

enable_hotarea=1
hotarea_size=10
hotarea_corner=2
hotarea_disable_on_fullscreen=1
```

## Jump Mode

Jump mode displays the standard overview grid and places a character hint on
each visible window. Press a hint to focus that window and leave the overview.
Press `Escape` to leave without selecting a window.

Jump mode has no shipped default binding.

### Keybindings

```ini
# Toggle the overview and its jump hints
bind=SUPER+ALT,Tab,togglejump,
```

| Interaction | Description |
| :--- | :--- |
| Hint key | Focus the window marked with that hint and close the overview. |
| `Escape` | Close jump mode without changing focus. |

| Command | Parameter | Description |
| :--- | :--- | :--- |
| `togglejump` | - | Enter the overview in jump mode; if an overview is already open, close it. |

### Configuration

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `jump_labels` | string | `HJKLASDFGQWERTYUIOPZXCVBNM` | Ordered characters assigned as window hints. Hints are assigned only to visible windows, so the string length limits how many windows receive one. Label matching uses the configured keyboard layouts and falls back to `us`. |

```ini
# Assign hints in this order
jump_labels=HJKLASDFGQWERTYUIOPZXCVBNM
```

Hint colors, border, padding, corner radius, and font use the
`jump_label_decorate_*` settings. See
[Overview Jump Mode](/docs/visuals/theming#overview-jump-mode) for the complete
list.

## Workspace Overview

The workspace overview is independent from the standard overview. Its scope is
selected when opening it: `all` creates one preview for every tag up to
`tag_num`, including empty workspaces, while `occupied` shows only tags that
contain windows. In `occupied` mode, tags seen at open or selected later with a
`view_*` dispatch remain as previews even after their workspaces become empty.
The default `all` and `occupied` modes use a fitted preview grid. The
`scroll_all` and `scroll_occupied` modes arrange workspaces as a scrolling strip,
similar to the scroller layout with workspaces as its items.
The strip can scroll horizontally or vertically according to
`workspace_overview_scroll_direction`; the selected workspace is centered when
possible. Scrolling and workspace-selection movement use
`animation_duration_workspace_overview_move` and
`animation_curve_workspace_overview_move`. The same move animation is used for
workspace layout changes; there is no separate layout animation.

Wheel scrolling uses the
[workspace overview scroll deceleration options](/docs/configuration/input#workspace-overview-scroll-deceleration).

Each tag preview mirrors its workspace, including the background wallpaper and
layer-shell surfaces on the `background` and `bottom` layers, such as status
panels. Layer-shell surfaces on the `top` and `overlay` layers stay at their
normal on-screen position, render directly above the workspace overview, and
continue to receive pointer input. Windows use their workspace geometry, so
partially visible windows in scrolling layouts remain partially visible in the
preview. A window that belongs to multiple tags appears in every matching
preview. Window and layer-surface commits refresh the affected previews, and
mapping or closing a window rebuilds the workspace previews automatically.

```ini
# Shipped default binding: all workspaces
bind=SUPER,Tab,toggleworkspaceoverview,all
# Optional binding: occupied workspaces only
bind=SUPER+SHIFT,Tab,toggleworkspaceoverview,occupied
# Optional binding: all workspaces in a scrolling strip
bind=SUPER+ALT,Tab,toggleworkspaceoverview,scroll_all
bind=SUPER+ALT,Escape,leaveworkspaceoverview,
```

| Command | Description |
| :--- | :--- |
| `toggleworkspaceoverview` | Toggle the workspace overview. `all` includes empty tags, `occupied` shows occupied tags only, and `scroll_all` / `scroll_occupied` use the corresponding scrolling layout. |
| `enterworkspaceoverview` | Enter the workspace overview with `all`, `occupied`, `scroll_all`, or `scroll_occupied`. |
| `leaveworkspaceoverview` | Leave the workspace overview without selecting a workspace. |

`toggleworkspaceoverview` and `enterworkspaceoverview` can reverse an in-progress
exit directly from its current zoom position. Toggling during the enter
animation similarly reverses back toward the selected workspace.

Pointer interaction:

| Action | Result |
| :--- | :--- |
| Left click a workspace | Switch to that workspace and leave the overview. |
| Left click a window preview | Switch to its workspace, focus the window, and leave the overview. |
| Drag a window within a workspace | Change its tiling order or floating position. |
| Drag a window to another workspace | Move its tag membership to the drop workspace. |
| Right click a window preview | Close the window. |
| Mouse wheel or horizontal scroll | Move to the previous or next workspace and center it in the scrolling strip. |

When a dragged window is over a tiled target, the preview shows the same
mouse-position-based insert zone used by ordinary drag-to-tile. Dropping uses
that zone's layout-specific insertion rule. The borderless indicator uses the
same `drop_color` style as normal layouts.

While dragging a window in scrolling mode, entering the edge in the configured
scroll direction starts a slow automatic scroll. Horizontal mode uses the left
and right edges; vertical mode uses the top and bottom edges.

When a window drag starts, the source workspace immediately lays out again
without that window, so the remaining windows fill the released space before
the drop is committed.

Keyboard interaction:

| Key | Result |
| :--- | :--- |
| Arrow keys or `h`/`j`/`k`/`l` | Select the adjacent workspace preview. |
| Enter | Switch to the selected workspace and leave the overview. |
| Escape | Leave without changing the active workspace. |

While dragging, holding Shift adds the destination tag without removing the
source tag. Holding Ctrl replaces all of the window's tags with the destination
tag. A plain drag removes only the tag represented by the preview that the drag
started from.

With `occupied` scope, kept previews remain empty; otherwise the overview closes
when every workspace becomes empty. `all` scope keeps all previews.

### Configuration

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `workspace_overview_border_width` | integer | `2` | Width of each workspace preview border and its inset around the preview texture. Range: 0–100. |
| `workspace_overview_gap` | integer | `5` | Gap between workspace preview cards. Range: 0–1000. |
| `workspace_overview_drag_threshold` | integer | `5` | Pointer distance required before a click becomes a window drag. Range: 0–1000. |
| `workspace_overview_occupied_keep_seen_tags` | integer | `1` | Keep tags seen at open or through `view_*` while the overview is open. Kept previews remain even if empty. Range: 0–1. |
| `workspace_overview_scroll_direction` | string | `vertical` | Scrolling workspace strip direction: `horizontal` or `vertical`. |
| `workspace_overview_scroll_ratio` | float | `0.8` | Maximum workspace preview size relative to the monitor in scrolling mode. Range: 0.1–1.0. |
| `workspace_overview_drag_scroll_speed` | float | `1.5` | Automatic edge-scroll speed while dragging a window, in workspace strides per second. Range: 0.1–10.0. |
| `workspace_overview_focused_dim_opacity` | float | `0.0` | Black overlay opacity on the selected workspace preview. Range: 0–1. |
| `workspace_overview_unfocused_dim_opacity` | float | `0.2` | Black overlay opacity on unselected workspace previews. Range: 0–1. |
| `animation_duration_workspace_overview_open` | integer | `260` | Enter animation duration in milliseconds. Range: 1–50000. |
| `animation_duration_workspace_overview_close` | integer | `260` | Exit animation duration in milliseconds. Range: 1–50000. |
| `animation_duration_workspace_overview_move` | integer | `382` | Workspace move animation duration in milliseconds. Range: 1–50000. |
| `animation_curve_workspace_overview_open` | string | `0.333333,1.0,0.666667,1.0` | Enter animation. Accepts a named easing or cubic-bezier value. |
| `animation_curve_workspace_overview_close` | string | `0.333333,1.0,0.666667,1.0` | Exit animation. Accepts a named easing or cubic-bezier value. |
| `animation_curve_workspace_overview_move` | string | `0.26,0.99,0.26,1.0` | Workspace scrolling and layout-change curve. Accepts a named easing or cubic-bezier value. |

```ini
workspace_overview_border_width=2
workspace_overview_gap=5
workspace_overview_drag_threshold=5
workspace_overview_occupied_keep_seen_tags=1
workspace_overview_scroll_direction=vertical
workspace_overview_scroll_ratio=0.8
workspace_overview_drag_scroll_speed=1.5

# Keep the selected workspace clear and dim the other previews
workspace_overview_focused_dim_opacity=0.0
workspace_overview_unfocused_dim_opacity=0.2
animation_duration_workspace_overview_open=260
animation_duration_workspace_overview_close=260
animation_duration_workspace_overview_move=382
animation_curve_workspace_overview_open=0.333333,1.0,0.666667,1.0
animation_curve_workspace_overview_close=0.333333,1.0,0.666667,1.0
animation_curve_workspace_overview_move=0.26,0.99,0.26,1.0
```

## Overcircle

Overcircle keeps the focused window in a centered column and distributes the
remaining windows across columns on its left and right. Repeated use of the same
binding cycles focus around the current monitor.

`next` and `prev` show windows from all tags. `current_next` and
`current_prev` restrict the overview to the current tagset. Releasing a modifier
key exits overcircle.

### Keybindings

```ini
# Shipped default binding
bind=ALT,Tab,overcircle,next

# Cycle forward/backward through all tags on the current monitor
bind=SUPER+ALT,j,overcircle,next
bind=SUPER+ALT+SHIFT,j,overcircle,prev

# Cycle forward/backward through the current tagset only
bind=SUPER+CTRL,j,overcircle,current_next
bind=SUPER+CTRL+SHIFT,j,overcircle,current_prev
```

| Command | Parameter | Description |
| :--- | :--- | :--- |
| `overcircle` | `next` | Enter or cycle forward through every window on the current monitor. |
| `overcircle` | `prev` | Enter or cycle backward through every window on the current monitor. |
| `overcircle` | `current_next` | Enter or cycle forward through the current tagset only. |
| `overcircle` | `current_prev` | Enter or cycle backward through the current tagset only. |

### Configuration

Overcircle uses the same inner and outer overview gaps as the standard
overview, plus settings for the centered column and title message bar.

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `overview_gap_inner` | integer | `5` | Gap between cards in the center and side columns. Range: 0–1000. |
| `overview_gap_outer` | integer | `30` | Gap around the overcircle layout. Range: 0–1000. |
| `overcircle_center_ratio` | float | `0.5` | Width of the centered focused window as a fraction of the usable screen width. Range: 0.1–0.9. |
| `title_message_bar_height` | integer | `52` | Height of the focused window's title message bar. Set to `0` to disable it. |
| `title_message_bar_gap` | integer | `20` | Gap between the focused card and its title message bar. |

```ini
overview_gap_inner=5
overview_gap_outer=30
overcircle_center_ratio=0.5
title_message_bar_height=52
title_message_bar_gap=20
```

The title message bar shows the window title on the first line and its app id
on the second. Its decoration is shared with the switcher; see
[Title Message Bar](/docs/visuals/theming#title-message-bar).

## Switcher

The switcher opens a centered panel of live window thumbnails. Use it to cycle
through the current tag, all tags on the current monitor, or all windows on all
monitors. Releasing any modifier key commits the highlighted window.

The switcher cannot open while any monitor is showing an overview, including
jump mode and overcircle.

### Keybindings

```ini
# Current tag
bind=ALT,grave,switcher,next
bind=ALT+SHIFT,grave,switcher,prev

# All tags on the current monitor
bind=SUPER+ALT,grave,switcher,all_tag_next
bind=SUPER+ALT+SHIFT,grave,switcher,all_tag_prev

# All monitors and tags
bind=SUPER+CTRL+ALT,grave,switcher,all_next
bind=SUPER+CTRL+ALT+SHIFT,grave,switcher,all_prev
```

| Command | Parameter | Description |
| :--- | :--- | :--- |
| `switcher` | `next` / `prev` | Open or cycle through windows on the current tag. |
| `switcher` | `all_tag_next` / `all_tag_prev` | Open or cycle through every window on the current monitor. |
| `switcher` | `all_next` / `all_prev` | Open or cycle through every window on every monitor. |

### Configuration

| Option | Default | Description |
| :--- | :--- | :--- |
| `switcher_delay` | `150` | Milliseconds to wait before the panel and title bar appear (`0` shows immediately). Quick taps that commit sooner never show the panel. |

The switcher has no dedicated size or layout options. Its focused thumbnail
frame uses `focus_color`, other thumbnail frames use `border_color`, and it
shares the title message bar with overcircle.

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `title_message_bar_height` | integer | `52` | Height of the highlighted window's title message bar. Set to `0` to disable it. |
| `title_message_bar_gap` | integer | `20` | Gap between the switcher panel and its title message bar. |
| `focus_color` | color | `0xc66b25ff` | Frame color of the highlighted thumbnail. |
| `border_color` | color | `0x444444ff` | Frame color of the other thumbnails. |

```ini
title_message_bar_height=52
title_message_bar_gap=20
focus_color=0xc66b25ff
border_color=0x444444ff
```

## Mouse Interaction

The overview-based modes and the switcher accept direct pointer input:

| Mode | Left click | Right click |
| :--- | :--- | :--- |
| Overview, jump mode, overcircle | Focus the clicked window and leave the overview. | Close the clicked window. |
| Switcher | Commit the clicked thumbnail. | Close the clicked window. |
| Switcher panel background | Close the switcher. | Close the switcher. |
