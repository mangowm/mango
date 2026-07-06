---
title: Shader Transitions
description: Write custom GLSL fragment shaders for window open/close animations.
---

mangowm can drive a window's open and close animation with a custom GLSL fragment shader instead of (or alongside) the built-in parametric fades and zooms. Each window is snapshotted to a texture once, then your shader runs every animation tick against that texture to produce the visible frame.

This is a small, focused feature. It only covers the open and close transitions (not move/tag/focus animations), and shader files are only discovered at startup and after a GPU reset. There is no hot-reload while the compositor is running.

## Quick start

1. Drop a `.frag` file in `~/.config/mango/shaders/`, e.g. `dissolve.frag`.
2. Reference it by filename (without the `.frag` extension) in your config:

```ini
effect_open=dissolve
effect_close=burn
```

3. Restart mangowm. Shader files are scanned once at startup and again after a GPU reset. Adding or editing a `.frag` file while the compositor is already running has no effect until the next restart.

## Writing a shader

A shader file is just the body of a GLSL ES 3.00 fragment shader, typically a single `void main() { ... }` function. The loader wraps your body with a fixed header before compiling it, equivalent to:

```glsl
#version 300 es
precision highp float;
uniform sampler2D u_texture;
uniform float u_progress;
uniform float u_time;
uniform vec2 u_size;
in vec2 v_texcoord;
out vec4 fragColor;
```

Do not redeclare any of these names in your file: `u_texture`, `u_progress`, `u_time`, `u_size`, `v_texcoord`, or `fragColor`. The header already declares them. Redeclaring any of them is a GLSL compile error and your shader will fail to load (see [Fail-safe behavior](#fail-safe-behavior)).

| Name | Type | Meaning |
| :--- | :--- | :--- |
| `u_texture` | `sampler2D` | A snapshot of the window's contents, sampled with `texture(u_texture, v_texcoord)`. |
| `u_progress` | `float` | Animation progress, `0.0` to `1.0`. See [Progress convention](#progress-convention) below. This is the one thing every shader must get right. |
| `u_time` | `float` | Seconds elapsed since the animation started. Useful for time-based effects (e.g. a glitch shimmer) independent of progress. |
| `u_size` | `vec2` | Output size in pixels (the window width and height being rendered into). |
| `v_texcoord` | `vec2` | Interpolated texture coordinate, `0.0` to `1.0` across the window. |
| `fragColor` | `out vec4` | Your shader's output color for the pixel. Write to this instead of `gl_FragColor`. |

Your shader writes its output to `fragColor`, which the header declares as `out vec4`. Since the engine composites the result with premultiplied alpha, the RGB channels you write must already be multiplied by the alpha you write. That means `fragColor = vec4(color.rgb * alpha, alpha)`, not `vec4(color.rgb, alpha)`. Forgetting this produces a white or light halo around fading-out windows.

### GLSL ES 3.00

The shader is compiled as GLSL ES 3.00. The loader prepends the `#version 300 es` line and the header above, so you write only the body:

- Sample textures with `texture(sampler, uv)`, not `texture2D(...)`.
- Write the output to `fragColor`, not `gl_FragColor`. The `out` variable is already declared in the header, so do not declare your own.
- Read the interpolated coordinate from `v_texcoord` (declared `in` by the header). Inputs from the vertex stage use `in`, not `varying`.
- No redeclaring `u_texture` / `u_progress` / `u_time` / `u_size` / `v_texcoord` / `fragColor` (see above).

### Precision: highp is guaranteed

GLSL ES 3.00 fragment shaders are guaranteed `highp` (fp32) floats. The classic noise hash works with no workaround:

```glsl
float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
```

You do not need the multi-round fp16-safe hashes found in older GLES2 shader code, and you do not need `GL_FRAGMENT_PRECISION_HIGH` guards. `mediump` is still available if you want it for a hot loop, but the default `highp` is fine everywhere.

## Progress convention

Write your shader so `u_progress == 0.0` means the window is fully present and `u_progress == 1.0` means the window is fully gone. The compositor runs progress in the right direction for both open and close, so a single shader body works for both:

- **Close**: progress is driven directly from the close animation's elapsed fraction (`progress = min(animation_passed, 1.0)`), so it runs `0 -> 1` as the window closes: present, then gone.
- **Open**: progress is driven from `1.0 - min(animation_passed, 1.0)`, so it runs `1 -> 0` as the window opens: gone, then present.

The engine inverts progress for you on open. You never need two different shaders (or an `if` on direction) for the open vs. close case. One body covering `progress: 0 = present, 1 = gone` dissolves a window away on close and dissolves it in on open.

## Configuration

| Setting | Scope | Description |
| :--- | :--- | :--- |
| `effect_open` | global | Name (no `.frag`) of the shader to use for window open animations. |
| `effect_close` | global | Name (no `.frag`) of the shader to use for window close animations. |

```ini
effect_open=dissolve
effect_close=burn
```

Both can also be set per-window via a `windowrule`, which takes precedence over the global setting for matching windows:

```ini
windowrule=appid:firefox,effect_close:glitch
```

If `effect_open`/`effect_close` is left unset (globally and per-window), or the window doesn't match a rule, the window uses the existing parametric animation (`animation_type_open`/`animation_type_close`, fade, zoom, etc.) exactly as before this feature existed. Shaders are strictly opt-in.

## Fail-safe behavior

Shader loading and selection are designed to never break your session:

- **Compile/link failure**: if a `.frag` file fails to compile or link, the loader logs an error (visible with `WLR_ERROR` logging) and skips that file. It is never registered. Other shaders in the directory still load normally.
- **Unknown shader name**: if `effect_open`/`effect_close` (global or per-window) names a shader that was never successfully loaded (typo, compile failure, or you just haven't created the file), that window silently falls back to the normal parametric open/close animation. No crash, no missing window content. It just behaves as if the shader setting weren't there.
- **Missing shader directory**: if `~/.config/mango/shaders/` doesn't exist, it's logged at `INFO` level and skipped. Having no custom shaders at all is a normal, supported configuration.

## Restart required for new files

`~/.config/mango/shaders/` is scanned for `*.frag` files once at startup, and again automatically after a GPU reset (which also clears all previously compiled shader programs). There is no file-watching or hot-reload. If you add a new `.frag` file or rename one while mangowm is running, you need to restart the compositor before `effect_open` / `effect_close` can reference it. Editing the contents of an already-loaded shader also requires a restart to take effect.

## Example shaders

Example shaders live in a separate repo, [ernestoCruz05/shader_examples](https://github.com/ernestoCruz05/shader_examples). Clone it and copy whichever `.frag` files you want into your shader directory:

```sh
git clone https://github.com/ernestoCruz05/shader_examples.git
cp shader_examples/*.frag ~/.config/mango/shaders/
```

Then restart mangowm and reference them by basename (without the `.frag` extension) in your config (see [Quick start](#quick-start)).
