[English](token-dashboard.md) · **简体中文**

# Token 用量面板

为 AI Passport 重新设计的 240×320 应用及本地电脑采集器：统计栏、日历热力图、各工具用量。复用完整 BSP，驱动和引脚未改动。使用本地 `main` 的 `0b9e4c8` 基线，在 `feature/token-dashboard` 独立工作树开发，原项目未提交的修改完整保留。

## 全自动采集

| 工具 | 自动读取来源 | 统计口径 | 验证范围 |
| --- | --- | --- | --- |
| Codex | `CODEX_HOME/sessions` 与 `archived_sessions` 的 JSONL；默认 `~/.codex` | 累计 `total_token_usage.total_tokens` 的增量，重复事件及日志副本去重；缓存和推理已包含，不重复加 | 本机真实记录、增量读取、测试样本 |
| Claude Code | `~/.claude/projects/**/*.jsonl`，含子 Agent | 输入、输出、缓存读取与缓存写入之和；按 API 消息 ID 去重，支持内容块及流式更新 | 自动化样本；本机未发现日志 |
| Cursor | 暂不支持 | 本机未找到经过验证的自动精确 Token 来源。不提供手动导入，不读取凭证，不调用私有接口，不用请求次数估算 | 来源不可用 |
| OpenCode | `${XDG_DATA_HOME:-~/.local/share}/opencode/opencode.db` 的只读 WAL 查询，或旧版 `storage/message/**/*.json` | v1 兼容 assistant 记录的输入、输出、推理、缓存读写之和 | SQLite/WAL 和 JSON 样本；本机未安装 |
| Gemini CLI | `~/.gemini/tmp/*/chats/session-*.json` 与 `.jsonl` | 消息内已汇总的 `tokens.total`；按消息 ID 去重 | JSON/JSONL 样本；本机未安装 |

依据：[Gemini 记录实现](https://github.com/google-gemini/gemini-cli/blob/main/packages/core/src/services/chatRecordingService.ts)、[OpenCode 兼容消息表](https://github.com/anomalyco/opencode/blob/dev/packages/core/src/session/sql.ts)、[OpenCode 统计实现](https://github.com/anomalyco/opencode/blob/dev/packages/opencode/src/cli/cmd/stats.ts)、[Claude Code 用量说明](https://code.claude.com/docs/en/costs)。

适配器仅支持以上明确的数字结构。OpenCode 新版只有 v2 `session_message` 记录、没有兼容消息行时，会标记暂不支持；未来字段或存储格式变更需要更新适配器。统计的是本地已记录的 Token 工作量，包含缓存处理，不代表账单费用、订阅额度或完整账户用量。首次采集前删除的历史、仅存在于远端的 Agent 不在覆盖范围内。已经采集的历史会在日志被清理后继续保留，来源状态单独反映当前可用性。不修改源日志或认证文件。

每 5 秒检查追加的 JSONL、已修改的 JSON 文件与数据库更新行。去重记录和文件游标保存在 `~/.local/share/token-dashboard/usage.sqlite3`，权限 0600，重启后继续增量读取。只保存哈希 ID、数字、时间戳、游标和已完成的 Codex 任务时长，不保存提问、代码、模型回复或凭证。这是持久的个人历史账本，没有自动过期；使用另一个 `--state` 路径可以建立独立账本。同一账本只运行一个采集进程。

## 启动电脑端

首次使用请按 [README 的下载、终端启动与配对步骤](../README.zh_CN.md#开始使用)操作，其中也说明了网页筛选、停止与再次启动的方法。

在仓库根目录运行：

```bash
./tools/run-token-dashboard.sh
# 仅本地实时采集和预览，不启用蓝牙：
./tools/run-token-dashboard.sh --no-ble
```

脚本按需创建仓库内虚拟环境并安装锁定的 Bleak 0.22.3，需要 Python 3.9 及以上。macOS 使用包含蓝牙用途声明的 `build/desktop/Token Dashboard.app`；`tools/package-token-macos.sh` 通过 py2app 0.28.8 的 alias 模式生成。脚本通过 macOS LaunchServices 启动应用，将输出转发到终端，并将 Ctrl+C 传给该应用实例；即使从 Warp 启动，蓝牙权限也归属 Token Dashboard。这是本机启动器，依赖当前路径的源码和虚拟环境，不是可移到另一台电脑的独立应用；移动目录后需重新打包。启动器在后台运行，需另行打开面板 URL。打开[本地面板](http://127.0.0.1:8964)。只监听本机回环地址，不加载外部脚本或字体，蓝牙只传汇总统计。macOS 需要给 Token Dashboard 蓝牙权限；Linux 需要 BlueZ，Windows 需要支持 BLE 的适配器。这些平台尚未完成设备测试。可指定 `--port`、`--interval`、`--device`（蓝牙 UUID/地址）和 `--state`。发现多台设备时不自动挑选，会显示标识，需用 `--device` 指定目标。

未安装登录启动项。保持采集器运行即可持续采集和自动重连。大量历史的首次扫描较慢，后续只处理新增或变化的记录。

macOS alias 启动器默认从应用包目录启动，入口会把工作目录统一为仓库根目录，
因此相对 `--state` 路径与 shell 启动器对应同一项目位置。

## 准确性与独立对账

运行 `python3 tools/audit_token_usage.py --url http://127.0.0.1:8964`，自定义账本时传入与
采集器相同的 `--state`。核对工具不导入采集器解析或归约函数：它按保存的字节游标独立重读
Codex 数字元信息，与只读账本逐条核对增量、比较每日汇总，并可核对面板累计量和每日数组。
输出只有汇总计数；含个人总量的报告应保存在 `build/` 等被忽略的本地目录。

`PASS` 只表示当前可读取日志中的数字运算一致。报告还会指出源日志已不可用的留存记录，以及
早于首条可见事件的初始累计基线。初始用量被归入首条可见事件所在日期，无法还原原本发生日期。
文件或面板正在变化时，可能需要重新核对。这个检查不能证明服务商账单正确，不能恢复首次采集前
删除的记录，也不能验证未安装工具的适配器。真实 Codex 对账与其他工具的结构样本测试分开报告。

## 设备界面与按键

- 上、下按下即响应：循环切换全部、Codex、Claude Code、Cursor、OpenCode、Gemini CLI；同步页也能直接切换回产品用量。释放或快速双击不会重复计算一次按下，长按上下键不额外导航。
- 确认短按：切换用量页与同步页。
- 确认双击：切换最近半年与前半年的活动图，返回用量页。
- 确认长按：开启 60 秒配对窗口，立即显示六位码，无需等待电脑提出密码请求。
- 静止五分钟息屏后，第一次按键仅唤醒，不触发导航。
- 显示配对码期间锁定导航，避免误操作隐藏配对码。

主界面顶部突出当前产品，配有上下箭头和位置指示。累计量采用 36 px 大字号，单日峰值、当前连续与最长连续天数分成三列，下面保留周一开始的活动热力图与每日用量；连接、电量等信息使用较轻的文字。超长数值自动降低字号以适配字段，来源不可用时显示明确状态。电脑展示滚动 364 天；设备分两页，每页 182 天。统一按北京时间 UTC+8 汇总。今天尚未使用时，昨天仍活跃的连续记录不会立即清零。峰值与连续天数按全部已采集历史计算，不局限于热力图窗口。最长任务仅取 Codex 成对的 `task_started` 和 `task_complete`，不以整个会话跨度猜测。未找到和不支持来源单独显示，不伪装成零用量。

使用许可完整的 Noto Sans CJK 子集，字号为 12、16、20、36 px。`tools/generate_token_fonts.py` 固定 lv_font_conv 1.5.3 和源字体哈希。主机测试逐字检查覆盖，包含一个已知缺字反例，并检查实际标签字体、文本宽度、屏幕边界、导航与可见产品联动，以及多次重建。动态文本限定为固定标签、ASCII 工具名、数字和已验证的单位，不支持任意用户文本或 emoji。

产品选择栏的中文标题采用 16 px 和较宽字间距，英文工具名使用内置 Montserrat 20，
与主数字形成明确层次。

60 秒无操作后调暗，5 分钟后息屏；配对时保持亮屏。为了持续同步，BLE 保持可用，未实现深睡或完成实际电流优化。

## 蓝牙与持久化

广播名 `TokenPassport-XXXX`。服务 UUID `f2d00001-7a43-4d65-a749-b0249cbb1001`，认证写入 RX 的编号为 `0002`，认证读取回执编号为 `0003`。首次配对需物理窗口、LE Secure Connections 和设备显示的六位码。窗口之外在启动安全交换前拒绝未绑定电脑；绑定检查采用 NimBLE 同样的已解析身份地址。
窗口内重试使用屏幕上的同一代码，已开始的配对交换保留代码至成功或断连；成功后关闭窗口并返回用量页。
日志不打印配对码。已绑定电脑可直接重连；替换旧绑定密钥必须重新开启物理窗口，只删除对应电脑的旧绑定。最多保存两个绑定。NVS 初始化异常不会自动擦除数据。

`TKD1` 快照共 7,421 字节，包含小端序列号、UTC 时间、北京时间日期、最长任务时长、总体峰值和连续天数、五个来源元信息、每来源 364 个 LE32 每日 Token 值及 IEEE CRC32。每个带响应的 ATT 写入携带 LE32 序列号、LE16 偏移和内容；接收器支持顺序分包及完全一致的重传，拒绝冲突、乱序和越界，完整解析并验证 CRC 后才应用。支持最小 ATT payload 20 字节；协商更大值可降低延迟。回执为 LE32 序列号加状态字节：0 表示已接收至 RAM，1 表示快照无效。只有与当前序列完全一致的回执才算同步成功。

蓝牙和按键回调不操作 LVGL 或写应用 NVS，完整快照交给应用任务。变化的快照最多每分钟保存一次，首次变化立即保存。回执确认的是 RAM 接收；突然断电可能丢失最后一分钟。保存失败保留上一份持久快照并提示错误；数据不变的心跳不会继续写 Flash。重启或断开后显示缓存，同步页显示同步日期，离线每日数字标注“同步日”。电脑自动发送变化，每 60 秒发送一次数据不变的心跳。配对、重连速度、保存时断电、最小 MTU 速度和射频距离仍需实机验收。

## 构建与验证

激活 ESP-IDF **5.5.3** 后运行：

```bash
./tools/validate.sh
```

包含上游静态检查和主机测试、采集与传输样本、独立 C 接收器测试、隔离环境固件构建、合并镜像和 ELF 归档验证，以及使用实际锁定 LVGL 的渲染测试。预览输出到 `build/token-preview/*.ppm`。构建产物和个人账本不进入版本控制。`components/bsp` 与 8 MB 默认分区均未改动：NVS `0x9000`、PHY `0xF000`、factory app `0x10000`。

交付 `build/FoloToy-AI-Passport-full.bin`，从 **0x0** 刷写，配套调试归档位于 `build/firmware/<SHA256>/`。不能把 app-only 镜像写到 0x0。合并镜像会填充间隙，可能重置旧应用数据、NVS 和蓝牙绑定。构建请求不授权烧录或全片擦除；交付的配对/标题修正版已在用户明确授权后烧录，未做全片擦除；后续新固件仍须确认后刷写。

精确交付标识和 Build、Host tests、Device tests、Unverified 状态见[验证结果](token-dashboard-validation.zh_CN.md)。
