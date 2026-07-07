# ScenarioBench 当前基准刷新报告

本目录保存 v1.0 发布文档冻结后刷新生成的 benchmark 报告。发布策略与 v1.0 benchmark 材料一致：仓库只提交公开摘要和 HTML 报告，原始 `metrics.json` 不放入仓库。

## 汇总

| ID | 场景 | 硬件档位 | 目标 FPS | 最大稳定路数 | 结果 | 英文报告 |
| --- | --- | --- | ---: | ---: | --- | --- |
| [vlm-77175-npu](vlm-77175-npu/report.zh-CN.html) | VLM 算法压测 (77175) | npu-yy-16t01-preview | 0.1 | 8 | 通过 | [English](vlm-77175-npu/report.html) |

## 说明

- VLM 报告使用更新后的 ScenarioBench VLM 口径：local 模式按 `targetFps` 注入取帧率，阶梯保持 60s，并以稳定窗口累计完成数计算 VLM 吞吐。
- 该报告基于同一 YY-16T01-Preview NPU 设备档位，作为 benchmark refresh 发布，不代表新的产品版本。
