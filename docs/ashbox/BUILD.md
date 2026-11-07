[简体中文](./BUILD.zh_CN.md) · **English**

# Build identity

Latest verified firmware archive:

- Archive: `build/firmware/f0d150b367e5be4813a5ed70a299b8062d9f325016746155ea0e5c178630593c`
- Verify: `python3 tools/archive_firmware.py verify <archive-directory>`

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

Save format: NVS namespace `ashbox` key `save`, magic `ASH2` / current ver 11 (older versions migrate; CRC failure creates a safe new game). Authority: `ASHBOX_SAVE_VER` in `main/ashbox_game.h`.
