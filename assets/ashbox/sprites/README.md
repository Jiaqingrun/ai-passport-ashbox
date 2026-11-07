[简体中文](./README.zh_CN.md) · **English**

# Ashbox apocalyptic pixel scenes

- Size: **240×176** full-bleed stage, RGB565, **3 frames** per scene
- Look: one hearth room. Scenes differ by fire, people, and the door
- Previews: `preview/hearth_all_f0.png`
- Paint + encode: `python3 tools/ashbox_hearth_paint.py && python3 tools/ashbox_sprites_encode.py`
- Default output is **indexed+RLE (ASR1)** under `assets/ashbox/sprites_rle/*.bin` (runtime decodes one frame into a shared buffer); `--format rgb565` is legacy debug only
