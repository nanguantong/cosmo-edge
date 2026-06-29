<div align="center">

<img src="docs/assets/cosmoedge-logo.png" width="320" alt="CosmoEdge">

**面向视频智能分析的生产级 C++ 边缘 AI 引擎，支持可视化流水线编排与端侧 VLM 工作流**

[![License](https://img.shields.io/badge/license-Apache%202.0-blue?style=flat-square)](LICENSE)
[![Runtime](https://img.shields.io/badge/runtime-C%2B%2B17-orange?style=flat-square)](#c-原生运行时)
[![Platform](https://img.shields.io/badge/platform-Sophon%20BM1688%20%2F%20x86%20Linux%20%2F%20Windows-purple?style=flat-square)](#支持平台)
[![Release](https://img.shields.io/badge/release-v0.1.0-green?style=flat-square)](https://github.com/cosmo-wander-ai/cosmo-edge/releases)
[![Stress Test](https://img.shields.io/badge/stress%20test-200%20video%20samples-brightgreen?style=flat-square)](#验证与性能)
[![Pipelines](https://img.shields.io/badge/pipelines-26%20validated-brightgreen?style=flat-square)](#验证与性能)

[快速开始](#快速开始) | [核心特性](#核心特性) | [验证与性能](#验证与性能) | [文档](#文档) | [硬件](#cosmoedge-ready-设备)

[English](README.md) | 简体中文

</div>

---

> 本文档与英文 README 保持同步更新。如内容存在差异，请以英文版和最新发布说明为准。

<div align="center">

https://github.com/user-attachments/assets/96eeba7e-5b00-4c54-97b3-3ee4571cd5a0

</div>

*在单台边缘设备上运行多路 AI 流水线、实时 OSD 叠加和事件输出。*

CosmoEdge 是一款面向生产级视频智能分析的 C++ 边缘 AI 引擎。它帮助团队从模型文件走向可运行应用：导入模型、编排流水线、接入视频源、在浏览器中查看 AI 叠加画面，并通过 MQTT 或 HTTP 推送结构化事件。

其 C++17 运行时负责多路视频处理、硬件解码、OSD 渲染和低开销边缘部署，提供在现场部署、监控、调试和维护边缘 AI 应用所需的运行时、Web 控制台和集成路径。

## 你可以构建什么

- 多摄像头安全监测，支持实时 OSD 和告警截图。
- 人流统计、越线检测、区域入侵和交通流量统计。
- 端侧 VLM 驱动的视觉巡检流程。
- 基于 GroundingDINO 文本提示词的长尾目标检测。
- 带模型管理、场景任务、告警和数据推送的边缘 AI 部署。

## 核心特性

### C++ 原生运行时

CosmoEdge 以 C++17 运行时为核心。对边缘视频系统来说，解码、推理调度、OSD 渲染、事件生成和流输出都需要在资源受限的设备上持续运行。

- 长期运行的多路视频负载开销更低。
- 直接集成硬件解码、NPU 运行时、内存池和流媒体组件。
- 面向设备化边缘部署，CPU、内存和线程行为更可预测。
- x86 开发模式和 Sophon 生产部署使用同一套引擎核心。

### 可视化流水线编排

在浏览器中构建视频 AI 工作流。通过可视化流水线编辑器连接视频源、AI 模型、后处理节点、OSD 渲染、告警规则和输出通道。

<div align="center">

https://github.com/user-attachments/assets/c9673081-ad73-4455-9486-1a3021358cdd

</div>

### 应用工作流

CosmoEdge 将运行时、Web 控制台和集成层连接成一条部署工作流：

```text
模型仓库 -> 场景任务 -> 实时分析 -> 告警管理 -> 数据推送
   |          |          |          |          |
上传/管理   配置流水线  多路 AI +   规则引擎   MQTT / HTTP
ONNX/bmodel 按场景绑定  OSD 叠加    告警截图   webhook
模型版本    摄像头      WebRTC 流   事件日志   集成
```

<details>
<summary><b>完整能力列表</b></summary>

| 模块 | 能力 |
| --- | --- |
| 模型仓库 | 模型上传、元数据管理、版本管理、热替换流程 |
| 场景任务 | 流水线绑定、摄像头绑定、任务调度、场景级配置 |
| 实时分析 | RTSP、视频文件、USB 摄像头、WebRTC 实时预览、HTTP-FLV 兜底 |
| 图片分析 | 批量图片上传、VLM 分析、结构化结果 |
| 告警管理 | 基于规则的告警、严重等级、告警截图、筛选、事件历史 |
| 数据集成 | MQTT 推送、HTTP webhook、结构化 JSON 事件格式 |
| 系统管理 | 仪表盘、设备状态、用户认证、国际化、配置管理 |

</details>

### 实时可视化调试

CosmoEdge 内置 OSD 系统，面向操作人员和开发者：

- 使用业务标签，避免直接展示原始模型类别名。
- 使用语义化颜色区分正常、预警、违规和不确定状态。
- 支持区域叠加、越线指示、计数器和事件面板。
- 调试视图可展示原始检测框、置信度、跟踪 ID 和模型输出。

### 提示词驱动 AI：GroundingDINO + VLM

在边缘设备上运行提示词驱动的视觉模型，作为异步流水线节点与传统 CV 流水线一起使用：

| 能力 | 工作方式 | 典型用途 |
| --- | --- | --- |
| GroundingDINO | 文本提示词 -> 开放词表目标检测 | 无需针对特定类别训练即可查找长尾目标 |
| 端侧 VLM | 封闭式问题 -> YES/NO 状态判断 | “柜门是否打开？” -> YES 时触发告警 |
| VLM 图片分析 | 图片上传 -> 结构化视觉检查 | 质量检测、合规检查 |

<div align="center">

https://github.com/user-attachments/assets/f47b541e-0d01-437d-86e1-4183f6e610fd

</div>

端侧 VLM 节点支持兼容的 Qwen3 VLM 系列和 Qwen3.5 多模态模型。认证设备包可提供 `CosmoEdge-VL-Judge-0.8B`，面向 YES/NO 视觉状态判断优化。

### 模型来源

**CosmoEdge 已支持：**

| 类别 | 支持模型/架构 | 流水线支持 |
| :--- | :--- | :--- |
| 目标检测 | YOLOv5、YOLOv8、YOLOv10、YOLOv11、YOLOv12、YOLO26 | 完整流水线 |
| 目标跟踪 | ByteTrack | 完整流水线 |
| 属性分类 | 安全帽、反光背心、工服等分类器 | 完整流水线 |
| 计数与统计 | 越线统计、区域计数、方向流量 | 完整流水线 |
| 开放词表检测 | GroundingDINO | 异步流水线节点 |
| 视觉状态判断 | Qwen3 VLM 系列、Qwen3.5 多模态模型（文本提示词 -> YES/NO） | 异步流水线节点 |
| 图片分析 | VLM 批量分析 | 独立任务 |

**模型生态兼容性：**

CosmoEdge 使用 ONNX 作为模型交换格式。来自主流 CV 训练框架的模型可以按文档中的转换路径导入：

- **Ultralytics (YOLO)**：使用 `yolo export format=onnx` 导出，然后通过模型仓库导入，或转换为 bmodel 用于 NPU 部署。
- **Roboflow**：在 Roboflow 上训练，导出 ONNX，再导入 CosmoEdge。
- **自定义模型**：任何兼容 ONNX 的检测或分类模型，都可以通过模型适配指南接入。

**Sophon 模型生态：**

CosmoEdge 运行在 Sophon BM1688 推理栈上。SOPHGO 官方模型仓库中的模型可以通过模型适配指南接入，指南覆盖后处理适配和流水线节点注册。

→ [SOPHGO Model Zoo (sophon-demo)](https://github.com/sophgo/sophon-demo)
→ [CosmoEdge Model Porting Guide](docs/tutorials/05-model-porting/model-porting.md)

## 快速开始

### 方案 A：x86 开发模式

无需边缘硬件即可试用 CosmoEdge。x86 开发模式使用与边缘部署相同的 UI 和工作流，但吞吐低于 Sophon NPU 模式。

```bash
# 1. Clone
git clone https://github.com/cosmo-wander-ai/cosmo-edge.git
cd cosmo-edge

# 2. 启动 x86 模式
# Linux：
sudo docker compose -f docker-compose.x86.yml up -d --build

# Windows (PowerShell/CMD)：
docker compose -f docker-compose.x86.windows.yml up -d --build

# 3. 打开 Web 控制台
# http://localhost:8080
```

> **USB 摄像头**：如有 USB 摄像头设备，请在启动前取消 `docker-compose.x86.yml` 中 `devices` 段的注释。

启动后，参照 [场景配置教程](docs/tutorials/02-scenario-config/scenario-config.md) 设置你的第一个 AI 检测场景。

### 方案 B：Sophon 边缘设备

该路径用于 Sophon NPU 加速部署。

```bash
# 1. Clone
git clone https://github.com/cosmo-wander-ai/cosmo-edge.git
cd cosmo-edge

# 2. 构建 Sophon/aarch64 发布包
sudo bash scripts/build_sophon_package.sh

# 3. 查看导出的发布包
ls -lh build_output/
# 输出的包名格式如：cosmo-V<version>-<hash>.tar.gz

# 4. 将安装包拷贝到 Sophon 边缘设备上（将 <device_ip> 替换为设备的实际 IP，默认是 192.168.100.1）
scp build_output/cosmo-V*.tar.gz root@<device_ip>:/tmp/

# 5. SSH 登录设备，解压并执行 install.sh 安装脚本
ssh root@<device_ip>
cd /tmp
tar -zxvf cosmo-V*.tar.gz
sudo bash scripts/install.sh

# 6. 重启设备以启动服务
sudo reboot
```

在 Windows PowerShell 下构建发布包：

```powershell
.\scripts\build_sophon_package.ps1
```

安装完成并重启设备后：
- **默认 IP**：`192.168.100.1`（请确保你的电脑与设备处于同一网段，例如配置静态 IP 为 `192.168.100.x`）
- **登录地址**：`http://192.168.100.1`
- **默认用户名**：`admin`
- **默认密码**：`admin`（首次登录后建议修改）

该路径会构建发布包并安装到 Sophon 设备。需要生产硬件时，认证 CosmoEdge 设备可提供预配置 Sophon 加速、生产模型包和部署支持。参见 [CosmoEdge-ready 设备](#cosmoedge-ready-设备)。

## 验证与性能

CosmoEdge 源自商业化代码库，开源发布前已完成内部系统验证。

| 范围 | 当前验证状态 |
| --- | --- |
| 视频压力测试 | 使用 200 个视频样本进行连续播放测试，未观察到内存泄漏或崩溃 |
| 流水线验证 | 26 条流水线已按内部场景基线完成验证（涵盖 CV、VLM 与 GroundingDINO） |
| 并发 CV 负载 | 已在单台 BM1688 设备上验证 16 路 CV 推理 |
| 回归测试 | 专职 QA 已完成多轮系统回归，最终发布回归进行中 |
| 试点部署 | 已在脱敏后的教育、智慧园区和工业安全试点场景中验证 |

### 性能基准

下列数据来自内部记录中的代表性系统组合。一个视频通道表示一路已解码输入流；多个场景任务可以共享同一解码流。E2E 延迟表示在所列负载下从帧到 OSD 或从帧到事件的端到端延迟，不是单模型推理耗时。

| 负载 | 视频通道数 | 场景任务数 | FPS 目标 | E2E&nbsp;延迟&nbsp;(ms) | 硬件 | 说明 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| 全流 YOLOv8n 检测 | 16 | 16 | 3/channel | 32&#8209;68 | BM1688 | 解码、推理和 OSD 均开启；稳定高负载场景 |
| 共享解码密集 CV 任务 | 4 | 20 | 3/channel | 84&#8209;141 | BM1688 | 多个场景任务共享解码流，展示任务并发能力 |
| 安全合规流水线 | 16 | 16 | 3/channel | 182&#8209;314 | BM1688 | 检测 + 跟踪 + 属性/规则 + 告警，代表性安全业务流水线 |
| 提示词驱动 AI 流水线 | 8 | 8 | 0.2/channel | 3154&#8209;4128 | BM1688 | 认证 `CosmoEdge-VL-Judge-0.8B`；VLM 异步节点；事件驱动路径，不参与逐帧同步 OSD |
| 单路 YOLOv8n 检测 | 1 | 1 | 24/channel | 25&#8209;30 | BM1688 | YOLOv8n 开发与评估负载 |
| x86 开发模式 | 1 | 1 | 24/channel | 235&#8209;250 | x86 CPU<br />(Intel(R) Core(TM) i7-14700HX) | YOLOv8n 开发与评估负载 |

## 架构

```text
+---------------------------------------------------------------+
| Web Frontend                                                  |
| Pipeline Editor | Management Console | Real-time View          |
+-------------------------------+-------------------------------+
                                | REST / WebSocket / MQTT
                                v
+---------------------------------------------------------------+
| C++ Engine Core                                               |
| Flow Engine | Media Pipeline | Inference | Services            |
| Task/Action | Decode/Encode | CV/VLM/DINO | Alarm/Event/Model  |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
| Hardware Abstraction                                          |
| Sophon BM1688 NPU/VPU/VPP | x86 CPU                           |
+---------------------------------------------------------------+
```

### 技术栈

| 层级 | 技术 |
| --- | --- |
| 引擎 | C++17、CMake、FFmpeg、SQLiteCpp |
| 推理 | Sophon BMRT、x86 模式使用 ONNX Runtime |
| 前端 | Vue.js、Vue Flow、Element Plus |
| 流媒体 | SRS 6.0、WebRTC、HTTP-FLV |
| 集成 | REST API、WebSocket、MQTT、HTTP webhook |

## 支持平台

| 平台 | 状态 | 目标用途 |
| --- | :---: | --- |
| Sophon BM1688 | 主力平台 | NPU 加速生产部署 |
| x86 Linux | 已支持 | 开发、评估、集成测试 |
| x86 Windows | 已支持 | 开发和评估 |
| Sophon BM1684X | 规划中 | NPU 加速部署 |

## CosmoEdge-ready 设备

CosmoEdge 是开源项目。仓库提供认证设备包所使用的同一套引擎、Web UI 和工作流。你可以使用自带模型，在 x86 上开发，并部署到兼容的边缘硬件。

认证设备帮助团队减少硬件适配和模型打包工作，提供预配置 NPU 加速、生产模型包和专属部署支持。

| 能力 | 开源仓库 | 认证设备包 |
| --- | :---: | :---: |
| C++ 引擎 | 包含 | 包含 |
| 可视化流水线编排器 | 包含 | 包含 |
| Web 管理控制台 | 包含 | 包含 |
| x86 开发模式 | 包含 | 包含 |
| Sophon NPU 运行时支持 | 源码支持，需自备硬件 | 预配置 |
| CV 模型包 | 自带模型 | 预安装（约 25 个生产 CV 模型） |
| `CosmoEdge-VL-Judge-0.8B` | 自带或定制模型包，需自行验证 | 预安装认证模型包 |
| GroundingDINO 包 | 自带或定制模型包 | 预安装 |
| 部署支持 | 社区支持 | 专属支持 |

认证设备提升部署就绪度，不把软件能力锁定在特定硬件上。

[获取认证设备](https://cosmoedge.dev/hardware)

## 文档

| 入口 | 读者 | 说明 |
| --- | --- | --- |
| [文档首页](docs/index.md) | 所有用户 | 完整文档索引和阅读路径 |
| [快速开始](docs/tutorials/01-quickstart/quickstart.md) | 所有用户 | 完成首次启动和场景体验 |
| [场景配置](docs/tutorials/02-scenario-config/scenario-config.md) | 集成商 | 构建场景级 AI 工作流 |
| [VLM 指南](docs/tutorials/03-vlm-guide/vlm-guide.md) | 开发者 | 使用提示词完成视觉状态判断 |
| [流水线编排](docs/tutorials/04-pipeline-orchestration/pipeline-orchestration.md) | 高级用户 | 可视化编排自定义流水线 |
| [模型适配指南](docs/tutorials/05-model-porting/model-porting.md) | 算法工程师 | 导入 ONNX 或目标运行时模型 |
| [构建指南](docs/guide/build.md) | 开发者 | 构建 x86 Docker 和 Sophon 发布包 |
| [API 概览](docs/reference/api.md) | 开发者 | REST、WebSocket 和 MQTT API 类别 |

## 发布状态与路线图

CosmoEdge 当前版本为 `v0.1.0`，项目已处于 `v1.0` 候选发布状态。剩余发布门槛是最后一轮回归测试。

- [X] C++17 边缘推理引擎
- [X] 可视化流水线编排器
- [X] Web 管理控制台
- [X] x86 Linux 和 Windows 开发模式
- [X] Sophon BM1688 发布打包
- [X] VLM 与 GroundingDINO 集成
- [X] 26 条流水线场景完成内部验证
- [ ] `v1.0` 最后一轮回归测试
- [ ] `v1.0` 发布标签与发布说明
- [ ] 补充公开性能摘要
- [ ] 扩充已验证流水线场景库
- [ ] 社区模型和场景示例
- [ ] GB28181 协议支持

## 贡献

CosmoEdge 正在准备 v1.0 发布，欢迎围绕以下方向进行聚焦贡献：

- 带日志和复现步骤的 bug 报告。
- 文档修正和教程改进。
- 场景示例和集成说明。
- 先通过 issue 讨论，再提交范围清晰的小型 PR。

提交 pull request 前，请先阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。

## FAQ

<details>
<summary><b>我需要 Sophon 设备才能体验 CosmoEdge 吗？</b></summary>

不需要。你可以在 Linux 或 Windows 上使用 x86 开发模式体验 UI、流水线工作流、模型管理和集成路径。Sophon 硬件主要用于生产级 NPU 吞吐。

</details>

<details>
<summary><b>开源仓库包含模型权重吗？</b></summary>

开源仓库不默认包含生产模型权重。你可以自带模型，包括兼容的 Qwen3 VLM 系列和 Qwen3.5 多模态模型；认证设备包可提供约 25 个预安装的生产 CV 模型，外加 `CosmoEdge-VL-Judge-0.8B` 和 GroundingDINO。社区或自定义模型建议按目标场景验证效果。

</details>

<details>
<summary><b>可以使用自己训练的模型吗？</b></summary>

可以。CosmoEdge 围绕模型导入和模型生命周期管理设计。模型适配指南会说明从 ONNX 或目标运行时格式进入模型仓库的推荐路径。

</details>

<details>
<summary><b>CosmoEdge 和推理服务器或 NVR 项目有什么区别？</b></summary>

CosmoEdge 是面向完整边缘 AI 工作流的应用运行时，不是单纯的模型服务层或视频录像系统。

</details>

<details>
<summary><b>CosmoEdge 是否已经生产可用？</b></summary>

代码库源自面向生产部署的商业化开发，并通过了内部压力、流水线和回归验证。CosmoEdge 当前版本为 `v0.1.0`，项目正在完成 `v1.0` 前最后一轮回归测试。核心工作流和发布打包按现场交付需求设计；公共 API 和贡献者流程在 v1.0 前仍可能有少量稳定性调整。

</details>

## 联系方式

- Community: [GitHub Discussions](https://github.com/cosmo-wander-ai/cosmo-edge/discussions)
- Partnership & Enterprise: hello@cosmowander.ai
- 安全报告：请参见 [SECURITY.md](SECURITY.md)，按私密披露流程上报安全漏洞

## License

CosmoEdge 使用 [Apache License 2.0](LICENSE) 开源许可。

```text
Copyright 2026 CosmoEdge Contributors

Licensed under the Apache License, Version 2.0
```

---

<div align="center">

An open-source project by Cosmo Wander AI and the CosmoEdge contributors.

Turn video AI models into deployable edge applications.

</div>
