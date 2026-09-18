---
title: Basic Configuration
description: Learn how to configure mangowm files, environment variables, and autostart scripts.
---

## Configuration File

mangowm uses a simple configuration file format. By default, it looks for a configuration file in `~/.config/mango/`.

1. **Locate Default Config**

   A fallback configuration is provided at `/etc/mango/config.toml`. You can use this as a reference.

2. **Create User Config**

   Copy the default config to your local config directory to start customizing.

   ```bash
   mkdir -p ~/.config/mango
   cp /etc/mango/config.toml ~/.config/mango/config.toml
   ```

3. **Launch with Custom Config (Optional)**

   If you prefer to keep your config elsewhere, you can launch mango with the `-c` flag.

   ```bash
   mango -c /path/to/your_config.toml
   ```

### Sub-Configuration

To keep your configuration organized, you can split it into multiple files and include them using the `source` keyword.

```ini
# Import keybindings from a separate file
source=~/.config/mango/bind.conf

# Relative paths work too
source=./theme.conf

# Optional: ignore if file doesn't exist (useful for shared configs)
source-optional=~/.config/mango/optional.conf
```

### Headers

Mango's configuration supports headers using a TOML-like format:
```ini
[deco.animation]
duration.close = 200
duration.open = 200
duration.focus = 400
duration.move = 300
duration.tag = 400
```

The above is equal to the following:
```ini
deco.animation.duration.close = 200
deco.animation.duration.open = 200
deco.animation.duration.focus = 400
deco.animation.duration.move = 300
deco.animation.duration.tag = 400
```

The header text is effectively prepended to each option in that "block", a new header overwrites a previous one.

Each config file (including sub-configurations) starts with the header set to `[]`, meaning none. you can reset the header like this at any time.

headers also have the ability to be conditional, a condition can be added with the `?` symbol:
```ini
[deco.animation ? test "$ENVIRONMENT_VARIABLE" == "sometext"]
duration.open = 200
...
```

this condition can also be used with an empty block: `[? test $ENV == true]`

Conditional blocks will suppress all settings after them until the next block if the provided command after the `?` exits non-0:
```ini
# this would be applied
[deco.animation ? exit 0]
duration.open = 200

# this would not
[deco.animation ? exit 1]
duration.close = 200
```

Some options are "global", which means they will always work regardless of header. this is the case for all legacy versions of options, as well as the following:
- all bind-related options
- all rule-related options
- exec/exec-once
- source/source-optional
- keymode
- env

These *will* still respect conditional blocks, but can be added under any header, so the following is valid:
```ini
# nested env key is global, and won't get resolved to deco.animation.env
[deco.animation]
duration.open = 200
env = OPENDURATION,200
```


### Validate Configuration

You can check your configuration for errors without starting mangowm:

```bash
mango -c /path/to/config.toml -p
```

Use with `source-optional` for shared configs across different setups.

## Environment Variables

You can define environment variables directly within your config file. These are set before the window manager fully initializes.

> **Warning:** Environment variables defined here will be **reset** every time you reload the configuration.

```ini
env=QT_IM_MODULES,wayland;fcitx
env=XMODIFIERS,@im=fcitx
```

## Autostart

mangowm can automatically run commands or scripts upon startup. There are two modes for execution:

| Command | Behavior | Usage Case |
| :--- | :--- | :--- |
| `exec-once` | Runs **only once** when mangowm starts. | Status bars, Wallpapers, Notification daemons |
| `exec` | Runs **every time** the config is reloaded. | Scripts that need to refresh settings |

### Example Setup

```ini
# Start the status bar once
exec-once=waybar

# Set wallpaper
exec-once=swaybg -i ~/.config/mango/wallpaper/room.png

# Reload a custom script on config change
exec=bash ~/.config/mango/reload-settings.sh
```
