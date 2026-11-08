#!/usr/bin/env python3
"""Fail if ashbox C string literals use CJK not listed in ashbox_font_symbols.txt."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Every player-facing unit, including the split page files (C-UI1). A new page
# unit used to escape this gate entirely, which is how font-missing glyphs shipped.
SOURCES = [
    ROOT / "main/ashbox_content.c",
    ROOT / "main/ashbox_game.c",
    ROOT / "main/ashbox_game_loot.c",
    ROOT / "main/ashbox_help_copy.c",
    ROOT / "main/ashbox_save.c",
    ROOT / "main/main.c",
] + sorted(ROOT.glob("main/ashbox_ui*.c"))
SYMBOLS = ROOT / "main/ashbox_font_symbols.txt"
FONT_C = ROOT / "assets/fonts/ashbox_font_16.c"
STR_RE = re.compile(r'"((?:\\.|[^"\\])*)"')
GLYPH_U_RE = re.compile(r"/\* U\+([0-9A-Fa-f]{4})")


def unescape_c_string(s: str) -> str:
    """Decode common C escapes without mangling UTF-8 code units."""
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
    allowed = set(SYMBOLS.read_text(encoding="utf-8"))
    missing: dict[str, set[str]] = {}
    for path in SOURCES:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        for m in STR_RE.finditer(text):
            raw = unescape_c_string(m.group(1))
            for ch in raw:
                o = ord(ch)
                if o <= 0x7E or ch.isspace():
                    continue
                if 0x80 <= o < 0xA0:
                    continue  # C1 controls from bad escapes — ignore
                if ch not in allowed:
                    missing.setdefault(ch, set()).add(path.relative_to(ROOT).as_posix())
    failed = False
    if missing:
        failed = True
        print("ashbox font glyphs: FAIL — missing from main/ashbox_font_symbols.txt:", file=sys.stderr)
        for ch in sorted(missing, key=ord):
            files = ", ".join(sorted(missing[ch]))
            print(f"  U+{ord(ch):04X} '{ch}' in {files}", file=sys.stderr)

    # symbols.txt must be fully present in the generated LVGL font bitmap.
    if FONT_C.exists():
        font_cps = {int(m.group(1), 16) for m in GLYPH_U_RE.finditer(FONT_C.read_text(encoding="utf-8", errors="replace"))}
        absent = []
        for ch in sorted(allowed):
            o = ord(ch)
            if o <= 0x7E or ch.isspace() or ch in "\n\r\t":
                continue
            if o not in font_cps:
                absent.append(ch)
        if absent:
            failed = True
            print("ashbox font glyphs: FAIL — in symbols.txt but missing from ashbox_font_16.c:", file=sys.stderr)
            for ch in absent:
                print(f"  U+{ord(ch):04X} '{ch}'", file=sys.stderr)
    else:
        failed = True
        print("ashbox font glyphs: FAIL — assets/fonts/ashbox_font_16.c missing", file=sys.stderr)

    if failed:
        return 1
    print("ashbox font glyphs: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
