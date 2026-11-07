**简体中文** · [English](./README.md)

# 《余烬》末世像素场景资产

- 画幅：**240×176** 全宽主舞台，RGB565，每景 **3 帧**
- 画风：同一间暖炉屋。各景只用火、人、门区分状态
- 预览：`preview/hearth_all_f0.png`
- 绘制并编码：`python3 tools/ashbox_hearth_paint.py && python3 tools/ashbox_sprites_encode.py`
- 默认输出 **indexed+RLE（ASR1）** 到 `assets/ashbox/sprites_rle/*.bin`（固件按帧解码到单缓冲）；`--format rgb565` 仅遗留调试
