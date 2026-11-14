<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Ember

An offline post-apocalyptic outpost sim on three buttons. You keep a hearth-lit
shelter and a handful of survivors: send them into ruins on timed expeditions,
ration food and water, build and trade, keep the nightly threat in check, and
follow three investigation lines that eventually turn the world one layer over.
No network, no account, and a world that keeps moving while the screen is dark.

## Publish information

- **Title (English)**: Ember 1.9.2
- **Community project**: `community-c919dfca` (project 1943), currently published
  on the store as 1.9.2: submitted 2026-11-12 as revision 5454 and approved,
  carrying the merged firmware `b447b700…` (2,987,024 bytes). The play was first
  published on 2026-11-03.
- **Cover**: `01-cover.png` — PNG, 1152x1536, 3:4. Publish metadata only; the
  image lives with the community publication and is not stored here. The store
  revision carries three more illustrations — `02-depart.png`, `03-return.png`,
  `04-door.png` — all 3:4 PNG and labelled as illustrations, not live photos.
- **Source**: <https://github.com/Jiaqingrun/ai-passport-ashbox>, branch
  `feature/ashbox-ember`.

**Description (as published):**

Turn AI Passport into Ember—an offline pocket outpost at the end of the world.

Home is a hearth room: the fire burns when people are in; the room changes when someone leaves, the field is ready, the stash is full, trade is open, the door feels wrong, someone returns, or night wakes you. On the home screen you can long-press Up/Down to browse only the station scenes that are valid in the current situation—crisis and return reports lock to that one scene. Soft wood crackles by the stove. You manage food, water, iron, parts, and embers, and send Ahe, Lao Que, and Xiao Mang into the ruins. Stay awake to tidy the outpost, or close the lid and sleep while the wasteland keeps time.

Leaving means route, person, and optional gear: a crowbar, or a pistol with rounds. Short routes are safest; the abandoned street farms food and parts; the deep digs for embers; open the gate for the hunt and jerky; overnight fits a long sleep. Coming home empty-handed is a real probability—Help and the route subline show baseline rates. High trust and lower fatigue help; a pistol with rounds can stop a sudden assault.

While someone is away, idle time only dims the screen so the countdown stays visible. Long-press OK for deep sleep; on wake, progress catches up. A full power-off freezes the countdown; deep-sleep wake is what catches that time up. Return choices and the night watch must be made while you are awake.

Build, trade, plant, expand the stash, read clues, and set overnight orders. Unlock the workbench on the Building page first, then craft water cans, crowbars, pistols, and ammo on the Gear page. Parts raise buildings and fit quiet wheels and door bolts; iron crafts carry gear. Milk clears fatigue; a heavy wound needs adrenaline within about a day or they will not make it; antibiotics cool a fever injury. Taking in a wanderer first shows their trait and bonuses for confirm. When threat climbs too high, you must stay awake for the night watch.

Important: installing or updating from the community store clears progress and settings. If you already have a save, back up with Save Manager first, then import after install. Progress already on this device usually continues at boot—that does not survive a community wipe without a prior backup.

Cover art is an atmosphere illustration, not a photo of the device. Forgotten rules live under Outpost → Help.

## What it does

- **Five-resource economy**: food, water, parts, iron, and embers, each with a
  pocket, a stash, and a visible cap. Embers are both a currency and a late-game
  resource; a top bar shows the five counts at all times, with a second five-slot
  strip under the home art.
- **Departures with visible odds**: five tiers — short (about 3 minutes), block
  (1 hour), deep (4 hours), hunt (2 hours, needs the gate opened for 3 embers),
  and overnight (8 hours). Every route subline states the food and water cost,
  the loot range, the base empty-handed chance, and the ember chance. Trust,
  fatigue, carry gear, and the gate bonus adjust those numbers, and an armed
  pistol can cancel one ambush.
- **People, not numbers**: three survivors with different biases (steady,
  parts-leaning, clue-leaning), each with trust and fatigue. Trust lowers the
  empty-handed chance; fatigue degrades output and blocks departure when it is
  high. Injuries include a heavy wound that needs adrenaline within about a day
  and a fever that slowly downgrades to weakness.
- **Threat and the night watch**: threat rises over time. At 80 with nobody away
  you must stay awake to talk, fortify, or hide; at 100 the door is forced open,
  threat falls to 79, the next six hours stay quiet, and the station pays parts.
  The crisis page is a real decision point rather than a summary: it can be left
  open while the device sleeps and does not resolve itself.
- **Buildings with trade-offs**: dorm, kitchen, workshop, gate, stash
  expansions, furniture, water well, and smaller fittings. Every upgrade raises
  the bill and carries a drawback as well as a benefit — a workshop speeds parts
  but raises threat, a kitchen lowers food use, a well upgrade pauses well water
  only, a dorm upgrade rests people faster without costing more food.
- **Three investigation lines and a meta-story**: the Tin Visitor, the Ember
  Whisper, and the Ash Tide Beyond the Gate, five steps each, one step per day
  (8 parts buys one more step that day). Finishing all three lines with the
  story layer turned over counts as a clear; two further chapters continue
  afterwards with a higher threat floor and thinner loot.
- **Deep sleep and wall-clock catch-up**: the game runs on real time. Deep sleep
  advances expeditions, fields, and wells at the full wall-clock rate while food
  use, parts production, threat rise, and water use are scaled down; on wake a
  report explains what happened. A blanked screen at the station never hides a
  running expedition countdown.
- **Journal, radio, and standing orders**: a ring buffer of short log lines, a
  radio line that switches to away-messages while someone is out, and an
  overnight standing order (none, ration, watch, rush) that only affects the
  deep-sleep segment.

## Interaction

- **Up / Down (click)**: move the highlight, page through the home buttons, or
  scroll a list.
- **OK (click)**: activate, confirm, page the opening text, or advance a report.
- **OK (long press)**: leave an inner page, or request deep sleep from the home
  screen (the press counts on release; a further press cancels).
- **Up / Down (long press)**: on the home screen, switch the scene that matches
  the current situation; in Help, jump between the eight sections.
- A persistent badge on the top bar marks pending items (report, crisis, station
  event, passer-by, harvest ready, stash full, trade available), and the "now"
  line states only the single most urgent thing.
- A notice that needs reading takes the screen and waits for OK; it is never
  wiped by the page repaint underneath it.

## Persistence and installation

- The game save lives in NVS under namespace `ashbox`, key `save`, blob magic
  `ASH2`, ABI version 13, 776 bytes; older saves migrate on load. Meta progress
  across runs uses key `meta` with magic `ASHM`.
- Settings → Backup / Import implement the `!EMBACKUP` USB protocol for the
  [Ember Save Manager](https://github.com/Jiaqingrun/AI-Passport_BS) release
  `save-manager-v1.0.1`: a backup is verified and written as an `.emsave` file,
  and an import first saves current progress as `Ember-before-import-…` before
  asking for confirmation.
- A community-store install or update writes a merged firmware image and clears
  the device, including this save. Back up first, then import after installing.
  Flashing the application alone (offset `0x10000`) keeps NVS and the save.

## Source and code layout

- Application code lives on branch `feature/ashbox-ember` of
  <https://github.com/Jiaqingrun/ai-passport-ashbox>, not upstream.
- `main/ashbox_game.c` holds the rules core and is host-tested;
  `main/ashbox_content.c` holds the content tables; `main/ashbox_ui*.c` is the
  LVGL application shell (home, pages, trade, report) and keeps every rules call
  inside the application runtime lock. `main/ashbox_save.*` implements the NVS
  blob, migrations, and the dual-slot CRC seal; `main/ashbox_backup.c` implements
  the USB save protocol.
- The UI is a purpose-built shell for this application, not the hardware test
  menu. Chinese UI text uses a generated 16 px subset regenerated from
  `main/ashbox_font_symbols.txt`.

## Verification

- **Build**: PASS — merged image 2,987,024 bytes; application 2,921,488 bytes of
  the 8,323,072-byte factory partition (35.1 % used).
- **Host tests**: PASS — `./tools/validate.sh --static`, including the rules-core
  suite, the save ABI static asserts, and the copy-layout checker.
- **Device tests**: PARTIAL — the merged image was flashed to an AI Passport
  (ESP32-C3, 8 MB) and booted with a clean log: save ABI 776 reported, I2C
  devices found, display ready, UI ready, USB save protocol ready.
- **Unverified**: the behavioural walk on hardware (trade list with both camps
  open, super-core confirmation, notice dismissal, crisis page left open), sleep
  current, and audio quality.
