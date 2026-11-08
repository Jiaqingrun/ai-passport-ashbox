**English** · [简体中文](./AUDIT_HISTORY.zh_CN.md)

# Audit history (retired snapshots)

One row per retired audit. The rules those audits produced live in
[AUDIT_REQUIREMENTS.md](./AUDIT_REQUIREMENTS.md) (checklist), [`PROJECT.md`](../../PROJECT.md)
(rails), [LAYOUT_LOCK.md](./LAYOUT_LOCK.md), [EXTENSION.md](./EXTENSION.md) (save ABI) and
[FLASH_SIZE_PLAN.md](./FLASH_SIZE_PLAN.md); the current open findings are in
[AUDIT_GAMEPLAY_20261108.md](./AUDIT_GAMEPLAY_20261108.md) with the plan in
[OPTIMIZE_PLAN_20261108.md](./OPTIMIZE_PLAN_20261108.md). Full text of each snapshot stays in git
history.

| Snapshot | Date | Scope | Verdict at the time | What lasted |
| --- | --- | --- | --- | --- |
| Gameplay / lifecycle / compliance | 2026-11-05 | Idle-loop shape, warehouse, return hooks | findings fixed | Farm got its own page, stash level independent of the kitchen, cost curve |
| Ten-round strict | 2026-11-05 | Compliance · content · interaction · extension · gameplay · code · power · optimization | mixed, fix-forward | The idea of a standing audit gate |
| Low-level code | 2026-11-05 | Save ABI · tasks and locks · deep sleep · audio | findings fixed | Lock order `runtime → LVGL`; rules units must not touch LVGL |
| Strict (Flash + whole-code) | 2026-11-06 | Size budget and hot units | PASS | Size plan alignment, per-audit Flash measurement |
| UI / visual / layout / fonts | 2026-11-06 | Panel geometry, sprites, glyphs | FAIL, then fixed | Font regeneration, now-band columns, list viewport clipping, modal bounds |
| Content Phase G (narrative / UX) | 2026-11-06 | Copy and information architecture | 11 findings, all fixed | Named quest rows, split quest advances, late watcher radio pool |
| Persistence | 2026-11-07 | Dual-slot save, CRC seal, wall-clock rewind | findings fixed | Never snap the wall clock backwards; urgent NVS flush after chores |
| Game-code bugs | 2026-11-07 | Rules · content · save · backup · UI wiring | FAIL, fixed forward | Bug fixes across the rules core; no new modules |
| Full strict | 2026-11-08 | Flash · code · copy layout · save v13 · stack | mandatory sections PASS | `ASHBOX_SAVE_VER=13`, frozen v12 fixture, stack fix-forward |
| Strict round 2 | 2026-11-08 | Same rails, ABI unchanged, archive retention | PASS | Firmware archive retention rule under `build/firmware/<sha256>/` |

Rule of thumb that came out of these rounds: run the host gates, keep the mandatory sections in
every audit, and fix FAIL findings inside the same change rather than deferring them.
