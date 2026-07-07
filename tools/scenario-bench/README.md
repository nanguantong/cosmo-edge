# scenario-bench

`scenario-bench` 是 CosmoEdge 场景任务压测工具。它在控制 PC 上运行，通过设备 HTTP API 完成算法编排导入、视频通道准备、任务绑定、阶梯式并发加压、运行指标采样和报告生成。

工具目标是产出可复现、可解释的容量上限结论，例如“某设备、某软件版本、某算法编排、某输入视频下，容量上限为 15 路；16 路因平均丢弃率超过阈值失败”。

设计背景见 [docs/guide/scenario-benchmark-design.md](../../docs/guide/scenario-benchmark-design.md)。

## 环境要求

- Node.js >= 20
- 控制 PC 可访问被测设备 Web/API 地址
- 已在 CosmoEdge Web 页面完成目标场景任务编排，并导出算法编排模板
- 场景包中包含导出的算法编排模板和输入视频

安装依赖：

```bash
cd tools/scenario-bench
npm install
```

## 快速运行

建议先执行 `doctor`，确认场景包、视频、输出目录和设备登录均正常：

```bash
node src/cli.js doctor \
  --scenario scenarios/no-helmet-99898-fps5-20260630 \
  --output reports/no-helmet-99898-fps5-host22 \
  --device http://192.168.0.22 \
  --user admin \
  --password admin
```

检查通过后运行压测：

```bash
node src/cli.js run \
  --device http://192.168.0.22 \
  --user admin \
  --password admin \
  --scenario scenarios/no-helmet-99898-fps5-20260630 \
  --output reports/no-helmet-99898-fps5-host22 \
  --cleanup \
  --verbose
```

输出目录包含：

| 文件 | 说明 |
| --- | --- |
| `report.html` | 面向人工阅读的报告，包含容量结论、判定口径、路数结果和阈值明细 |
| `summary.json` | 机器可读摘要，适合后续做批量汇总或版本对比 |
| `metrics.json` | 完整采样数据，包含每个 tick 的通道指标和硬件资源 |
| `metrics.partial.json` | 运行过程中持续写入的临时采样文件，用于异常中断兜底 |

## 创建场景包

场景包依赖一个预先导出的算法编排模板。该模板不是工具自动生成的，需要先在 CosmoEdge Web 页面上完成一次场景任务编排，然后从平台导出。

推荐流程：

1. 在被测设备或同软件版本设备上，通过 Web 页面创建目标场景任务。
2. 完成算法流程、原子能力、阈值、FPS、告警节点、调度等业务参数配置。
3. 确认该任务可以在单路视频上正常运行。
4. 从平台导出算法编排模板，保存为 `algorithm-template.json`。
5. 使用 `init-scenario` 将模板、输入视频和压测阶梯配置组装成场景包。

`scenario-bench` 会在压测开始前调用 `algorithm/layout/save`，把这个导出的编排模板导入到本次被测设备上。因此，同一个场景包可以复用于同型号或同软件版本设备的横向对比；但如果算法版本、编排结构或业务参数发生变化，应重新导出模板。

可以用 `init-scenario` 从导出的算法模板和视频快速生成场景包：

```bash
node src/cli.js init-scenario \
  --name my-scenario \
  --template C:/path/to/algorithm-template.json \
  --video C:/path/to/input.mp4 \
  --output scenarios/my-scenario \
  --display-name "未戴安全帽检测 - fps5" \
  --algorithm-id 99898 \
  --schedule-id e89c6c6385e5454b35cde0d1653vg
```

生成后建议立即执行：

```bash
node src/cli.js doctor --scenario scenarios/my-scenario --output reports/my-scenario
```

## 场景包结构

一个场景包是一个目录，`scenario.yml` 是唯一配置入口。单任务和多任务使用同一套模型：单任务只是 `tasks` 里只有一个元素。

| 文件 | 说明 |
| --- | --- |
| `scenario.yml` | 场景名称、通道池、任务列表、容量扫描范围和阈值 |
| `algorithm-template.json` 或其他模板 JSON | 从平台导出的算法编排模板；每个 task 可指定自己的模板 |
| `*.mp4` | local 模式下的本地输入视频 |

`scenario.yml` 示例：

```yaml
name: "no-helmet-99898-fps5"
displayName: "未戴安全帽检测(算法 99898, fps5)"
sampleIntervalSec: 3

channels:
  mode: local
  repeatCount: 0
  sources:
    - name: "no-helmet-01"
      file: "LX0000000007.mp4"

tasks:
  - id: no-helmet
    displayName: 未戴安全帽检测
    type: cv
    algorithmId: "99898"
    scheduleId: "e89c6c6385e5454b35cde0d1653vg"
    template: algorithm-template.json

loadProfile:
  - channels: 1
    holdSec: 30
  - channels: 24
    holdSec: 30

thresholds:
  pass:
    maxCriticalPathLatencyMs: 200
    maxDetectorLatencyMs: 150
    avgDiscardRate: 0.05
    maxPacketDiscardRate: 0.01
```

默认运行模式为 `--profile capacity`，工具会将 `loadProfile` 展开为连续路数扫描。以上配置表示最多扫描到 24 路，实际执行路数为 `1,2,3,...,24`。`holdSec` 使用相邻配置的保持时间；常规场景中保持各项一致即可。若某个 step 省略 `holdSec`，工具会按 workload 类型填默认值：纯 CV 场景 30s，包含 VLM 任务的场景 60s；显式配置始终优先。

核心语义：

- `channels` 定义通道池和视频源；`repeatCount: 0` 表示 local 视频循环播放。
- `tasks` 定义可绑定到通道的算法任务，每个任务有自己的算法模板、调度和任务类型。
- 工具会自动解析模板：存在 `DA_00003`/`PDA_00003` 时识别为可观测的 VLM 任务，
  无需手工修改 `type`。
- `BA_00004.enableLlmReview=1` 属于告警节点内部的嵌套审核。当前 RunningDetail 不暴露
  该内部调用的计数与耗时，因此工具会在运行前明确拒绝这类场景，避免回退到 detector
  指标后产生看似正常但错误的报告。
- `bindings` 可选，定义任务与通道的绑定矩阵；省略时所有任务绑定当前阶梯中的全部通道。
- 单任务、多任务使用同一条执行链路：导入所有任务模板 -> 创建通道池 -> 按阶梯增加通道 -> 按绑定矩阵批量绑定任务 -> 采样 -> 判定 -> 生成报告。

`bindings[].channels` 可以使用：

| 写法 | 含义 |
| --- | --- |
| `all` | 绑定当前阶梯中的全部通道 |
| `[1, 2, 3]` | 绑定第 1/2/3 个通道（1-based） |
| `{ from: 1, to: 8 }` | 绑定第 1 到第 8 个通道 |

报告会同时输出整体容量结论和分任务汇总。整体容量仍表示“该 workload 在当前判定阈值下可稳定支撑的最大通道数”；分任务汇总用于定位多任务压测中是哪一个任务先成为瓶颈。

### 任务策略与 VLM

`tasks[].type` 会选择不同的判定策略：

| type | 策略 | 默认判定口径 |
| --- | --- | --- |
| `cv` | 传统视觉算法 | 检测/关键链路延时、丢弃率；运行期 FPS 熔断使用 1 路基线折半 |
| `vlm` | 视觉语言模型分析 | 分析 FPS 达标率、采样缺失率、可选端到端延时；运行期不使用基线折半 |

模板包含 `DA_00003`/`PDA_00003` 时会自动识别为 VLM；若该节点配置了 `fps`，工具会
自动提取为 `targetFps`。模板没有配置时必须显式给出，例如 0.2fps：

```yaml
tasks:
  - id: vlm
    displayName: 0.8B VLM 分析
    type: vlm
    algorithmId: "12345"
    scheduleId: "e89c6c6385e5454b35cde0d1653vg"
    template: vlm-template.json
    targetFps: 0.2

thresholds:
  taskTypes:
    vlm:
      minFpsRatio: 0.8
      maxMissingRate: 0
      maxEndToEndLatencyMs: 30000
      avgDiscardRate: 0.05
```

`local` 模式下，直接 VLM 任务会自动把 `targetFps` 注入为 `param.videoReadFps`，
让 demux 按分析频率取帧，避免 0.1fps 级别的大模型被源视频原生帧率持续推帧压爆。
VLM 场景省略 `loadProfile[].holdSec` 时默认保持 60s，给模型加载、低频推理完成计数和稳定窗口统计留出足够观察时间；需要压长稳态时仍可显式写成 120s 或更长。

阈值合并顺序为：策略默认值 -> `thresholds.pass` -> `thresholds.taskTypes.<type>` 或 `thresholds.strategies.<type>` -> `thresholds.tasks.<taskId>`。因此可以在同一个 workload 内让 CV 和 VLM 使用不同规则。VLM 不会继承全局 `maxDetectorLatencyMs`、`maxCriticalPathLatencyMs`、`maxAvgNodeLatencyMs`，端到端延时需要在 `taskTypes.vlm` 或 `tasks.<taskId>` 下显式配置。

## 命令说明

### `doctor`

用于运行前检查。默认检查本地环境和场景包；如果提供设备参数，则同时检查设备登录和设备信息。

```bash
node src/cli.js doctor --scenario <dir> [--output <dir>] [--device <url> --user <u> --password <p>]
```

检查项包括：

- Node.js 版本
- `scenario.yml`、算法模板和 local 视频文件
- 算法 ID、目标 FPS、视频模式、并发阶梯
- `layout/save` payload 可生成
- local 视频文件存在
- 输出目录可写
- 设备登录和设备信息查询

### `run`

执行完整压测。

```bash
node src/cli.js run --device <url> --user <u> --password <p> --scenario <dir> --output <dir> [options]
```

常用参数：

| 参数 | 说明 |
| --- | --- |
| `--cleanup` | 运行结束后删除本次创建的通道，推荐开启 |
| `--verbose` | 打印每次采样日志，便于排查 |
| `--skip-import` | 跳过 `algorithm/layout/save`，仅在设备上已存在对应编排时使用 |
| `--no-reuse` | 不复用已有 bench 通道，总是新建 |
| `--profile <mode>` | `capacity` 默认，连续扫描路数上限；`configured` 按 `scenario.yml` 原始配置执行 |
| `--ramp-batch-size <n>` | 每次 ramp 新增通道数，默认 1 |
| `--ramp-batch-delay-sec <n>` | ramp 批次间隔，默认 15 秒 |

### `init-scenario`

生成新的场景包。

```bash
node src/cli.js init-scenario --name <name> --template <algorithm-template.json> --video <file> [options]
```

常用参数：

| 参数 | 说明 |
| --- | --- |
| `--output <dir>` | 新场景目录，默认 `scenarios/<name>` |
| `--display-name <name>` | 报告展示名称 |
| `--algorithm-id <id>` | 算法 ID；不传时从模板推导 |
| `--schedule-id <id>` | 调度 ID |
| `--target-fps <n>` | 当模板无法暴露目标 FPS 时写入 `tasks[].targetFps` |

`init-scenario` 会识别模板中的直接 VLM 节点（`DA_00003`/`PDA_00003`）：VLM 场景生成 `type: vlm`，默认阶梯保持 60s；其他 CV 场景默认 30s。

## 判定口径

报告面向“路数上限”输出。默认 `capacity` 模式会从 1 路开始逐路增加并保持采样，直到达到配置上限或触发失败/熔断。

| 概念 | 含义 |
| --- | --- |
| 容量上限 | 最后一个完整执行、且所有报告阈值均 PASS 的连续路数 |
| 路数 PASS/FAIL | 按 `scenario.yml` 中 `thresholds` 的阈值对某个路数进行判定 |
| 瓶颈停止 | 运行期保护熔断，用于避免继续加压导致设备不可用 |
| 基线 FPS | 第 1 阶梯稳定窗口的最低处理 FPS；CV 为检测吞吐，直接 VLM 为推理完成吞吐 |

例如某次运行中：

- 1-15 路均 PASS
- 16 路平均丢弃率 `0.0418 > 0.02`，因此 16 路 FAIL
- 工具停止继续加压

此时准确结论是“容量上限 15 路”。失败路数及失败原因仍会保留在报告明细中。

如需只按 `scenario.yml` 中列出的阶梯执行，可显式使用 `--profile configured`。此模式只能给出已验证配置点，不能给出连续容量上限。

## 指标聚合

每个阶梯会持续采样。为了降低通道刚绑定时的瞬态抖动影响，报告汇总只使用该阶梯后半段采样点作为稳定窗口。

主要指标口径：

| 指标 | 聚合方式 |
| --- | --- |
| 处理 FPS | CV 任务取检测吞吐；直接 VLM 任务使用稳定窗口累计完成数差分计算推理完成吞吐，避免低频任务被单个采样点 0 FPS 误判 |
| 检测 FPS | 前置 CV 检测吞吐，仅作诊断参考，不作为 VLM 任务的处理吞吐 |
| VLM 平均延时 | RunningDetail 中 Qwen3VL 节点的 `durationAvgUs`，换算为毫秒 |
| 平均丢弃率 | 先计算每个通道稳定窗口平均丢弃率，再对通道取平均；报告 PASS/FAIL 使用该值 |
| 最差通道丢弃率 | 先计算每个通道稳定窗口平均丢弃率，再取通道间最大值；用于辅助定位单路异常，不作为默认容量判定 |
| 检测节点延时 | 单通道稳定窗口内取平均，再取通道间最大值 |
| 关键链路延时 | CV 包含解码、检测、跟踪、分类、判断；直接 VLM 任务额外加入 Qwen3VL 节点平均耗时 |
| NPU/CPU/内存峰值 | 稳定窗口内硬件资源峰值 |

报告 PASS/FAIL 使用 `scenario.yml` 中的 `thresholds`。运行期瓶颈停止使用独立保护规则，典型规则包括：

- 稳定窗口最低 FPS 低于第 1 阶梯基线的 50%
- 运行期通道平均丢弃率连续采样超过 5%
- CPU、NPU 或内存连续高位运行

VLM 模式还执行有效性校验：缺少直接 VLM 节点遥测时，不会继续使用前置 detector
指标给出 PASS。`BA_00004` 内嵌审核由于平台接口不可观测，ScenarioBench 会直接拒绝
运行并给出原因。

## 输入模式

当前主要支持 `local` 模式：工具将本地视频上传到设备，再创建本地视频通道。该模式适合作为容量基准，因为输入内容固定，跨版本和跨设备对比更稳定。

`rtsp-fidelity` 和 `rtsp-deterministic` 是预留模式，适合后续扩展到生产 RTSP 保真或可控 RTSP 推流。

## 编排导入

默认运行会调用 `algorithm/layout/save` 将 `algorithm-template.json` 导入设备。这个文件应来自 Web 页面中已配置完成的场景任务导出包，包含算法流程、原子能力、节点参数和任务配置等信息。工具不会自动设计或生成算法流程，只负责导入已有编排并对其做并发压测。

工具会对平台导出的模板做必要的 payload 归一化，例如将 `algorithmCategory`、`algorithmUsage` 转为字符串，以适配后端 DTO。

仅当确认设备上已经存在对应算法编排时，才建议使用 `--skip-import`。如果跳过导入但设备上没有对应编排，任务绑定可能失败，采样会缺失有效数据。

## 使用建议

- 每次正式压测前先运行 `doctor`。
- 正式压测建议使用 `--cleanup`，避免通道残留影响后续测试。
- 不同设备、软件版本、算法模板或输入视频应使用不同输出目录。
- 对外引用结果时优先使用 `summary.json` 和 `report.html` 首页结论。
- 如需做版本趋势对比，使用 `summary.json` 汇总，不直接解析 HTML。

## 当前限制

- 当前主要验证 `local` 视频模式。
- 报告为静态 HTML，暂不包含资源曲线图。
- 事件、MQTT、HTTP 推送等业务闭环校验尚未纳入默认判定。
- `metrics.json` 体积较大，适合程序分析，不建议人工直接阅读。
