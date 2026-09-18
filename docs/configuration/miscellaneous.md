---
title: Miscellaneous
description: Advanced settings for XWayland, focus behavior, and system integration.
---

## System & Hardware

| Setting | Default | Description |
| :--- | :--- | :--- |
| `system.xwayland.persistence` | `1` | Keep XWayland running even when no X11 apps are open (reduces startup lag). |
| `system.xwayland.ignore.scale` | `0` | DIsable global scale for xwayland.|
| `system.syncobj.enable` | `1` | Enable `drm_syncobj` timeline support (helps with gaming stutter/lag). **Requires restart.** |
| `system.lock.transparent` | `0` | Allow the lock screen to be transparent. |
| `system.shortcuts.inhibit` | `1` | Allow shortcuts to be inhibited by clients. |

## Focus & Input

| Setting | Default | Description |
| :--- | :--- | :--- |
| `focus.on.activate` | `1` | Automatically focus windows when they request activation. |
| `focus.sloppyfocus` | `1` | Focus follows the mouse cursor. |
| `focus.warp.cursor` | `1` | Warp the cursor to the center of the window when focus changes via keyboard. |
| `cursor.hide.timeout` | `0` | Hide the cursor after `N` seconds of inactivity (`0` to disable). |
| `cursor.hide.on.keypress` | `0` | Hide the cursor on keypress. |
| `window.drag.tile.to.tile` | `0` | Allow dragging a tiled window onto another to swap their positions. |
| `window.drag.tile.small` | `1` | Allow dragging a tiled window temporarily to small size.|
| `window.drag.corner` | `3` | Corner for drag-to-tile detection (0: none, 1–3: corners, 4: auto-detect). |
| `window.drag.warp.cursor` | `1` | Warp cursor when dragging windows to tile. |
| `input.axis.bind.apply.timeout` | `100` | Timeout (ms) for detecting consecutive scroll events for axis bindings. |

## Multi-Monitor & Tags

| Setting | Default | Description |
| :--- | :--- | :--- |
| `focus.cross.monitor` | `0` | Allow directional focus to cross monitor boundaries. |
| `focus.direction.zone.overlap` | `1` | When enabled, directional focus only selects windows that overlap the current window on the perpendicular axis (y for left/right, x for up/down); returns nothing if none qualify. |
| `window.move.cross.monitor` | `0` | Allow the `exchange_client` and `move_client` dispatchers to reach across monitor boundaries. With `exchange_client` the two windows swap monitors; with `move_client` the window moves onto the monitor holding the neighbor (or lying in the move direction when there is none) and is inserted in front of or behind that neighbor instead of swapping with it. While disabled, both dispatchers keep the windows on the current monitor. |
| `focus.cross.tag` | `0` | Allow directional focus to cross into other tags. |
| `tag.view.current.to.back` | `0` | Toggling the current tag switches back to the previously viewed tag. |
| `scratchpad.cross.monitor` | `0` | Share the scratchpad pool across all monitors. |
| `scratchpad.single` | `1` | Only allow one scratchpad (named or standard) to be visible at a time. |
| `tag.num` | `9` | Number of tags/workspaces (1–31). On config reload, clients on tags beyond this count are moved to the last tag. |
| `tag.gather` | `0` | When `1`, occupied tags are compacted to consecutive tags starting at 1, eliminating gaps. For example, with windows on tags 1, 3 and 9, they move to 1, 2 and 3, and the current view follows. |

## Window Behavior

| Setting | Default | Description |
| :--- | :--- | :--- |
| `window.snap.enable` | `0` | Snap floating windows to edges or other windows. |
| `window.snap.distance` | `30` | Max distance (pixels) to trigger floating snap. |
| `deco.border.hide.single` | `0` | Remove window borders when only one window is visible on the tag. |
| `deco.gap.smart` | `0` | Disable gaps when only one window is present. |
| `window.idleinhibit.ignore.visible` | `0` | Allow invisible clients (e.g., background audio players) to inhibit idle. |
| `window.idleinhibit.when.fullscreen` | `0` | Keep idle inhibited while a fullscreen window is focused. |
| `tag.carousel` | `0` | Enable tag carousel (cycling through tags). |
| `window.drag.tile.refresh.interval` | `8.0` | Interval (1.0–16.0) to refresh tiled window resize during drag. Too small may cause application lag. |
| `window.drag.floating.refresh.interval` | `8.0` | Interval (1.0–16.0) to refresh floating window resize during drag. Too small may cause application lag. |
