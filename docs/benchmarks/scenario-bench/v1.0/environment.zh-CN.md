# ScenarioBench v1.0 基准测试环境

本目录保存 CosmoEdge v1.0 发布文档使用的脱敏后基准测试摘要。

## 硬件档位

### npu-yy-16t01-preview

- 设备型号：YY-16T01-Preview
- 设备标识：已脱敏为 NPU-Device-A 和 NPU-Device-B
- 软件版本：V1.0.0.0
- 硬件内核：SMP Fri Nov 28 11:48:56 CST 2025
- 角色：面向高并发 CV 工作负载和 VLM 推理的 NPU 加速边缘节点。

### x86-cpu-baseline

- 设备型号：X86-TRIAL
- 设备标识：已脱敏为 X86-Baseline
- 软件版本：V0.1.0.0
- 角色：仅 CPU 的对照基线。该 benchmark 用于展示通用 CPU 执行与 v1.0 NPU 加速设备档位之间的容量差异。

## 模型与视频输入

- 检测模型：YOLOv8n.pt，模型 ID 6047042，版本 V1.0.0
- 分类模型：YOLOv8n-cls.pt，模型 ID 7486163，版本 V1.0.0
- 模型来源：https://docs.ultralytics.com/
- 测试视频：data/test-video/Safety Helmet.mp4

## 发布文件

每个场景目录保留 README 和发布文档需要长期引用的公开文件：

- summary.json：脱敏后的机器可读 benchmark 摘要。
- report.html：脱敏后的人类可读 ScenarioBench 报告。

完整原始 metrics 不提交到仓库。需要原始采样数据时，请使用 v1.0 release asset 中的 metrics.json。
