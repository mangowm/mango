---
title: Basic Configuration
description: Learn how to configure mangowm files, environment variables, and autostart scripts.
---

## Configuration File

mangowm uses a simple configuration file format. By default, it looks for a configuration file in `~/.config/mango/`.

1. **Locate Default Config**

   A fallback configuration is provided at `/etc/mango/config.conf`. You can use this as a reference.

2. **Create User Config**

   Copy the default config to your local config directory to start customizing.

   ```bash
   mkdir -p ~/.config/mango
   cp /etc/mango/config.conf ~/.config/mango/config.conf
   ```

3. **Launch with Custom Config (Optional)**

   If you prefer to keep your config elsewhere, you can launch mango with the `-c` flag.

   ```bash
   mango -c /path/to/your_config.conf
   ```

### TOML Format

mangowm also reads TOML config files. When looking for a user config,
`~/.config/mango/config.conf` is preferred, then `~/.config/mango/config.toml`
(likewise under `/etc/mango/`).

See [TOML Conversion](/docs/configuration/toml) for the syntax and how to convert
an existing conf file.

### Sub-Configuration

To keep your configuration organized, you can split it into multiple files and include them using the `source` keyword.

```ini
# Import keybindings from a separate file
source=~/.config/mango/bind.conf

# Relative paths work too
source=./theme.conf

# Optional: ignore if file doesn't exist (useful for shared configs)
source_optional=~/.config/mango/optional.conf
```

### Validate Configuration

You can check your configuration for errors without starting mangowm:

```bash
mango -c /path/to/config.conf -p
```

Use with `source_optional` for shared configs across different setups.

## Environment Variables

You can define environment variables directly within your config file. These are set before the window manager fully initializes.

> **Warning:** Environment variables defined here will be **reset** every time you reload the configuration.

```ini
env=QT_IM_MODULES,wayland;fcitx
env=XMODIFIERS,@im=fcitx
```

## Configuration Variables

You can define your own variables and reuse them anywhere in the config. A
variable is defined with `var=name,value` and referenced with `$name` or
`${name}`. Names must start with a letter or `_`, followed by letters, digits,
or `_`.

```ini
var=term,kitty
var=editor,nvim
var=screenshot_dir,~/Pictures/Screenshots

bind=SUPER,Return,spawn,$term
bind=SUPER,E,spawn,$term -e $editor
bind=SUPER,Print,spawn_shell,grim ${screenshot_dir}/$(date +%Y%m%d%H%M%S).png
```

Variables must be defined before they are used, and they are shared across
`source`/`source_optional` files (the included file can use variables defined
before the `source` line).

> **Note:** Only names you actually defined are expanded. Anything else is left
> untouched, so shell commands keep working exactly as before:
>
> ```ini
> # $HOME, $(date ...), $1 and $$ are not config variables here,
> # so the shell still receives them verbatim.
> bind=SUPER,P,spawn_shell,echo "$HOME" | rofi -dmenu
> ```
>
> Defining a variable with the same name as a shell variable you use in a
> `spawn_shell` command will cause mango to expand it first. Prefer distinctive
> names to avoid surprises.

## Autostart

mangowm can automatically run commands or scripts upon startup. There are two modes for execution:

| Command | Behavior | Usage Case |
| :--- | :--- | :--- |
| `exec_once` | Runs **only once** when mangowm starts. | Status bars, Wallpapers, Notification daemons |
| `exec` | Runs **every time** the config is reloaded. | Scripts that need to refresh settings |

### Example Setup

```ini
# Start the status bar once
exec_once=waybar

# Set wallpaper
exec_once=swaybg -i ~/.config/mango/wallpaper/room.png

# Reload a custom script on config change
exec=bash ~/.config/mango/reload-settings.sh
```
