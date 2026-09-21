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

### A game ignores the mouse or the cursor is not hidden when using a fullscreen window rule

Some games (Proton/Wine titles) open a short-lived helper window (a splash/loading window showing the app id) before the real game window. A rule like

```ini
windowrule=isfullscreen:1,appid:steam_app_526870
```

matches **every** window of that app, including that helper window, so mango forces it to fullscreen as well. The helper window then gets `_NET_WM_STATE_FULLSCREEN` and a resize to the whole monitor, and when the real game window maps mango's "a new tiled window kicks the fullscreen window out of fullscreen" logic un-fullscreens and re-tiles it again. Wine ties its mouse capture and cursor visibility to those window states, so after this churn the game ends up with a broken capture state: the cursor is not hidden, the pointer lock is created and dropped repeatedly, and the game's own cursor and the compositor cursor disagree (e.g. the menu cursor is not where the game thinks it is).

Restrict `isfullscreen` to the real game window with `title:` (the helper window has no title):

```ini
windowrule=isfullscreen:1,appid:steam_app_526870,title:Satisfactory
```

Rules match on `appid` **and** `title` when both are given, so only the main window is made fullscreen.

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
