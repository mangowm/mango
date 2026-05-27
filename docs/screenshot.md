---

title: Screenshots
description: Example screenshot keybindings and capture workflows for mangowm.

---

mangowm does not include a built-in screenshot tool. This keeps the compositor lean.
Instead, compose your own workflow from small Wayland utilities and bind them to keys;

| Tool | Purpose |
| :--- | :--- |
| [`grim`](https://github.com/emersion/grim) | Capture the screen or a region to a file |
| [`slurp`](https://github.com/emersion/slurp) | Interactively select a region for `grim` |
| [`wl-copy`](https://github.com/bugaevc/wl-clipboard) | Copy screenshots directly to the clipboard |
| [`satty`](https://github.com/gabm/Satty) | Annotate screenshots before saving |
| [`wayfreeze`](https://github.com/nicbk/wayfreeze) | Freeze the screen before capture |

Install the required with your package manager or from source.

`grim` writes to the file path you give it, but **will not create missing directories**. Create one first:

```bash
mkdir -p ~/Pictures/Screenshots
```

Any directory works. `~/Pictures/Screenshots/` is just a convention.

## Quick Binds

Short, single-step commands can be placed directly in `config.conf` with `spawn_shell`.
No script file needed.

### Fullscreen

Captures the entire display.

```ini
bind=NONE,Print,spawn_shell,grim $HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png
```

### Region

Select an area with `slurp` before capturing.

```ini
bind=SHIFT,Print,spawn_shell,g=$(slurp -d) && [ -n "$g" ] && grim -g "$g" $HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png
```

### Pointer

Captures the full screen including the cursor.

```ini
bind=ALT,Print,spawn_shell,grim -c $HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png
```

### Clipboard

Captures to a temporary file and copies the image to the clipboard; no file is saved.

```ini
bind=CTRL,Print,spawn_shell,f=$(mktemp -t screenshot-XXXXXX.png) && grim "$f" && wl-copy < "$f" && rm -f "$f"
```

### Annotate

Captures and opens `satty` for drawing before saving.

```ini
bind=SUPER,Print,spawn_shell,f=$HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png && grim "$f" && satty --filename "$f" --output-filename "$f" --actions-on-enter save-to-file --early-exit
```

## Script Binds

When a command involves multi-step logic, geometry parsing, FIFOs, or screen freezing,
move it into a script and invoke it with `spawn` instead of `spawn_shell`.

Create the scripts directory first:

```bash
mkdir -p ~/.config/mango/scripts/screenshot
```

### Window

Uses `mmsg` (ships with mango) to capture the focused window.

`~/.config/mango/scripts/screenshot/window.sh`:

```bash
#!/usr/bin/env bash
geometry=$(mmsg get focusing-client | jq -r '"\(.x),\(.y) \(.width)x\(.height)"')
[ -z "$geometry" ] && exit 1
grim -g "$geometry" "$HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png"
```

```ini
bind=CTRL+SHIFT,Print,spawn,$HOME/.config/mango/scripts/screenshot/window.sh
```

### Freeze

Freezes the screen with `wayfreeze` before capturing.

`~/.config/mango/scripts/screenshot/freeze.sh`:

```bash
#!/usr/bin/env bash
pipe=$(mktemp -u).fifo
mkfifo "$pipe"
wayfreeze --after-freeze-timeout 100 --after-freeze-cmd "echo > $pipe" &
wayfreeze_pid=$!
read -r < "$pipe"
grim "$HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png"
kill "$wayfreeze_pid" 2>/dev/null
rm -f "$pipe"
```

```ini
bind=CTRL+SUPER,Print,spawn,$HOME/.config/mango/scripts/screenshot/freeze.sh
```

### Freeze + Region

Freeze, then select a region with `slurp`. Cleans up on cancel.

`~/.config/mango/scripts/screenshot/freeze-region.sh`:

```bash
#!/usr/bin/env bash
pipe=$(mktemp -u).fifo
mkfifo "$pipe"
wayfreeze --after-freeze-timeout 100 --after-freeze-cmd "echo > $pipe" &
wayfreeze_pid=$!
read -r < "$pipe"
geometry=$(slurp -d)
if [[ -z "$geometry" ]]; then
  kill "$wayfreeze_pid" 2>/dev/null
  rm -f "$pipe"
  exit 1
fi
grim -g "$geometry" "$HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png"
kill "$wayfreeze_pid" 2>/dev/null
rm -f "$pipe"
```

```ini
bind=SHIFT+SUPER,Print,spawn,$HOME/.config/mango/scripts/screenshot/freeze-region.sh
```

Make all three scripts executable:

```bash
chmod +x ~/.config/mango/scripts/screenshot/*.sh
```

## All-in-One Script

Prefer fewer files? A single script with subcommands covers every mode above.
Place it in the same directory and use it in place of the individual scripts.

`~/.config/mango/scripts/screenshot/screenshot.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail
mkdir -p "$HOME/Pictures/Screenshots"
filepath="$HOME/Pictures/Screenshots/$(date +%Y%m%d%H%M%S).png"

case "${1:-fullscreen}" in
  region)
    g=$(slurp -d); [ -z "$g" ] && exit 1
    grim -g "$g" "$filepath" ;;
  window)
    g=$(mmsg get focusing-client | jq -r '"\(.x),\(.y) \(.width)x\(.height)"')
    [ -z "$g" ] && exit 1
    grim -g "$g" "$filepath" ;;
  freeze)
    p=$(mktemp -u).fifo; mkfifo "$p"
    wayfreeze --after-freeze-timeout 100 --after-freeze-cmd "echo > $p" & wp=$!
    read -r < "$p"; grim "$filepath"
    kill "$wp" 2>/dev/null; rm -f "$p" ;;
  freeze-region)
    p=$(mktemp -u).fifo; mkfifo "$p"
    wayfreeze --after-freeze-timeout 100 --after-freeze-cmd "echo > $p" & wp=$!
    read -r < "$p"; g=$(slurp -d)
    if [ -z "$g" ]; then kill "$wp" 2>/dev/null; rm -f "$p"; exit 1; fi
    grim -g "$g" "$filepath"
    kill "$wp" 2>/dev/null; rm -f "$p" ;;
  annotate)
    grim "$filepath"; satty --filename "$filepath" --output-filename "$filepath" --actions-on-enter save-to-file --early-exit ;;
  *) grim "$filepath" ;;
esac
```

Make the script executable:


```bash
chmod +x ~/.config/mango/scripts/screenshot/screenshot.sh
```

Then add the binds to `config.conf`:

```ini
bind=NONE,Print,spawn,$HOME/.config/mango/scripts/screenshot/screenshot.sh fullscreen
bind=SHIFT,Print,spawn,$HOME/.config/mango/scripts/screenshot/screenshot.sh region
bind=CTRL+SHIFT,Print,spawn,$HOME/.config/mango/scripts/screenshot/screenshot.sh window
bind=CTRL+SUPER,Print,spawn,$HOME/.config/mango/scripts/screenshot/screenshot.sh freeze
bind=SHIFT+SUPER,Print,spawn,$HOME/.config/mango/scripts/screenshot/screenshot.sh freeze-region
bind=SUPER,Print,spawn,$HOME/.config/mango/scripts/screenshot/screenshot.sh annotate
```
