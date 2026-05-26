---
title: XDG Portals
description: Set up screen sharing, clipboard, keyring, and file pickers using XDG portals.
---

## Portal Configuration

You can customize portal settings via the following paths:

- **User Configuration (Priority):** `~/.config/xdg-desktop-portal/mango-portals.conf`
- **System Fallback:** `/usr/share/xdg-desktop-portal/mango-portals.conf`

> **Warning:** If you previously added `dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP=wlroots` to your config, remove it. Mango now handles this automatically.

## Screen Sharing

To enable screen sharing (OBS, Discord, WebRTC), you need `xdg-desktop-portal-wlr`.

1. **Install Dependencies**

   `pipewire`, `pipewire-pulse`, `xdg-desktop-portal-wlr`

2. **Optional: Add to autostart**

   In some situations the portal may not start automatically. You can add this to your autostart script to ensure it launches:

   ```bash
   /usr/lib/xdg-desktop-portal-wlr &
   ```

3. **Restart your computer** to apply changes.

### Known Issues

- **Window screen sharing:** Some applications may have issues sharing individual windows. See [#184](https://github.com/mangowm/mango/pull/184) for workarounds.

- **Screen recording lag:** If you experience stuttering during screen recording, see [xdg-desktop-portal-wlr#351](https://github.com/emersion/xdg-desktop-portal-wlr/issues/351).

## Clipboard Manager

Use `cliphist` to manage clipboard history.

**Dependencies:** `wl-clipboard`, `cliphist`, `wl-clip-persist`

**Autostart Config:**

```bash
# Keep clipboard content after app closes
wl-clip-persist --clipboard regular --reconnect-tries 0 &

# Watch clipboard and store history
wl-paste --type text --watch cliphist store &
```

## GNOME Keyring

If you need to store passwords or secrets (e.g., for VS Code or Minecraft launchers), install `gnome-keyring`.

**Configuration:**

Add the following to `~/.config/xdg-desktop-portal/mango-portals.conf`:

```ini
[preferred]
default=gtk
org.freedesktop.impl.portal.ScreenCast=wlr
org.freedesktop.impl.portal.Screenshot=wlr
org.freedesktop.impl.portal.Secret=gnome-keyring
org.freedesktop.impl.portal.Inhibit=none
```

## File Picker (File Selector)

**Dependencies:** `xdg-desktop-portal`, `xdg-desktop-portal-gtk`

Reboot your computer once to apply.