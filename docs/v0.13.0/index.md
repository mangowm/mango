---
title: Introduction
description: A lightweight and feature-rich Wayland compositor based on dwl.
---


**mango** is a Wayland compositor based on [dwl](https://codeberg.org/dwl/dwl/). It aims to be as lightweight as `dwl` and can be built completely within a few seconds, without compromising on functionality.

> **Philosophy:** **Lightweight & Fast**: mango is designed to be minimal yet functional. It compiles in seconds and offers a robust set of features out of the box.

## Feature Highlights

Beyond basic window management, mangowm provides a rich set of features designed for a modern Wayland experience.

- **[Animations](/docs/visuals/animations)** — Smooth, customizable animations for opening, moving, closing windows and tag switching.
- **[Layouts](/docs/window-management/layouts)** — Supports Scroller, Master-Stack, Monocle, Grid, Deck, and more, with per-tag layouts.
- **[Visual Effects](/docs/visuals/effects)** — Built-in blur, shadows, corner radius, and opacity effects powered by scenefx.
- **[IPC & Scripting](/docs/ipc)** — Control the compositor externally with robust IPC support for custom scripts and widgets.

## Additional Features

- **XWayland Support** — Excellent compatibility for legacy X11 applications.
- **Tag System** — Uses tags instead of workspaces, allowing separate window layouts for each tag.
- **Input Methods** — Great support for text input v2/v3 (Fcitx5, IBus).
- **Window States** — Rich states including swallow, minimize, maximize, fullscreen, and overlay.
- **Hot-Reload Config** — Simple external configuration that supports hot-reloading without restarting.
- **Scratchpads** — Support for both Sway-like and named scratchpads.

## Community

- **[Join the mangowm Discord](https://discord.gg/CPjbDxesh5)** — Chat with the community, get support, share your setup, and stay updated with the latest mangowm news.
- **[Join the GitHub Discussions](https://github.com/mangowm/mango/discussions)** — Ask questions, request features, report issues, or share ideas directly with contributors and other users.

## Acknowledgements

This project is built upon the hard work of several open-source projects:

- **[wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)** — Implementation of the Wayland protocol.
- **[mwc](https://github.com/nikoloc/mwc)** — Basal window animation reference.
- **[dwl](https://codeberg.org/dwl/dwl)** — Basal dwl features.
- **[sway](https://github.com/swaywm/sway)** — Sample implementation of the Wayland protocol.
- **[scenefx](https://github.com/wlrfx/scenefx)** — Library to simplify adding window effects.
