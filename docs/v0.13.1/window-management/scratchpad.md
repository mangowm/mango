---
title: Scratchpad
description: Manage hidden "scratchpad" windows for quick access.
---

mangowm supports two types of scratchpads: the standard pool (Sway-like) and named scratchpads.

## Standard Scratchpad

Any window can be sent to the "scratchpad" pile, which hides it. You can then cycle through them.

**Keybindings:**

```ini
# Send current window to scratchpad
bind=SUPER,i,minimized

# Toggle (show/hide) the scratchpad
bind=ALT,z,toggle_scratchpad

# Retrieve window from scratchpad (restore)
bind=SUPER+SHIFT,i,restore_minimized
```

---

## Named Scratchpad

Named scratchpads are bound to specific keys and applications. When triggered, mangowm will either launch the app (if not running) or toggle its visibility.

**1. Define the Window Rule**

You must identify the app using a unique `appid` or `title` and mark it as a named scratchpad. The application must support setting a custom appid or title at launch. Common examples:

- `st -c my-appid` — sets the appid
- `kitty -T my-title` — sets the window title
- `foot --app-id my-appid` — sets the appid

Use `none` as a placeholder when you only want to match by one field.

```ini
# Match by appid
windowrule=isnamedscratchpad:1,width:1280,height:800,appid:st-yazi

# Match by title
windowrule=isnamedscratchpad:1,width:1000,height:700,title:kitty-scratch
```

**2. Bind the Toggle Key**

Format: `bind=MOD,KEY,toggle_named_scratchpad,appid,title,command`

Use `none` for whichever field you are not matching on.

```ini
# Match by appid: launch 'st' with class 'st-yazi' running 'yazi'
bind=alt,h,toggle_named_scratchpad,st-yazi,none,st -c st-yazi -e yazi

# Match by title: launch 'kitty' with window title 'kitty-scratch'
bind=alt,k,toggle_named_scratchpad,none,kitty-scratch,kitty -T kitty-scratch
```

---

## Appearance

You can customize the size of scratchpad windows relative to the screen.

```ini
scratchpad_width_ratio=0.8
scratchpad_height_ratio=0.9
scratchpadcolor=0x516c93ff
```
