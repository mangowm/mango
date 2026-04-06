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
| `rr` | integer | 0-7 | Monitor transform |
| `custom` | integer | 0, 1 | Enable custom mode (not supported on all displays — may cause black screen) |

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
mmsg -d toggle_monitor,eDP-1

# By make and model
mmsg -d toggle_monitor,make:Chimei Innolux Corporation&&model:0x15F5

# By serial
mmsg -d toggle_monitor,serial:12345678
```

---

## Tearing (Game Mode)

Tearing allows games to bypass the compositor's VSync for lower latency.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `allow_tearing` | `0` | Global tearing control: `0` (Disable), `1` (Enable), `2` (Fullscreen only). |

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

Add this to `/etc/environment` and reboot:

```bash
WLR_DRM_NO_ATOMIC=1
```

Or run mango with the environment variable:

```bash
WLR_DRM_NO_ATOMIC=1 mango
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

Some GPUs have compatibility issues with `syncobj_enable=1` — it may crash apps like `kitty` that use syncobj. Set `WLR_DRM_NO_ATOMIC=1` in `/etc/environment` and reboot to resolve this.

---

## Power Management

You can control monitor power using the `mmsg` IPC tool.

```bash
# Turn off
mmsg -d disable_monitor,eDP-1

# Turn on
mmsg -d enable_monitor,eDP-1

# Toggle
mmsg -d toggle_monitor,eDP-1
```

You can also use `wlr-randr` for monitor management:

```bash
# Turn off monitor
wlr-randr --output eDP-1 --off

# Turn on monitor
wlr-randr --output eDP-1 --on

# Show all monitors
wlr-randr
```

---

## Screen Scale

### Without Global Scale (Recommended)

- If you do not use XWayland apps, you can use monitor rules or `wlr-randr` to set a global monitor scale.
- If you are using XWayland apps, it is not recommended to set a global monitor scale.

You can set scale like this, for example with a 1.4 factor.

**Dependencies:**

```bash
yay -S xorg-xrdb
yay -S xwayland-satellite
```

**In config file:**

```ini
env=QT_AUTO_SCREEN_SCALE_FACTOR,1
env=QT_WAYLAND_FORCE_DPI,140
```

**In autostart:**

```bash
echo "Xft.dpi: 140" | xrdb -merge
gsettings set org.gnome.desktop.interface text-scaling-factor 1.4
```

**Edit autostart for XWayland:**

```bash
# Start xwayland
/usr/sbin/xwayland-satellite :11 &
# Apply scale 1.4 for xwayland
sleep 0.5s && echo "Xft.dpi: 140" | xrdb -merge
```

### Using xwayland-satellite to Prevent Blurry XWayland Apps

If you use fractional scaling, you can use `xwayland-satellite` to automatically scale XWayland apps to prevent blurriness, for example with a scale of 1.4.

**Dependencies:**

```bash
yay -S xwayland-satellite
```

**In config file:**

```ini
env=DISPLAY,:2
exec-once=xwayland-satellite :2
monitorrule=name:eDP-1,width:1920,height:1080,refresh:60,x:0,y:0,scale:1.4,vrr:0,rr:0
```

> **Warning:** Use a `DISPLAY` value other than `:1` to avoid conflicting with mangowm.

---

## Virtual Monitors

You can create and manage virtual displays through IPC commands:

```bash
# Create virtual output
mmsg -d create_virtual_output

# Destroy all virtual outputs
mmsg -d destroy_all_virtual_output
```

You can configure virtual monitors using `wlr-randr`:

```bash
# Show all monitors
wlr-randr

# Configure virtual monitor
wlr-randr --output HEADLESS-1 --pos 1921,0 --scale 1 --custom-mode 1920x1080@60Hz
```

Virtual monitors can be used for screen sharing with tools like [Sunshine](https://github.com/LizardByte/Sunshine) and [Moonlight](https://github.com/moonlight-stream/moonlight-android), allowing other devices to act as extended monitors.