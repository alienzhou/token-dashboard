[English](README.md) · **简体中文**

# AI Passport Token 用量面板

自动采集本机 AI Agent 的 Token 用量，在本地浏览器查看，并通过加密 BLE
把汇总统计同步到 240×320 的 AI Passport。仓库包含完整 ESP-IDF 项目与 FoloToy BSP。

![设备界面，使用合成测试数据](assets/images/token-dashboard-preview.png)

- 自动适配 Codex、Claude Code、OpenCode 兼容消息存储与 Gemini CLI。Cursor 明确标记暂不支持，无需手动导入。
- 每 5 秒增量采集，账本持久化并去重；本地面板显示累计量、单日峰值、连续天数与活动图。
- 设备端通过上下键切换产品，突出累计用量，展示三项次级指标、半年热力图，并使用六位码配对。
- 数据留在本机：仅采集数字元信息；提问、代码、凭证、个人账本、设备日志及真实用量截图不提交 Git。

## 启动电脑端

需要 Python 3.9 及以上，在仓库根目录运行：

```bash
./tools/run-token-dashboard.sh
# 只采集和显示本地面板，不启用蓝牙：
./tools/run-token-dashboard.sh --no-ble
```

打开[本地面板](http://127.0.0.1:8964)，保持采集器运行即可持续更新。macOS 启动脚本
创建包含蓝牙用途声明的本机 alias 应用，依赖当前源码目录。首次配对需长按设备确认键，
在系统弹窗输入设备屏幕显示的代码。上下键切换产品，确认打开同步页，双击确认切换活动图时间窗口。

## 核对数字

```bash
python3 tools/audit_token_usage.py --url http://127.0.0.1:8964
# 自定义账本时，采集器和核对工具必须使用相同的 --state。
```

独立的只读核对工具重算当前 Codex 日志，与账本逐条对账，并比较面板累计量及每日数字。
报告列出缺失、差异、重复、计数重置、留存历史和初始累计基线。与日志一致不等于与服务商账单一致。
部分初始累计基线早于可见的第一条事件，无法还原最初发生日期。目前只有 Codex 使用真实本地记录验证，
其他适配器只有公开结构的样本验证。

## 构建与刷写

目标：**ESP32-C3、8 MB Flash、无 PSRAM**，ESP-IDF **5.5.3**。

```bash
# 先激活自己的 ESP-IDF 5.5.3 环境。
./tools/validate.sh
```

检查生成可从 **0x0** 刷写的 `build/FoloToy-AI-Passport-full.bin`，以及位于
`build/firmware/<SHA256>/` 的固定匹配 ELF/MAP 归档。整包会清空旧 NVS 设置和蓝牙绑定，
不需要全片擦除。生成的固件不提交版本控制。

采集结构、限制、BLE 与持久化见[应用说明](docs/token-dashboard.zh_CN.md)，精确测试固件和待验收项见
[验证结果](docs/token-dashboard-validation.zh_CN.md)。开发前阅读 [AGENTS.md](AGENTS.md)，
应用开发分支为 `feature/token-dashboard`。

基于 [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)，保留其 [MIT 许可证](LICENSE)。
Noto CJK 字体遵循 [SIL OFL 1.1](assets/fonts/OFL.txt)。
