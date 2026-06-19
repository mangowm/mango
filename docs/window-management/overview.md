---
title: Overview
description: Configure the overview mode for window navigation.
---

## Overview Settings

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `hotarea_size` | integer | `10` | Hot area size in pixels. |
| `enable_hotarea` | integer | `0` | Enable hot areas (0: disable, 1: enable). |
| `hotarea_corner` | integer | `2` | Hot area corner (0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right). |
| `ov_tab_mode` | integer | `1` | Overview tab mode (0: disable, 1: enable). |
| `overviewgappi` | integer | `5` | Inner gap in overview mode. |
| `overviewgappo` | integer | `30` | Outer gap in overview mode. |
| `ov_no_resize` | integer | `1` | Disable window resizing in overview mode. |

### Setting Descriptions

- `enable_hotarea` — Toggles overview when the cursor enters the configured corner.
- `hotarea_size` — Size of the hot area trigger zone in pixels.
- `hotarea_corner` — Corner that triggers the hot area (0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right).
- `ov_tab_mode` — Circles focus through windows in overview; releasing the mod key exits overview.
- `ov_no_resize` — Disables resizing of windows in overview mode(use snap to display).

### Mouse Interaction in Overview

When in overview mode:

- **Left mouse button** — Jump to (focus) a window.
- **Right mouse button** — Close a window.