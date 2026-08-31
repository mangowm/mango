# Changelog

All notable changes to **MangoWM HDR & Color Management** will be documented in this file.

## [0.16.2-hdr1] - 2026-08-28

### Added
- **`frog-color-management-v1` Protocol Implementation:**
  - Added protocol specification [`protocols/frog-color-management-v1.xml`](protocols/frog-color-management-v1.xml).
  - Implemented `frog_color_management_factory_v1` global singleton (Global Interface #29) and `frog_color_managed_surface` interface in [`src/ext-protocol/frog-color.h`](src/src/ext-protocol/frog-color.h).
  - Integrated `frog-color-management-v1` into `protocols/meson.build` and `src/mango.c`.
  - Added dynamic `preferred_metadata` event generation advertising ST.2084 PQ / scRGB linear transfer functions, Rec.2020 color volume, and hardware luminance capabilities to clients.
- **Automated Display EDID Mastering Luminance Engine:**
  - Integrated `libdisplay-info` into `src/ext-protocol/hdr.h` and `meson.build`.
  - Added `output_detect_hdr_luminance()` to automatically read CTA-861 HDR Static Metadata directly from `/sys/class/drm/*/edid`.
  - Added automatic fallback to $1000\text{ nits}$ peak, $0.0001\text{ nits}$ black level, and $250\text{ nits}$ full-frame average for HDR-capable OLED displays when EDID metadata blocks are unpopulated.
- **Dynamic Surface Re-negotiation:**
  - Hooked `frog_color_update_all_surfaces()` into `togglehdr_output()` in `src/ext-protocol/hdr.h` to broadcast updated luminance and transfer functions across all active client surfaces when HDR mode is toggled at runtime.
- **Validation Test Suite & Documentation:**
  - Created standalone C test client [`test_frog_client.c`](test_frog_client.c) to perform automated Wayland protocol handshakes and verify `preferred_metadata` callbacks.
  - Created comprehensive [`PLAN.md`](PLAN.md), [`README.md`](README.md), and local Arch Linux PKGBUILD [`PKGBUILD`](PKGBUILD).

### Fixed
- Fixed HDR detection in DirectX 11 (DXVK) and Wine/Proton games running natively under Wayland without Gamescope.
- Eliminated washed-out colors and incorrect tone curves caused by unmanaged SDR-to-HDR color space presentation.
- Prevented Gamescope WSI implicit layer panics by enabling direct pure Wayland presentation.

### Tested & Verified
- Verified with *Overwatch 2* (DX11 / DXVK) using `DXVK_HDR=1 PROTON_ENABLE_WAYLAND=1 PROTON_ENABLE_HDR=1`.
- Verified on Samsung Odyssey OLED G9 ($5120\times1440$ @ $144\text{Hz}$) powered by AMD Radeon RX 7900 XTX (Navi 31 / RADV).
