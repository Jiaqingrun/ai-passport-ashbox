[简体中文](./publish-to-community.zh_CN.md) · **English**

# Publish to the AI Passport Community

Human-facing entry point for releasing this project's firmware to the
[AI Passport Community](https://ai-passport.folotoy.cn). A release is not a commit: commit during
development, release only when the firmware is finished and usable.

## Decide the destination first

- **Community** — publish the firmware to the market; follow the workflow below.
- **Git** — commit the finished code, then open a release. The artifact may come from CI
  (tag-triggered `.github/workflows/build-firmware.yml`) or from a local build; follow
  [`commit-and-pr.md`](../../contribution/commit-and-pr.md) and the
  [CI release flow](../ci/CI-build-and-release.md).
- **Not yet** — the code is unfinished; keep developing.

## Community workflow

Install the official publisher and run it for this project, then review its preview before anything
is uploaded: `https://ai-passport.folotoy.cn/skills/folotoy-ai-passport-publisher.zip`.

The publisher skill defines the exact steps (inspect the project, prepare bilingual title and
description, resolve the source page, prepare and validate a cover, authorize through the official
site, preview every field). The repository does not store or commit that skill.

## Required inputs

- **Firmware** — the merged image `build/FoloToy-AI-Passport-full.bin`. It must pass
  `./tools/validate.sh --firmware` and the [firmware layout](../engineering/firmware-layout.md).
  Never substitute the app-only image from `idf.py build`.
- **Cover** — portrait 3:4 JPEG / PNG / WebP, at most 10 MiB.
- **Source** — the public HTTPS Git page of the firmware repository (GitHub, Gitee, GitLab,
  Codeberg, or another reachable HTTPS Git page). A fork owner publishes from the fork's own page.

## Boundaries

- Upload only to `https://ai-passport.folotoy.cn`. Publishing and updating are external mutations.
- A preview the author has not confirmed does not authorize upload.
- The assistant never requests, receives, or stores authorization credentials.
- Never retry a rejected upload automatically; report the response and resolve the cause first.
- Never weaken, bypass, or remove the firmware-layout gate to make a submission pass.
- An update uses the one-time update prompt issued by the official site; never reuse an old code.

## After publishing

Ask the developer whether to archive the application into the upstream
[`docs/reference/`](../../reference/README.md) archive. If so, generate a bilingual text-only
summary under `docs/reference/<username>/<app-name>/` (bilingual `README.md` / `.zh_CN.md`),
record the cover only as file name and format, register it in both index languages, and use the
`plays-archive` skill to open the PR against upstream `FoloToy/ai-passport`. Do not commit the
cover image or a firmware `.bin` there.
