---
title: TOML Conversion
description: How each conf construct is written in TOML.
---

mangowm reads `.conf` and `.toml` with the same option names and values; only
the syntax differs. This page only explains how to move a conf file to TOML, not
what each option does — see the other configuration pages for that.

The format is picked from the file extension (`.conf` / `.toml`); a file with
another extension is sniffed (a leading `[` means TOML). A `source`d file is
parsed with the reader that matches its own extension, so the two can be mixed.

> **Reference example:** a complete, working TOML config lives in the
> [`mango-config` `toml` branch](https://github.com/DreamMaoMao/mango-config/tree/toml).

## How conf maps to TOML

| conf | TOML |
| :--- | :--- |
| `key=value` | `[global]` + `key = value` |
| `env=NAME,value` | `[env]` + `"NAME" = "value"` |
| `var=name,value` | `[var]` + `name = "value"` |
| `bind=mod,key,func,args` | `[bind.<key_mode>]` + `"mod+key" = "func,args"` |
| `bindc=mod,key,func,args` | `[bind.<key_mode>.conflict]` + `"mod+key" = "func,args"` |
| `mousebind=mod,btn,func,args` | `[mousebind.<key_mode>]` + `"mod+btn" = "func,args"` |
| `axisbind=mod,dir,func,args` | `[axisbind.<key_mode>]` + `"mod+dir" = "func,args"` |
| `gesturebind=mod,dir,fingers,func,args` | `[gesturebind.<key_mode>]` + `"mod+dir+fingers" = "func,args"` |
| `switchbind=fold,func,args` | `[switchbind.<key_mode>]` + `"fold" = "func,args"` |
| `monitor_rule=k:v,k:v` | `[[rule.monitor_rule]]` + `k = v` |
| `tag_rule=k:v,k:v` | `[[rule.tag_rule]]` + `k = v` |
| `layer_rule=k:v,k:v` | `[[rule.layer_rule]]` + `k = v` |
| `window_rule=k:v,k:v` | `[[rule.window_rule]]` + `k = v` |
| `window_rule_once=k:v,k:v` | `[[rule.window_rule_once]]` + `k = v` |
| `device_rule=k:v,k:v` | `[[rule.device_rule]]` + `k = v` |

## Plain options → [global]

Everything that is a single `key=value` line goes into `[global]`. Keep the same
key names and values.

```ini
blur=1
border_radius=8
root_color=0x201b14ff
animation_curve_open=0.46,1.0,0.29,1
```

```toml
[global]
blur = 1
border_radius = 8
root_color = 0x201b14ff
animation_curve_open = [0.46, 1.0, 0.29, 1]
```

## env and var → tables

In conf the name and value are packed into one comma string; in TOML they become
two keys.

```ini
env=QT_IM_MODULE,fcitx
env=XMODIFIERS,@im=fcitx
var=term,foot
```

```toml
[env]
"QT_IM_MODULE" = "fcitx"
"XMODIFIERS" = "@im=fcitx"

[var]
term = "foot"
```

Variables are still referenced as `$name` / `${name}`, in values **and** in bind
key combinations (e.g. `"$Mod+Return" = "spawn,$term"`).

Values in `[env]` and `[var]` are always treated as strings: if you write a bare
number or boolean (`"DPI" = 140`, `"FLAG" = true`), it is read as the literal
text `"140"` / `"true"`. Quoting them is still recommended.

## Bindings

In conf, `key_mode` is a state line and `bind=mod,key,func,args` follows it. In
TOML the key_mode becomes a table suffix and the modifier and key are joined with
`+` on the left of `=`.

```ini
key_mode=default
bind=Alt,Return,spawn,foot

key_mode=resize
bind=SUPER,Left,resizewin,-10,0
```

```toml
[bind.default]
"Alt+Return" = "spawn,foot"

[bind.resize]
"SUPER+Left" = "resizewin,-10,0"
```

The key_mode segment is **required** (`default` when there is no `key_mode` line).
A binding with no modifier is just the key: `"XF86AudioMute" = "spawn,..."`.
`code:N` works too: `"code:24" = "killclient"`.

The bind flag (the letters in `bindl` / `bindc` / ...) becomes a further suffix:

| conf key | TOML suffix |
| :--- | :--- |
| `bind` | *(none)* |
| `bindsym` | `.sym` |
| `bindl` | `.lock` |
| `bindr` | `.release` |
| `bindp` | `.pass` |
| `bindc` | `.conflict` |

```ini
bindc=SUPER,a,resizewin,+10,0
bindl=CTRL,comma,spawn,brightness.sh down
```

```toml
[bind.default.conflict]
"SUPER+a" = "resizewin,+10,0"

[bind.default.lock]
"CTRL+comma" = "spawn,brightness.sh down"
```

Flags can be combined, e.g. `bindrc` → `[bind.default.release.conflict]`.

The other binding kinds follow the same pattern:

```ini
mousebind=SUPER,btn_left,moveresize,curmove
axisbind=SUPER,UP,viewtoleft_have_client
gesturebind=none,left,3,focusdir,left
switchbind=fold,spawn,external-monitor on
```

```toml
[mousebind.default]
"SUPER+btn_left" = "moveresize,curmove"

[axisbind.default]
"SUPER+UP" = "viewtoleft_have_client"

[gesturebind.default]
"none+left+3" = "focusdir,left"

[switchbind.default]
"fold" = "spawn,external-monitor on"
```

## Rules

Every `xxxrule=` line becomes one `[[rule.xxx]]` element, and the `k:v,k:v` pairs
become `k = v`.

```ini
window_rule=is_floating:1,width:800,height:900,app_id:mpv
tag_rule=id:1,layout_name:tile
layer_rule=animation_type_open:zoom,layer_name:rofi
monitor_rule=name:eDP-1,width:1920,height:1080,refresh:60,x:0,y:0,scale:1
device_rule=type:trackpad,tap_to_click:1,natural_scrolling:0
```

```toml
[[rule.window_rule]]
is_floating = 1
width = 800
height = 900
app_id = "mpv"

[[rule.tag_rule]]
id = 1
layout_name = "tile"

[[rule.layer_rule]]
animation_type_open = "zoom"
layer_name = "rofi"

[[rule.monitor_rule]]
name = "eDP-1"
width = 1920
height = 1080
refresh = 60
x = 0
y = 0
scale = 1

[[rule.device_rule]]
type = "trackpad"
tap_to_click = 1
natural_scrolling = 0
```

## Repeated lines → arrays

TOML forbids repeating a key inside one table, so conf keys that appear on many
lines become **one key with an array value**. This applies to `source`,
`source_optional`, `exec` and `exec_once`.

```ini
source=./env.conf
source=./bind.conf
exec_once=waybar
exec_once=swaybg -i wall.png
```

```toml
[global]
source = ["./env.toml", "./bind.toml"]
exec_once = ["waybar", "swaybg -i wall.png"]
```

Repeated rules (`window_rule`, `tag_rule`, ...) use array-of-tables instead, so
each `[[rule.<type>]]` is a new rule. Repeating the *same key combination* in one
bind table is the one remaining case: put the actions in an array.

```toml
[bind.default]
"SUPER+r" = ["spawn_shell,config-check.sh", "reload_config"]
```

## Values

- Strings are quoted (`"foot"`, `'foot'`); numbers and booleans stay bare
  (`8`, `0.9`, `true`, `false`).
- Colors stay `0xRRGGBBAA` (`0x201b14ff`).
- Comma lists (`animation_curve_*`, `scroller_proportion_preset`,
  `circle_layout`) become arrays: `0.5,0.8,1.0` → `[0.5, 0.8, 1.0]`.
- A value that is itself a string (`"us,ru"`, a command line) is quoted, so it
  may contain commas.

> **Note:** In TOML a table runs until the next header. All top-level options
> (`source`, `exec`, plain settings, `[env]`, `[var]`) must come before the first
> binding or rule table; put rules and `[bind.*]` tables after them.
