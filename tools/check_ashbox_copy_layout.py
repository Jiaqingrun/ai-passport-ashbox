#!/usr/bin/env python3
"""Host gate: player-facing instruction copy must wrap, not clip/truncate.

Checks:
1. Help strings stay within the wrapped-row budget (UTF-8 + display width).
2. paint_help uses WRAP (no CLIP / circular marquee for help body).
3. Help paint does not snprintf into a tiny pad buffer that truncates UTF-8.
4. Viewport math: HELP_VIEW * HELP_ROW_STEP fits LIST_LAYER_H.
5. Modal create uses WRAP for instruction lines.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELP_C = ROOT / "main" / "ashbox_help_copy.c"
HELP_H = ROOT / "main" / "ashbox_help_copy.h"
UI_C = ROOT / "main" / "ashbox_ui.c"
# Page units were split out of ashbox_ui.c (C-UI1); search them all for painters.
UI_FILES = sorted(ROOT.glob("main/ashbox_ui*.c"))

# ashbox_font_16 is ~16px wide for CJK; label width 196 ≈ 12 cells.
HELP_CELLS_PER_LINE = 12
# The help row is ROW_H tall with 4 px padding top and bottom; 18 px lines.
HELP_LABEL_PAD = 8
JOURNAL_MAX_UTF8 = 34  # char[36] minus NUL, kept under 35 so a CJK glyph fits
GUIDE_MAX_LINES = 4
STRIP_CELL_MAX_PX = 44
HELP_MAX_UTF8 = 160  # UI sets text directly; leave headroom for future pads
LIST_LAYER_H = 228


def fail(msg: str) -> None:
    print(f"check_ashbox_copy_layout: FAIL {msg}", file=sys.stderr)
    sys.exit(1)


def parse_define(text: str, name: str) -> int:
    m = re.search(rf"#define\s+{name}\s+(\d+)", text)
    if not m:
        fail(f"missing #define {name}")
    return int(m.group(1))


def display_cells(s: str) -> float:
    cells = 0.0
    for ch in s:
        cells += 1.0 if ord(ch) > 127 else 0.5
    return cells


def extract_help_lines(text: str) -> list[str]:
    block = re.search(
        r"k_help_lines\[ASHBOX_HELP_COUNT\]\s*=\s*\{(.*?)\};",
        text,
        re.S,
    )
    if not block:
        fail("k_help_lines table not found")
    return re.findall(r'"((?:\\.|[^"\\])*)"', block.group(1))


def main() -> int:
    help_h = HELP_H.read_text(encoding="utf-8")
    help_c = HELP_C.read_text(encoding="utf-8")
    ui = UI_C.read_text(encoding="utf-8")

    count = parse_define(help_h, "ASHBOX_HELP_COUNT")
    view = parse_define(help_h, "ASHBOX_HELP_VIEW")
    row_h = parse_define(help_h, "ASHBOX_HELP_ROW_H")
    step = parse_define(help_h, "ASHBOX_HELP_ROW_STEP")
    label_w = parse_define(help_h, "ASHBOX_HELP_LABEL_W")

    lines = extract_help_lines(help_c)
    if len(lines) != count:
        fail(f"help line count {len(lines)} != ASHBOX_HELP_COUNT {count}")

    if 8 + view * step > LIST_LAYER_H:
        fail(
            f"help viewport overflow: 8+{view}*{step}="
            f"{8 + view * step} > LIST_LAYER_H {LIST_LAYER_H}"
        )
    if row_h > step:
        fail(f"HELP_ROW_H {row_h} > HELP_ROW_STEP {step}")
    if label_w < 160:
        fail(f"HELP_LABEL_W {label_w} too narrow for wrap")

    max_wrap_lines = (row_h - HELP_LABEL_PAD) // 18
    if max_wrap_lines < 1:
        fail(f"HELP_ROW_H {row_h} leaves no line at 18 px")
    max_cells = HELP_CELLS_PER_LINE * max_wrap_lines
    for i, s in enumerate(lines):
        b = len(s.encode("utf-8"))
        cells = display_cells(s)
        if b > HELP_MAX_UTF8:
            fail(f"help[{i}] utf8={b} > {HELP_MAX_UTF8}: {s}")
        if cells > max_cells + 0.5:
            fail(
                f"help[{i}] display≈{cells:.1f} cells needs >{max_wrap_lines} "
                f"lines at width≈{HELP_CELLS_PER_LINE}: {s}"
            )

    # paint_help must WRAP and must not use a tiny snprintf pad.
    # It may live in ashbox_ui.c or a split page unit.
    body = None
    for src in UI_FILES:
        text = src.read_text(encoding="utf-8")
        start = text.find("void paint_help(void)")
        if start < 0:
            continue
        end = text.find("\nvoid ", start + 1)
        end_static = text.find("\nstatic void ", start + 1)
        stops = [x for x in (end, end_static) if x > 0]
        body = text[start:min(stops)] if stops else text[start:]
        break
    if body is None:
        fail("paint_help() not found")
    if "LV_LABEL_LONG_MODE_WRAP" not in body:
        fail("paint_help must use LV_LABEL_LONG_MODE_WRAP")
    if "LV_LABEL_LONG_MODE_CLIP" in body or "SCROLL_CIRCULAR" in body:
        fail("paint_help must not CLIP or marquee-scroll instruction lines")
    if re.search(r"char\s+padded\s*\[\s*\d+\s*\]", body):
        fail("paint_help must not snprintf into a pad buffer (truncates UTF-8)")
    if "ashbox_help_line" not in body:
        fail("paint_help must set text from ashbox_help_line()")

    # Modal instruction lines must be created with WRAP.
    if "label_set_wrap(s_modal_lines[0]" not in ui:
        fail("modal line0 must use label_set_wrap")
    if "label_set_wrap(s_modal_lines[1]" not in ui:
        fail("modal line1 (body) must use label_set_wrap")
    if "LV_LABEL_LONG_MODE_WRAP" not in ui:
        fail("UI must define WRAP usage for instruction labels")

    # Hint band is one line tall: require width + non-CLIP long mode.
    if not re.search(
        r"s_hint\s*=\s*mk_label[\s\S]{0,240}?LV_LABEL_LONG_MODE_SCROLL_CIRCULAR",
        ui,
    ):
        fail("s_hint must be width-bound with SCROLL_CIRCULAR (hint band is 1 line)")


    # ---- P0-15: journal literals, guide pages, resource strip, modal wording ----
    game_c = (ROOT / "main" / "ashbox_game.c").read_text(encoding="utf-8")
    for m in re.finditer(r'ash_game_log\(g,\s*"((?:\\.|[^"\\])*)"\)', game_c):
        raw = m.group(1)
        text = raw.encode("utf-8").decode("unicode_escape") if "\\" in raw else raw
        b = len(text.encode("utf-8"))
        if b > JOURNAL_MAX_UTF8:
            fail(f"journal literal {b} B > {JOURNAL_MAX_UTF8} B: {text}")
    for m in re.finditer(r'snprintf\(logb, sizeof\(logb\),\s*"((?:\\.|[^"\\])*)"', game_c):
        lit = m.group(1)
        if "%" not in lit and len(lit.encode("utf-8")) > JOURNAL_MAX_UTF8:
            fail(f"journal literal {len(lit.encode('utf-8'))} B > {JOURNAL_MAX_UTF8} B: {lit}")

    guide_seen = 0
    for src in UI_FILES:
        text = src.read_text(encoding="utf-8")
        block = re.search(r"static const char \*body\[ASH_GUIDE_DONE\] = \{(.*?)\};", text, re.S)
        if not block:
            continue
        guide_seen += 1
        for m in re.finditer(r'"((?:\\.|[^"\\])*)"', block.group(1)):
            lines = m.group(1).split("\\n")
            if len(lines) > GUIDE_MAX_LINES:
                fail(f"guide page needs {len(lines)} lines > {GUIDE_MAX_LINES}")
            for line in lines:
                if display_cells(line) > HELP_CELLS_PER_LINE:
                    fail(f"guide line ≈{display_cells(line):.1f} cells > {HELP_CELLS_PER_LINE}: {line}")
    if guide_seen == 0:
        fail("guide body table not found")

    strip = re.findall(r"lv_obj_set_width\((s_water|s_food|s_parts|s_ember|s_people),\s*(\d+)\)", ui)
    if len(strip) != 5:
        fail(f"resource strip must size five cells, found {len(strip)}")
    for name, px in strip:
        if int(px) > STRIP_CELL_MAX_PX:
            fail(f"{name} cell {px} px > {STRIP_CELL_MAX_PX} (five cells + margins must fit 240)")

    if "按任意键" in ui:
        fail("modal copy must not promise any key: only OK closes it")

    print(
        "check_ashbox_copy_layout: PASS "
        f"(help={count} view={view} row={row_h}/{step} label_w={label_w})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
