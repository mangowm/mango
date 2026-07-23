---
title: Miscellaneous
description: Advanced settings for XWayland, focus behavior, and system integration.
---

## System & Hardware

| Setting | Default | Description |
| :--- | :--- | :--- |
| `xwayland_persistence` | `1` | Keep XWayland running even when no X11 apps are open (reduces startup lag). |
| `syncobj_enable` | `1` | Enable `drm_syncobj` timeline support (helps with gaming stutter/lag). **Requires restart.** |
| `allow_lock_transparent` | `0` | Allow the lock screen to be transparent. |
| `allow_shortcuts_inhibit` | `1` | Allow shortcuts to be inhibited by clients. |

## Session Restore

Session restore is opt-in and disabled by default. When enabled, Mango saves
restorable applications during a clean shutdown, relaunches them at the next
startup, and restores their tag, monitor, floating or fullscreen state, and
floating geometry as their windows map.

```ini
session_restore=1
```

Mango prefers exact commands for applications it spawned itself. For other
applications, it tries to recover a suitable command from process metadata,
normalizing desktop-entry and Flatpak launchers before falling back to raw
process arguments.

Use `session_launch` when an application needs an explicit launch command.
An app ID-only rule applies to every matching window; adding a title allows
different commands for windows sharing an app ID.

```ini
session_launch=foot|foot
session_launch=foot|gamma|foot -a foot -T gamma -e sh -lc "sleep 600"
```

Mango restores only from its session file when that file is owned by the
current user and is not writable by group or others. The file is stored at
`$XDG_DATA_HOME/mango/session.json`, or
`$HOME/.local/share/mango/session.json` when `XDG_DATA_HOME` is unset.

Current limitations:

- Minimized state is not restored.
- Target outputs must already exist before clients map; Mango does not recreate
  outputs or reconcile windows when an output appears later.
- Manual `session_launch` rules may still be needed for unusual wrappers,
  applications without reliable launcher metadata, and duplicate windows that
  intentionally require different commands.

## Focus & Input

| Setting | Default | Description |
| :--- | :--- | :--- |
| `focus_on_activate` | `1` | Automatically focus windows when they request activation. |
| `sloppyfocus` | `1` | Focus follows the mouse cursor. |
| `warpcursor` | `1` | Warp the cursor to the center of the window when focus changes via keyboard. |
| `cursor_hide_timeout` | `0` | Hide the cursor after `N` seconds of inactivity (`0` to disable). |
| `cursor_hide_on_keypress` | `0` | Hide the cursor on keypress. |
| `drag_tile_to_tile` | `0` | Allow dragging a tiled window onto another to swap their positions. |
| `drag_tile_small` | `1` | Allow dragging a tiled window temporarily to small size.|
| `drag_corner` | `3` | Corner for drag-to-tile detection (0: none, 1–3: corners, 4: auto-detect). |
| `drag_warp_cursor` | `1` | Warp cursor when dragging windows to tile. |
| `axis_bind_apply_timeout` | `100` | Timeout (ms) for detecting consecutive scroll events for axis bindings. |

## Multi-Monitor & Tags

| Setting | Default | Description |
| :--- | :--- | :--- |
| `focus_cross_monitor` | `0` | Allow directional focus to cross monitor boundaries. |
| `exchange_cross_monitor` | `0` | Allow exchanging clients across monitor boundaries. |
| `focus_cross_tag` | `0` | Allow directional focus to cross into other tags. |
| `view_current_to_back` | `0` | Toggling the current tag switches back to the previously viewed tag. |
| `scratchpad_cross_monitor` | `0` | Share the scratchpad pool across all monitors. |
| `single_scratchpad` | `1` | Only allow one scratchpad (named or standard) to be visible at a time. |

## Window Behavior

| Setting | Default | Description |
| :--- | :--- | :--- |
| `enable_floating_snap` | `0` | Snap floating windows to edges or other windows. |
| `snap_distance` | `30` | Max distance (pixels) to trigger floating snap. |
| `no_border_when_single` | `0` | Remove window borders when only one window is visible on the tag. |
| `idleinhibit_ignore_visible` | `0` | Allow invisible clients (e.g., background audio players) to inhibit idle. |
| `tag_carousel` | `0` | Enable tag carousel (cycling through tags). |
| `drag_tile_refresh_interval` | `8.0` | Interval (1.0–16.0) to refresh tiled window resize during drag. Too small may cause application lag. |
| `drag_floating_refresh_interval` | `8.0` | Interval (1.0–16.0) to refresh floating window resize during drag. Too small may cause application lag. |
