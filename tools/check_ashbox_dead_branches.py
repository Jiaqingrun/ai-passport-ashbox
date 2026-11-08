#!/usr/bin/env python3
"""CI dead-branch scan for ashbox quest wiring.

Any branch with quest_id != 255 must appear in an outcome or tag pool,
unless listed in SYSTEM_ONLY (ash-tide seed trade / harvest advance).
Also rejects outcome/tag refs outside the branch table.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "main/ashbox_content.c"

# Quest branches advanced only by trade/harvest systems (not choice pools).
SYSTEM_ONLY = {27, 28}

BRANCH_RE = re.compile(
    r"/\*\s*(\d+)\s*\*/\s*\{[^}]*?,\s*(\d+|255),\s*(\d+|255),\s*\"",
    re.MULTILINE,
)
OUTCOME_RE = re.compile(r"/\*[^*]*\*/\s*\{\{(\d+),\s*(\d+),\s*(\d+)\},\s*(\d+)\}")
# Tag pools carry a variable number of candidate ids (5 since C-1).
TAG_RE = re.compile(r'\{"[^"]+",\s*\{([\d,\s]+)\},\s*(\d+)\}')


def main() -> int:
    text = CONTENT.read_text(encoding="utf-8")
    branches: dict[int, tuple[int, int]] = {}
    for m in BRANCH_RE.finditer(text):
        bid = int(m.group(1))
        qid = int(m.group(2))
        need = int(m.group(3))
        branches[bid] = (qid, need)
    if not branches:
        print("check_ashbox_dead_branches: no branches parsed", file=sys.stderr)
        return 1

    reachable: set[int] = set()
    max_id = max(branches)

    def add_ids(ids: list[int], where: str) -> None:
        for i in ids:
            if i < 0 or i > max_id or i not in branches:
                print(f"check_ashbox_dead_branches: bad ref {i} in {where}", file=sys.stderr)
                raise SystemExit(1)
            reachable.add(i)

    for m in OUTCOME_RE.finditer(text):
        n = int(m.group(4))
        ids = [int(m.group(1)), int(m.group(2)), int(m.group(3))][:n]
        add_ids(ids, "outcome")

    for m in TAG_RE.finditer(text):
        n = int(m.group(2))
        ids = [int(x) for x in m.group(1).replace(" ", "").split(",") if x]
        add_ids(ids[:n], "tag")

    dead: list[int] = []
    for bid, (qid, _need) in sorted(branches.items()):
        if qid == 255:
            continue
        if bid in reachable or bid in SYSTEM_ONLY:
            continue
        dead.append(bid)

    if dead:
        print(
            "check_ashbox_dead_branches: quest branches not in outcome∪tag "
            f"and not system-only: {dead}",
            file=sys.stderr,
        )
        return 1

    print(
        f"check_ashbox_dead_branches: PASS "
        f"(branches={len(branches)} reachable={len(reachable)} system_only={sorted(SYSTEM_ONLY)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
