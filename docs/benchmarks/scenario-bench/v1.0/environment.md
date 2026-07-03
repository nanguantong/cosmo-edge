# ScenarioBench v1.0 Benchmark Environment

This directory contains sanitized benchmark summaries used by the CosmoEdge v1.0 release documentation.

## Hardware Profiles

### npu-yy-16t01-preview

- Device model: YY-16T01-Preview
- Device identifiers: anonymized as NPU-Device-A and NPU-Device-B
- Software version: V1.0.0.0
- Hardware kernel: SMP Fri Nov 28 11:48:56 CST 2025
- Role: NPU accelerated edge node for high-concurrency CV workloads and VLM inference.

### x86-cpu-baseline

- Device model: X86-TRIAL
- Device identifier: anonymized as X86-Baseline
- Software version: V0.1.0.0
- Role: CPU-only comparison baseline. This benchmark is included to show the capacity gap between general-purpose CPU execution and the NPU accelerated v1.0 device class.

## Model And Video Inputs

- Detection model: YOLOv8n.pt, model ID 6047042, version V1.0.0
- Classification model: YOLOv8n-cls.pt, model ID 7486163, version V1.0.0
- Model source: https://docs.ultralytics.com/
- Test video: data/test-video/Safety Helmet.mp4

## Published Files

Each scenario directory keeps the public, long-lived artifacts needed by README and release documentation:

- summary.json: sanitized machine-readable benchmark summary.
- report.html: sanitized human-readable ScenarioBench report.

Full raw metrics are intentionally not committed here. Keep metrics.json in the v1.0 release asset package when raw sampling data is needed.