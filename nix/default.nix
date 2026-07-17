{
  lib,
  libX11,
  libinput,
  libxcb,
  libdrm,
  libxkbcommon,
  pcre2,
  pango,
  cjson,
  pixman,
  pkg-config,
  stdenv,
  wayland,
  wayland-protocols,
  wayland-scanner,
  libxcb-wm,
  xwayland,
  meson,
  ninja,
  scenefx,
  wlroots_0_20,
  libGL,
  enableXWayland ? true,
  debug ? false,
}:
stdenv.mkDerivation {
  pname = "mango";
  version = "nightly";

  src = lib.cleanSourceWith {
    src = ../.;
    filter =
      path: type:
      let
        relPath = lib.removePrefix ((toString ../.) + "/") (toString path);
      in
      !(relPath == ".git" || lib.hasPrefix ".git/" relPath || relPath == "build" || lib.hasPrefix "build/" relPath);
  };

  mesonFlags = [
    (lib.mesonEnable "xwayland" enableXWayland)
    (lib.mesonBool "asan" debug)
  ];

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    wayland-scanner
  ];

  buildInputs =
    [
      libinput
      libxcb
      libxkbcommon
      pcre2
      pango
      cjson
      pixman
      wayland
      wayland-protocols
      wlroots_0_20
      scenefx
      libGL
      libdrm
    ]
    ++ lib.optionals enableXWayland [
      libX11
      libxcb-wm
      xwayland
    ];

  passthru = {
    providedSessions = ["mango"];
  };

  meta = {
    mainProgram = "mango";
    description = "Practical and Powerful wayland compositor (dwm but wayland)";
    homepage = "https://github.com/mangowm/mango";
    license = lib.licenses.gpl3Plus;
    maintainers = [];
    platforms = lib.platforms.unix;
  };
}
