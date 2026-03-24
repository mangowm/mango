---
title: Input Devices
description: Configure keyboard layouts, mouse sensitivity, and touchpad gestures.
---

## Device Configuration

mangowm provides granular control over different input devices.

### Keyboard Settings

Control key repeat rates and layout rules.

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `repeat_rate` | `int` | `25` | How many times a key repeats per second. |
| `repeat_delay` | `int` | `600` | Delay (ms) before a held key starts repeating. |
| `numlockon` | `0` or `1` | `0` | Enable NumLock on startup. |
| `xkb_rules_rules` | `string` | - | XKB rules file (e.g., `evdev`, `base`). Usually auto-detected. |
| `xkb_rules_model` | `string` | - | Keyboard model (e.g., `pc104`, `macbook`). |
| `xkb_rules_layout` | `string` | - | Keyboard layout code (e.g., `us`, `de`, `us,de`). |
| `xkb_rules_variant` | `string` | - | Layout variant (e.g., `dvorak`, `colemak`, `intl`). |
| `xkb_rules_options` | `string` | - | XKB options (e.g., `caps:escape`, `ctrl:nocaps`). |

**Example:**

```ini
repeat_rate=40
repeat_delay=300
numlockon=1
xkb_rules_layout=us,de
xkb_rules_variant=dvorak
xkb_rules_options=caps:escape,ctrl:nocaps
```

---

### Trackpad Settings

Specific settings for laptop touchpads. Some settings may require a relogin to take effect.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `disable_trackpad` | `0` | Set to `1` to disable the trackpad entirely. |
| `tap_to_click` | `1` | Tap to trigger a left click. |
| `tap_and_drag` | `1` | Tap and hold to drag items. |
| `trackpad_natural_scrolling` | `0` | Invert scrolling direction (natural scrolling). |
| `scroll_method` | `1` | `1` (Two-finger), `2` (Edge), `4` (Button). |
| `click_method` | `1` | `1` (Button areas), `2` (Clickfinger). |
| `drag_lock` | `1` | Lock dragging after tapping. |
| `disable_while_typing` | `1` | Disable trackpad while typing. |
| `left_handed` | `0` | Swap left/right buttons. |
| `middle_button_emulation` | `0` | Emulate middle button. |
| `swipe_min_threshold` | `1` | Minimum swipe threshold. |

---

**Detailed descriptions:**

- `scroll_method` values:
  - `0` — Never send scroll events (no scrolling).
  - `1` — Two-finger scrolling: send scroll events when two fingers are logically down on the device.
  - `2` — Edge scrolling: send scroll events when a finger moves along the bottom or right edge.
  - `4` — Button scrolling: send scroll events when a button is held and the device moves along a scroll axis.

- `click_method` values:
  - `0` — No software click emulation.
  - `1` — Button areas: use software-defined areas on the touchpad to generate button events.
  - `2` — Clickfinger: the number of fingers determines which button is pressed.

- `accel_profile` values:
  - `0` — No acceleration.
  - `1` — Flat: no dynamic acceleration. Pointer speed = original input speed × (1 + `accel_speed`).
  - `2` — Adaptive: slow movement results in less acceleration, fast movement results in more.

- `button_map` values:
  - `0` — 1/2/3 finger tap maps to left / right / middle.
  - `1` — 1/2/3 finger tap maps to left / middle / right.

- `send_events_mode` values:
  - `0` — Send events from this device normally.
  - `1` — Do not send events from this device.
  - `2` — Disable this device when an external pointer device is plugged in.

---

### Mouse Settings

Configuration for external mice.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `mouse_natural_scrolling` | `0` | Invert scrolling direction. |
| `accel_profile` | `2` | `0` (None), `1` (Flat), `2` (Adaptive). |
| `accel_speed` | `0.0` | Speed adjustment (-1.0 to 1.0). |
| `left_handed` | `0` | Swap left and right buttons. |
| `middle_button_emulation` | `0` | Emulate middle button. |
| `swipe_min_threshold` | `1` | Minimum swipe threshold. |
| `send_events_mode` | `0` | `0` (Enabled), `1` (Disabled), `2` (Disabled on external mouse). |
| `button_map` | `0` | `0` (Left/right/middle), `1` (Left/middle/right). |

---

---

## Keyboard Layout Switching

To bind multiple layouts and toggle between them, define the layouts in `xkb_rules_layout` and use `xkb_rules_options` to set a toggle key combination. Then bind `switch_keyboard_layout` to trigger a switch.

```ini
# Define two layouts: US QWERTY and US Dvorak
xkb_rules_layout=us,us
xkb_rules_variant=,dvorak
xkb_rules_options=grp:lalt_lshift_toggle
```

Or bind it manually to a key:

```ini
# Bind Alt+Shift_L to cycle keyboard layout
bind=alt,shift_l,switch_keyboard_layout
```

Use `mmsg -g -k` to query the current keyboard layout at any time.

---

## Input Method Editor (IME)

To use Fcitx5 or IBus, set these environment variables in your config file.

> **Info:** These settings require a restart of the window manager to take effect.

**For Fcitx5:**

```ini
env=GTK_IM_MODULE,fcitx
env=QT_IM_MODULE,fcitx
env=QT_IM_MODULES,wayland;fcitx
env=SDL_IM_MODULE,fcitx
env=XMODIFIERS,@im=fcitx
env=GLFW_IM_MODULE,ibus
```

**For IBus:**

```ini
env=GTK_IM_MODULE,ibus
env=QT_IM_MODULE,ibus
env=XMODIFIERS,@im=ibus
```
