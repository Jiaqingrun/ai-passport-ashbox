[简体中文](./COMMUNITY_PUBLISH.zh_CN.md) · **English**

# Community store update checklist (Ember)

Use this when preparing the next publisher update for play **1943**. Release entry and install
source: [`publish-to-community.md`](../development/release/publish-to-community.md).

## Copy change policy (hard · 2026-11-08)

Store **description** and **instructions** are a frozen baseline. The live baseline is the store's latest **approved** revision:

- **Incremental edits only by default:** change paragraphs that match real firmware changes in this release; **do not** fully rewrite, expand into an encyclopedia, or do unrelated polish rewrites each publish.
- **Changelog:** only this release’s landed additions/fixes; do not restate the whole playbook.
- **Required blocks** (Save Manager link, community wipe, import / `RESTORED 3`, etc.) change only when facts change; otherwise keep them as-is.
- **Exception:** a full rewrite is allowed only when the creator explicitly asks for one in that session; otherwise agents and the publish flow stay incremental.

In-game Help (“How to play”) still updates with rule changes (see `PROJECT.md`); that is separate from this store-side “no major rewrite” rule.

## Save Manager link (required)

In **game instructions** (Chinese and English), the Save Manager download must point to the current release, not an old tag:

```text
https://github.com/Jiaqingrun/AI-Passport_BS/releases/download/save-manager-v1.0.1/EmberSaveManager.zip
```

Do not leave `save-manager-v1.0.0` in published instructions after 1.0.1 exists.

## Import / RESTORED copy (required)

Keep (or restore) player-facing wording that covers:

1. Community install/update **wipes** the device; back up with Save Manager first, then import after install.
2. Progress already on **this** device continues at boot (older Ember saves migrate). That does **not** survive a community wipe without a prior backup.
3. NVS game save lives in namespace `ashbox`, key `save` (blob magic `ASH2` is **not** the namespace name).
4. After import, trust the device screen (day / stash / clues). If the web UI reports “written but verify failed” (`RESTORED 3`): check the device first; if progress matches the backup, upgrade Ember firmware (old firmware could mis-report success as `3`). If progress is wrong, re-import `Ember-before-import-…emsave` and keep the original backup.
5. Same `device_id` only; default confirm focus stays on Cancel.

## Artwork (fixed set · hard)

Every update of this play re-uploads the same four illustrations from
`assets/ashbox/promo/` — the creator's chosen set, not something regenerated per release:

| File | Role |
| --- | --- |
| `01-cover.png` | cover |
| `02-depart.png` | extra image 1 |
| `03-return.png` | extra image 2 |
| `04-door.png` | extra image 3 |

All four are portrait 3:4. Label them as illustrations, not live photos. Do not swap,
regenerate, reorder or drop one of them unless the creator says so in that session; the
store revision must always carry all four.

## Firmware

- Upload the verified **merged** `FoloToy-AI-Passport-full.bin` from the gate archive.
- Post-import NVS verify must use `ashbox` / `save` (`ashbox_save_nvs_readable`).
