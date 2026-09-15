---
title: Animations
description: Configure smooth transitions for windows and layers.
---

## Enabling Animations

mangowm supports animations for both standard windows and layer shell surfaces (like bars and notifications).

```ini
deco.animation.enable=1
deco.layer.animation.enable=1
```

## Animation Types

You can define different animation styles for opening and closing windows and layer surfaces.

Available types: `slide`, `zoom`, `fade`, `none`.

```ini
deco.animation.type.open=zoom
deco.animation.type.close=slide
deco.layer.animation.type.open=slide
deco.layer.animation.type.close=slide
```

## Fade Settings

Control the fade-in and fade-out effects for animations.

```ini
deco.animation.fade.in.enable=1
deco.animation.fade.out.enable=1
deco.animation.fade.in.begin.opacity=0.5
deco.animation.fade.out.begin.opacity=0.5
```

- `deco.animation.fade.in.enable` — Enable fade-in effect (0: disable, 1: enable)
- `deco.animation.fade.out.enable` — Enable fade-out effect (0: disable, 1: enable)
- `deco.animation.fade.in.begin.opacity` — Starting opacity for fade-in animations (0.0–1.0)
- `deco.animation.fade.out.begin.opacity` — Starting opacity for fade-out animations (0.0–1.0)

## Zoom Settings

Adjust the zoom ratios for zoom animations.

```ini
deco.animation.zoom.initial.ratio=0.4
deco.animation.zoom.end.ratio=0.8
```

- `deco.animation.zoom.initial.ratio` — Initial zoom ratio
- `deco.animation.zoom.end.ratio` — End zoom ratio

## Durations

Control the speed of animations (in milliseconds).

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `deco.animation.duration.move` | integer | `500` | Move animation duration (ms) |
| `deco.animation.duration.open` | integer | `400` | Open animation duration (ms) |
| `deco.animation.duration.tag` | integer | `300` | Tag animation duration (ms) |
| `deco.animation.duration.close` | integer | `300` | Close animation duration (ms) |
| `deco.animation.duration.focus` | integer | `0` | Focus change (opacity transition) animation duration (ms) |

```ini
deco.animation.duration.move=500
deco.animation.duration.open=400
deco.animation.duration.tag=300
deco.animation.duration.close=300
deco.animation.duration.focus=0
```

## Custom Bezier Curves

Bezier curves determine the "feel" of an animation (e.g., linear vs. bouncy). The format is `x1,y1,x2,y2`.

You can visualize and generate curve values using online tools like [cssportal.com](https://www.cssportal.com/css-cubic-bezier-generator/) or [easings.net](https://easings.net).

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `deco.animation.curve.open` | string | `0.46,1.0,0.29,0.99` | Open animation bezier curve |
| `deco.animation.curve.move` | string | `0.46,1.0,0.29,0.99` | Move animation bezier curve |
| `deco.animation.curve.tag` | string | `0.46,1.0,0.29,0.99` | Tag animation bezier curve |
| `deco.animation.curve.close` | string | `0.46,1.0,0.29,0.99` | Close animation bezier curve |
| `deco.animation.curve.focus` | string | `0.46,1.0,0.29,0.99` | Focus change (opacity transition) animation bezier curve |
| `deco.animation.curve.fade.in` | string | `0.46,1.0,0.29,0.99` | Open opacity animation bezier curve |
| `deco.animation.curve.fade.out` | string | `0.5,0.5,0.5,0.5` | Close opacity animation bezier curve |

```ini
deco.animation.curve.open=0.46,1.0,0.29,0.99
deco.animation.curve.move=0.46,1.0,0.29,0.99
deco.animation.curve.tag=0.46,1.0,0.29,0.99
deco.animation.curve.close=0.46,1.0,0.29,0.99
deco.animation.curve.focus=0.46,1.0,0.29,0.99
deco.animation.curve.fade.in=0.46,1.0,0.29,0.99
deco.animation.curve.fade.out=0.5,0.5,0.5,0.5
```

## Tag Animation Direction

Control the direction of tag switch animations.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `deco.animation.tag.direction` | `1` | Tag animation direction (1: horizontal, 0: vertical) |