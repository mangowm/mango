let
  inherit (builtins) fromJSON readFile;

  lock = fromJSON (readFile ./flake.lock);
  node = lock.nodes.${lock.nodes.root.inputs.nixpkgs}.locked;

  nixpkgs = fetchTarball {
    url = with node; "https://github.com/${owner}/${repo}/archive/${rev}.tar.gz";
    sha256 = node.narHash;
  };
in
  {pkgs ? import nixpkgs {}}: let
    inherit (pkgs) callPackage;
    inherit (pkgs.lib.modules) importApply;

    package = callPackage ./nix/package.nix {};
  in {
    overlay = final: prev: {
      mango = package;
    };

    inherit package;

    nixosModule = importApply ./nix/nix-module.nix package;
    hmModule = importApply ./nix/hm-module.nix package;
  }
