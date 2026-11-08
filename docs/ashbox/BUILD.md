[简体中文](./BUILD.zh_CN.md) · **English**

# Build identity

Latest verified firmware archive:

- Archive: `build/firmware/445875b747cdcf8075afa6db55fd937df0280cca9737c22eb917203af8d16050`
- Built: 2026-11-08 10:38, ESP-IDF v5.5.3
- App image: 2,924,960 B = 35.1% of the 8,323,072 B factory partition (64.9% free)
- ELF sha256: `d76466afc5cc42126326c35ab916485732714666f697bda9c400ab29ec4aec02`
- Verify: `python3 tools/archive_firmware.py verify <archive-directory>`

### Firmware archive retention (local housekeeping)

`build/firmware/` is a local build output directory and is gitignored. This repository's actual rule: keep every archive referenced by maintained documentation, the 3 newest archives, and the newest archive of each calendar day; prune the rest. The kept and pruned sets are recorded in `build/firmware/ARCHIVE_INDEX.md`. This is local developer housekeeping, not a product contract.

## Flashing and saves (important)

Ashbox saves live in NVS (`0x9000`). **Idle progress is precious — default to preserve-save flashing during development.**

| Mode | Command / tool | When |
| --- | --- | --- |
| **Preserve save (default)** | `./tools/flash_ashbox_preserve_save.sh /dev/cu.usbmodemXXXX [archive-dir]` — writes factory app at `0x10000` only | Iteration, bugfix, on-device acceptance |
| Full refresh | Merged `FoloToy-AI-Passport-full.bin` at `0x0` | Blank device, partition-table change, or **explicit wipe consent** |

Merged flash from `0x0` erases through the NVS region and **wipes the save**. Do not treat it as the daily default.

### Community update copy (mandatory)

Official community “Install” **overwrites all current device content** (see [Install a community play](https://ai-passport.folotoy.cn/guides/flash-official-firmware/)).  
**Every future community version changelog must state clearly:**

- Community install/update is **not** local preserve-save flashing;
- Reinstalling clears on-device progress and settings — install only if that is acceptable;
- Do not promise “update keeps your save”.

Local development still uses `flash_ashbox_preserve_save.sh`; that script is **not** shipped to other players via the community package.

Also: expedition remaining time is flushed to NVS about every **15 s** while awake (and immediately on depart / report / crisis / chore). Preserve-save flashing alone is not enough if the firmware never persisted the countdown.

Save format: NVS namespace `ashbox` key `save`, magic `ASH2` / current ver 13, blob 776 B (older versions migrate; CRC failure creates a safe new game). Authority: `ASHBOX_SAVE_VER` in `main/ashbox_game.h` and `ASHBOX_SAVE_BLOB_SIZE` in `main/ashbox_save.h`.
