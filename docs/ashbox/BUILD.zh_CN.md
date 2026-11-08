**简体中文** · [English](./BUILD.md)

# 构建身份

最新已校验固件归档：

- 归档：`build/firmware/445875b747cdcf8075afa6db55fd937df0280cca9737c22eb917203af8d16050`
- 构建：2026-11-08 10:38，ESP-IDF v5.5.3
- 应用镜像：2,924,960 B = 8,323,072 B factory 分区的 35.1%（剩余 64.9%）
- ELF sha256：`d76466afc5cc42126326c35ab916485732714666f697bda9c400ab29ec4aec02`
- 复核：`python3 tools/archive_firmware.py verify <归档目录>`

### 固件归档保留规则（本机整理）

`build/firmware/` 是本机构建输出目录，已被 gitignore 忽略。本仓库的实际规则：保留被维护中的文档引用的归档、最新 3 个归档，以及每个自然日最新的归档；其余删除。保留与删除的记录写在 `build/firmware/ARCHIVE_INDEX.md`。这只是本机开发整理，不是产品契约。

## 刷机与存档（重要）

《余烬》存档在 NVS（`0x9000`）。**放置进度很珍贵，日常开发默认保档刷机。**

| 方式 | 命令 / 工具 | 何时用 |
| --- | --- | --- |
| **保档（默认）** | `./tools/flash_ashbox_preserve_save.sh /dev/cu.usbmodemXXXX [归档目录]` — 只写 `0x10000` 应用区 | 迭代玩法、修 bug、真机验收 |
| 完整刷新 | 合并镜像 `FoloToy-AI-Passport-full.bin` 写 `0x0` | 空白机、分区表变更、或**明确同意清档** |

合并镜像从 `0x0` 写入时擦除范围会盖住 NVS，**存档会没**。不要把合并刷当作日常默认。

### 社区更新必写（硬性）

官方玩法社区「安装」会**覆盖设备当前全部内容**（见 [安装社区玩法](https://ai-passport.folotoy.cn/guides/flash-official-firmware/)）。  
**下次向社区提交版本时，更新说明必须明确写清：**

- 社区安装 / 更新**不是**本机保档刷机；
- 重新安装会清空本机进度与设置，请自行接受后再装；
- 勿承诺「更新不丢档」。

本机开发仍用 `flash_ashbox_preserve_save.sh`；该脚本**不会**随社区包提供给其他玩家。

另：远征倒计时在醒着推进时会**每 15 秒**写入 NVS（出发/战报/危机/站事立即写）。只保档刷机不够——若固件从不落盘剩余时间，复位后仍会回到出发满时。

存档格式：NVS 命名空间 `ashbox`、键 `save`，magic `ASH2` / 现行 ver 13、blob 776 字节（旧 ver 可迁移；CRC 失败则安全新建）。以 `main/ashbox_game.h` 的 `ASHBOX_SAVE_VER` 和 `main/ashbox_save.h` 的 `ASHBOX_SAVE_BLOB_SIZE` 为准。
