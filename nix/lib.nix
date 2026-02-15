lib:
let
  inherit (lib)
    attrNames
    filterAttrs
    foldl
    generators
    partition
    removeAttrs
    ;

  inherit (lib.strings)
    concatMapStrings
    hasPrefix
    ;

  /**
    Convert a structured Nix attribute set into Mango's configuration format.

    This function takes a nested attribute set and converts it into Mango-compatible
    configuration syntax, supporting top, bottom, and regular command sections.

    Commands are flattened using the `flattenAttrs` function, and attributes are formatted as
    `key = value` pairs. Lists are expanded as duplicate keys to match Mango's expected format.

    Configuration:

    * `topCommandsPrefixes` - A list of prefixes to define **top** commands (default: `[]`).
    * `bottomCommandsPrefixes` - A list of prefixes to define **bottom** commands (default: `[]`).

    Attention:

    - The function ensures top commands appear **first** and bottom commands **last**.
    - The generated configuration is a **single string**, suitable for writing to a config file.
    - Lists are converted into multiple entries, ensuring compatibility with Mango.

    # Inputs

    Structured function argument:

    : topCommandsPrefixes (optional, default: `[]`)
      : A list of prefixes that define **top** commands. Any key starting with one of these
        prefixes will be placed at the beginning of the configuration.
    : bottomCommandsPrefixes (optional, default: `[]`)
      : A list of prefixes that define **bottom** commands. Any key starting with one of these
        prefixes will be placed at the end of the configuration.

    Value:

    : The attribute set to be converted to Hyprland configuration format.

    # Type

    ```
    toMango :: AttrSet -> AttrSet -> String
    ```

    # Examples
    :::{.example}

    ## Basic mangowc configuration

    ```nix
    let
      config = {
        blur = 1;
        blur_params_radius = 5;
        border_radius = 6;
        animations = 1;
        animation_duration_open = 400;
      };
    in lib.toMango {} config
    ```

    **Output:**
    ```
    animations = 1
    animation_duration_open = 400
    blur = 1
    blur_params_radius = 5
    border_radius = 6
    ```

    ## Using nested attributes

    ```nix
    let
      config = {
        blur = 1;
        blur_params = {
          radius = 5;
          num_passes = 2;
          noise = 0.02;
        };
        animation_curve = {
          open = "0.46,1.0,0.29,1";
          close = "0.08,0.92,0,1";
        };
      };
    in lib.toMango {} config
    ```

    **Output:**
    ```
    animation_curve_close = 0.08,0.92,0,1
    animation_curve_open = 0.46,1.0,0.29,1
    blur = 1
    blur_params_noise = 0.02
    blur_params_num_passes = 2
    blur_params_radius = 5
    ```

    ## Using lists for duplicate keys

    ```nix
    let
      config = {
        bind = [
          "SUPER,r,reload_config"
          "Alt,space,spawn,rofi -show drun"
          "Alt,Return,spawn,foot"
        ];
        tagrule = [
          "id:1,layout_name:tile"
          "id:2,layout_name:scroller"
        ];
      };
    in lib.toMango {} config
    ```

    **Output:**
    ```
    bind = SUPER,r,reload_config
    bind = Alt,space,spawn,rofi -show drun
    bind = Alt,Return,spawn,foot
    tagrule = id:1,layout_name:tile
    tagrule = id:2,layout_name:scroller
    ```

    ## Using keymodes (submaps)

    ```nix
    let
      config = {
        bind = [
          "SUPER,Q,killclient"
          "ALT,R,setkeymode,resize"
        ];
        keymode = {
          resize = {
            bind = [
              "NONE,Left,resizewin,-10,0"
              "NONE,Right,resizewin,10,0"
              "NONE,Escape,setkeymode,default"
            ];
          };
        };
      };
    in lib.toMango {} config
    ```

    **Output:**
    ```
    bind = SUPER,Q,killclient
    bind = ALT,R,setkeymode,resize

    keymode = resize
    bind = NONE,Left,resizewin,-10,0
    bind = NONE,Right,resizewin,10,0
    bind = NONE,Escape,setkeymode,default
    ```

    :::
  */
  toMango =
    {
      topCommandsPrefixes ? [ ],
      bottomCommandsPrefixes ? [ ],
    }:
    attrs:
    let
      toMango' =
        attrs:
        let
          # Specially configured `toKeyValue` generator with support for duplicate keys
          # and a legible key-value separator.
          mkCommands = generators.toKeyValue {
            mkKeyValue = generators.mkKeyValueDefault { } " = ";
            listsAsDuplicateKeys = true;
            indent = ""; # No indent, since we don't have nesting
          };

          # Extract keymode definitions if they exist
          keymodes = attrs.keymode or { };
          attrsWithoutKeymodes = removeAttrs attrs [ "keymode" ];

          # Generate keymode blocks
          # Format: keymode=name\nbind=...\nbind=...\n
          mkKeymodeBlock =
            name: modeAttrs:
            let
              modeCommands = flattenAttrs (p: k: "${p}_${k}") modeAttrs;
            in
            "keymode = ${name}\n${mkCommands modeCommands}";

          keymodeBlocks =
            if keymodes == { } then
              ""
            else
              "\n" + concatMapStrings (name: mkKeymodeBlock name keymodes.${name} + "\n") (attrNames keymodes);

          # Flatten the attrset, combining keys in a "path" like `"a_b_c" = "x"`.
          # Uses `flattenAttrs` with an underscore separator.
          commands = flattenAttrs (p: k: "${p}_${k}") attrsWithoutKeymodes;

          # General filtering function to check if a key starts with any prefix in a given list.
          filterCommands = list: n: foldl (acc: prefix: acc || hasPrefix prefix n) false list;

          # Partition keys into top commands and the rest
          result = partition (filterCommands topCommandsPrefixes) (attrNames commands);
          topCommands = filterAttrs (n: _: builtins.elem n result.right) commands;
          remainingCommands = removeAttrs commands result.right;

          # Partition remaining commands into bottom commands and regular commands
          result2 = partition (filterCommands bottomCommandsPrefixes) result.wrong;
          bottomCommands = filterAttrs (n: _: builtins.elem n result2.right) remainingCommands;
          regularCommands = removeAttrs remainingCommands result2.right;
        in
        # Concatenate strings from mapping `mkCommands` over top, regular, and bottom commands.
        # Keymodes are appended at the end.
        concatMapStrings mkCommands [
          topCommands
          regularCommands
          bottomCommands
        ]
        + keymodeBlocks;
    in
    toMango' attrs;

  /**
    Flatten a nested attribute set into a flat attribute set, using a custom key separator function.

    This function recursively traverses a nested attribute set and produces a flat attribute set
    where keys are joined using a user-defined function (`pred`). It allows transforming deeply
    nested structures into a single-level attribute set while preserving key-value relationships.

    Configuration:

    * `pred` - A function `(string -> string -> string)` defining how keys should be concatenated.

    # Inputs

    Structured function argument:

    : pred (required)
      : A function that determines how parent and child keys should be combined into a single key.
        It takes a `prefix` (parent key) and `key` (current key) and returns the joined key.

    Value:

    : The nested attribute set to be flattened.

    # Type

    ```
    flattenAttrs :: (String -> String -> String) -> AttrSet -> AttrSet
    ```

    # Examples
    :::{.example}

    ```nix
    let
      nested = {
        a = "3";
        b = { c = "4"; d = "5"; };
      };

      separator = (prefix: key: "${prefix}.${key}");  # Use dot notation
    in lib.flattenAttrs separator nested
    ```

    **Output:**
    ```nix
    {
      "a" = "3";
      "b.c" = "4";
      "b.d" = "5";
    }
    ```

    :::
  */
  flattenAttrs =
    pred: attrs:
    let
      flattenAttrs' =
        prefix: attrs:
        builtins.foldl' (
          acc: key:
          let
            value = attrs.${key};
            newKey = if prefix == "" then key else pred prefix key;
          in
          acc // (if builtins.isAttrs value then flattenAttrs' newKey value else { "${newKey}" = value; })
        ) { } (builtins.attrNames attrs);
    in
    flattenAttrs' "" attrs;
in
{
  inherit flattenAttrs toMango;
}
