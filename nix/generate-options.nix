self:
{
  pkgs,
  lib ? pkgs.lib,
  module,
  optionPrefix,
}:
let
  eval = lib.evalModules {
    modules = [
      (import module self)
      { _module.check = false; }
    ];
    specialArgs = { inherit pkgs; };
  };

  optionsDoc = pkgs.nixosOptionsDoc {
    options = eval.options;
    transformOptions =
      opt:
      opt
      // {
        visible = opt.visible && !opt.internal;
        name = lib.removePrefix optionPrefix opt.name;
      };
  };
in
optionsDoc.optionsJSON
