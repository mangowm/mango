self: {
  pkgs,
  lib ? pkgs.lib,
  module,
  optionPrefix,
}: let
  # Absolute store path of the flake root, used to compute relative subpaths
  repoPath = toString self;

  eval = lib.evalModules {
    modules = [
      (import module self)
      {_module.check = false;}
    ];
    specialArgs = {inherit pkgs;};
  };

  # Relative path of the module file within the repo (e.g. "nix/hm-modules.nix")
  moduleSubpath = lib.removePrefix "/" (lib.removePrefix repoPath (toString module));

  # Declaration entry linking each option back to its source file on GitHub
  moduleDeclaration = {
    url = "https://github.com/mangowm/mango/blob/main/${moduleSubpath}";
    name = "<mango/${moduleSubpath}>";
  };

  optionsDoc = pkgs.nixosOptionsDoc {
    options = eval.options;
    transformOptions = opt:
      opt
      // {
        visible = opt.visible && !opt.internal;
        # Strip the option prefix so docs show "enable" instead of "programs.mango.enable"
        name = lib.removePrefix optionPrefix opt.name;
        declarations = [moduleDeclaration];
      };
  };
in
  optionsDoc.optionsJSON
