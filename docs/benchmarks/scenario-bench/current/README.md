# ScenarioBench Current Benchmark Refresh

This directory contains refreshed benchmark reports generated after the v1.0 release documentation freeze. These reports keep the same publication policy as the v1.0 benchmark artifacts: public summaries and HTML reports are committed, while raw `metrics.json` traces stay out of the repository.

## Summary

| ID | Scenario | Hardware profile | Target FPS | Max stable channels | Result | Chinese report |
| --- | --- | --- | ---: | ---: | --- | --- |
| [vlm-77175-npu](vlm-77175-npu/report.html) | VLM Review NPU Benchmark (77175) | npu-yy-16t01-preview | 0.1 | 8 | PASS | [zh-CN](vlm-77175-npu/report.zh-CN.html) |

## Notes

- The VLM report uses the refreshed ScenarioBench VLM methodology: local input is throttled by `targetFps`, each step holds for 60s, and throughput is computed from stable-window completion counters.
- The report is based on the same YY-16T01-Preview NPU device class and is published as a benchmark refresh, not as a new product release.
