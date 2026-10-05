---
title: Miscellaneous
description: Advanced settings for XWayland, focus behavior, and system integration.
---

## System & Hardware

| Setting | Default | Description |
| :--- | :--- | :--- |
| `xwayland_persistence` | `1` | Keep XWayland running even when no X11 apps are open (reduces startup lag). |
| `xwayland_ignore_scale` | `0` | DIsable global scale for xwayland.|
| `sync_obj_enable` | `1` | Enable `drm_syncobj` timeline support (helps with gaming stutter/lag). **Requires restart.** |
| `allow_lock_transparent` | `0` | Allow the lock screen to be transparent. |
| `allow_shortcuts_inhibit` | `1` | Allow shortcuts to be inhibited by clients. |
| `auto_reload_config` | `1` | Watch the main config file and every `source`/`source_optional` file, then reload immediately after any of them changes. |

## Focus & Input

| Setting | Default | Description |
| :--- | :--- | :--- |
| `focus_on_activate` | `1` | Automatically focus windows when they request activation. |
| `sloppy_focus` | `1` | Focus follows the mouse cursor. |
| `map_focus_monitor` | `0` | Map tablets to the focused monitor automatically. When disabled, a tablet is only mapped to a monitor if a `device` rule pins it via `monitor`. |
| `warp_cursor` | `1` | Warp the cursor to the center of the window when focus changes via keyboard. |
| `cursor_hide_timeout` | `0` | Hide the cursor after `N` seconds of inactivity (`0` to disable). |
| `cursor_hide_on_keypress` | `0` | Hide the cursor on keypress. |
| `drag_tile_to_tile` | `0` | Allow dragging a tiled window onto another to swap their positions. |
| `drag_tile_small` | `1` | Allow dragging a tiled window temporarily to small size.|
| `drag_corner` | `3` | Corner for drag-to-tile detection (0: none, 1–3: corners, 4: auto-detect). |
| `drag_warp_cursor` | `1` | Warp cursor when dragging windows to tile. |
| `axis_bind_apply_timeout` | `100` | Timeout (ms) for detecting consecutive scroll events for axis bindings. |
| `disable_middle_paste` | `0` | Disable middle-click paste by turning off the whole primary selection. Only affects Wayland apps; X11 apps are unaffected. Restart apps after re-enabling. |

## Multi-Monitor & Tags

| Setting | Default | Description |
| :--- | :--- | :--- |
| `focus_cross_monitor` | `0` | Allow directional focus to cross monitor boundaries. |
| `focus_direction_only_zone_overlap` | `1` | When enabled, directional focus only selects windows that overlap the current window on the perpendicular axis (y for left/right, x for up/down); returns nothing if none qualify. |
| `exchange_cross_monitor` | `0` | Allow the `exchange_client` and `move_client` dispatchers to reach across monitor boundaries. With `exchange_client` the two windows swap monitors; with `move_client` the window moves onto the monitor holding the neighbor (or lying in the move direction when there is none) and is inserted in front of or behind that neighbor instead of swapping with it. While disabled, both dispatchers keep the windows on the current monitor. |
| `focus_cross_tag` | `0` | Allow directional focus to cross into other tags. |
| `view_current_to_back` | `0` | Toggling the current tag switches back to the previously viewed tag. |
| `scratchpad_cross_monitor` | `0` | Share the scratchpad pool across all monitors. |
| `single_scratchpad` | `1` | Only allow one scratchpad (named or standard) to be visible at a time. |
| `tag_num` | `9` | Number of tags/workspaces (1–31). On config reload, clients on tags beyond this count are moved to the last tag. |
| `tag_gather` | `0` | When `1`, occupied tags are compacted to consecutive tags starting at 1, eliminating gaps. For example, with windows on tags 1, 3 and 9, they move to 1, 2 and 3, and the current view follows. |

## Window Behavior

| Setting | Default | Description |
| :--- | :--- | :--- |
| `enable_floating_snap` | `0` | Snap floating windows to edges or other windows. |
| `snap_distance` | `30` | Max distance (pixels) to trigger floating snap. |
| `float_full_to_top` | `0` | Let fullscreen, floating and layer-shell `top` windows share one layer so they can cover each other; which one ends up on top depends on which was opened or raised last. When `0`, they are split into separate layers instead: floating windows below, fullscreen windows above layer-shell `top` windows. |
| `no_border_when_single` | `0` | Remove window borders when only one window is visible on the tag. |
| `smart_gaps` | `0` | Disable gaps when only one window is present. |
| `idle_inhibit_ignore_visible` | `0` | Allow invisible clients (e.g., background audio players) to inhibit idle. |
| `idle_inhibit_when_fullscreen` | `0` | Keep idle inhibited while a fullscreen window is focused. |
| `tag_carousel` | `0` | Enable tag carousel (cycling through tags). |
| `drag_tile_refresh_interval` | `8.0` | Interval (1.0–16.0) to refresh tiled window resize during drag. Too small may cause application lag. |
| `drag_floating_refresh_interval` | `8.0` | Interval (1.0–16.0) to refresh floating window resize during drag. Too small may cause application lag. |

## Config errors

When the config file contains errors, mango shows a `mangonag` bar at the top of
the focused output.

- **Close** dismisses the bar.
- **Edit** opens the first reported error in your editor at the reported line.

The editor is taken from `$EDITOR`, then `$VISUAL`, then `vi`; terminal editors
open in `$TERMINAL`.
