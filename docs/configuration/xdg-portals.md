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

   `pipewire`, `pipewire-pulse`, `xdg-desktop-portal-wlr`, `rofi`

   > **Note:** `xdg-desktop-portal-wlr` has no picker of its own. When an application asks to share a screen, it launches an external one and tries `slurp`, `wmenu`, `wofi`, `rofi`, `bemenu`, `mew` and `fuzzel` in that order. If none of them is installed the picker never appears and sharing just fails, so install at least one of them (`rofi` is the common choice). This is not needed if you skip the picker as described below.

2. **Optional: Add to autostart**

   In some situations the portal may not start automatically. You can add this to your autostart script to ensure it launches:

   ```bash
   /usr/lib/xdg-desktop-portal-wlr &
   ```

3. **Restart your computer** to apply changes.

### Known Issues

- **Tencent Meeting asks for a screen but nothing happens:** `xdg-desktop-portal-wlr` opens its chooser during `SelectSources()` and only replies once you picked something, while Tencent Meeting calls `Start()` immediately and never waits for that reply. The portal frontend then rejects the start with `Sources not selected`, so the picker closes without sharing anything. Pick a fixed output and skip the chooser:

  ```ini
  # ~/.config/xdg-desktop-portal-wlr/config
  [screencast]
  output_name=eDP-1
  chooser_type=none
  ```

  Replace `eDP-1` with your output name (see `mmsg get all-monitors`). The configuration is only read when the portal starts, so restart it afterwards (a re-login or reboot works too):

  ```bash
  systemctl --user restart xdg-desktop-portal-wlr
  ```

  This applies to every application: screen sharing always captures that output and never asks. Applications that do wait for the reply (OBS, Firefox, Chromium) also work with the default chooser, so only add this if you need it. Tencent Meeting additionally only gets the first share attempt per app start right, so restart it before sharing.

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
