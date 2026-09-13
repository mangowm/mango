---
title: FAQ
description: Frequently asked questions and troubleshooting.
---

### How do I arrange tiled windows with my mouse?

You can enable the `drag_tile_to_tile` option in your config. This allows you to drag a tiled window onto another to swap them.

```ini
drag_tile_to_tile=1
```

---

### Why is my background blurry or why does blur look wrong?

Blur applies to the transparent areas of windows. To disable it entirely, set `blur=0`.

If you are experiencing **performance issues with blur**, make sure `blur_optimized=1` (the default). This caches the wallpaper as the blur background, which is much cheaper on the GPU:

```ini
blur_optimized=1
```

---

### Blur shows my wallpaper instead of the real background content

This is expected behavior when `blur_optimized=1` (the default). The optimizer caches the wallpaper to reduce GPU load — windows will blur against the wallpaper rather than the actual content stacked beneath them.

If you want blur to composite against the true background (i.e., show whatever is actually behind the window), set:

```ini
blur_optimized=0
```

> **Warning:** Disabling `blur_optimized` significantly increases GPU consumption and may cause rendering lag, especially on lower-end hardware.

---

### My games are lagging or stuttering

Try enabling **SyncObj** timeline support.

```ini
syncobj_enable=1
```

---

### My games have high input latency

You can enable **Tearing** (similar to VSync off).

First, enable it globally:

```ini
allow_tearing=1
```

Then force it for your specific game:

```ini
windowrule=force_tearing:1,title:Counter-Strike 2
```

> **Warning:** Some graphics cards require setting `env=WLR_DRM_NO_ATOMIC,1` in config before mango starts for tearing to work. 
> See [Monitors — Tearing](/docs/configuration/monitors#tearing-game-mode) for details.

---

### The cursor escapes to another monitor when playing a fullscreen game

Some X11 games (typically Proton/Wine titles, e.g. Satisfactory) capture the mouse by hiding the cursor and grabbing the pointer with `confine_to`. Xwayland emulates that with a locked pointer constraint, and under some conditions Xwayland drops that lock by itself (when the game releases the X11 confine while keeping the mouse captured). Once the lock is gone the compositor has nothing left to confine the pointer, so the cursor can move onto another monitor.

Add a window rule to force the cursor to stay inside the game window while it is focused:

```ini
windowrule=confine_pointer:1,appid:steam_app_526870
```

Use `appid:` or `title:` to match your game — see [Window Rules](/docs/window-management/rules) for the available matching options.

`confine_pointer=1` keeps the cursor 5px inside the window's surface whenever that window is the focused, visible window. The confinement is released as soon as the window loses focus, gets hidden, minimized, or moved to a hidden tag. It does not rely on the pointer constraints protocol, so it keeps working even after Xwayland drops the game's pointer lock.

---

### How do I use pipes `|` in spawn commands?

The standard `spawn` command does not support shell pipes directly. You must use `spawn_shell` instead.

```ini
bind=SUPER,P,spawn_shell,echo "hello" | rofi -dmenu
```

---

### Certain key combinations do not work on my keyboard layout.

`bind` automatically converts keysym to keycode, which is compatible with most layouts but can sometimes be imprecise. If a key combination is not triggering, use the **keycode** directly instead of the key name.

Run `wev` and press the key to find its keycode, then use it in your bind:

```ini
# Instead of:
bind=ALT,q,killclient

# Use the keycode (e.g., code:24 = q on most layouts):
bind=ALT,code:24,killclient
```

You can also use `binds` (the `s` flag) to match by keysym instead of keycode:

```ini
binds=ALT,q,killclient
```
