# scenario-bench

CosmoEdge 场景任务可复现压测工具。运行在控制 PC，通过设备 HTTP API 驱动边缘设备完成并发阶梯压测，周期采样任务运行详情与硬件资源，自动判定算力瓶颈拐点，输出 JSON / HTML 报告。

设计依据见 [`docs/guide/scenario-benchmark-design.md`](../../docs/guide/scenario-benchmark-design.md)。

> 本文档既是工具说明，也是**操作指南**——包含真实设备上踩过的坑和对应处置方式，照着「快速开始 → 实战流程 → 常见问题」即可跑通一轮压测。

---

## 一、安装

```bash
cd tools/scenario-bench
npm install        # 仅依赖 yaml
```

要求 **Node >= 20**（使用原生 `fetch` / `node:crypto`）。实测 Node 22/24 均可。

---

## 二、快速开始（一轮完整压测）

> 前置：设备已上电、Web 可访问、账号密码默认 `admin / admin`、场景包里的编排模板可被 `layout/save` 导入（工具已做类型归一化，全量导出包直接用）。

```bash
cd tools/scenario-bench

node src/cli.js run \
  --device http://192.168.0.22 \
  --user admin \
  --password admin \
  --scenario scenarios/no-helmet-99898-fps5-20260630 \
  --output reports/no-helmet-20260630 \
  --cleanup --verbose
```

跑完后在 `reports/no-helmet-20260630/` 下：

- `metrics.json` —— 全量采样数据（每个 tick 的 FPS / 丢弃率 / 资源），程序化分析用
- `report.html` —— 人读报告，含阶梯判定与瓶颈拐点
- `metrics.partial.json` —— 运行中实时写入，进程崩了也能保住已采数据

### 关于 `layout/save`（编排导入，已修复）

工具会在开跑前自动调用 `algorithm/layout/save` 把场景包里的编排模板导入设备。平台最新导出的全量包（100KB+）携带 `algorithmCategory` / `algorithmUsage` 等**数值型字段**，而后端 DTO（`AlgorithmDto_Layout.cc` 的 `MsgLayoutSaveRecv`）把这些字段全部声明为 `std::string`——nlohmann `get_to<std::string>()` 遇到 JSON 数字会抛 `type_error`，被路由层报成 **code 24「参数异常」**。

工具已在 `layoutSavePayload`（`scenario-package.js`）里做了**类型归一化**：只组装 DTO 认识的字段，并把 category/usage 强制转字符串。所以现在**全量导出包可以直接用，无需 `--skip-import`**，也无需手工裁剪模板。

仅当**编排模板已在设备上、且不想再触发导入**时才加 `--skip-import`（例如复测同一个算法）。注意：`--skip-import` 跳过导入后，工具仍用 `scenario.yml` 的 `algorithmId` 去绑定任务——如果设备上没有这套编排，任务会绑不上、采样全是空转（详见 Q1）。

---

## 三、实战流程（逐步）

### 1. 准备场景包

一个场景 = `scenarios/<name>/` 目录，含 4 个文件：

| 文件 | 说明 | 来源 |
| --- | --- | --- |
| `algorithm-template.json` | 算法编排导出包（字段值含 JSON 字符串需二次解析） | 平台「导出」得到 |
| `scenario.yml` | 名称、algorithmId、scheduleId、采样间隔、并发阶梯 `loadProfile` | 手写 |
| `thresholds.yml` | PASS/FAIL 判定阈值 | 手写 |
| `videos.yml` | 视频源模式与来源 | 手写 |
| `<video>.mp4` | local 模式的视频样本 | 自备，**固定不变**才可复现 |

可直接复制 `scenarios/no-helmet-99898-fps5-20260630/` 作为模板改。改名时同步改 `scenario.yml` 的 `name` 字段（它决定通道前缀 `bench-<name>`）。

### 2. 校验场景包能正确解析（跑之前先验，省得跑到一半失败）

```bash
node -e "import('./src/scenario-package.js').then(({ScenarioPackage})=>{
  const p=new ScenarioPackage('scenarios/no-helmet-99898-fps5-20260630').load();
  console.log({algo:p.algorithmId, fps:p.targetFps, mode:p.videoMode, confVer:!!p.template.confVersionId, profile:p.loadProfile});
})"
```

期望输出类似：`{ algo:'99898', fps:5, mode:'local', confVer:true, profile:[...] }`。`fps` 为 `null` 说明模板里没找到 `AA_00001` 节点的 fps，吞吐比值判定会被跳过（不影响瓶颈判定）。

### 3. 确认设备状态再开跑

```bash
# Web 可达 + 登录端点活着
curl -s -m 8 -X POST http://192.168.0.22/gtw/cwai/login/dologin \
  -H "Content-Type: application/json" -d "{}" | head -c 200
```

返回含 `"msgText"` 的登录失败提示是正常的（没带账号），说明端点活着。详见「常见问题」关于 **eMMC 临界** 和 **设备业务接口超时** 的处置。

### 4. 启动压测（后台跑，约 8–15 分钟）

见「快速开始」命令。一轮 5 阶梯 × 30s + 24 路视频上传，总时长约 3–5 分钟（取决于上传速度）。建议加 `--verbose` 并 `tee` 到日志：

```bash
node src/cli.js run --device ... --skip-import --cleanup --verbose 2>&1 \
  | tee reports/run-<scenario>-<date>.log
```

### 5. 看结果

关键日志行（grep 出来即可判断）：

```
[INFO] [baseline] step 1 steady minFps=... → gate at fps<... or discard>0.05
[INFO] [step N] steady minFps=... meanDiscard=... npu=... cpu=... → ok, continuing
[WARN] [step N] ramp fuse tripped: ...        ← 瓶颈触发，自动停在此阶梯
[INFO] Report written: ...
```

- **`→ ok, continuing`** = 该阶梯通过，继续加压。
- **`ramp fuse tripped` / `BOTTLENECK`** = 拐点，工具自动停止后续加压（不会傻跑到 24 路把设备拖死）。
- `metrics.json` 顶层 `status` / `bottleneck` / `baselineFps` 是结构化结论。

---

## 四、判定逻辑（怎么算「瓶颈」）

工具用**两级熔断**，都用稳态采样（每阶梯保持期的后半段，排除刚绑定时 fps/丢弃率的爬坡抖动）：

| 判据 | 阈值 | 含义 |
| --- | --- | --- |
| FPS 折半 | 稳态 minFps < baseline × 0.5 | 并发上去后实测处理帧率比第 1 阶梯基线跌掉一半 |
| 丢弃率 | **均值** discard > 5%，连续 2 次采样 | 队列开始持续丢帧，算力跟不上输入 |
| 内存熔断 | 内存 ≥ 98% 连续 3 次 / 或 60s 均值 ≥ 95% | 接近 OOM |
| CPU/NPU | ≥ 98% 连续 3 次 | 资源饱和 |

**首阶梯（1 路）的稳态 minFps 作为基线**，后续阶梯与之对比。命中任一熔断即记录瓶颈拐点并提前停止。

### 为什么丢弃率用「均值」而非「最大值」

丢弃率判定取**一个阶梯稳态期内所有通道 × 所有 tick 的丢弃率均值**，而不是单点最大值。原因：单点 max 对瞬态扰动（GC 停顿、刚绑定通道的稳定期、偶发尖峰）过于敏感，容易**误判中断**——实测中 16 路 NPU 已到 96% 但均值丢弃仅 1.7%，若用 max 某个尖峰大概率 >5% 就会提前误停；改用均值后能正确「继续加压」到真实拐点。均值平滑了瞬态，同时仍能捕捉到「丢弃率持续高位」这种真正的过载。

### 延时指标用「通道内均值 + 通道间最大」

报告里的 **关键链路延时** / **检测节点延时** 同样不能取单点最大值，否则一次 GC 或调度抖动就会被层层 max 放大成整阶梯的判定值直接 FAIL。采用两层聚合：

1. **通道内**：单个通道在稳态期各 tick 的延时取**均值** —— 削掉单 tick 尖峰。
2. **通道间**：取所有「通道均值」里的**最大值** —— 保留「最慢那一路」这个真实瓶颈信号。

这与丢弃率「tick 间均值」的降噪思路一致。注意不能用全局均值：实测 16 路中有 35ms 的快路和 142ms 的慢路并存，全局均值（78ms）会被快路稀释、**掩盖真实瓶颈**；而「通道均值取 max」（142ms）既能反映最慢通道，又剔除了单 tick 抖动（旧的全局 max 是 158ms）。

阈值判定（`thresholds.yml` 的 `maxDetectorLatencyMs` / `maxCriticalPathLatencyMs`）比对的就是这个「最慢通道的平均延时」。

### 采样参数与保持期

- `sampleIntervalSec`（采样间隔）与每阶梯 `holdSec`（保持期）共同决定每阶梯采样点数：`ticks = floor(holdSec / sampleIntervalSec)`。
- 推荐配置：`holdSec: 30` + `sampleIntervalSec: 3` = **每阶梯 10 个点**（稳态后半 5 点），均值判定有足够样本抗扰动，单轮 5 阶梯约 3 分钟。
- 若加大保持期（如长稳测试），保持期越长，稳态点越多，均值越平滑；但单轮耗时也线性增长。

> local 模式下 demux 以**源视频原生帧率全速推帧**（实测 ~42fps，远超编排设定 fps），所以 `minProcessFpsRatio`（实测/设定）通常 ≫ 1 恒 PASS——**判瓶颈看并发上去后实测 fps 是否被压低、均值丢弃率是否上升**，而不是看 fps 比值。

---

## 五、命令行参数

```
必填:
  --device <url>              设备地址，例如 http://192.168.0.22
  --user <account>            登录账号
  --password <plain>          登录密码（内部 MD5 大写后传输）
  --scenario <dir>            场景包目录
  --output <dir>              报告输出目录

常用:
  --cleanup                   结束后删除本次创建的通道（推荐，避免残留）
  --verbose                   打印每 tick 采样日志（排障必加）
  --no-reuse                  不复用已存在的 bench 通道，总是新建
  --skip-import               跳过 algorithm/layout/save（仅复测已导入的算法时用；首次/换算法时不要加）

调优（一般不用改）:
  --ramp-batch-size <n>       每阶梯新增通道的批次大小，默认 1
  --ramp-batch-delay-sec <n>  批次间隔秒数，默认 15
  --lang <code>               Accept-Language，默认 zh-CN
  -h, --help                  帮助
```

---

## 六、场景包字段速查

### scenario.yml

```yaml
name: no-helmet-99898-fps5-20260630     # 决定通道前缀 bench-<name>
displayName: 未戴安全帽检测
algorithmId: "99898"                     # 字符串，对应设备算法 code
scheduleId: "e89c6c6385e5454b35cde0d1653vg"  # 平台默认 24/7 调度
sampleIntervalSec: 3                     # 采样间隔（推荐 3s，配合 30s 保持期 = 10 点/步）
videoRepeatCount: 0                      # 0=无限循环（local 持续产生负载）

loadProfile:                            # 并发阶梯，首阶梯作为 fps 基线
  - channels: 1
    holdSec: 30                          # 保持期 30s（推荐）
  - channels: 4
    holdSec: 30
  - channels: 8
    holdSec: 30
  - channels: 16
    holdSec: 30
  - channels: 24
    holdSec: 30
```

### videos.yml

```yaml
mode: local          # MVP 默认；rtsp-fidelity / rtsp-deterministic 属第二阶段

local:
  - name: no-helmet-01
    file: LX0000000007.mp4     # 相对场景目录；工具会分片上传到设备再建渠道
    # filePath: /data/.../x.mp4 # 也可直接指向设备上已有文件，跳过上传
```

| mode | 用途 | 说明 |
| --- | --- | --- |
| `local` | 容量基准（默认） | `Camera/AddVideo`，输入完全一致，可跨版本/跨设备对比 |
| `rtsp-fidelity` | 生产保真 | `Camera/Add`，真实 RTSP，验证丢包/重连（第二阶段） |
| `rtsp-deterministic` | 可复现 + 走网络 | 本地文件经 ffmpeg/MediaMTX 打 RTSP（第二阶段） |

---

## 七、关键实现细节（改代码前必读）

- **taskId 规则**：`videoChannelId + "_" + algorithmCode`（见 `CameraTaskUnit.cc`），工具本地计算，`RunningDetail` 用 taskId 过滤（非 channelId）。
- **取帧基准 fps**：从编排图 `AA_00001` 节点 `configObject.params.fps` 提取，作为 `minProcessFpsRatio` 分母。
- **RunningDetail 静默过滤**：action 数量 ≤ 2 的任务不出现在响应中，工具对缺失路标记 `missing: true`。
- **增量绑定**：local 视频任务 OFF 后无法再 ON（demux 不重启本地文件），故每阶梯只绑定**新增**通道，已运行的通道全程不打断，仅最终 teardown 全关。
- **每路独立上传**：`AddVideo` 会 move 掉 temp 文件，同一上传路径不能跨通道复用，所以 N 路 = N 次分片上传。
- **部分报告兜底**：即使中途设备挂起/超时导致中断，已采集的数据仍会写入 `metrics.partial.json` 并产出 `status=aborted` 的报告，不会静默丢失。

---

## 八、常见问题（真机实测踩坑）

### Q1：任务起不来，采样全是 `fps=- / discard=-`，CPU 1%

**根因几乎都是「场景模板与设备上的编排不一致」**。工具用 `scenario.yml` 的 `algorithmId` 去 `ApplyParamsBatch` 绑定任务，如果设备上没有这套编排（或编排版本对不上），绑定会 `partially failed`，任务空转，`RunningDetail` 采不到任何数据，所有熔断失效——白白加压到 24 路。

排查与处置：
1. **先正常跑（不加 `--skip-import`）**，让工具自动导入编排。`layout/save` 已做类型归一化，全量导出包可直接用。
2. 看日志里有没有 `Layout saved.` —— 没有就是导入失败（看是否 code 24）。
3. 看有没有 `ApplyParamsBatch partially failed on: LXxxxx` —— 有就是模板与设备编排对不上，换正确的场景模板重跑。
4. 切换算法版本（如 fps 3→5、code 15→99898）时，**必须换对应的 `algorithm-template.json`**，不能只改 `scenario.yml` 的 `algorithmId`。

### Q2：`layout/save` 报 code 24「参数异常」（历史问题，工具已修复）

平台导出包把 `algorithmCategory` / `algorithmUsage` 序列化成 JSON 数字，而后端 DTO 要 string。工具的 `layoutSavePayload` 已强制转字符串，**正常情况下不会再遇到**。若仍报 code 24，检查模板里这两个字段的类型，或临时加 `--skip-import`（前提是编排已在设备上）。

### Q3：eMMC 临界满（94–96%）影响压测吗？

**不影响吞吐判定，但影响「下一轮能不能上传起来」。** local 模式 eMMC 只在上传阶段写一次，推理热路径走内存不碰磁盘。但每轮 24 路 ≈ 480MB 上传文件，`--cleanup` 只删通道记录**不删设备临时文件**，多轮累积会把 eMMC 撑到真正写不进去。处置：

- 每轮开跑前清理设备 `/data/cwaiuserdata/tmp/model_upload/` 下历史 `chunk_*` 残留。
- 跑完检查 `metrics.json` 里 `eMMCUtilization` 是否还在涨；只升不降就该清理了。

### Q4：设备业务接口（Camera/Page、QueryHardwareResource）超时，但登录正常

常见于**刚结束一轮高负载压测**后，设备业务服务还在回收（删通道、回收内存）。等待 30–60s 后重试即可，不是工具问题。压测工具自身每个请求有 20s 超时。

### Q5：第一阶梯内存就偏高

这是设备常态（系统服务占用），不是工具引入的。**真正的瓶颈信号是「并发加上去后均值丢弃率连续超 5%」或「FPS 折半」**，不是单看内存绝对值。

### Q6：怎么换一台机器跑

只改 `--device`，其余参数和场景包完全不变——这正是 local 模式可复现的意义。建议每台机器用不同的 `--output` 目录归档，便于横向对比。

### Q7：想要单遍验证（不循环）

`scenario.yml` 里设 `videoRepeatCount: 1`，本地视频只读一轮。默认 `0` 是无限循环（持续负载）。

---

## 九、当前限制（MVP）

- 仅 `local` 视频模式；rtsp 两种模式与推流器属第二阶段。
- 报告为静态 HTML 表格，无资源曲线图（第二阶段）。
- 事件统计依赖后续为 `test/push-test-service` 补 `/stats` 端点。
- `RunningDetail` 当前不返回 task 级实测 FPS，报告中 `measuredFps` 为 `processCountPeriod / periodMs` 推算值。
