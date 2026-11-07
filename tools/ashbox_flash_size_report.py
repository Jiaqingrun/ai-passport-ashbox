#!/usr/bin/env python3
"""Report Ashbox firmware Flash footprint from build/ artifacts.

Prefers the newest verified archive under build/firmware/<sha>/, then falls
back to build/FoloToy-AI-Passport.{elf,bin}. Stale build/ copies from older
demo trees report empty sprite/PCM buckets — always pass --elf/--bin or use
auto-discovery after ./tools/validate.sh --firmware.

Usage:
  python3 tools/ashbox_flash_size_report.py
  python3 tools/ashbox_flash_size_report.py --elf build/firmware/<sha>/FoloToy-AI-Passport.elf
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FACTORY = 0x7F0000
APP_NAME = "FoloToy-AI-Passport"


def find_nm() -> str | None:
    which = shutil.which("riscv32-esp-elf-nm")
    if which:
        return which
    home = Path.home() / ".espressif" / "tools" / "riscv32-esp-elf"
    if home.is_dir():
        cands = sorted(home.glob("*/riscv32-esp-elf/bin/riscv32-esp-elf-nm"))
        if cands:
            return str(cands[-1])
    return None


def find_size() -> str | None:
    which = shutil.which("riscv32-esp-elf-size")
    if which:
        return which
    home = Path.home() / ".espressif" / "tools" / "riscv32-esp-elf"
    if home.is_dir():
        cands = sorted(home.glob("*/riscv32-esp-elf/bin/riscv32-esp-elf-size"))
        if cands:
            return str(cands[-1])
    return None


def newest_firmware_pair() -> tuple[Path, Path] | None:
    """Return (elf, bin) from the newest build/firmware/<sha>/ archive."""
    root = ROOT / "build" / "firmware"
    if not root.is_dir():
        return None
    best: tuple[float, Path, Path] | None = None
    for d in root.iterdir():
        if not d.is_dir():
            continue
        elf = d / f"{APP_NAME}.elf"
        app = d / f"{APP_NAME}.bin"
        if not elf.is_file() or not app.is_file():
            continue
        mtime = max(elf.stat().st_mtime, app.stat().st_mtime)
        if best is None or mtime > best[0]:
            best = (mtime, elf, app)
    if best is None:
        return None
    return best[1], best[2]


def resolve_paths(elf_arg: Path | None, bin_arg: Path | None) -> tuple[Path, Path]:
    if elf_arg is not None and bin_arg is not None:
        return elf_arg, bin_arg
    pair = newest_firmware_pair()
    if pair is not None:
        elf, app = pair
        if elf_arg is None:
            elf_arg = elf
        if bin_arg is None:
            bin_arg = app
        return elf_arg, bin_arg
    return (
        elf_arg or (ROOT / "build" / f"{APP_NAME}.elf"),
        bin_arg or (ROOT / "build" / f"{APP_NAME}.bin"),
    )


def symbol_sizes(nm: str, elf: Path) -> list[tuple[int, str, str]]:
    out = subprocess.check_output([nm, "-S", "--size-sort", str(elf)], text=True)
    rows: list[tuple[int, str, str]] = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 4:
            continue
        try:
            sz = int(parts[1], 16)
        except ValueError:
            continue
        rows.append((sz, parts[2], parts[3]))
    return rows


def symbol_addrs(nm: str, elf: Path) -> dict[str, int]:
    """Plain nm addresses for embedded binary start/end pairs."""
    out = subprocess.check_output([nm, str(elf)], text=True)
    addrs: dict[str, int] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        try:
            addr = int(parts[0], 16)
        except ValueError:
            continue
        addrs[parts[-1]] = addr
    return addrs


def blob_size(addrs: dict[str, int], prefix: str) -> int | None:
    """Size of CMake target_add_binary_data blob named <prefix>.

    Symbols are `_binary_<prefix>_start` / `_binary_<prefix>_end`. Also accepts
    a bare `<prefix>_length` uint32 when present.
    """
    start = addrs.get(f"_binary_{prefix}_start")
    end = addrs.get(f"_binary_{prefix}_end")
    if start is not None and end is not None and end >= start:
        return end - start
    # Some toolchains also emit <prefix>_length as a absolute symbol value;
    # ignore — length is not reliably in the address field.
    return None


def sum_matching(rows: list[tuple[int, str, str]], pred) -> int:
    return sum(sz for sz, _t, name in rows if pred(name))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", type=Path, default=None, help="ELF path (default: newest build/firmware/*/)")
    ap.add_argument("--bin", type=Path, default=None, help="App .bin path (default: matching archive)")
    args = ap.parse_args()

    elf, app_bin = resolve_paths(args.elf, args.bin)

    if not elf.is_file():
        print(f"ERROR: missing ELF {elf}", file=sys.stderr)
        return 1
    if not app_bin.is_file():
        print(f"ERROR: missing app bin {app_bin}", file=sys.stderr)
        return 1

    try:
        rel_elf = elf.resolve().relative_to(ROOT)
    except ValueError:
        rel_elf = elf
    print(f"elf                  {rel_elf}")
    print(f"bin                  {app_bin.name} ({app_bin.parent.name})")

    app = app_bin.stat().st_size
    print(f"app.bin              {app:8d}  ({app/1024/1024:.3f} MiB)")
    print(f"factory partition    {FACTORY:8d}  ({FACTORY/1024/1024:.3f} MiB)")
    print(f"factory free         {FACTORY-app:8d}  ({(FACTORY-app)/FACTORY*100:.1f}%)")

    # Stale build/ demo trees are ~1 MiB and lack Ashbox blobs.
    if app < 1_500_000:
        print(
            "WARN: app.bin looks too small for Ashbox (expected ~2.7–3.0 MiB). "
            "Re-run after ./tools/validate.sh --firmware, or pass --elf/--bin "
            "from build/firmware/<sha>/.",
            file=sys.stderr,
        )

    size = find_size()
    if size:
        out = subprocess.check_output([size, "-A", str(elf)], text=True)
        print("\n=== sections ===")
        for line in out.splitlines():
            if any(
                k in line
                for k in (
                    ".flash.text",
                    ".flash.rodata",
                    ".flash.appdesc",
                    ".iram0.text",
                    ".dram0.data",
                    ".dram0.bss",
                    "Total",
                )
            ):
                print(line)

    nm = find_nm()
    if not nm:
        print("\nWARN: riscv32-esp-elf-nm not found; skip symbol buckets", file=sys.stderr)
        return 0

    rows = symbol_sizes(nm, elf)
    addrs = symbol_addrs(nm, elf)
    scenes = (
        "idle",
        "away",
        "weak",
        "threat",
        "return",
        "night",
        "gate",
        "duo",
        "farm",
        "stash",
        "trade",
        "watch",
    )
    sprites = 0
    missing_frames = 0
    for sc in scenes:
        for fi in range(3):
            sz = blob_size(addrs, f"{sc}_f{fi}_bin")
            if sz:
                sprites += sz
            else:
                missing_frames += 1
    radio = 0
    for name in (
        "radio_scream_f_pcm",
        "radio_scream_m_pcm",
        "radio_help_pcm",
        "radio_search_pcm",
        "radio_laugh_pcm",
    ):
        sz = blob_size(addrs, name)
        if sz:
            radio += sz
    hearth = blob_size(addrs, "hearth_fire_pcm") or 0
    font = sum_matching(rows, lambda n: n == "glyph_bitmap" or n.endswith(".glyph_bitmap"))
    # Prefer ashbox glyph_bitmap (large); Montserrat is the smaller one when both exist.
    glyph_rows = [(sz, name) for sz, _t, name in rows if "glyph_bitmap" in name]
    montserrat = sum_matching(rows, lambda n: "montserrat" in n.lower())

    print("\n=== symbol buckets (ASR1 / PCM via start–end) ===")
    print(f"{'sprites ASR1':24s} {sprites:8d}  ({sprites/1024/1024:.3f} MiB)")
    if missing_frames:
        print(f"{'  missing frames':24s} {missing_frames:8d}")
    print(f"{'radio pcm x5':24s} {radio:8d}  ({radio/1024:.1f} KiB)")
    print(f"{'hearth pcm':24s} {hearth:8d}  ({hearth/1024:.1f} KiB)")
    print(f"{'glyph_bitmap sum':24s} {font:8d}  ({font/1024:.1f} KiB)")
    if glyph_rows:
        for sz, name in sorted(glyph_rows, reverse=True):
            print(f"  {name:22s} {sz:8d}")
    print(f"{'montserrat symbols':24s} {montserrat:8d}  ({montserrat/1024:.1f} KiB)")
    print(f"{'pcm total':24s} {radio+hearth:8d}  ({(radio+hearth)/1024:.1f} KiB)")

    print("\n=== top 15 symbols (nm -S) ===")
    for sz, typ, name in sorted(rows, reverse=True)[:15]:
        print(f"{sz:8d} {typ} {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
