---
title: Animations
description: Configure smooth transitions for windows and layers.
---

## Enabling Animations

mangowm supports animations for both standard windows and layer shell surfaces (like bars and notifications).

```ini
animations=1
layer_animations=1
```

## Animation Types

You can define different animation styles for opening and closing windows and layer surfaces.

Available types: `slide`, `zoom`, `fade`, `none`.

```ini
animation_type_open=zoom
animation_type_close=slide
layer_animation_type_open=slide
layer_animation_type_close=slide
```

## Fade Settings

Control the fade-in and fade-out effects for animations.

```ini
animation_fade_in=1
animation_fade_out=1
fade_in_begin_opacity=0.5
fade_out_begin_opacity=0.5
```

- `animation_fade_in` — Enable fade-in effect (0: disable, 1: enable)
- `animation_fade_out` — Enable fade-out effect (0: disable, 1: enable)
- `fade_in_begin_opacity` — Starting opacity for fade-in animations (0.0–1.0)
- `fade_out_begin_opacity` — Starting opacity for fade-out animations (0.0–1.0)

## Zoom Settings

Adjust the zoom ratios for zoom animations.

```ini
zoom_initial_ratio=0.4
zoom_end_ratio=0.8
```

- `zoom_initial_ratio` — Initial zoom ratio
- `zoom_end_ratio` — End zoom ratio

## Durations

Control the speed of animations (in milliseconds).

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `animation_duration_move` | integer | `500` | Move animation duration (ms) |
| `animation_duration_open` | integer | `400` | Open animation duration (ms) |
| `animation_duration_tag` | integer | `300` | Tag animation duration (ms) |
| `animation_duration_close` | integer | `300` | Close animation duration (ms) |
| `animation_duration_focus` | integer | `0` | Focus change (opacity transition) animation duration (ms) |

```ini
animation_duration_move=500
animation_duration_open=400
animation_duration_tag=300
animation_duration_close=300
animation_duration_focus=0
```

## Animation Curves

Every curve accepts a named easing or a cubic Bézier `x1,y1,x2,y2`.

Named easings include `linear`, `easein`, `easeout`, `easeinout`,
`easeinsine`/`easeoutsine`/`easeinoutsine`,
`easeinquad`/`easeoutquad`/`easeinoutquad`,
`easeincubic`/`easeoutcubic`/`easeinoutcubic`,
`easeinquart`/`easeoutquart`/`easeinoutquart`,
`easeinquint`/`easeoutquint`/`easeinoutquint`,
`easeinexpo`/`easeoutexpo`/`easeinoutexpo`,
`easeincirc`/`easeoutcirc`/`easeinoutcirc`,
`easeinback`/`easeoutback`/`easeinoutback`,
`easeinelastic`/`easeoutelastic`/`easeinoutelastic`,
`easeinbounce`/`easeoutbounce`/`easeinoutbounce`, and `snappy`.

You can visualize and generate curve values using online tools like [cssportal.com](https://www.cssportal.com/css-cubic-bezier-generator/) or [easings.net](https://easings.net).

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `animation_curve_open` | string | `0.46,1.0,0.29,0.99` | Open animation curve |
| `animation_curve_move` | string | `0.46,1.0,0.29,0.99` | Move animation curve |
| `animation_curve_tag` | string | `0.46,1.0,0.29,0.99` | Tag animation curve |
| `animation_curve_close` | string | `0.46,1.0,0.29,0.99` | Close animation curve |
| `animation_curve_focus` | string | `0.46,1.0,0.29,0.99` | Focus animation curve |
| `animation_curve_opacity_fade_in` | string | `0.46,1.0,0.29,0.99` | Open opacity curve |
| `animation_curve_opacity_fade_out` | string | `0.5,0.5,0.5,0.5` | Close opacity curve |
| `animation_duration_workspace_overview_open` | integer | `260` | Workspace overview enter duration in milliseconds |
| `animation_duration_workspace_overview_close` | integer | `260` | Workspace overview exit duration in milliseconds |
| `animation_duration_workspace_overview_move` | integer | `382` | Workspace overview move animation duration in milliseconds |
| `animation_curve_workspace_overview_open` | string | `0.333333,1.0,0.666667,1.0` | Workspace overview enter named easing or cubic-bezier value |
| `animation_curve_workspace_overview_close` | string | `0.333333,1.0,0.666667,1.0` | Workspace overview exit named easing or cubic-bezier value |
| `animation_curve_workspace_overview_move` | string | `0.26,0.99,0.26,1.0` | Workspace overview scrolling and layout-change named easing or cubic-bezier value |

```ini
animation_curve_open=0.46,1.0,0.29,0.99
animation_curve_move=0.46,1.0,0.29,0.99
animation_curve_tag=0.46,1.0,0.29,0.99
animation_curve_close=0.46,1.0,0.29,0.99
animation_curve_focus=0.46,1.0,0.29,0.99
animation_curve_opacity_fade_in=0.46,1.0,0.29,0.99
animation_curve_opacity_fade_out=0.5,0.5,0.5,0.5
animation_duration_workspace_overview_open=260
animation_duration_workspace_overview_close=260
animation_duration_workspace_overview_move=382
animation_curve_workspace_overview_open=0.333333,1.0,0.666667,1.0
animation_curve_workspace_overview_close=0.333333,1.0,0.666667,1.0
animation_curve_workspace_overview_move=0.26,0.99,0.26,1.0
```

## Tag Animation Direction

Control the direction of tag switch animations.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `tag_animation_direction` | `1` | Tag animation direction (1: horizontal, 0: vertical) |
