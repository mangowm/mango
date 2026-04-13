#!/usr/bin/env python3
import json
import re
from pathlib import Path

DOCS_DIR = Path("docs")
WIKI_DIR = Path("wiki-temp")

FRONTMATTER_RE = re.compile(r"\A---\s*\n.*?^---\s*\n", re.DOTALL | re.MULTILINE)
DOCS_LINK_RE = re.compile(r"\[([^\]]+)\]\(/docs/(?:[^/)]+/)*([^/)#]+)(#[^)]+)?\)")


def collect_all_files() -> list[tuple[Path, str]]:
    files = []

    def from_dir(directory: Path) -> list[Path]:
        meta = directory / "meta.json"
        if meta.exists():
            data = json.loads(meta.read_text())
            return [directory / f"{p}.md" for p in data.get("pages", []) if (directory / f"{p}.md").exists()]
        return sorted(directory.glob("*.md"))

    for src in from_dir(DOCS_DIR):
        files.append((src, "Home" if src.stem == "index" else src.stem))

    for subdir in sorted(DOCS_DIR.iterdir()):
        if subdir.is_dir():
            for src in from_dir(subdir):
                files.append((src, src.stem))

    return files


def main() -> None:
    files = collect_all_files()

    contents = {src: src.read_text() for src, _ in files}

    for src, dest_name in files:
        text = FRONTMATTER_RE.sub("", contents[src], count=1).lstrip("\n")
        text = DOCS_LINK_RE.sub(lambda m: f"[{m.group(1)}]({m.group(2)}{m.group(3) or ''})", text)
        (WIKI_DIR / f"{dest_name}.md").write_text(text)

    lines: list[str] = []
    current_section = None
    for src, dest_name in files:
        section = "General" if src.parent == DOCS_DIR else src.parent.name.replace("-", " ").title()
        if section != current_section:
            if current_section is not None:
                lines.append("")
            lines.append(f"## {section}\n")
            current_section = section
        if dest_name != "Home":
            title = dest_name.replace("-", " ").replace("_", " ").title()
            lines.append(f"- [[{dest_name}|{title}]]")

    (WIKI_DIR / "_Sidebar.md").write_text("\n".join(lines))


if __name__ == "__main__":
    main()
