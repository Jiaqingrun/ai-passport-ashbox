<p align="right">
  <strong>简体中文</strong> · <a href="publish-to-community.md">English</a>
</p>

# 发布到 AI Passport 社区

把本项目固件发布到 [AI Passport 社区](https://ai-passport.folotoy.cn) 的入口页。发布不是提交：开发中提交，**开发完成、固件可用**才发布。

## 先决定发到哪里

- **社区** —— 发到社区市场，走下面的流程。
- **Git** —— 先提交完成后的代码，再开 release。产物可由 CI（tag 触发 `.github/workflows/build-firmware.yml`）或本地构建产生，遵循 [`commit-and-pr.md`](../../contribution/commit-and-pr.zh_CN.md) 与 [CI 发布流程](../ci/CI-build-and-release.zh_CN.md)。
- **都还没到** —— 代码未完成，继续开发。

## 社区流程

让助手安装官方发布助手并跑一遍，**先看预览再上传**：
`https://ai-passport.folotoy.cn/skills/folotoy-ai-passport-publisher.zip`

skill 定义精确步骤（检查项目、准备中英文文案、解析源码页、准备并校验封面、经官网授权、逐项预览）。本仓库不保存、不提交该 skill；更新用官网下发的更新授权提示词，不手抄、不复用旧码。

## 必须提供

- **固件** —— 合并镜像 `build/FoloToy-AI-Passport-full.bin`，须通过 `./tools/validate.sh --firmware` 与[固件布局](../engineering/firmware-layout.zh_CN.md)；不得用 `idf.py build` 的应用单镜像替代。
- **封面** —— 竖版 3:4 的 JPEG / PNG / WebP，≤ 10 MiB。
- **源码** —— 固件仓库的公开 HTTPS Git 项目页（GitHub、Gitee、GitLab、Codeberg 或其它公开可达者）；fork 所有者从其 fork 页发布。

## 边界

- 只上传到 `https://ai-passport.folotoy.cn`；发布与更新都是外部变更。
- 作者未确认的预览**不构成上传授权**。
- 助手不索取、不接收、不存储任何授权凭证。
- 被拒的上传不自动重试；先看服务端响应再处理。
- 不得为了过审弱化、绕过或删除固件布局门禁。

## 发布之后

询问开发者是否把应用归档到上游 [`docs/reference/`](../../reference/README.zh_CN.md)。若同意，在
`docs/reference/<username>/<app-name>/` 生成双语纯文本总结（`README.md` / `.zh_CN.md`），封面只记录文件名与格式，在两个语言版本的索引里登记，并用 `plays-archive` skill 向上游 `FoloToy/ai-passport` 开 PR。不要把封面图或固件 `.bin` 提交进该目录。
