{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    scenefx = {
      url = "github:wlrfx/scenefx";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    scenefx,
  }: let
    inherit (nixpkgs.lib) genAttrs;

    # Systems mangowm supports. Options that call `forEachSystem` will generate an attribute for each of these options.
    systems = [
      "x86_64-linux"
      "aarch64-linux"
    ];

    # Helper function that generates an attribute set by calling the provided `perSystem` function for each system in `systems` defined above.
    forEachSystem = perSystem:
      genAttrs systems (
        system:
          perSystem {
            inherit system;
            pkgs = nixpkgs.legacyPackages.${system};
          }
      );
  in {
    overlays.default = final: prev: {
      inherit (self.packages.${final.stdenv.hostPlatform.system}) mango;
    };

    packages = forEachSystem (
      {
        pkgs,
        system,
      }: let
        inherit (pkgs) callPackage;
        mango = callPackage ./nix {
          inherit (scenefx.packages.${system}) scenefx;
        };
      in {
        inherit mango;
        default = mango;
        hm-options-json = callPackage (import ./nix/generate-options.nix self) {
          module = ./nix/hm-modules.nix;
          optionPrefix = "wayland.windowManager.mango.";
        };
        nixos-options-json = callPackage (import ./nix/generate-options.nix self) {
          module = ./nix/nixos-modules.nix;
          optionPrefix = "programs.mango.";
        };
      }
    );

    nixosModules.mango = import ./nix/nixos-modules.nix self;
    hmModules.mango = import ./nix/hm-modules.nix self;

    devShells = forEachSystem (
      {system, ...}: {
        default = self.packages.${system}.mango;
      }
    );

    formatter = forEachSystem (
      {pkgs, ...}: pkgs.alejandra
    );
  };
}
