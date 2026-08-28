# MangoWM with Native HDR & Valve Color Management Engine

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)
[![Platform: Wayland](https://img.shields.io/badge/Platform-Wayland%20(wlroots%200.20)-orange.svg)]()
[![HDR: ST.2084 PQ](https://img.shields.io/badge/HDR-ST.2084%20PQ%20%2F%20Rec.2020-green.svg)]()
[![Protocols: frog & wp_color](https://img.shields.io/badge/Protocols-frog__color__v1%20%7C%20wp__color__v2-purple.svg)]()

A lightweight, high-performance Wayland tiling compositor featuring **native HDR (High Dynamic Range)** support, automated display mastering metadata extraction via `libdisplay-info`, and full implementation of Valve's **`frog-color-management-v1`** protocol for zero-overhead native Linux gaming (Proton, DXVK, Wine, VKD3D).

---

## 🌟 Key Features

* **Valve `frog-color-management-v1` Protocol Support:**  
  Direct integration with Proton, Wine, DXVK, and Gamescope. Games like *Overwatch 2*, *Cyberpunk 2077*, and *Doom Eternal* immediately detect HDR display capabilities without needing complex nested gamescope setups.
* **Automated EDID HDR Metadata Extraction (`libdisplay-info`):**  
  Reads CTA-861 HDR Static Metadata blocks directly from physical monitor EDIDs at runtime, automatically pulling accurate Peak Luminance (e.g. $1000\text{ nits}$ on QD-OLED / Mini-LED), Minimum Luminance ($0.0001\text{ nits}$ for true OLED black), and Maximum Full-Frame Average Luminance.
* **Dynamic HDR Negotiation & Broadcast:**  
  Switching HDR on/off (`SUPER + h` / `togglehdr`) instantly broadcasts updated `preferred_metadata` events to all connected clients, allowing games to adapt without restarting.
* **Pure Wayland Zero-Overhead Fast Path:**  
  Eliminates nested compositor latency and avoids double tone-mapping artifacts.
* **Tested & Validated Hardware:**  
  Verified on AMD Radeon RX 7000 series (RDNA3 / RADV) with Samsung Odyssey OLED G9 ($5120\times1440$ @ $144\text{Hz}$, ST.2084 PQ / BT.2020).

---

## 🏗️ Architecture & Protocol Pipeline

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                 WAYLAND CLIENTS                                         │
│                                                                                         │
│   [Overwatch (DX11/DXVK)]          [Cyberpunk (VKD3D/DX12)]          [SDR App / Browser]│
│            │                                  │                              │          │
│   (frog-color-management)            (wp-color-management)             (wl_surface)     │
│   scRGB / Rec.2020 PQ                Linear FP16 / BT.2020              sRGB 8-bit      │
└────────────┬──────────────────────────────────┬──────────────────────────────┬──────────┘
             │                                  │                              │
             ▼                                  ▼                              ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                               MANGO-HDR COMPOSITOR CORE                                 │
│                                                                                         │
│  1. Protocol Handshake:                                                                 │
│     - frog_color_management_factory_v1 (Global Interface #29)                          │
│     - wp_color_manager_v1 (Version 2)                                                   │
│                                                                                         │
│  2. EDID Metadata Extraction:                                                           │
│     - libdisplay-info -> CTA-861-G HDR Static Metadata (Min/Max Nits, Primaries)        │
│                                                                                         │
│  3. Color Transform & Tone-Mapping (wlroots-0.20):                                      │
│     - 10-bit & 16-bit Render Formats (DRM_FORMAT_XRGB2101010 / ABGR2101010)            │
│     - ST.2084 PQ EOTF & BT.2020 Color Space Output                                      │
│                                                                                         │
│  4. Fast Path KMS Presentation:                                                         │
│     - Direct DRM connector commit with HDR_OUTPUT_METADATA property                     │
└────────────────────────────────────────────┬────────────────────────────────────────────┘
                                             │
                                             ▼
                               ┌───────────────────────────┐
                               │  KMS / DRM Output (DP-1)  │
                               │  10-Bit ST.2084 PQ        │
                               │  OLED Display Engine      │
                               │  (0.0000 -> 1000 Nits)    │
                               └───────────────────────────┘
```

---

## 🚀 Quick Start & Installation

### Option 1: Arch Linux / CachyOS (`makepkg`)

```bash
# Clone the repository
git clone -b wl-only https://github.com/another-hubgit/mango.git
cd mango

# Build and install package
makepkg -si
```

### Option 2: Building from Source (Meson & Ninja)

#### Dependencies
* `wlroots-0.20`
* `wayland-protocols >= 1.41`
* `wayland-server`, `wayland-client`
* `libdisplay-info >= 0.2.0`
* `libdrm`, `pixman-1`, `libinput`, `xkbcommon`, `cjson`, `pangocairo`, `pcre2`

```bash
git clone -b wl-only https://github.com/another-hubgit/mango.git
cd mango
meson setup build --prefix=/usr --buildtype=release
ninja -C build
sudo ninja -C build install
```

---

## 🎮 Gaming & Client Configuration

### 1. DirectX 11 Games (*Overwatch 2*, *Apex Legends*, etc.)
DX11 games use **DXVK**. DXVK requires `DXVK_HDR=1` to advertise HDR color spaces:

**Steam Launch Options:**
```text
DXVK_HDR=1 PROTON_ENABLE_WAYLAND=1 PROTON_ENABLE_HDR=1 %command%
```

### 2. DirectX 12 Games (*Cyberpunk 2077*, *Alan Wake 2*, etc.)
DX12 games use **VKD3D-Proton**, which communicates directly with Wayland color protocols:

**Steam Launch Options:**
```text
PROTON_ENABLE_WAYLAND=1 PROTON_ENABLE_HDR=1 %command%
```

### 3. Native Media Playback (*MPV*)
```bash
mpv --vo=gpu-next --target-colorspace-hint=yes movie.hdr.mkv
```

---

## ⚙️ Configuration (`~/.config/mango/config.conf`)

Add or adjust monitor rules in your configuration:

```toml
# Enable 10-bit render pipeline
hdr_depth = 10

# Configure HDR on output DP-1 (hdr:2 enables HDR mode)
monitorrule = name:DP-1,width:5120,height:1440,refresh:144,x:0,y:1080,scale:1,rr:0,vrr:0,hdr:2

# Shortcut to toggle HDR on/off
bind = SUPER, h, togglehdr
```

---

## 📜 Credits & Acknowledgments

* **[MangoWM](https://github.com/mangowm/mango):** The upstream Wayland compositor project by DreamMaoMao / BlackCherry.
* **[Joshua Ashton](https://github.com/Joshua-Ashton) & [Xaver Hugl](https://github.com/Zamundaaa):** Authors of the `frog-color-management-v1` protocol and foundational HDR Wayland work at Valve Software and KDE.
* **[wlroots](https://gitlab.freedesktop.org/wlroots/wlroots):** Modular Wayland compositor library.
* **[libdisplay-info](https://gitlab.freedesktop.org/emersion/libdisplay-info):** Modern EDID and DisplayID parsing library by Simon Ser (emersion).

---

## 📄 License
This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**.
