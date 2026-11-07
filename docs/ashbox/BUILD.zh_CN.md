**简体中文** · [English](./BUILD.md)

# 构建身份

最新已校验固件归档：

- 归档：`build/firmware/f0d150b367e5be4813a5ed70a299b8062d9f325016746155ea0e5c178630593c`
- 复核：`python3 tools/archive_firmware.py verify <归档目录>`

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

存档格式：NVS 命名空间 `ashbox`、键 `save`，magic `ASH2` / 现行 ver 11（旧 ver 可迁移；CRC 失败则安全新建）。以 `main/ashbox_game.h` 的 `ASHBOX_SAVE_VER` 为准。
