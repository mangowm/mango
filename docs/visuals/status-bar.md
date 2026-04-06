---
title: Status Bar
description: Configure Waybar for mangowm.
---

## Module Configuration

mangowm is compatible with Waybar's `ext/workspaces` module (Wayland standard) or the `dwl/tags` module. We recommend `ext/workspaces` for the best experience.

> **Tip:** You can also use the `dwl/tags` module, but `ext/workspaces` provides better integration with mangowm's features. The `ext/workspaces` module requires **Waybar > 0.14.0**.

### `config.jsonc`

Add the following to your Waybar configuration:

```jsonc
{
  "modules-left": [
    "ext/workspaces",
    "dwl/window"
  ],
  "ext/workspaces": {
    "format": "{icon}",
    "ignore-hidden": true,
    "on-click": "activate",
    "on-click-right": "deactivate",
    "sort-by-id": true
  },
  "dwl/window": {
    "format": "[{layout}] {title}"
  }
}
```

## Styling

You can style the tags using standard CSS in `style.css`.

### `style.css`

```css
#workspaces {
  border-radius: 4px;
  border-width: 2px;
  border-style: solid;
  border-color: #c9b890;
  margin-left: 4px;
  padding-left: 10px;
  padding-right: 6px;
  background: rgba(40, 40, 40, 0.76);
}

#workspaces button {
  border: none;
  background: none;
  box-shadow: inherit;
  text-shadow: inherit;
  color: #ddca9e;
  padding: 1px;
  padding-left: 1px;
  padding-right: 1px;
  margin-right: 2px;
  margin-left: 2px;
}

#workspaces button.hidden {
  color: #9e906f;
  background-color: transparent;
}

#workspaces button.visible {
  color: #ddca9e;
}

#workspaces button:hover {
  color: #d79921;
}

#workspaces button.active {
  background-color: #ddca9e;
  color: #282828;
  margin-top: 5px;
  margin-bottom: 5px;
  padding-top: 1px;
  padding-bottom: 0px;
  border-radius: 3px;
}

#workspaces button.urgent {
  background-color: #ef5e5e;
  color: #282828;
  margin-top: 5px;
  margin-bottom: 5px;
  padding-top: 1px;
  padding-bottom: 0px;
  border-radius: 3px;
}

#tags {
  background-color: transparent;
}

#tags button {
  background-color: #fff;
  color: #a585cd;
}

#tags button:not(.occupied):not(.focused) {
  font-size: 0;
  min-width: 0;
  min-height: 0;
  margin: -17px;
  padding: 0;
  color: transparent;
  background-color: transparent;
}

#tags button.occupied {
  background-color: #fff;
  color: #cdc885;
}

#tags button.focused {
  background-color: rgb(186, 142, 213);
  color: #fff;
}

#tags button.urgent {
  background: rgb(171, 101, 101);
  color: #fff;
}

#window {
  background-color: rgb(237, 196, 147);
  color: rgb(63, 37, 5);
}
```

## Complete Configuration Example

> **Tip:** You can find a complete Waybar configuration for mangowm at [waybar-config](https://github.com/DreamMaoMao/waybar-config).