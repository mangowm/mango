# mangowm-ng

mango with HDR, blur, rounded corners, box shadows and the rest of the scenefx
effects running on the **Vulkan** renderer. No patches to apply: every change
lives in these three branches.

| what | repo | branch |
| --- | --- | --- |
| wlroots fork, split scene/output Vulkan render passes | `m8l8th814n-eng/wlroots-vkfx` | `vulkan-effects` |
| scenefx, Vulkan blur/corners/shadows + HDR color transforms | `m8l8th814n-eng/scenefx` | `vulkan-hdr` |
| the compositor | `m8l8th814n-eng/mango` | `mangowm-ng` |

You only clone mango. `subprojects/*.wrap` points meson at the other two, so
they are fetched and built as subprojects unless the system already has them
installed.

## Arch

Three packages, built in this order:

```sh
git clone -b mangowm-ng https://github.com/m8l8th814n-eng/mango.git
cd mango/packaging
(cd wlroots-vkfx && makepkg -si)
(cd scenefx0.5   && makepkg -si)
(cd mangowm-ng   && makepkg -si)
```

`wlroots-vkfx` installs alongside a stock `wlroots0.20`, it does not replace it.

## Any other distro

```sh
git clone -b mangowm-ng https://github.com/m8l8th814n-eng/mango.git
cd mango
meson setup build
ninja -C build
meson install -C build
```

wlroots-vkfx and scenefx are pulled in and linked statically. If the machine
already has either one installed system-wide, meson uses that copy instead —
force the subproject with `meson setup build --force-fallback-for=wlroots-vkfx,scenefx`.

The Arch packages above are the other way round: three separate shared
libraries, so anything else on the system can link them too.

Build deps: meson, ninja, glslang, vulkan-headers, wayland-protocols, libdrm,
pixman, libxkbcommon, libinput, libdisplay-info, libliftoff, seatd, lcms2,
cjson, pango, xorg-xwayland.

## Running it

The Vulkan renderer is not the wlroots default, so ask for it:

```sh
WLR_RENDERER=vulkan mango
```

HDR is per monitor, in `~/.config/mango/config.conf`:

```
monitorrule=eDP-1,...,hdr:2
hdr_sdr_nits=400
```

`hdr:1` uses static mastering values, `hdr:2` reads them from the panel's EDID.
`hdr_sdr_nits` is the SDR white level inside an HDR signal (203 is the spec
default, higher makes SDR content brighter). `monitorrule=...,icc:/path.icc`
applies an ICC profile as the output color transform instead.
