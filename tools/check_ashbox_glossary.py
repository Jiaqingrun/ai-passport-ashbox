#!/usr/bin/env python3
"""Fail if player-facing Ashbox C strings use forbidden slang / alias names.

Canonical names live in PROJECT.zh_CN.md §玩家文案. This gate keeps the common
alias regressions out of UI/help/game/content string literals.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    ROOT / "main/ashbox_content.c",
    ROOT / "main/ashbox_game.c",
    ROOT / "main/ashbox_game_loot.c",
    ROOT / "main/ashbox_help_copy.c",
    ROOT / "main/ashbox_ui.c",
    ROOT / "main/ashbox_save.c",
]
STR_RE = re.compile(r'"((?:\\.|[^"\\])*)"')

# Forbidden player-facing substrings → required name (for the error message).
FORBIDDEN: list[tuple[str, str]] = [
    ("短火", "手枪"),
    ("火子", "子弹"),
    ("空回", "空手而归"),
    ("铁条", "撬棍"),
    ("工作站", "工作台"),
    ("墙钟", "时钟"),
    ("水桶", "水罐"),
    ("罐背用仓", "水罐/背包各占1仓位"),
    ("口粮按", "口粮300占1仓位"),
    ("铁按", "铁50占1仓位"),
]


def unescape_c_string(s: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(s):
        if s[i] != "\\" or i + 1 >= len(s):
            out.append(s[i])
            i += 1
            continue
        nxt = s[i + 1]
        simple = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "0": "\0"}
        if nxt in simple:
            out.append(simple[nxt])
            i += 2
            continue
        if nxt == "x" and i + 3 < len(s):
            out.append(chr(int(s[i + 2 : i + 4], 16)))
            i += 4
            continue
        out.append(nxt)
        i += 2
    return "".join(out)


def main() -> int:
    hits: list[str] = []
    for path in SOURCES:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        for m in STR_RE.finditer(text):
            raw = unescape_c_string(m.group(1))
            for bad, good in FORBIDDEN:
                if bad in raw:
                    line = text[: m.start()].count("\n") + 1
                    hits.append(
                        f"{path.relative_to(ROOT).as_posix()}:{line}: "
                        f"forbidden '{bad}' → use '{good}': {raw[:48]}"
                    )
    if hits:
        print("check_ashbox_glossary: FAIL", file=sys.stderr)
        for h in hits:
            print(f"  {h}", file=sys.stderr)
        return 1
    print(f"check_ashbox_glossary: PASS (banned={len(FORBIDDEN)} patterns)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
