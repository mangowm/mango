#!/usr/bin/env python3
# Post-processes nixos-render-docs commonmark output into a single docs page.
# Input: two pre-rendered .md files (NixOS and HM), produced by nixos-render-docs.
# Steps: strip internal _module.args section, promote ## headings to ###.
import re
import sys

HEADER = (
    "---\n"
    "title: Nix Module Options\n"
    "description: NixOS and Home Manager configuration options for mangowm.\n"
    "---\n\n"
    "> **Note:** This document is automatically generated from the Nix module source code.\n\n"
)

SECTIONS = [
    ("NixOS", "**System-level options via `programs.mango`.**"),
    ("Home Manager", "**Configure mangowm declaratively via `wayland.windowManager.mango`.**"),
]


def process(md):
    # Remove internal _module.args option injected by the module system
    md = re.sub(r'## _module\\\.args.*?(?=\n## |\Z)', '', md, flags=re.DOTALL)
    # Promote option headings (##) to subheadings (###) under the section (##)
    md = re.sub(r'^## ', '### ', md, flags=re.MULTILINE)
    return md.strip()


def main():
    if len(sys.argv) != 4:
        sys.exit("Usage: generate-nix-options-docs.py <nixos.md> <hm.md> <output.md>")

    nixos_md, hm_md, output_md = sys.argv[1:4]

    with open(output_md, 'w', encoding='utf-8') as out:
        out.write(HEADER)
        for path, (title, subtitle) in zip([nixos_md, hm_md], SECTIONS):
            with open(path, 'r', encoding='utf-8') as f:
                md = f.read()
            out.write(f"## {title}\n\n{subtitle}\n\n")
            out.write(process(md))
            out.write('\n\n')
            print(f"Written {title} section.")


if __name__ == "__main__":
    main()
