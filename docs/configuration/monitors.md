---
title: Monitors
description: Manage display outputs, resolution, scaling, and tearing.
---

## Monitor Rules

You can configure each display output individually using the `monitorrule` keyword.

**Syntax:**

```ini
monitorrule=name:Values,Parameter:Values,Parameter:Values
```

> **Info:** If any of the matching fields (`name`, `make`, `model`, `serial`) are set, **all** of the set ones must match to be considered a match. Use `wlr-randr` to get your monitor's name, make, model, and serial.

### Parameters

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `name` | string | Any | Match by monitor name (supports regex) |
| `make` | string | Any | Match by monitor manufacturer |
| `model` | string | Any | Match by monitor model |
| `serial` | string | Any | Match by monitor serial number |
| `width` | integer | 0-9999 | Monitor width |
| `height` | integer | 0-9999 | Monitor height |
| `refresh` | float | 0.001-9999.0 | Monitor refresh rate |
| `x` | integer | 0-99999 | X position |
| `y` | integer | 0-99999 | Y position |
| `scale` | float | 0.01-100.0 | Monitor scale |
| `vrr` | integer | 0, 1 | Enable variable refresh rate |
| `hdr` | integer | 0, 1, 2 | Enable hdr support. `2` additionally reads the panel's mastering luminance and color primaries from its EDID, so tone mapping targets the display's real limits instead of generic defaults. `hdr_min_lum`/`hdr_max_lum`/`hdr_max_avg_lum` still override it. |
| `hdr_min_lum` | float | 0.0-10000.0 | Mastering display minimum luminance, cd/m² (0 = unset) |
| `hdr_max_lum` | float | 0.0-10000.0 | Mastering display peak luminance, also sent as max_cll, cd/m² (0 = unset) |
| `hdr_max_avg_lum` | float | 0.0-10000.0 | Max frame-average light level (max_fall), cd/m² (0 = unset) |
| `hdr_force` | integer | 0, 1 | Enable HDR even when the EDID does not advertise BT.2020/PQ |
| `icc` | string | - | Path to an ICC profile applied as the output color transform (e.g. `/usr/share/color/icc/MyDisplay.icc`). Mutually exclusive with `hdr`: when both are set, HDR takes precedence and the ICC profile is ignored. Set `hdr:0` to use the ICC profile |
| `rr` | integer | 0-7 | Monitor transform |
| `custom` | integer | 0, 1 | Enable custom mode (not supported on all displays — may cause black screen) |
| `disable` | integer | 0, 1 | Disable the monitor |

### Transform Values

| Value | Rotation |
| :--- | :--- |
| `0` | No transform |
| `1` | 90° counter-clockwise |
| `2` | 180° counter-clockwise |
| `3` | 270° counter-clockwise |
| `4` | 180° vertical flip |
| `5` | Flip + 90° counter-clockwise |
| `6` | Flip + 180° counter-clockwise |
| `7` | Flip + 270° counter-clockwise |

> **Critical:** If you use XWayland applications, **never use negative coordinates** for your monitor positions. This is a known XWayland bug that causes click events to malfunction. Always arrange your monitors starting from `0,0` and extend into positive coordinates.

> **Note:** that "name" is a regular expression. If you want an exact match, you need to add `^` and `$` to the beginning and end of the expression, for example, `^eDP-1$` matches exactly the string `eDP-1`.

### Examples

```ini
# Laptop display: 1080p, 60Hz, positioned at origin
monitorrule=name:^eDP-1$,width:1920,height:1080,refresh:60,x:0,y:10

# Match by make and model instead of name
monitorrule=make:Chimei Innolux Corporation,model:0x15F5,width:1920,height:1080,refresh:60,x:0,y:0

# Virtual monitor with pattern matching
monitorrule=name:HEADLESS-.*,width:1920,height:1080,refresh:60,x:1926,y:0,scale:1,rr:0,vrr:0
```

---

## Monitor Spec Format

Several commands (`focusmon`, `tagmon`, `disable_monitor`, `enable_monitor`, `toggle_monitor`, `viewcrossmon`, `tagcrossmon`) accept a **monitor_spec** string to identify a monitor.

**Format:**

```text
name:xxx&&make:xxx&&model:xxx&&serial:xxx
```

- Any field can be omitted and there is no order requirement.
- If all fields are omitted, the string is treated as the monitor name directly (e.g., `eDP-1`).
- Use `wlr-randr` to find your monitor's name, make, model, and serial.

**Examples:**

```bash
# By name (shorthand)
mmsg dispatch toggle_monitor,eDP-1

# By make and model
mmsg dispatch toggle_monitor,make:Chimei Innolux Corporation&&model:0x15F5

# By serial
mmsg dispatch toggle_monitor,serial:12345678
```

---

## Tearing (Game Mode)

Tearing allows games to bypass the compositor's VSync for lower latency.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `allow_tearing` | `0` | Global tearing control: `0` (Disable), `1` (Enable), `2` (Fullscreen only). |

## HDR
> HDR is only supported in wl-only branch, since it requires the `vulkan` renderer but scenefx is not supported yet.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `hdr_depth` | `2`| Set the hdr depth for the current display. `0` is Default, `1` is HDR8, `2` is HDR10. |
| `hdr_sdr_nits` | `203` | SDR white level in nits on PQ outputs, 50-10000. `203` is the BT.2408 reference; higher values are panel- and taste-specific. `reload_config` applies it live. |

## Color Adjustment

Post-blend look-up table applied to every output, in the same state slot as a
gamma ramp. It works with HDR on, where it operates on PQ-encoded values.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `color_gamma` | `1.0` | Gamma, 0.1-10. Above 1 brightens midtones, same direction as `xrandr --gamma`. |
| `color_contrast` | `1.0` | Contrast around mid grey, 0.1-10. |
| `color_red` | `1.0` | Red channel gain, 0-4. |
| `color_green` | `1.0` | Green channel gain, 0-4. |
| `color_blue` | `1.0` | Blue channel gain, 0-4. |
| `color_yellow` | `0.0` | Warmth, 0-1. Scales blue down on top of `color_blue`; `1.0` removes blue entirely. |

Left at the defaults no table is built and the output keeps its untouched
pipeline. `reload_config` and `setoption` apply changes live. A gamma-control
client such as wlsunset writes to the same slot and will fight these values.

- you should enable HDR in monitorrule first, refer to [Monitors — Monitor Rules](/docs/configuration/monitors#monitor-rules)
- you must set `env=WLR_RENDERER,vulkan` before mango starts.

#### for example(must relogin once after setting):
```ini
env=WLR_RENDERER,vulkan
monitorrule=name:eDP-1,model:0x15F5,width:1920,height:1080,refresh:60,x:0,y:0,scale:1,vrr:0,rr:0:hdr:1
```

### Toggling HDR at runtime

`monitorrule` sets the state at startup; `togglehdr` changes it without a config
reload, the way sway's `output <name> hdr on|off|toggle` does.

```sh
mmsg dispatch togglehdr              # toggle the focused monitor
mmsg dispatch togglehdr,on           # force on
mmsg dispatch togglehdr,off,eDP-1    # a named output
mmsg dispatch togglehdr,toggle,all   # every output at once
```

With no argument it toggles the focused monitor. Reloading the config re-applies
`monitorrule` and overrides whatever `togglehdr` last set.

`all` applies to every enabled output. In toggle mode it makes one decision for
all of them — if anything is on, everything goes off — rather than flipping each
output against its own state. Outputs that cannot do HDR are skipped without
their state being touched.

### Mastering display metadata

`hdr:1` alone declares BT.2020 primaries and the PQ transfer function, but leaves
the mastering display fields at zero, so the panel has nothing to tone-map
against. Set them to your panel's values:

```ini
monitorrule=name:eDP-1,...,hdr:1,hdr_max_lum:616,hdr_max_avg_lum:400
```

`di-edid-decode` prints them under *HDR Static Metadata Data Block*. `hdr_max_lum`
is sent both as the mastering peak and as max_cll. Leaving any of the three at `0`
leaves that field unset, which is the previous behaviour.

> `hdr_min_lum` has no effect on wlroots 0.20.x: the minimum was scaled the wrong
> way in `backend/drm/atomic.c` and every value underflowed to 0. Fixed upstream
> by wlroots commit `f6a01b40`, not backported to the 0.20 branch.

### Panels whose EDID hides the HDR block

Some panels declare HDR only inside a **DisplayID 2.0** extension, with the
CTA-861 blocks nested in a container (tag `0x81`). This is legal EDID 1.4, but
wlroots reads HDR capability through libdisplay-info's CTA path and comes back
empty, so `hdr:1` is silently ignored on a panel that handles PQ.

`hdr_force:1` skips the two EDID-derived checks:

```ini
monitorrule=name:eDP-1,...,hdr:1,hdr_force:1,hdr_max_lum:616,hdr_max_avg_lum:400
```

It does not skip the renderer check: output colour transforms only exist in the
Vulkan renderer, so `WLR_RENDERER=vulkan` is still required.


### Configuration

**Enable Globally:**

```ini
allow_tearing=1
```

**Enable per Window:**

Use a window rule to force tearing for specific games.

```ini
windowrule=force_tearing:1,title:vkcube
```

### Tearing Behavior Matrix

| `force_tearing` \ `allow_tearing` | DISABLED (0) | ENABLED (1) | FULLSCREEN_ONLY (2) |
| :--- | :--- | :--- | :--- |
| **UNSPECIFIED** (0) | Not Allowed | Follows tearing_hint | Only fullscreen follows tearing_hint |
| **ENABLED** (1) | Not Allowed | Allowed | Only fullscreen allowed |
| **DISABLED** (2) | Not Allowed | Not Allowed | Not Allowed |

### Graphics Card Compatibility

> **Warning:** Some graphics cards require setting the `WLR_DRM_NO_ATOMIC` environment variable before mango starts to successfully enable tearing.

Add this to config and relogin mango:
```
env=WLR_DRM_NO_ATOMIC,1
```

---

## GPU Compatibility

If mango cannot display correctly or shows a black screen, try selecting a specific GPU:

```bash
# Use a single GPU
WLR_DRM_DEVICES=/dev/dri/card1 mango

# Use multiple GPUs
WLR_DRM_DEVICES=/dev/dri/card0:/dev/dri/card1 mango
```

Some GPUs have compatibility issues with `syncobj_enable=1` — it may crash apps like `kitty` that use syncobj. Set `env=WLR_DRM_NO_ATOMIC,1` in `config.conf` and relogin to resolve this.

---

## Power Management

You can control monitor power using the `mmsg` IPC tool.
> Notice: This sleep command does not remove the monitor, it only turns the power off.

```bash
# Turn power off
mmsg dispatch sleep_monitor,eDP-1

# Turn power on
mmsg dispatch wakeup_monitor,eDP-1

# Toggle power
mmsg dispatch sleep_toggle_monitor,eDP-1
```

You can also use `wlr-randr` for monitor management:

```bash
# remove a monitor
mmsg dispatch disable_monitor,eDP-1

# add a monitor
mmsg dispatch enable_monitor,eDP-1

# Show all monitors spec
wlr-randr
```

---

## Screen Scale(1.5 scale example)

```ini
# don't scale xwayland in global to avoid blurry
xwayland_ignore_scale=1
# scale:1.5 to scale native wayland app
monitorrule=name:eDP-1,width:1920,height:1080,refresh:60,x:0,y:0,scale:1.5
# use dpi to scale xwayland(1.5 * 96 = 144)
exec-once=echo "Xft.dpi: 144" | xrdb -merge
```
---

## Virtual Monitors

You can create and manage virtual displays through IPC commands:

```bash
# Create virtual output
mmsg dispatch create_virtual_output

# Destroy all virtual outputs
mmsg dispatch destroy_all_virtual_output
```

You can configure virtual monitors using `wlr-randr`:

```bash
# Show all monitors
wlr-randr

# Configure virtual monitor
wlr-randr --output HEADLESS-1 --pos 1921,0 --scale 1 --custom-mode 1920x1080@60Hz
```

Virtual monitors can be used for screen sharing with tools like [Sunshine](https://github.com/LizardByte/Sunshine) and [Moonlight](https://github.com/moonlight-stream/moonlight-android), allowing other devices to act as extended monitors.
