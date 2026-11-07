[简体中文](./AUDIT_BUG.zh_CN.md) · **English**

# Ashbox — Strict game-code bug audit

| Item | Value |
| --- | --- |
| Date | 2026-11-07 (Asia/Shanghai); revised same day after parallel tracks |
| Authority | [`PROJECT.md`](../../PROJECT.md) audit rails · [`AUDIT_REQUIREMENTS.md`](./AUDIT_REQUIREMENTS.md) |
| Scope | Rules / content / save / embbackup / UI wiring |

```text
Audit mandatory: Flash size = PASS
Audit mandatory: Code optimization = PASS
Bug verdict: P0+P1 FIXED; expedition no auto-blank; deep-sleep wake clamps catch-up
Save: ASHBOX_SAVE_VER=11 (uint16 day latches); blob 648
```

Full ID tables (B1–B20), Flash snapshot, and fix order: [AUDIT_BUG.zh_CN.md](./AUDIT_BUG.zh_CN.md).

### Landed 2026-11-07

1. **P0** — B11/B20, B12–B14, B15, B16/B17, B1/B2  
2. **P1** — B18 no v2 head-only; B3 baseline fail-rate wording; B19 empty+fever copy; B4 modal layout; B5 gear pick count  
3. **Community** — no auto-blank during expedition; deep-sleep wake clamps wall-clock catch-up  

### Parallel tracks (merged)

- Rules / Save-USB / UI wiring audits (prior agent tracks; merged into the Chinese table)
