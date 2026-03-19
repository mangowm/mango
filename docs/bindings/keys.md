---
title: Key Bindings
description: Define keyboard shortcuts and modes.
---

## Syntax

Key bindings follow this format:

```ini
bind[flags]=MODIFIERS,KEY,COMMAND,PARAMETERS
```

- **Modifiers**: `SUPER`, `CTRL`, `ALT`, `SHIFT`, `NONE` (combine with `+`, e.g. `SUPER+CTRL+ALT`).
- **Key**: Key name (from `xev` or `wev`) or keycode (e.g., `code:24` for `q`).

> **Info:** `bind` automatically converts keysym to keycode for comparison. This makes it compatible with all keyboard layouts, but the matching may not always be precise. If a key combination doesn't work on your keyboard layout, use a keycode instead (e.g., `code:24` instead of `q`).

### Flags

- `l`: Works even when screen is locked.
- `s`: Uses keysym instead of keycode to bind.
- `r`: Triggers on key release instead of press.
- `p`: Pass key event to client.

**Examples:**

```ini
bind=SUPER,Q,killclient
bindl=SUPER,L,spawn,swaylock

# Using keycode instead of key name
bind=ALT,code:24,killclient

# Combining keycodes for modifiers and keys
bind=code:64,code:24,killclient
bind=code:64+code:133,code:24,killclient

# Bind with no modifier
bind=NONE,XF86MonBrightnessUp,spawn,brightnessctl set +5%

# Bind a modifier key itself as the trigger key
bind=alt,shift_l,switch_keyboard_layout
```

## Key Modes (Submaps)

You can divide key bindings into named modes. Rules:

1. Set `keymode=<name>` before a group of `bind` lines — those binds only apply in that mode.
2. If no `keymode` is set before a bind, it belongs to the `default` mode.
3. The special `common` keymode applies its binds **across all modes**.

Use `setkeymode` to switch modes, and `mmsg -b` to query the current mode.

```ini
# Binds in 'common' apply in every mode
keymode=common
bind=SUPER,r,reload_config

# Default mode bindings
keymode=default
bind=ALT,Return,spawn,foot
bind=SUPER,F,setkeymode,resize

# 'resize' mode bindings
keymode=resize
bind=NONE,Left,resizewin,-10,0
bind=NONE,Right,resizewin,+10,0
bind=NONE,Escape,setkeymode,default
```

### Single Modifier Key Binding

When binding a modifier key itself, use `NONE` for press and the modifier name for release:

```ini
# Trigger on press of Super key
bind=none,Super_L,spawn,rofi -show run

# Trigger on release of Super key
bindr=Super,Super_L,spawn,rofi -show run
```

## Dispatchers List

### Window Management

| Command | Param | Description |
| :--- | :--- | :--- |
| `killclient` | - | Close the focused window. |
| `togglefloating` | - | Toggle floating state. |
| `toggle_all_floating` | - | Toggle all visible clients floating state. |
| `togglefullscreen` | - | Toggle fullscreen. |
| `togglefakefullscreen` | - | Toggle "fake" fullscreen (remains constrained). |
| `togglemaximizescreen` | - | Maximize window (keep decoration/bar). |
| `toggleglobal` | - | Pin window to all tags. |
| `toggle_render_border` | - | Toggle border rendering. |
| `centerwin` | - | Center the floating window. |
| `minimized` | - | Minimize window to scratchpad. |
| `restore_minimized` | - | Restore window from scratchpad. |
| `toggle_scratchpad` | - | Toggle scratchpad. |
| `toggle_named_scratchpad` | `appid,title,cmd` | Toggle named scratchpad. Launches app if not running, otherwise shows/hides it. |

### Focus & Movement

| Command | Param | Description |
| :--- | :--- | :--- |
| `focusdir` | `left/right/up/down` | Focus window in direction. |
| `focusstack` | `next/prev` | Cycle focus within the stack. |
| `focuslast` | - | Focus the previously active window. |
| `exchange_client` | `left/right/up/down` | Swap window with neighbor in direction. |
| `exchange_stack_client` | `next/prev` | Exchange window position in stack. |
| `zoom` | - | Swap focused window with Master. |

### Tags & Monitors

| Command | Param | Description |
| :--- | :--- | :--- |
| `view` | `-1/0/[1, tag_count]` or `mask [,synctag]` | View tag. `-1` = previous tagset, `0` = all tags, mask e.g. `1\|3\|5`. Optional `synctag` (0/1) syncs the action to all monitors. |
| `viewtoleft` | `[synctag]` | View previous tag. Optional `synctag` (0/1) syncs to all monitors. |
| `viewtoright` | `[synctag]` | View next tag. Optional `synctag` (0/1) syncs to all monitors. |
| `viewtoleft_have_client` | `[synctag]` | View left tag and focus client if present. Optional `synctag` (0/1). |
| `viewtoright_have_client` | `[synctag]` | View right tag and focus client if present. Optional `synctag` (0/1). |
| `viewcrossmon` | `tag,monitor_spec` | View specified tag on specified monitor. |
| `tag` | `[1, tag_count] [,synctag]` | Move window to tag. Optional `synctag` (0/1) syncs to all monitors. |
| `tagsilent` | `[1, tag_count]` | Move window to tag without focusing it. |
| `tagtoleft` | `[synctag]` | Move window to left tag. Optional `synctag` (0/1). |
| `tagtoright` | `[synctag]` | Move window to right tag. Optional `synctag` (0/1). |
| `tagcrossmon` | `tag,monitor_spec` | Move window to specified tag on specified monitor. |
| `toggletag` | `[0, tag_count]` | Toggle tag on window (0 means all tags). |
| `toggleview` | `[1, tag_count]` | Toggle tag view. |
| `comboview` | `[1, tag_count]` | View multi tags pressed simultaneously. |
| `focusmon` | `left/right/up/down/monitor_spec` | Focus monitor by direction or [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `tagmon` | `left/right/up/down/monitor_spec,[keeptag]` | Move window to monitor by direction or [monitor spec](/docs/configuration/monitors#monitor-spec-format). `keeptag` is 0 or 1. |

### Layouts

| Command | Param | Description |
| :--- | :--- | :--- |
| `setlayout` | `name` | Switch to layout (e.g., `scroller`, `tile`). |
| `switch_layout` | - | Cycle through available layouts. |
| `incnmaster` | `+1/-1` | Increase/Decrease number of master windows. |
| `setmfact` | `+0.05` | Increase/Decrease master area size. |
| `set_proportion` | `float` | Set scroller window proportion (0.0–1.0). |
| `switch_proportion_preset` | - | Cycle proportion presets of scroller window. |
| `scroller_stack` | `left/right/up/down` | Move window inside/outside scroller stack by direction. |
| `incgaps` | `+/-value` | Adjust gap size. |
| `togglegaps` | - | Toggle gaps. |

### System

| Command | Param | Description |
| :--- | :--- | :--- |
| `spawn` | `cmd` | Execute a command. |
| `spawn_shell` | `cmd` | Execute shell command (supports pipes `\|`). |
| `spawn_on_empty` | `cmd,tagnumber` | Open command on empty tag. |
| `reload_config` | - | Hot-reload configuration. |
| `quit` | - | Exit mangowm. |
| `toggleoverview` | - | Toggle overview mode. |
| `create_virtual_output` | - | Create a headless monitor (for VNC/Sunshine). |
| `destroy_all_virtual_output` | - | Destroy all virtual monitors. |
| `toggleoverlay` | - | Toggle overlay state for the focused window. |
| `toggle_trackpad_enable` | - | Toggle trackpad enable. |
| `setkeymode` | `mode` | Set keymode. |
| `switch_keyboard_layout` | `[index]` | Switch keyboard layout. Optional index (0, 1, 2...) to switch to specific layout. |
| `setoption` | `key,value` | Set config option temporarily. |
| `disable_monitor` | `monitor_spec` | Shutdown monitor. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `enable_monitor` | `monitor_spec` | Power on monitor. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `toggle_monitor` | `monitor_spec` | Toggle monitor power. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |

### Media Controls

> **Warning:** Some keyboards don't send standard media keys. Run `wev` and press your key to check the exact key name.

#### Brightness

Requires: `brightnessctl`

```ini
bind=NONE,XF86MonBrightnessUp,spawn,brightnessctl s +2%
bind=SHIFT,XF86MonBrightnessUp,spawn,brightnessctl s 100%
bind=NONE,XF86MonBrightnessDown,spawn,brightnessctl s 2%-
bind=SHIFT,XF86MonBrightnessDown,spawn,brightnessctl s 1%
```

#### Volume

Requires: `wpctl` (WirePlumber)

```ini
bind=NONE,XF86AudioRaiseVolume,spawn,wpctl set-volume @DEFAULT_SINK@ 5%+
bind=NONE,XF86AudioLowerVolume,spawn,wpctl set-volume @DEFAULT_SINK@ 5%-
bind=NONE,XF86AudioMute,spawn,wpctl set-mute @DEFAULT_SINK@ toggle
bind=SHIFT,XF86AudioMute,spawn,wpctl set-mute @DEFAULT_SOURCE@ toggle
```

### Floating Window Movement

| Command | Param | Description |
| :--- | :--- | :--- |
| `smartmovewin` | `left/right/up/down` | Move floating window by snap distance. |
| `smartresizewin` | `left/right/up/down` | Resize floating window by snap distance. |
| `movewin` | `(x,y)` | Move floating window. |
| `resizewin` | `(width,height)` | Resize window. |