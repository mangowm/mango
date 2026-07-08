---
title: Status Bar
description: Configure Waybar for mangowm.
---

## Module Configuration

mangowm is compatible with Waybar's `ext/workspaces` module (Wayland standard) or the `dwl/tags` module. We recommend `ext/workspaces` for the best experience.

### `config.jsonc`

Add the following to your Waybar configuration:

```jsonc
{
  "modules-left": [
    "mango/workspaces",
    "mango/layout",
    "mango/window"
  ],
  "modules-right": [
    "mango/language",
    "mango/keymode",
  ],
  "mango/workspaces": {
      "format": "{icon}",
      "hide-empty": true,
      "on-click": "activate",
      "on-click-right": "toggle",
      "overview-label": "OVERVIEW",
  },
  "mango/keymode": {
  	"format": "{}",
  	// "format-default": " Default",
    // "format-test": " Test",
  },
  "mango/window": {
    "format": "{}",
	  "icon-size": 20
  },
  "mango/layout": {
      "format": "{}",
      // "format-S": "Scroller",
      // "format-T": "Tile",
  },
  "mango/language": {
  "format": "{short}",
  },
}
```

## Styling Example

You can style the tags using standard CSS in `style.css`.

### `style.css`

```css
#workspaces {
  border-color: #c9b890;
  background: rgba(40, 40, 40, 0.76);
}

#workspaces button {
  background: none;
  color: #ddca9e;
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
}

#workspaces button.urgent {
  background-color: #ef5e5e;
  color: #282828;
}

#workspaces button.overview {
  background-color: #ef5e5e;
  color: #282828;
}

#window {
  background-color: #CA9297;
  color: #282828;
}

window#waybar.empty #window {
    background: none;
    margin: 0px;
    padding: 0px;
}

#layout {
  background-color: #CA9297;
  color: #282828;
}

#language {
  background-color: #CA9297;
  color: #282828;
}

#keymode {
  background-color: #CA9297;
  color: #282828;
}

```

## Complete Configuration Example

> **Tip:** You can find a complete Waybar configuration for mangowm at [waybar-config](https://github.com/DreamMaoMao/waybar-config).