# Mango Wayland Compositor
<div>
  <img src="https://github.com/mangowm/mango/blob/main/assets/mango-transparency-256.png" alt="MangoWM Logo" width="120"/>
</div>

This project's development is based on [dwl](https://codeberg.org/dwl/dwl/).


1. **Lightweight & Fast Build**

   - _Mango_ is as lightweight as _dwl_, and can be built completely within a few seconds. Despite this, _Mango_ does not compromise on functionality.

2. **Feature Highlights**
   - In addition to basic WM functionality, Mango provides:
     - Excellent xwayland support.
     - Base tags not workspaces (supports separate window layouts for each tag)
     - Smooth and customizable complete animations (window open/move/close, tag enter/leave,layer open/close/move)
     - Excellent input method support (text input v2/v3)
     - Flexible window layouts with easy switching (scroller, master-stack, monocle,center-master, etc.)
     - Rich window states (swallow, minimize, maximize, unglobal, global, fakefullscreen, overlay, etc.)
     - Simple yet powerful external configuration(support shortcuts hot-reload)
     - Sway-like scratchpad and named scratchpad
     - Ipc support(get/send message from/to compositor by external program)
     - Hycov-like overview
     - Window effects from scenefx (blur, shadow, corner radius, opacity)
     - Zero flickering - every frame is perfect.

https://github.com/user-attachments/assets/bb83004a-0563-4b48-ad89-6461a9b78b1f

# Mango's Vision

**Mango's primary goal is stability**: After months of testing and development—and aside from a few lingering GPU compatibility issues—it should now be stable enough. I don't plan on making many breaking changes.

**Mango's preference is practicality**: I tend to add features that genuinely help with daily workflows—things that make our work more convenient.

**Mango won't cater to every user preference**: For niche feature requests, I'll take a wait-and-see approach. I'll only consider adding them if they get a significant number of upvotes.

# Our discord
[mangowm](https://discord.gg/CPjbDxesh5)

# Supported layouts
- tile
- scroller
- monocle
- grid
- deck
- center_tile
- vertical_tile
- vertical_grid
- vertical_scroller
- tgmix

# Installation

[![Packaging status](https://repology.org/badge/vertical-allrepos/mangowm.svg)](https://repology.org/project/mangowm/versions)

## Dependencies

- wayland
- wayland-protocols
- libinput
- libdrm
- libxkbcommon
- pixman
- libdisplay-info
- libliftoff
- hwdata
- seatd
- pcre2
- xorg-xwayland
- libxcb

## Arch Linux
The package is in the Arch User Repository and is available for manual download [here](https://aur.archlinux.org/packages/mangowm-git) or through a AUR helper like yay:
```bash
yay -S mangowm-git

```

## Gentoo Linux
The package is in the community-maintained repository called GURU.
First, add GURU repository:

```bash
emerge --ask --verbose eselect-repository
eselect repository enable guru
emerge --sync guru
```

Then, add `gui-libs/scenefx` and `gui-wm/mangowm` to the `package.accept_keywords`.

Finally, install the package:

```bash
emerge --ask --verbose gui-wm/mangowm
```

## Fedora Linux
The package is in the third-party Terra repository.
First, add the [Terra Repository](https://terra.fyralabs.com/).

Then, install the package:

```bash
dnf install mangowm
```

## Guix System
The package definition is described in the source repository.
First, add `mangowm` channel to `channels.scm` file:

```scheme
;; In $HOME/.config/guix/channels.scm
(cons (channel
        (name 'mangowm)
        (url "https://github.com/mangowm/mango.git")
        (branch "main"))
      ... ;; Your other channels
      %default-channels)
```

Then, run `guix pull` and after update you can either run
`guix install mangowm` or add it to your configuration via:

```scheme
(use-modules (mangowm)) ;; Add mangowm module

;; Add mangowm to packages list
(packages (cons*
            mangowm-git
            ... ;; Other packages you specified
            %base-packages))
```

And then rebuild your system.

## Other

```bash
git clone -b 0.19.2 https://gitlab.freedesktop.org/wlroots/wlroots.git
cd wlroots
meson build -Dprefix=/usr
sudo ninja -C build install

git clone -b 0.4.1 https://github.com/wlrfx/scenefx.git
cd scenefx
meson build -Dprefix=/usr
sudo ninja -C build install

git clone https://github.com/mangowm/mango.git
cd mangowm
meson build -Dprefix=/usr
sudo ninja -C build install
```

## Suggested Tools

### Hybrid component
- [dms-shell](https://github.com/AvengeMedia/DankMaterialShell)

### Independent component
- Application launcher (rofi, bemenu, wmenu, fuzzel)
- Terminal emulator (foot, wezterm, alacritty, kitty, ghostty)
- Status bar (waybar, eww, quickshell, ags), waybar is preferred
- Wallpaper setup (swww, swaybg)
- Notification daemon (swaync, dunst,mako)
- Desktop portal (xdg-desktop-portal, xdg-desktop-portal-wlr, xdg-desktop-portal-gtk)
- Clipboard (wl-clipboard, wl-clip-persist, cliphist)
- Gamma control/night light (wlsunset, gammastep)
- Miscellaneous (xfce-polkit, wlogout)

## Some Common Default Keybindings

- alt+return: open foot terminal
- alt+space: open rofi launcher
- alt+q: kill client
- alt+left/right/up/down: focus direction
- super+m: quit mango

## My Dotfiles

### Daily
- Dependencies

```bash
yay -S rofi foot xdg-desktop-portal-wlr swaybg waybar wl-clip-persist cliphist wl-clipboard wlsunset xfce-polkit swaync pamixer wlr-dpms sway-audio-idle-inhibit-git swayidle dimland-git brightnessctl swayosd wlr-randr grim slurp satty swaylock-effects-git wlogout sox
```

### Dms
- Dependencies
```bash
yay -S foot xdg-desktop-portal-wlr swaybg wl-clip-persist cliphist wl-clipboard sway-audio-idle-inhibit-git brightnessctl grim slurp satty matugen-bin dms-shell-git

```
- use my dms config

```bash
git clone -b dms https://github.com/DreamMaoMao/mango-config.git ~/.config/mango
```
- use my daily config

```bash
git clone https://github.com/DreamMaoMao/mango-config.git ~/.config/mango
```


## Config Documentation

Refer to the repo wiki [wiki](https://github.com/mangowm/mango/wiki/)

or the website docs [docs](https://mangowm.github.io/)

# NixOS + Home-manager

The repo contains a flake that provides a NixOS module and a home-manager module for mango.
Use the NixOS module to install mango with other necessary components of a working Wayland environment.
Use the home-manager module to declare configuration and autostart for mango.

Here's an example of using the modules in a flake:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    flake-parts.url = "github:hercules-ci/flake-parts";
    mango = {
      url = "github:mangowm/mango";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };
  outputs =
    inputs@{ self, flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      debug = true;
      systems = [ "x86_64-linux" ];
      flake = {
        nixosConfigurations = {
          hostname = inputs.nixpkgs.lib.nixosSystem {
            system = "x86_64-linux";
            modules = [
              inputs.home-manager.nixosModules.home-manager

              # Add mango nixos module
              inputs.mango.nixosModules.mango
              {
                programs.mango.enable = true;
              }
              {
                home-manager = {
                  useGlobalPkgs = true;
                  useUserPackages = true;
                  backupFileExtension = "backup";
                  users."username".imports =
                    [
                      (
                        { ... }:
                        {
                          wayland.windowManager.mango = {
                            enable = true;
                            settings = ''
                              # see config.conf
                            '';
                            autostart_sh = ''
                              # see autostart.sh
                              # Note: here no need to add shebang
                            '';
                          };
                        }
                      )
                    ]
                    ++ [
                      # Add mango hm module
                      inputs.mango.hmModules.mango
                    ];
                };
              }
            ];
          };
        };
      };
    };
}
```

# Packaging mango

To package mango for other distributions, you can check the reference setup for:

- [nix](https://github.com/mangowm/mango/blob/main/nix/default.nix)
- [arch](https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=mangowm-git).
- [gentoo](https://data.gpo.zugaina.org/guru/gui-wm/mangowm)

You might need to package `scenefx` for your distribution, check availability [here](https://github.com/wlrfx/scenefx.git).

If you encounter build errors when packaging `mango`, feel free to create an issue and ask a question, but
Read The Friendly Manual on packaging software in your distribution first.

# Thanks to These Reference Repositories

- https://gitlab.freedesktop.org/wlroots/wlroots - Implementation of Wayland protocol

- https://github.com/dqrk0jeste/owl - Basal window animation

- https://codeberg.org/dwl/dwl - Basal dwl feature

- https://github.com/swaywm/sway - Sample of Wayland protocol

- https://github.com/wlrfx/scenefx - Make it simple to add window effect.


# Sponsor
At present, I can only accept sponsorship through an encrypted connection.
If you find this project helpful to you, you can offer sponsorship in the following ways.

<img width="650" height="870" alt="image" src="https://github.com/user-attachments/assets/8c860317-90d2-4071-971d-f1a92b674469" />


Thanks to the following friends for their sponsorship of this project

[@tonybanters](https://github.com/tonybanters)
