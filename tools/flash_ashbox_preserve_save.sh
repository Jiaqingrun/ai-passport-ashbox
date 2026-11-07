#!/usr/bin/env bash
# Flash Ashbox application only — keeps NVS save (and settings) intact.
# Use merged 0x0 flash only for blank devices or intentional full wipe.
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

PORT="${1:-}"
ARCHIVE="${2:-}"

if [[ -z "$PORT" ]]; then
    echo "Usage: $0 <serial-port> [firmware-archive-dir]" >&2
    echo "Example: $0 /dev/cu.usbmodem1201 build/firmware/<sha>" >&2
    exit 2
fi

if [[ -z "$ARCHIVE" ]]; then
    ARCHIVE="$(ls -1dt build/firmware/*/ 2>/dev/null | head -1 || true)"
    ARCHIVE="${ARCHIVE%/}"
fi

if [[ -z "$ARCHIVE" || ! -d "$ARCHIVE" ]]; then
    echo "No firmware archive found. Run ./tools/validate.sh --firmware first." >&2
    exit 1
fi

APP_BIN="$ARCHIVE/FoloToy-AI-Passport.bin"
if [[ ! -f "$APP_BIN" ]]; then
    echo "Missing app image: $APP_BIN" >&2
    exit 1
fi

python3 tools/archive_firmware.py verify "$ARCHIVE"

echo "Preserve-save flash:"
echo "  port:    $PORT"
echo "  archive: $ARCHIVE"
echo "  write:   0x10000 (factory app only; NVS at 0x9000 untouched)"
echo "  image:   $APP_BIN"

python -m esptool --chip esp32c3 -p "$PORT" -b 460800 \
    --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_freq 80m --flash_size 8MB \
    0x10000 "$APP_BIN"

echo "Done. Save/settings in NVS should still be present."
