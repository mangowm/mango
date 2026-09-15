---
title: Window Effects
description: Add visual polish with blur, shadows, and opacity.
---

## Blur

Blur creates a frosted glass effect for transparent windows.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `deco.blur.enable` | `0` | Enable blur for windows. |
| `deco.layer.blur.enable` | `0` | Enable blur for layer surfaces (like bars/docks). |
| `deco.blur.optimized` | `1` | Caches the wallpaper and blur background, significantly reducing GPU usage. Disabling it will significantly increase GPU consumption and may cause rendering lag. **Highly recommended.** |
| `deco.blur.params.radius` | `5` | The strength (radius) of the blur. |
| `deco.blur.params.num.passes` | `1` | Number of passes. Higher = smoother but more expensive. |
| `deco.blur.params.noise` | `0.02` | Blur noise level. |
| `deco.blur.params.brightness` | `0.9` | Blur brightness adjustment. |
| `deco.blur.params.contrast` | `0.9` | Blur contrast adjustment. |
| `deco.blur.params.saturation` | `1.2` | Blur saturation adjustment. |

> **Warning:** Blur has a relatively high impact on performance. If your hardware is limited, it is not recommended to enable it. If you experience lag with blur on, ensure `deco.blur.optimized=1` — disabling it will significantly increase GPU consumption and may cause rendering lag. To disable blur entirely, set `deco.blur.enable=0`.

---

## Shadows

Drop shadows help distinguish floating windows from the background.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `deco.shadow.enable` | `0` | Enable shadows. |
| `deco.layer.shadow.enable` | `0` | Enable shadows for layer surfaces. |
| `deco.shadow.only.floating` | `1` | Only draw shadows for floating windows (saves performance). |
| `deco.shadow.size` | `10` | Size of the shadow. |
| `deco.shadow.blur` | `15` | Shadow blur amount. |
| `deco.shadow.position.x` | `0` | Shadow X offset. |
| `deco.shadow.position.y` | `0` | Shadow Y offset. |
| `deco.shadow.color` | `0x000000ff` | Color of the shadow. |

```ini
# Example shadows configuration
deco.shadow.enable=1
deco.layer.shadow.enable=1
deco.shadow.only.floating=1
deco.shadow.size=12
deco.shadow.blur=15
deco.shadow.position.x=0
deco.shadow.position.y=0
deco.shadow.color=0x000000ff
```

---

## Opacity & Corner Radius

Control the transparency and roundness of your windows.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `deco.border.radius` | `0` | Window corner radius in pixels. |
| `border_radius_location_default` | `0` | Corner radius location: `0` (all), `1` (top-left), `2` (top-right), `3` (bottom-left), `4` (bottom-right), `5` (closest corner). |
| `deco.border.radius.hide.single` | `0` | Disable radius if only one window is visible. |
| `deco.opacity.focused` | `1.0` | Opacity for the active window (0.0 - 1.0). |
| `deco.opacity.unfocused` | `1.0` | Opacity for inactive windows (0.0 - 1.0). |

```ini
# Window corner radius in pixels
deco.border.radius=0

# Corner radius location (0=all, 1=top-left, 2=top-right, 3=bottom-left, 4=bottom-right)
border_radius_location_default=0

# Disable radius if only one window is visible
deco.border.radius.hide.single=0

# Opacity for the active window (0.0 - 1.0)
deco.opacity.focused=1.0

# Opacity for inactive windows
deco.opacity.unfocused=1.0
```