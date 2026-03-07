self: {
  lib,
  config,
  pkgs,
  ...
}: let
  cfg = config.wayland.windowManager.mango;
  variables = lib.concatStringsSep " " cfg.systemd.variables;
  extraCommands = lib.concatStringsSep " && " cfg.systemd.extraCommands;
  systemdActivation = ''${pkgs.dbus}/bin/dbus-update-activation-environment --systemd ${variables}; ${extraCommands}'';
  autostart_sh = pkgs.writeShellScript "autostart.sh" ''
    ${lib.optionalString cfg.systemd.enable systemdActivation}
    ${cfg.autostart_sh}
  '';
  validatedConfig = pkgs.runCommand "mango-config.conf" { } ''
    cp ${pkgs.writeText "mango-config.conf" cfg.settings} "$out"

    ${cfg.package}/bin/mango -c "$out" -p || exit 1
  '';
in {
  options = {
    wayland.windowManager.mango = with lib; {
      enable = mkOption {
        type = types.bool;
        default = false;
      };
      package = lib.mkOption {
        type = lib.types.package;
        default = self.packages.${pkgs.stdenv.hostPlatform.system}.mango;
        description = "The mango package to use";
      };
      systemd = {
        enable = mkOption {
          type = types.bool;
          default = pkgs.stdenv.isLinux;
          example = false;
          description = ''
            Whether to enable {file}`mango-session.target` on
            mango startup. This links to
            {file}`graphical-session.target`.
            Some important environment variables will be imported to systemd
            and dbus user environment before reaching the target, including
            * {env}`DISPLAY`
            * {env}`WAYLAND_DISPLAY`
            * {env}`XDG_CURRENT_DESKTOP`
            * {env}`XDG_SESSION_TYPE`
            * {env}`NIXOS_OZONE_WL`
            You can extend this list using the `systemd.variables` option.
          '';
        };
        variables = mkOption {
          type = types.listOf types.str;
          default = [
            "DISPLAY"
            "WAYLAND_DISPLAY"
            "XDG_CURRENT_DESKTOP"
            "XDG_SESSION_TYPE"
            "NIXOS_OZONE_WL"
            "XCURSOR_THEME"
            "XCURSOR_SIZE"
          ];
          example = ["--all"];
          description = ''
            Environment variables imported into the systemd and D-Bus user environment.
          '';
        };
        extraCommands = mkOption {
          type = types.listOf types.str;
          default = [
            "systemctl --user reset-failed"
            "systemctl --user start mango-session.target"
          ];
          description = ''
            Extra commands to run after D-Bus activation.
          '';
        };
        xdgAutostart = mkEnableOption ''
          autostart of applications using
          {manpage}`systemd-xdg-autostart-generator(8)`
        '';
      };
      settings = mkOption {
        description = "mango config content";
        type = types.lines;
        default = "";
        example = ''
          # menu and terminal
          bind=Alt,space,spawn,rofi -show drun
          bind=Alt,Return,spawn,foot
        '';
      };
      autostart_sh = mkOption {
        description = "WARRNING: This is a shell script, but no need to add shebang";
        type = types.lines;
        default = "";
        example = ''
          waybar &
        '';
      };
    };
  };

  config = lib.mkIf cfg.enable {
    home.packages = [cfg.package];
    xdg.configFile = {
      "mango/config.conf" = lib.mkIf (cfg.settings != "") {
        source = validatedConfig;
      };
      "mango/autostart.sh" = lib.mkIf (cfg.autostart_sh != "") {
        source = autostart_sh;
        executable = true;
      };
    };
    systemd.user.targets.mango-session = lib.mkIf cfg.systemd.enable {
      Unit = {
        Description = "mango compositor session";
        Documentation = ["man:systemd.special(7)"];
        BindsTo = ["graphical-session.target"];
        Wants =
          [
            "graphical-session-pre.target"
          ]
          ++ lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
        After = ["graphical-session-pre.target"];
        Before = lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
      };
    };
  };
}
