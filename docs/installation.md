---
title: Installation
description: Install mangowm on AerynOS, Arch, Fedora, Gentoo, Guix System, NixOS, PikaOS, or build from source.
---

## Package Installation

mangowm is available as a pre-built package on several distributions. Choose your distribution below.

---

### AerynOS

mangowm is available in the **AerynOS package repository**.

You can install it using the `moss` package manager:

```bash
sudo moss install mangowm
```

---

### Arch Linux

mangowm is available in the **Arch User Repository (AUR)**.

You can install it using an AUR helper like `yay` or `paru`:

```bash
yay -S mangowm-git
```

> **Tip:** This package pulls the latest git version, ensuring you have the newest features and fixes.

---

### Fedora

The package is in the third-party **Terra repository**. First, add the Terra Repository.

> **Warning:** Both commands require root privileges. Use `sudo` if needed.

```bash
dnf install --nogpgcheck --repofrompath 'terra,https://repos.fyralabs.com/terra$releasever' terra-release
```

Then, install the package:

```bash
dnf install mangowm
```

---

### Gentoo

The package is hosted in the community-maintained **GURU** repository.

1. **Add the GURU repository**
   ```bash
   emerge --ask --verbose eselect-repository
   eselect repository enable guru
   emerge --sync guru
   ```

2. **Unmask packages**
   Add the required packages to your `package.accept_keywords` file:
   - `gui-libs/scenefx`
   - `gui-wm/mangowm`

3. **Install mango**
   ```bash
   emerge --ask --verbose gui-wm/mangowm
   ```

---

### Guix System

The package definition is described in the source repository.

1. **Add mango channel**
   Add to `$HOME/.config/guix/channels.scm`:
   ```scheme
   (cons (channel
           (name 'mangowm)
           (url "https://github.com/mangowm/mango.git")
           (branch "main"))
         %default-channels)
   ```

2. **Install**
   After running `guix pull`, you can install mangowm:
   ```bash
   guix install mangowm
   ```

   Or add it to your system configuration using the mangowm module:
   ```scheme
   (use-modules (mangowm))

   (packages (cons*
               mangowm-git
               ... ;; Other packages
               %base-packages))
   ```

> **Tip:** For more information, see the [Guix System documentation](https://guix.gnu.org/manual/devel/en/html_node/Channels.html).

---

### NixOS

The repository provides a Flake with a NixOS module.

1. **Add flake input**
   ```nix
   # flake.nix
   {
     inputs = {
       nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
       mangowm = {
         url = "github:mangowm/mango";
         inputs.nixpkgs.follows = "nixpkgs";
       };
       # other inputs ...
     };
   }
   ```

2. **Import the NixOS module**

   **Option A — Import in `configuration.nix`:**
   ```nix
   # configuration.nix (or any other file that you import)
   {inputs, ...}: {
     imports = [
       inputs.mangowm.nixosModules.mango
       # .. other imports ...
     ];

     # ...
   }
   ```

   **Option B — Import directly in flake:**
   ```nix
   # flake.nix
   {
     # ...

     outputs = { self, nixpkgs, mangowm, ...}@inputs: let
       inherit (nixpkgs) lib;
       # ...
     in {
       nixosConfigurations.YourHostName = lib.nixosSystem {
         modules = [
           mangowm.nixosModules.mango # or inputs.mangowm.nixosModules.mango
           # other imports ...
         ];
       };
     }
   }
   ```

3. **Enable the module**
   ```nix
   # configuration.nix (or any other file that you import)
   {
     programs.mango.enable = true;
   }
   ```

4. **Extra options**
   - `programs.mango.package` — the mango package to use, allows usage of custom mango drvs
   - `programs.mango.addLoginEntry` (default: `true`) — adds login entry to the display manager

---

### PikaOS

mangowm is available in the **PikaOS package repository**.

You can install it using the `pikman` package manager:

```bash
pikman install mangowc
```

---

## Building from Source

If your distribution isn't listed above, or you want the latest unreleased changes, you can build mangowm from source.

> **Info:** Ensure the following dependencies are installed before proceeding:
> - `wayland`
> - `wayland-protocols`
> - `libinput`
> - `libdrm`
> - `libxkbcommon`
> - `pixman`
> - `libdisplay-info`
> - `libliftoff`
> - `hwdata`
> - `seatd`
> - `pcre2`
> - `xorg-xwayland`
> - `libxcb`

You will need to build `wlroots` and `scenefx` manually as well.

1. **Build wlroots**
   Clone and install the specific version required (check README for latest version).
   ```bash
   git clone -b 0.19.3 https://gitlab.freedesktop.org/wlroots/wlroots.git
   cd wlroots
   meson build -Dprefix=/usr
   sudo ninja -C build install
   ```

2. **Build scenefx**
   This library handles the visual effects.
   ```bash
   git clone -b 0.4.1 https://github.com/wlrfx/scenefx.git
   cd scenefx
   meson build -Dprefix=/usr
   sudo ninja -C build install
   ```

3. **Build mangowm**
   Finally, compile the compositor itself.
   ```bash
   git clone https://github.com/mangowm/mango.git
   cd mango
   meson build -Dprefix=/usr
   sudo ninja -C build install
   ```
