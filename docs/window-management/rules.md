---
title: Rules
description: Define behavior for specific windows, tags, and layers.
---

## Window Rules

Window rules allow you to set specific properties (floating, opacity, size, animations, etc.) for applications based on their `app_id` or `title`. You can set all parameters in one line, and if you both set app_id and title, the window will only follow the rules when app_id and title both match.

**Format:**

```ini
# Set window rules that apply to every times when the window is opened
window_rule=Parameter:Values,title:Values
window_rule=Parameter:Values,Parameter:Values,app_id:Values,title:Values

# Set window rules that only apply once when the window is opened
window_rule_once=Parameter:Values,title:Values
window_rule_once=Parameter:Values,Parameter:Values,app_id:Values,title:Values
```

### State & Behavior Parameters

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `app_id` | string | Any | Match by application ID, supports regex |
| `title` | string | Any | Match by window title, supports regex |
| `is_floating` | integer | `0` / `1` | Force floating state |
| `is_fullscreen` | integer | `0` / `1` | Force fullscreen state |
| `is_fake_fullscreen` | integer | `0` / `1` | Force fake-fullscreen state (window stays constrained) |
| `is_global` | integer | `0` / `1` | Open as global window (sticky across tags) |
| `is_overlay` | integer | `0` / `1` | Make it always in top layer |
| `is_open_silent` | integer | `0` / `1` | Open without focus |
| `is_tag_silent` | integer | `0` / `1` | Don't focus if client is not in current view tag |
| `force_fake_maximize` | integer | `0` / `1` (default 1) | The state of client set to fake maximized |
| `ignore_maximize` | integer | `0` / `1` (default 1) | Don't handle maximize request from client |
| `ignore_minimize` | integer | `0` / `1` (default 1) | Don't handle minimize request from client |
| `force_tiled_state` | integer | `0` / `1` | Deceive the window into thinking it is tiling, so it better adheres to assigned dimensions |
| `single_scratchpad` | integer | `0` / `1` (default 1) | Only show one out of named scratchpads or the normal scratchpad |
| `allow_shortcuts_inhibit` | integer | `0` / `1` (default 1) | Allow shortcuts to be inhibited by clients |
| `idle_inhibit_when_focus` | integer | `0` / `1` (default 0) | Automatically keep idle inhibit active when this window is focused |
| `vrr_only_fullscreen` | integer | `0` / `1` (default 0) | VRR only fullscreen,you need to turn `vrr` to `0` in monitor rule first |
| `shield_when_capture` | integer | `0` / `1` | Shield window when captured |
| `force_render` | integer | `0` / `1` | Force render frame even if the window is not visible |
| `activation_bypass` | integer | `0` / `1` | Bypass xdg-activation authentication: activation requests for this window are treated as authenticated, so the normal activation behavior applies regardless of token validity |


### Geometry & Position

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `width` | float | 0-9999 | Window width when it becomes a floating window,if the value below 1, it will be the percentage of the screen width,otherwise it will be the pixel value |
| `height` | float | 0-9999 | Window height when it becomes a floating window,if the value below 1, it will be the percentage of the screen height,otherwise it will be the pixel value |
| `offset_x` | integer | -999-999 | X offset from center (%), 100 is the edge of screen with outer gap |
| `offset_y` | integer | -999-999 | Y offset from center (%), 100 is the edge of screen with outer gap |
| `monitor` | string | Any | Assign to monitor by [monitor spec](/docs/configuration/monitors#monitor-spec-format) (name, make, model, or serial) |
| `tags` | mask | `0-9` / `1\|3\|5` | Assign to specific one tag (use `0` for special workspace overlay) or multiple tags (use `\|` to split multiple tags) |
| `no_force_center` | integer | `0` / `1` | Window does not force center |
| `no_size_hint` | integer | `0` / `1` | Don't use min size and max size for size hints |

### Visuals & Decoration

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `no_blur` | integer | `0` / `1` | Window does not have blur effect |
| `no_border` | integer | `0` / `1` | Remove window border |
| `no_shadow` | integer | `0` / `1` | Not apply shadow |
| `no_radius` | integer | `0` / `1` | Not apply corner radius |
| `no_animation` | integer | `0` / `1` | Not apply animation |
| `focused_opacity` | integer | `0` / `1` | Window focused opacity |
| `unfocused_opacity` | integer | `0` / `1` | Window unfocused opacity |
| `allow_csd` | integer | `0` / `1` | Allow client side decoration |
| `confine_pointer` | integer | `0` / `1` | While this window is focused and visible, force the cursor to stay inside it (does not require the client to use the pointer constraints protocol) |

> **Tip:** For detailed visual effects configuration, see the [Window Effects](/docs/visuals/effects) page for blur, shadows, and opacity settings.

### Layout & Scroller

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `scroller_proportion` | float | 0.1-1.0 | Set scroller proportion |
| `scroller_proportion_single` | float | 0.1-1.0 | Set scroller auto adjust proportion when it is single window |

> **Tip:** For comprehensive layout configuration, see the [Layouts](/docs/window-management/layouts) page for all layout options and detailed settings.

### Animation

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `animation_type_open` | string | zoom, slide, fade, none | Set open animation |
| `animation_type_close` | string | zoom, slide, fade, none | Set close animation |
| `no_fade_in` | integer | `0` / `1` | Window ignores fade-in animation |
| `no_fade_out` | integer | `0` / `1` | Window ignores fade-out animation |

> **Tip:** For detailed animation configuration, see the [Animations](/docs/visuals/animations) page for available types and settings.

### Terminal & Swallowing

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `is_term` | integer | `0` / `1` | A new GUI window will replace the is_term window when it is opened |
| `no_swallow` | integer | `0` / `1` | The window will not replace the is_term window |

### Global & Special Windows

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `global_key_binding` | string | `[mod combination][-][key]` | Global keybinding (only works for Wayland apps) |
| `is_unmanaged_global` | integer | `0` / `1` | Open as unmanaged global window (for desktop pets or camera windows) |
| `is_named_scratchpad` | integer | `0` / `1` | 0: disable, 1: named scratchpad |

> **Tip:** For scratchpad usage, see the [Scratchpad](/docs/window-management/scratchpad) page for detailed configuration examples.

### Performance & Tearing

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `force_tearing` | integer | `0` / `1` | Set window to tearing state, refer to [Tearing](/docs/configuration/monitors#tearing-game-mode) |

### Examples

```ini
# Set specific window size and position
window_rule=width:1000,height:900,app_id:yesplaymusic,title:Demons

# Global keybindings for OBS Studio
window_rule=global_key_binding:ctrl+alt-o,app_id:com.obsproject.Studio
window_rule=global_key_binding:ctrl+alt-n,app_id:com.obsproject.Studio
window_rule=is_open_silent:1,app_id:com.obsproject.Studio

# Force tearing for games
window_rule=force_tearing:1,title:vkcube

# Skip xdg-activation authentication for this app
window_rule=activation_bypass:1,app_id:org.example.App
window_rule=force_tearing:1,title:Counter-Strike 2

# Named scratchpad for file manager
window_rule=is_named_scratchpad:1,width:1280,height:800,app_id:st-yazi

# Custom opacity for specific apps
window_rule=focused_opacity:0.8,app_id:firefox
window_rule=unfocused_opacity:0.6,app_id:foot

# Position windows relative to screen center
window_rule=offset_x:20,offset_y:-30,width:800,height:600,app_id:alacritty

# Send to specific tag and monitor
window_rule=tags:9,monitor:HDMI-A-1,app_id:discord

# Terminal swallowdby setup
window_rule=is_term:1,app_id:st
window_rule=no_swallow:1,app_id:foot

# Disable client-side decorations
window_rule=allow_csd:1,app_id:firefox

# Unmanaged global window (desktop pets, camera)
window_rule=is_unmanaged_global:1,app_id:cheese

# Named scratchpad toggle
bind=alt,h,toggle_named_scratchpad,st-yazi,none,st -c st-yazi -e yazi
```

---

## Tag Rules

You can set all parameters in one line. If only `id` is set, the rule is followed when the id matches. If any of `monitor_name`, `monitor_make`, `monitor_model`, or `monitor_serial` are set, the rule is followed only if **all** of the set monitor fields match.

> **Warning:** Layouts set in tag rules have a higher priority than monitor rule layouts.

**Format:**

```ini
tag_rule=id:Values,Parameter:Values,Parameter:Values
tag_rule=id:Values,monitor_name:eDP-1,Parameter:Values,Parameter:Values
tag_rule=id:Values,monitor_make:xxx,monitor_model:xxx,Parameter:Values
tag_rule=id:*,Parameter:Values
```

> **Tip:** See [Layouts](/docs/window-management/layouts#supported-layouts) for detailed descriptions of each layout type.

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `id` | integer / wildcard | 0-9 / `*` | Match by tag id, 0 means the ~0 tag. Use `*` to match all tags at once |
| `monitor_name` | string | monitor name | Match by monitor name |
| `monitor_make` | string | monitor make | Match by monitor manufacturer |
| `monitor_model` | string | monitor model | Match by monitor model |
| `monitor_serial` | string | monitor serial | Match by monitor serial number |
| `layout_name` | string | layout name | Layout name to set |
| `no_render_border` | integer | `0` / `1` | Disable render border |
| `open_as_floating` | integer | `0` / `1` | New open window will be floating|
| `no_hide` | integer | `0` / `1` | Not hide even if the tag is empty |
| `master_count` | integer | 0, 99 | Number of master windows |
| `master_factor` | float | 0.1–0.9 | Master area factor |
| `scroller_default_proportion` | float | 0.1-1.0 | Set scroller  default proportion. |
| `scroller_default_proportion_single` | float | 0.1-1.0 | Set scroller auto adjust proportion when it is single window(only apply when set `scroller_ignore_proportion_single` to `0`) |
| `scroller_ignore_proportion_single` | integer | `0` / `1` | Ignore scroller single proportion setting. |

### Examples

```ini
# Set layout for all tags at once (equivalent to the two rules below)
tag_rule=id:*,layout_name:scroller

# Set layout for specific tags
tag_rule=id:1,layout_name:scroller
tag_rule=id:2,layout_name:scroller

# Limit to specific monitor
tag_rule=id:1,monitor_name:eDP-1,layout_name:scroller
tag_rule=id:2,monitor_name:eDP-1,layout_name:scroller

# Persistent tags (1-4) with layout assignment
tag_rule=id:1,no_hide:1,layout_name:scroller
tag_rule=id:2,no_hide:1,layout_name:scroller
tag_rule=id:3,monitor_name:eDP-1,no_hide:1,layout_name:scroller
tag_rule=id:4,monitor_name:eDP-1,no_hide:1,layout_name:scroller

# Advanced tag configuration with master layout settings
tag_rule=id:5,layout_name:tile,master_count:2,master_factor:0.6
tag_rule=id:6,monitor_name:HDMI-A-1,layout_name:monocle,no_render_border:1

# set scroller proportion for specific tag
tag_rule=id:1,layout_name:scroller,scroller_default_proportion_single:0.5,scroller_ignore_proportion_single:0,scroller_default_proportion:0.9,monitor_name:HDMI-A-1

```

> **Tip:** For Waybar configuration with persistent tags, see [Status Bar](/docs/visuals/status-bar) documentation.

---

## Layer Rules

You can set all parameters in one line. Target "layer shell" surfaces like status bars (`waybar`), launchers (`rofi`), or notification daemons.

**Format:**

```ini
layer_rule=layer_name:Values,Parameter:Values,Parameter:Values
```

> **Tip:** You can use `mmsg get last_open_surface` to get the last open layer name for debugging.

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `layer_name` | string | layer name | Match name of layer, supports regex |
| `animation_type_open` | string | slide, zoom, fade, none | Set open animation |
| `animation_type_close` | string | slide, zoom, fade, none | Set close animation |
| `no_blur` | integer | `0` / `1` | Disable blur |
| `no_animation` | integer | `0` / `1` | Disable layer animation |
| `no_shadow` | integer | `0` / `1` | Disable layer shadow |
| `shield_when_capture`| integer | `0` / `1` | Shield layer when captured.(it is better to combination with `no_animation:1`) |

> **Tip:** For animation types, see [Animations](/docs/visuals/animations#animation-types). For visual effects, see [Window Effects](/docs/visuals/effects).

### Examples

```ini
# No blur or animation for slurp selection layer (avoids occlusion and ghosting in screenshots)
layer_rule=no_animation:1,no_blur:1,layer_name:selection

# Zoom animation for Rofi with multiple parameters
layer_rule=animation_type_open:zoom,no_animation:0,layer_name:rofi

# Disable animations and shadows for notification daemon
layer_rule=no_animation:1,no_shadow:1,layer_name:swaync

# Multiple effects for launcher
layer_rule=animation_type_open:slide,animation_type_close:fade,no_blur:1,layer_name:wofi
```
