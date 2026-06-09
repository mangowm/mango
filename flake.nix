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

        mango = callPackage ./nix/package.nix {
          inherit (scenefx.packages.${system}) scenefx;
        };

        generateOptions = callPackage (import ./nix/generate-options.nix self);
      in {
        inherit mango;
        default = mango;
        hm-options-json = generateOptions {
          module = ./nix/hm-module.nix;
          optionPrefix = "wayland.windowManager.mango.";
        };
        nixos-options-json = generateOptions {
          module = ./nix/nixos-module.nix;
          optionPrefix = "programs.mango.";
        };
      }
    );

    nixosModules.mango = { pkgs, ... }: {
      imports = [
        (import ./nix/nixos-module.nix self.packages.${pkgs.stdenv.hostPlatform.system}.default)
      ];
    };
    hmModules.mango = { pkgs, ... }: {
      imports = [
        (import ./nix/hm-module.nix self.packages.${pkgs.stdenv.hostPlatform.system}.default)
      ];
    };

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
