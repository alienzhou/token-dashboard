[English](README.md) · **简体中文**

<div align="center">

# Token 足迹

### 每一次 AI 创作，都看得见。

自动采集本地用量，在电脑看全貌，把创作足迹带在身边。

[开始使用](#开始使用) · [支持的工具](#支持的工具) · [核对数字](#核对数字) · [应用说明](docs/token-dashboard.zh_CN.md)

![Python 3.9+](https://img.shields.io/badge/Python-3.9%2B-174D3D?style=flat-square) ![数据留在本机](https://img.shields.io/badge/Data-Local-174D3D?style=flat-square) ![蓝牙同步](https://img.shields.io/badge/Sync-Bluetooth-174D3D?style=flat-square) ![MIT 许可证](https://img.shields.io/badge/License-MIT-174D3D?style=flat-square)

</div>

<table>
  <tr>
    <td width="50%"><img src="docs/assets/token-dashboard/01-studio.png" alt="Token 足迹棚拍风格 AI 效果示意，使用模拟数据" width="100%"></td>
    <td width="50%"><img src="docs/assets/token-dashboard/02-desktop.png" alt="Codex 用量的桌面场景 AI 效果示意，使用模拟数据" width="100%"></td>
  </tr>
</table>

*以上为参考设备外形制作的 AI 效果示意图，全部用量均为模拟数据。*

| 自动收集 | 一眼看清 | 数据留在本机 |
| --- | --- | --- |
| 每 5 秒读取兼容的工具记录，无需手动导入。 | 累计 Token、单日峰值、连续天数和半年活动图。 | 本地 Web 面板查看完整统计，通过加密蓝牙把汇总数据同步到设备。 |

## 开始使用

电脑端已在 **macOS** 验证。准备 **Python 3.9+**，并先将[合并固件](#构建与刷写)刷入 AI Passport。设备无需配网。

**1. 启动采集器。**

```bash
git clone --branch feature/token-dashboard https://github.com/alienzhou/token-dashboard.git
cd token-dashboard
./tools/run-token-dashboard.sh
```

首次运行会安装依赖；出现蓝牙权限提示时允许访问。请保留当前源码目录，macOS 启动器依赖此目录。

**2. 打开本地面板。**

访问 [127.0.0.1:8964](http://127.0.0.1:8964)。只想在电脑采集和查看时，使用 `./tools/run-token-dashboard.sh --no-ble` 启动。

**3. 配对随身设备。**

长按设备**确认键**，在电脑配对弹窗输入屏幕显示的六位码。设备显示**已连接**即建立同步；配对窗口超时后再次长按确认键。

**4. 继续创作。**

保持采集器运行，正常使用 AI 工具即可每 5 秒采集。电脑按 `Ctrl+C` 退出，下次启动会保留已采集历史。

## 用量，一眼看清

<table>
  <tr>
    <td width="50%"><img src="docs/assets/token-dashboard/03-ui-overview.png" alt="当前固件主界面渲染，展示累计用量与活动热力图" width="100%"></td>
    <td width="50%"><img src="docs/assets/token-dashboard/04-agent-comparison.png" alt="当前固件的 Codex 与 Claude Code 界面对照渲染" width="100%"></td>
  </tr>
</table>

*以上从当前固件源码渲染，使用合成测试数据，非实机截图。[原始主界面](docs/assets/token-dashboard/screens/all-agents.png) · [Codex](docs/assets/token-dashboard/screens/codex.png) · [Claude Code](docs/assets/token-dashboard/screens/claude-code.png)*

| 按键 | 操作 |
| --- | --- |
| 上 / 下 | 在全部产品与各个工具之间切换 |
| 短按确认 | 切换用量页与同步页 |
| 双击确认 | 切换半年活动图时间窗口 |
| 长按确认 | 开启物理配对窗口并显示六位码 |

## 支持的工具

| 工具 | 自动采集 | 验证情况 |
| --- | --- | --- |
| Codex | 兼容的本地用量记录 | 已使用真实本地记录独立对账 |
| Claude Code | 兼容的本地用量记录 | 结构样本验证 |
| OpenCode | 兼容消息存储；仅有 v2 存储时不支持 | 结构样本验证 |
| Gemini CLI | 兼容的本地用量记录 | 结构样本验证 |
| Cursor | 暂不支持 | 尚无可靠的本地适配器，不提供手动导入 |

能否采集取决于工具实际写出的记录。遇到缺失数据，请先查看[采集结构与限制](docs/token-dashboard.zh_CN.md)。

## 核对数字

```bash
python3 tools/audit_token_usage.py --url http://127.0.0.1:8964
```

独立的只读核对工具重算可用的 Codex 日志，与账本逐条对账，并检查面板累计量及每日数字。报告列出缺失、差异、重复、计数重置、留存历史和初始累计基线。自定义账本时，采集器和核对工具必须使用相同的 `--state`。

**日志用量不等于服务商账单或订阅额度。** 部分初始累计基线早于第一条可见事件，无法还原其最初发生日期。目前只有 Codex 使用真实本地记录验证，其他适配器为结构样本验证。

## 构建与刷写

<details>
<summary><strong>展开固件、验证与刷写说明</strong></summary>

仓库包含完整 ESP-IDF 项目与可复用 FoloToy BSP。目标：**ESP32-C3、8 MB Flash、无 PSRAM**，ESP-IDF **5.5.3**。

```bash
# 先激活 ESP-IDF 5.5.3 环境。
./tools/validate.sh
```

检查生成 `build/FoloToy-AI-Passport-full.bin`，用于从 **0x0** 刷写；匹配的固定 ELF/MAP 归档位于 `build/firmware/<SHA256>/`。

**整包会清空已有 NVS 设置和蓝牙绑定。** 无需全片擦除。生成的固件不提交 Git。

已测试固件的构建与主机检查通过；设备刷写和限时启动检查通过，完整配对、显示、断电和射频验收仍以[验证报告](docs/token-dashboard-validation.zh_CN.md)中的未验证项为准。构建成功不代表硬件验证通过。

</details>

## 深入了解

- [应用说明](docs/token-dashboard.zh_CN.md)：采集、蓝牙同步、持久化和适配限制。
- [验证结果](docs/token-dashboard-validation.zh_CN.md)：精确测试固件与待完成的设备检查。
- [开发约定](AGENTS.md)：开发前先阅读，应用分支为 `feature/token-dashboard`。

仅采集数字元信息；提问、代码、凭证、个人账本、设备日志和真实用量截图不提交 Git。

基于 [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)，保留其 [MIT 许可证](LICENSE)。Noto CJK 字体遵循 [SIL OFL 1.1](assets/fonts/OFL.txt)。
