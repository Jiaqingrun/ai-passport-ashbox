[简体中文](./AUDIT_REQUIREMENTS.zh_CN.md) · **English**

# Ashbox audit requirements (standing)

**Authority:** [`PROJECT.md`](../../PROJECT.md) section “Audit rails”. This file is the operational checklist. Conflict: `PROJECT.md` > this checklist > historical audit write-ups under `docs/archive/` (none yet for this rule).

Every audit in this repository (ten-round, code, content, UI/visual, pre-release, or any task explicitly called an audit) **must** include sections **1–3** below. Missing any means the audit is incomplete.

After **any** Ashbox product change (rules / UI / player copy / help / settings) in a session, the agent must run a **strict post-change audit** before delivery: execute the host gates below, walk the applicable rows here, **and fix FAIL findings in the same change** — do not leave known clip/wrap/truncation or gate failures for a later “audit-only” pass.

## 1. Flash / firmware size

| Check | Pass criteria |
| --- | --- |
| Image identity | Record SHA/path of a recent verified merged or app image from `./tools/validate.sh --firmware` (or state reuse + why not rebuilt) |
| Budget | Factory partition size vs `app.bin` (or equivalent); free % |
| Top consumers | Break down at least sprites, font, PCM/SFX, and code/rodata |
| Plan alignment | Compare to [`FLASH_SIZE_PLAN.md`](./FLASH_SIZE_PLAN.md); note drift |
| Allowed vs forbidden | List lossless opportunities; explicitly restate bans (cut scenes/frames, shrink 240×176, lossy art, shorten radio content) unless the user overrides |

## 2. Whole-codebase optimization

| Check | Pass criteria |
| --- | --- |
| Scope | Rules vs UI boundary, large/hot units, duplication, untested host-testable paths, locks/tasks, save ABI, dead code/branches, build fat |
| Evidence | Each finding cites file/symbol/metric or test gap |
| Actionability | Each item: suggestion, experience/save impact, priority; at least one concrete next step |
| No filler | Ban vague “consider refactor” with no target |

## 3. Player-facing copy layout (no obscure / no silent truncate)

Instruction surfaces — Help (how to play), guide/coach/block modals, and other multi-sentence how-to copy — must remain fully readable on the 240×320 panel.

| Check | Pass criteria |
| --- | --- |
| Help wrap | `paint_help` uses `LV_LABEL_LONG_MODE_WRAP` with a tall enough row; **forbidden**: CLIP or circular marquee as the only way to read a help line |
| No tiny pad | Help (and other instruction paints) must not `snprintf` into a buffer shorter than the UTF-8 source (truncation counts as FAIL) |
| Modal wrap | Modal title/body lines are width-bound + WRAP so long guide/coach/reason text is not clipped by the card edge |
| Hint band | `s_hint` is width-bound; because the locked hint band is ~one line tall, use SCROLL_CIRCULAR (or shorten copy) — never unbounded CLIP |
| Host gate | `python3 tools/check_ashbox_copy_layout.py` PASS (wired into `./tools/validate.sh --ashbox`) |
| Glossary / one name | `python3 tools/check_ashbox_glossary.py` PASS; player strings follow `PROJECT.zh_CN.md` canonical names (no short-fire / iron-bar / work-station / wall-clock / water-bucket aliases; slot copy in plain arithmetic) |
| Fix-forward | Any FAIL from this section is fixed before delivery; “defer to next audit” is not allowed for clip/wrap/truncate/glossary |

## 4. Delivery header (recommended)

```text
Audit mandatory: Flash size = PASS|FAIL|DEFERRED(reason)
Audit mandatory: Code optimization = PASS|FAIL|DEFERRED(reason)
Audit mandatory: Copy layout = PASS|FAIL|DEFERRED(reason)
```

`DEFERRED` is only for blocked measurement (e.g. no IDF env for Flash); copy-layout host gate is never deferred when UI/copy files changed. Still list qualitative risks and the exact unblock step when deferred.

## Related

- Size plan: [`FLASH_SIZE_PLAN.md`](./FLASH_SIZE_PLAN.md)
- Ten-round audit: [`AUDIT_10R.md`](./AUDIT_10R.md)
- Low-level code: [`AUDIT_CODE.md`](./AUDIT_CODE.md)
- Product index: [`README.md`](./README.md)
