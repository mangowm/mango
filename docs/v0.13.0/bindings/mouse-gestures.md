---
title: Mouse & Gestures
description: Configure mouse buttons, scrolling, gestures, and lid switches.
---

## Mouse Bindings

Assign actions to mouse button presses with optional modifier keys.

### Syntax

```ini
mousebind=MODIFIERS,BUTTON,COMMAND,PARAMETERS
```

- **Modifiers**: `SUPER`, `CTRL`, `ALT`, `SHIFT`, `NONE`. Combine with `+` (e.g., `SUPER+CTRL`)
- **Buttons**: `btn_left`, `btn_right`, `btn_middle`, `btn_side`, `btn_extra`, `btn_forward`, `btn_back`, `btn_task`

> **Warning:** When modifiers are set to `NONE`, only `btn_middle` works in normal mode. `btn_left` and `btn_right` only work in overview mode.

### Examples

```ini
# Window manipulation
mousebind=SUPER,btn_left,moveresize,curmove
mousebind=SUPER,btn_right,moveresize,curresize
mousebind=SUPER+CTRL,btn_right,killclient

# Overview mode (requires NONE modifier)
mousebind=NONE,btn_left,toggleoverview,-1
mousebind=NONE,btn_right,killclient,0
mousebind=NONE,btn_middle,togglemaximizescreen,0
```

---

## Axis Bindings

Map scroll wheel movements to actions for workspace and window navigation.

### Syntax

```ini
axisbind=MODIFIERS,DIRECTION,COMMAND,PARAMETERS
```

- **Direction**: `UP`, `DOWN`, `LEFT`, `RIGHT`

### Examples

```ini
axisbind=SUPER,UP,viewtoleft_have_client
axisbind=SUPER,DOWN,viewtoright_have_client
```

---

## Gesture Bindings

Enable touchpad swipe gestures for navigation and window management.

### Syntax

```ini
gesturebind=MODIFIERS,DIRECTION,FINGERS,COMMAND,PARAMETERS
```

- **Direction**: `up`, `down`, `left`, `right`
- **Fingers**: `3` or `4`

> **Info:** Gestures require proper touchpad configuration. See [Input Devices](/docs/configuration/input) for touchpad settings like `tap_to_click` and `disable_while_typing`.

### Examples

```ini
# 3-finger: Window focus
gesturebind=none,left,3,focusdir,left
gesturebind=none,right,3,focusdir,right
gesturebind=none,up,3,focusdir,up
gesturebind=none,down,3,focusdir,down

# 4-finger: Workspace navigation
gesturebind=none,left,4,viewtoleft_have_client
gesturebind=none,right,4,viewtoright_have_client
gesturebind=none,up,4,toggleoverview
gesturebind=none,down,4,toggleoverview
```

---

## Switch Bindings

Trigger actions on hardware events like laptop lid open/close.

### Syntax

```ini
switchbind=FOLD_STATE,COMMAND,PARAMETERS
```

- **Fold State**: `fold` (lid closed), `unfold` (lid opened)

> **Warning:** Disable system lid handling in `/etc/systemd/logind.conf`:
>
> ```ini
> HandleLidSwitch=ignore
> HandleLidSwitchExternalPower=ignore
> HandleLidSwitchDocked=ignore
> ```

### Examples

```ini
switchbind=fold,spawn,swaylock -f -c 000000
switchbind=unfold,spawn,wlr-dpms on
```
